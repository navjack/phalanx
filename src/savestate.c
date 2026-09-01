#include "savestate.h"

#include "common_cpu_infra.h"
#include "common_rtl.h"
#include "cpu_state.h"
#include "snes/saveload.h"
#include "snes/snes.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* Chunk appended to SNESRecomp's snes_saveload blob. The framework snapshot
 * models the SNES devices and WRAM, but Phalanx executes the whole program on
 * the LLE tier: the live 65816 register file is the runtime's CpuState, and
 * the frame scheduler's continuation lives in PhalanxMachine. Neither is part
 * of snes_saveload, so a snapshot without this chunk cannot resume. */
enum {
  kExtraMagic = 0x584C4850u, /* "PHLX" */
  kExtraVersion = 1u,
};

static PhalanxMachine *s_machine;
static bool s_extra_seen;

/* One walker for both directions: SaveLoadInfo streams a field in or out
 * depending on how the caller built it, so save and load cannot drift.
 *
 * `saving` exists only for the sentinels. A SaveLoadInfo that hits end of
 * file stops writing to its destinations and leaves them holding whatever
 * they held, so a reader that seeded them with the expected constants would
 * accept a truncated chunk as a complete one. Reading starts them at zero
 * instead, and the chunk counts as present only when both the leading and
 * trailing sentinel survive the walk. */
static void WalkExtra(SaveLoadInfo *sli, bool saving) {
  uint32_t magic = saving ? kExtraMagic : 0;
  uint32_t version = saving ? kExtraVersion : 0;
  uint32_t tail = saving ? kExtraMagic : 0;
  sli->func(sli, &magic, sizeof(magic));
  sli->func(sli, &version, sizeof(version));

  sli->func(sli, &g_cpu.A, sizeof(g_cpu.A));
  sli->func(sli, &g_cpu.X, sizeof(g_cpu.X));
  sli->func(sli, &g_cpu.Y, sizeof(g_cpu.Y));
  sli->func(sli, &g_cpu.S, sizeof(g_cpu.S));
  sli->func(sli, &g_cpu.D, sizeof(g_cpu.D));
  sli->func(sli, &g_cpu.DB, sizeof(g_cpu.DB));
  sli->func(sli, &g_cpu.PB, sizeof(g_cpu.PB));
  sli->func(sli, &g_cpu.host_return_valid, sizeof(g_cpu.host_return_valid));
  sli->func(sli, &g_cpu.P, sizeof(g_cpu.P));
  sli->func(sli, &g_cpu.m_flag, sizeof(g_cpu.m_flag));
  sli->func(sli, &g_cpu.x_flag, sizeof(g_cpu.x_flag));
  sli->func(sli, &g_cpu.emulation, sizeof(g_cpu.emulation));
  sli->func(sli, &g_cpu._flag_N, sizeof(g_cpu._flag_N));
  sli->func(sli, &g_cpu._flag_V, sizeof(g_cpu._flag_V));
  sli->func(sli, &g_cpu._flag_Z, sizeof(g_cpu._flag_Z));
  sli->func(sli, &g_cpu._flag_C, sizeof(g_cpu._flag_C));
  sli->func(sli, &g_cpu._flag_I, sizeof(g_cpu._flag_I));
  sli->func(sli, &g_cpu._flag_D, sizeof(g_cpu._flag_D));
  sli->func(sli, &g_cpu.cycles, sizeof(g_cpu.cycles));
  sli->func(sli, &g_cpu.master_cycles, sizeof(g_cpu.master_cycles));

  sli->func(sli, &s_machine->resume_pc24, sizeof(s_machine->resume_pc24));
  sli->func(sli, &s_machine->frame_start_master,
            sizeof(s_machine->frame_start_master));
  sli->func(sli, &s_machine->frames, sizeof(s_machine->frames));

  /* Host-side state the guest can observe: the FastROM select bit paces
   * $80-FF code, and the last HDMAEN write drives the frame renderer. */
  sli->func(sli, &g_memsel, sizeof(g_memsel));
  sli->func(sli, &g_snesrecomp_last_hdmaen, sizeof(g_snesrecomp_last_hdmaen));

  /* APU pacing anchors. Restoring the guest clock without these makes the
   * first catch-up after a load charge the SPC700 the whole elapsed run. */
  sli->func(sli, &g_main_cpu_cycles_estimate,
            sizeof(g_main_cpu_cycles_estimate));
  sli->func(sli, &g_apu_pace_cycles_estimate,
            sizeof(g_apu_pace_cycles_estimate));
  sli->func(sli, &g_apu_last_sync_cycles, sizeof(g_apu_last_sync_cycles));
  sli->func(sli, &g_apu_last_sync_master, sizeof(g_apu_last_sync_master));

  sli->func(sli, &tail, sizeof(tail));

  s_extra_seen = magic == kExtraMagic && version == kExtraVersion &&
                 tail == kExtraMagic;
}

void PhalanxStateSaveExtra(SaveLoadInfo *sli) {
  if (s_machine)
    WalkExtra(sli, true);
}

void PhalanxStateLoadExtra(SaveLoadInfo *sli, uint32_t version) {
  (void)version;
  if (s_machine)
    WalkExtra(sli, false);
}

void PhalanxStateLoaded(uint32_t version) {
  (void)version;
  if (!s_machine || !s_extra_seen)
    return;
  /* cpu->ram aliases the runtime's g_ram; the pointer itself is host state
   * and is deliberately never serialized. */
  g_cpu.ram = g_ram;
  s_machine->failed = false;
  snes_frame_counter = (int)s_machine->frames;
}

/* ── In-memory snapshot, used to roll a failed load back ─────────────────── */

typedef struct MemSli {
  SaveLoadInfo base;
  uint8_t *buffer; /* NULL while sizing */
  size_t capacity;
  size_t used;
  bool load;
  bool overflow;
} MemSli;

static void MemSliFunc(SaveLoadInfo *sli, void *data, size_t n) {
  MemSli *mem = (MemSli *)sli;
  if (mem->buffer) {
    if (mem->used + n > mem->capacity) {
      mem->overflow = true;
      return;
    }
    if (mem->load)
      memcpy(data, mem->buffer + mem->used, n);
    else
      memcpy(mem->buffer + mem->used, data, n);
  }
  mem->used += n;
}

static void WalkAll(SaveLoadInfo *sli, bool saving) {
  snes_saveload(g_snes, sli);
  WalkExtra(sli, saving);
}

static uint8_t *CaptureRollback(size_t *size_out) {
  MemSli sizing = { { &MemSliFunc }, NULL, 0, 0, false, false };
  WalkAll(&sizing.base, true);

  uint8_t *buffer = (uint8_t *)malloc(sizing.used);
  if (!buffer)
    return NULL;
  MemSli writer = { { &MemSliFunc }, buffer, sizing.used, 0, false, false };
  WalkAll(&writer.base, true);
  if (writer.overflow) {
    free(buffer);
    return NULL;
  }
  *size_out = sizing.used;
  return buffer;
}

static void ApplyRollback(uint8_t *buffer, size_t size) {
  MemSli reader = { { &MemSliFunc }, buffer, size, 0, true, false };
  WalkAll(&reader.base, false);
  g_cpu.ram = g_ram;
  snes_frame_counter = (int)s_machine->frames;
}

/* ── Public interface ────────────────────────────────────────────────────── */

void PhalanxSaveStateBind(PhalanxMachine *machine) {
  s_machine = machine;
}

bool PhalanxSaveStatePrepare(const char *directory) {
  if (!directory || !directory[0])
    return false;
  if (chdir(directory) != 0) {
    fprintf(stderr, "Phalanx: could not enter %s: %s\n",
            directory, strerror(errno));
    return false;
  }
  if (mkdir("saves", 0755) != 0 && errno != EEXIST) {
    fprintf(stderr, "Phalanx: could not create %s/saves: %s\n",
            directory, strerror(errno));
    return false;
  }
  return true;
}

bool PhalanxSaveStateSave(int slot) {
  char path[64];
  if (!s_machine || s_machine->failed ||
      !PhalanxSaveStateSlotPath(slot, path, sizeof(path)))
    return false;

  RtlSaveSnapshot(path);

  /* RtlSaveSnapshot reports write failures to stderr and returns void; the
   * file it was asked to produce is the only observable result. */
  struct stat info;
  return stat(path, &info) == 0 && info.st_size > 0;
}

bool PhalanxSaveStateLoad(int slot) {
  char path[64];
  if (!s_machine || !PhalanxSaveStateSlotPath(slot, path, sizeof(path)))
    return false;

  struct stat info;
  if (stat(path, &info) != 0)
    return false;

  size_t rollback_size = 0;
  uint8_t *rollback = CaptureRollback(&rollback_size);

  s_extra_seen = false;
  const bool loaded = RtlLoadSnapshot(path);
  if (loaded && s_extra_seen) {
    free(rollback);
    return true;
  }

  /* A truncated file, a foreign snapshot, or one written before this chunk
   * existed leaves the SNES devices half-restored and the LLE continuation
   * pointing into the old run. Put the live machine back as it was. */
  if (rollback) {
    ApplyRollback(rollback, rollback_size);
    free(rollback);
  } else {
    s_machine->failed = true;
  }
  fprintf(stderr, "Phalanx: %s is not a resumable Phalanx state\n", path);
  return false;
}

