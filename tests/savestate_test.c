/*
 * Save-state round trip.
 *
 * SNESRecomp's snes_saveload models the SNES devices and WRAM, but Phalanx
 * runs the whole program on the LLE tier: the live 65816 register file is the
 * runtime's CpuState and the frame scheduler's continuation lives in
 * PhalanxMachine, neither of which the framework snapshot knows about. This
 * test fails if any of that host-side state stops being captured — a load
 * that silently resumes the wrong continuation looks fine for a few frames
 * and then diverges.
 */
#include "input.h"
#include "machine.h"
#include "savestate.h"

#include "common_cpu_infra.h"
#include "common_rtl.h"
#include "cpu_state.h"
#include "sha256.h"
#include "snes/ppu.h"
#include "snes/snes.h"
#include "spc_player.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

enum {
  kFrameWidth = 256,
  kFrameHeight = 224,
  kBootFrames = 240,
  kCompareFrames = 90,
};

/* Globals the runner expects the game layer to define (src/main.c owns these
 * in the app; here they only have to exist). */
bool g_new_ppu = true;
bool g_ws_active = false;
int g_ws_extra = 0;
SpcPlayer *g_spc_player = NULL;

void RtlApuLock(void) {}
void RtlApuUnlock(void) {}

void Die(const char *error) {
  fprintf(stderr, "savestate: fatal: %s\n", error ? error : "unknown");
  exit(1);
}

static const RtlGameInfo kGameInfo = {
  .title = "phalanx",
  .save_name_prefix = "phalanx",
  .state_save_extra = PhalanxStateSaveExtra,
  .state_load_extra = PhalanxStateLoadExtra,
  .on_state_loaded = PhalanxStateLoaded,
};

static uint32_t g_pixels[kFrameWidth * kFrameHeight];

typedef struct Blob {
  uint8_t bytes[192];
  size_t used;
} Blob;

static void Append(Blob *blob, const void *data, size_t size) {
  memcpy(blob->bytes + blob->used, data, size);
  blob->used += size;
}

/* The execution state a resumed run must reproduce: guest memory plus the
 * registers and scheduler continuation that decide the next instruction.
 * Rendering is deliberately not part of this — PhalanxMachineRender advances
 * PPU and HDMA line state, so a digest that rendered would not be a
 * read-only observation of the machine. */
static void StateDigest(const PhalanxMachine *machine, uint8_t out[32]) {
  Blob blob = { { 0 }, 0 };
  uint8_t ram_digest[32];
  sha256_compute(g_ram, 0x20000, ram_digest);
  Append(&blob, ram_digest, sizeof(ram_digest));
  Append(&blob, &machine->resume_pc24, sizeof(machine->resume_pc24));
  Append(&blob, &machine->frame_start_master,
         sizeof(machine->frame_start_master));
  Append(&blob, &machine->frames, sizeof(machine->frames));
  Append(&blob, &g_cpu.A, sizeof(g_cpu.A));
  Append(&blob, &g_cpu.X, sizeof(g_cpu.X));
  Append(&blob, &g_cpu.Y, sizeof(g_cpu.Y));
  Append(&blob, &g_cpu.S, sizeof(g_cpu.S));
  Append(&blob, &g_cpu.D, sizeof(g_cpu.D));
  Append(&blob, &g_cpu.DB, sizeof(g_cpu.DB));
  Append(&blob, &g_cpu.PB, sizeof(g_cpu.PB));
  Append(&blob, &g_cpu.P, sizeof(g_cpu.P));
  Append(&blob, &g_cpu.emulation, sizeof(g_cpu.emulation));
  Append(&blob, &g_cpu.master_cycles, sizeof(g_cpu.master_cycles));
  Append(&blob, &g_memsel, sizeof(g_memsel));
  Append(&blob, &g_snesrecomp_last_hdmaen, sizeof(g_snesrecomp_last_hdmaen));
  sha256_compute(blob.bytes, blob.used, out);
}

/* The same state plus the last frame the run produced. Only meaningful at the
 * end of a sequence of frames, which is how both runs below use it. */
static void FrameDigest(const PhalanxMachine *machine, uint8_t out[32]) {
  Blob blob = { { 0 }, 0 };
  uint8_t part[32];
  StateDigest(machine, part);
  Append(&blob, part, sizeof(part));
  sha256_compute((const uint8_t *)g_pixels, sizeof(g_pixels), part);
  Append(&blob, part, sizeof(part));
  sha256_compute(blob.bytes, blob.used, out);
}

/* One iteration of the app's loop: run the frame, then draw it. */
static bool RunFrames(PhalanxMachine *machine, unsigned count) {
  for (unsigned i = 0; i < count; i++) {
    if (!PhalanxMachineRunFrame(machine, 0)) {
      fprintf(stderr, "savestate: the machine failed at frame %llu\n",
              (unsigned long long)machine->frames);
      return false;
    }
    PhalanxMachineRender(g_pixels, kFrameWidth * sizeof(uint32_t));
  }
  return true;
}

static bool Same(const uint8_t a[32], const uint8_t b[32]) {
  return memcmp(a, b, 32) == 0;
}

static void Report(const char *label, const uint8_t digest[32]) {
  fprintf(stderr, "  %s ", label);
  for (unsigned i = 0; i < 8; i++)
    fprintf(stderr, "%02x", digest[i]);
  fputc('\n', stderr);
}

static uint8_t *ReadWholeFile(const char *path, size_t *size_out) {
  FILE *file = fopen(path, "rb");
  if (!file)
    return NULL;
  fseek(file, 0, SEEK_END);
  const long length = ftell(file);
  if (length <= 0) {
    fclose(file);
    return NULL;
  }
  rewind(file);
  uint8_t *data = (uint8_t *)malloc((size_t)length);
  if (!data || fread(data, 1, (size_t)length, file) != (size_t)length) {
    free(data);
    fclose(file);
    return NULL;
  }
  fclose(file);
  *size_out = (size_t)length;
  return data;
}

static bool WriteFile(const char *path, const uint8_t *data, size_t size) {
  FILE *file = fopen(path, "wb");
  if (!file)
    return false;
  const bool ok = fwrite(data, 1, size, file) == size;
  fclose(file);
  return ok;
}

/* Copy `source` to `destination` with the last `drop` bytes cut off. */
static bool CopyTruncated(const char *source, const char *destination,
                          size_t drop) {
  size_t size = 0;
  uint8_t *data = ReadWholeFile(source, &size);
  if (!data || size <= drop) {
    free(data);
    return false;
  }
  const bool ok = WriteFile(destination, data, size - drop);
  free(data);
  return ok;
}

int main(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr, "usage: phalanx_savestate_test <rom>\n");
    return 2;
  }

  size_t rom_size = 0;
  uint8_t *rom = ReadWholeFile(argv[1], &rom_size);
  if (!rom) {
    fprintf(stderr, "savestate: could not read %s\n", argv[1]);
    return 2;
  }

  RtlRegisterGame(&kGameInfo);
  if (!SnesInit(rom, (int)rom_size)) {
    fprintf(stderr, "savestate: SNESRecomp rejected %s\n", argv[1]);
    free(rom);
    return 2;
  }
  free(rom);

  PhalanxMachine machine;
  if (!PhalanxMachineInit(&machine, g_rom, rom_size)) {
    fprintf(stderr, "savestate: invalid interrupt vectors\n");
    return 2;
  }
  PhalanxSaveStateBind(&machine);

  char work_dir[] = "/tmp/phalanx-savestate-XXXXXX";
  if (!mkdtemp(work_dir) || !PhalanxSaveStatePrepare(work_dir)) {
    fprintf(stderr, "savestate: could not prepare a state directory\n");
    return 2;
  }

  if (!RunFrames(&machine, kBootFrames))
    return 1;

  uint8_t at_save[32];
  StateDigest(&machine, at_save);

  if (PhalanxSaveStateLoad(0)) {
    fprintf(stderr, "savestate: an empty slot reported a successful load\n");
    return 1;
  }
  uint8_t after_empty_load[32];
  StateDigest(&machine, after_empty_load);
  if (!Same(at_save, after_empty_load)) {
    fprintf(stderr, "savestate: a failed load disturbed the machine\n");
    return 1;
  }

  if (!PhalanxSaveStateSave(0)) {
    fprintf(stderr, "savestate: slot 1 could not be written\n");
    return 1;
  }
  if (!PhalanxSaveStateSlotUsed(0, NULL, 0)) {
    fprintf(stderr, "savestate: slot 1 reads as empty after saving\n");
    return 1;
  }

  if (!RunFrames(&machine, kCompareFrames))
    return 1;
  uint8_t first_run[32];
  FrameDigest(&machine, first_run);
  uint8_t first_run_state[32];
  StateDigest(&machine, first_run_state);
  if (Same(at_save, first_run_state)) {
    fprintf(stderr, "savestate: %d frames changed nothing; the test cannot "
                    "tell a resumed run from a stalled one\n", kCompareFrames);
    return 1;
  }

  if (!PhalanxSaveStateLoad(0)) {
    fprintf(stderr, "savestate: slot 1 could not be loaded\n");
    return 1;
  }
  uint8_t restored[32];
  StateDigest(&machine, restored);
  if (!Same(at_save, restored)) {
    fprintf(stderr, "savestate: the loaded state does not match the saved "
                    "state\n");
    Report("saved   ", at_save);
    Report("restored", restored);
    return 1;
  }

  if (!RunFrames(&machine, kCompareFrames))
    return 1;
  uint8_t second_run[32];
  FrameDigest(&machine, second_run);
  if (!Same(first_run, second_run)) {
    fprintf(stderr, "savestate: the resumed run diverged from the original "
                    "after %d frames\n", kCompareFrames);
    Report("original", first_run);
    Report("resumed ", second_run);
    return 1;
  }

  /* A slot that is not a complete Phalanx state must be refused outright
   * rather than half-applied: the framework walker restores the SNES devices
   * before anything can notice the file is short, so without the rollback a
   * bad file would leave the LLE continuation pointing into a foreign run.
   *
   * Two shapes, because they fail at different places. The first carries a
   * valid framework header over a body that runs out mid-device. The second
   * is a genuine state with its tail cut off, which is the only thing that
   * catches a chunk that starts correctly and then stops. */
  static const uint8_t kShortBody[64] = { 0x53, 0x4c, 0x54, 0x52, 5, 0, 0, 0 };
  if (!WriteFile("saves/phalanx1.sav", kShortBody, sizeof(kShortBody)) ||
      !CopyTruncated("saves/phalanx0.sav", "saves/phalanx2.sav", 4)) {
    fprintf(stderr, "savestate: could not write the damaged slots\n");
    return 2;
  }

  for (int damaged = 1; damaged <= 2; damaged++) {
    uint8_t before_load[32];
    StateDigest(&machine, before_load);
    if (PhalanxSaveStateLoad(damaged)) {
      fprintf(stderr, "savestate: damaged slot %d reported a successful "
                      "load\n", damaged + 1);
      return 1;
    }
    uint8_t after_load[32];
    StateDigest(&machine, after_load);
    if (!Same(before_load, after_load)) {
      fprintf(stderr, "savestate: rejected slot %d left the machine "
                      "changed\n", damaged + 1);
      Report("before", before_load);
      Report("after ", after_load);
      return 1;
    }
  }

  unlink("saves/phalanx0.sav");
  unlink("saves/phalanx1.sav");
  unlink("saves/phalanx2.sav");
  rmdir("saves");
  rmdir(work_dir);

  printf("save states: %d-frame round trip, rejection, and rollback passed\n",
         kCompareFrames);
  return 0;
}
