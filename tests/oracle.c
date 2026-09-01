#include "input.h"
#include "audio_trace.h"
#include "snes/apu.h"
#include "snes/cart.h"
#include "snes/dma.h"
#include "snes/dsp.h"
#include "snes/dsp_shadow.h"
#include "snes/interp816.h"
#include "snes/ppu.h"
#include "snes/snes.h"
#include "snes/spc.h"
#include "types.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

uint8_t g_ram[0x20000];
Snes *g_snes;
Ppu *g_ppu;
Dma *g_dma;
bool g_new_ppu = true;
bool g_fail = false;
bool g_ws_active = false;
int g_ws_extra = 0;

static Interp816 *g_oracle_cpu;
static uint64_t g_oracle_frames;
static int g_nmi_delay;
static FILE *g_apu_log;
static FILE *g_apu_source_log;
static FILE *g_diag_log;
static unsigned g_apu_log_count;
static uint32_t g_oracle_instruction_pc;
static uint64_t g_spc_snapshot_sample_start;
static bool g_spc_snapshot_sample_start_valid;

/* This is deliberately not an SPC interchange file.  The standard SPC
 * layout cannot represent the S-DSP's live envelope, BRR-history, echo/FIR,
 * or sample-clock state.  This local format serializes the existing canonical
 * apu_saveload walk plus the explicitly-unsaved port scheduler, so it can
 * resume the native APU at an exact sample boundary. */
typedef struct ApuSnapshotHeader {
  uint8_t magic[16];
  uint32_t version;
  uint32_t saveload_bytes;
  uint32_t port_queue_count;
} ApuSnapshotHeader;

typedef struct ApuSnapshotSli {
  SaveLoadInfo base;
  uint8_t *cursor;
  size_t remaining;
  bool load;
  bool error;
} ApuSnapshotSli;

static void ApuSnapshotSliFunc(SaveLoadInfo *sli, void *data, size_t n) {
  ApuSnapshotSli *state = (ApuSnapshotSli *)sli;
  if (state->error || n > state->remaining) {
    state->error = true;
    return;
  }
  if (state->load)
    memcpy(data, state->cursor, n);
  else
    memcpy(state->cursor, data, n);
  state->cursor += n;
  state->remaining -= n;
}

static size_t ApuSnapshotSize(void) {
  return offsetof(Apu, pad) + 6 - offsetof(Apu, ram) +
      sizeof(Dsp) - offsetof(Dsp, ram) +
      offsetof(Spc, cyclesUsed) - offsetof(Spc, a);
}

static void DumpApuSnapshot(const char *path) {
  if (!path) return;
  const size_t bytes = ApuSnapshotSize();
  uint8_t *payload = malloc(bytes);
  if (!payload) Die("could not allocate APU snapshot");
  ApuSnapshotSli sli = {{ApuSnapshotSliFunc}, payload, bytes, false, false};
  apu_saveload(g_snes->apu, &sli.base);
  if (sli.error || sli.remaining != 0) Die("could not serialize APU snapshot");
  ApuSnapshotHeader header = {{0}, 1, (uint32_t)bytes,
      g_snes->apu->portQTail - g_snes->apu->portQHead};
  memcpy(header.magic, "PHALANX-APU-STATE", sizeof(header.magic));
  FILE *f = fopen(path, "wb");
  if (!f || fwrite(&header, 1, sizeof(header), f) != sizeof(header) ||
      fwrite(payload, 1, bytes, f) != bytes ||
      fwrite(g_snes->apu->portQueue, 1, sizeof(g_snes->apu->portQueue), f) !=
          sizeof(g_snes->apu->portQueue) ||
      fwrite(&g_snes->apu->portQHead, 1, sizeof(g_snes->apu->portQHead), f) !=
          sizeof(g_snes->apu->portQHead) ||
      fwrite(&g_snes->apu->portQTail, 1, sizeof(g_snes->apu->portQTail), f) !=
          sizeof(g_snes->apu->portQTail) || fclose(f) != 0)
    Die("could not write APU snapshot output");
  free(payload);
}

static int RunApuSnapshotReplay(const char *path, const char *wav_path,
                                uint64_t samples) {
  FILE *f = fopen(path, "rb");
  ApuSnapshotHeader header;
  if (!f || fread(&header, 1, sizeof(header), f) != sizeof(header) ||
      memcmp(header.magic, "PHALANX-APU-STATE", sizeof(header.magic)) != 0 ||
      header.version != 1 || header.saveload_bytes != ApuSnapshotSize())
    Die("invalid or incompatible APU snapshot");
  uint8_t *payload = malloc(header.saveload_bytes);
  if (!payload || fread(payload, 1, header.saveload_bytes, f) != header.saveload_bytes ||
      fread(g_snes->apu->portQueue, 1, sizeof(g_snes->apu->portQueue), f) !=
          sizeof(g_snes->apu->portQueue) ||
      fread(&g_snes->apu->portQHead, 1, sizeof(g_snes->apu->portQHead), f) !=
          sizeof(g_snes->apu->portQHead) ||
      fread(&g_snes->apu->portQTail, 1, sizeof(g_snes->apu->portQTail), f) !=
          sizeof(g_snes->apu->portQTail) || fclose(f) != 0)
    Die("could not read APU snapshot");
  ApuSnapshotSli sli = {{ApuSnapshotSliFunc}, payload, header.saveload_bytes,
                        true, false};
  apu_saveload(g_snes->apu, &sli.base);
  free(payload);
  if (sli.error || sli.remaining != 0 ||
      g_snes->apu->portQTail - g_snes->apu->portQHead != header.port_queue_count)
    Die("could not restore APU snapshot");
  audio_trace_reset();
  uint64_t produced = 0;
  while (produced < samples) {
    apu_cycle(g_snes->apu);
    audio_trace_sample_clocks(&produced, NULL);
  }
  if (audio_trace_dump_wav(wav_path, 0, samples, NULL, NULL) != 0)
    Die("could not write APU snapshot replay WAV");
  return 0;
}

extern uint64_t g_spc_pc_histogram[0x10000];
extern int g_spc_pc_max_seen;

static uint32_t WramFingerprint(void) {
  uint32_t h = 2166136261u;
  for (size_t i = 0; i < sizeof(g_ram); i++) {
    h ^= g_ram[i];
    h *= 16777619u;
  }
  return h;
}

static void DumpFramePpm(const char *path) {
  if (!path) return;
  enum { kWidth = 256, kHeight = 224 };
  uint32_t pixels[kWidth * kHeight];
  memset(pixels, 0, sizeof(pixels));
  PpuBeginDrawing(g_ppu, (uint8_t *)pixels, kWidth * sizeof(uint32_t),
                  kPpuRenderFlags_NewRenderer);
  ppu_runLine(g_ppu, 0);
  for (unsigned line = 1; line <= kHeight; line++)
    ppu_runLine(g_ppu, line);
  FILE *f = fopen(path, "wb");
  if (!f) Die("could not open screenshot output");
  fprintf(f, "P6\n%d %d\n255\n", kWidth, kHeight);
  for (size_t i = 0; i < kWidth * kHeight; i++) {
    const uint32_t p = pixels[i];
    fputc((int)(p >> 16), f);
    fputc((int)(p >> 8), f);
    fputc((int)p, f);
  }
  fclose(f);
}

static void DumpWram(const char *path) {
  if (!path) return;
  FILE *f = fopen(path, "wb");
  if (!f) Die("could not open WRAM dump output");
  if (fwrite(g_ram, 1, sizeof(g_ram), f) != sizeof(g_ram))
    Die("could not write WRAM dump");
  fclose(f);
}

// Dump the actual normal-run audio event ring. Unlike PHALANX_SPC_TRACE_ARAM,
// this observes the uploaded driver while the 65816, SPC700, and DSP are all
// running together. sample is the native S-DSP output-sample clock.
static void DumpAudioEvents(const char *path) {
  if (!path) return;
  FILE *f = fopen(path, "w");
  if (!f) Die("could not open DSP event output");
  AudioTraceStats stats;
  audio_trace_get_stats(&stats);
  uint64_t first = stats.event_count > AUDIO_TRACE_EVENT_RING
      ? stats.event_count - AUDIO_TRACE_EVENT_RING : 0;
  fprintf(f, "# index sample type addr value aux producer\n");
  AudioTraceEvent events[4096];
  while (first < stats.event_count) {
    uint64_t oldest = 0;
    uint32_t n = audio_trace_copy_events(first,
        (uint32_t)(sizeof(events) / sizeof(events[0])), events, &oldest);
    if (first < oldest) first = oldest;
    if (n == 0) break;
    for (uint32_t i = 0; i < n; i++) {
      const AudioTraceEvent *e = &events[i];
      fprintf(f, "%llu %llu %u %02X %02X %u %u\n",
              (unsigned long long)(first + i),
              (unsigned long long)e->sample_idx, e->type, e->addr, e->val,
              e->aux, e->producer);
    }
    first += n;
  }
  if (fclose(f) != 0) Die("could not close DSP event output");
}

static void DumpDspRegisters(const char *path) {
  if (!path) return;
  FILE *f = fopen(path, "wb");
  if (!f) Die("could not open DSP register output");
  if (fwrite(g_snes->apu->dsp->ram, 1, 0x80, f) != 0x80)
    Die("could not write DSP register output");
  if (fclose(f) != 0) Die("could not close DSP register output");
}

// Write the standard 0x10200-byte SPC snapshot layout. This is a direct
// capture of the native running APU, not a synthesized music export.
static void DumpSpcSnapshot(const char *path) {
  if (!path) return;
  uint8_t image[0x10200];
  memset(image, 0, sizeof(image));
  static const char signature[] = "SNES-SPC700 Sound File Data v0.30";
  memcpy(image, signature, sizeof(signature) - 1);
  image[0x21] = 0x1a;
  image[0x22] = 0x1a;
  image[0x23] = 0x1b; // no ID666 metadata follows
  image[0x24] = 0x1e;
  const Spc *spc = g_snes->apu->spc;
  image[0x25] = (uint8_t)spc->pc;
  image[0x26] = (uint8_t)(spc->pc >> 8);
  image[0x27] = spc->a;
  image[0x28] = spc->x;
  image[0x29] = spc->y;
  image[0x2a] = (uint8_t)((spc->n ? 0x80 : 0) | (spc->v ? 0x40 : 0) |
      (spc->p ? 0x20 : 0) | (spc->b ? 0x10 : 0) | (spc->h ? 0x08 : 0) |
      (spc->i ? 0x04 : 0) | (spc->z ? 0x02 : 0) | (spc->c ? 0x01 : 0));
  image[0x2b] = spc->sp;
  memcpy(image + 0x100, g_snes->apu->ram, 0x10000);
  // LakeSnes keeps the SPC I/O registers outside ram[]. Populate the RAM
  // image a standalone SPC player will see rather than leaking stale bytes.
  uint8_t *io = image + 0x100 + 0xf0;
  io[0x0] = 0x0a;
  io[0x1] = (uint8_t)((g_snes->apu->timer[0].enabled ? 0x01 : 0) |
      (g_snes->apu->timer[1].enabled ? 0x02 : 0) |
      (g_snes->apu->timer[2].enabled ? 0x04 : 0) |
      (g_snes->apu->romReadable ? 0x80 : 0));
  io[0x2] = g_snes->apu->dspAdr;
  io[0x3] = g_snes->apu->dsp->ram[g_snes->apu->dspAdr & 0x7f];
  io[0x4] = g_snes->apu->inPorts[0];
  io[0x5] = g_snes->apu->inPorts[1];
  io[0x6] = g_snes->apu->inPorts[2];
  io[0x7] = g_snes->apu->inPorts[3];
  io[0x8] = g_snes->apu->inPorts[4];
  io[0x9] = g_snes->apu->inPorts[5];
  io[0xa] = g_snes->apu->timer[0].target;
  io[0xb] = g_snes->apu->timer[1].target;
  io[0xc] = g_snes->apu->timer[2].target;
  io[0xd] = (uint8_t)(g_snes->apu->timer[0].counter & 0xf);
  io[0xe] = (uint8_t)(g_snes->apu->timer[1].counter & 0xf);
  io[0xf] = (uint8_t)(g_snes->apu->timer[2].counter & 0xf);
  memcpy(image + 0x10100, g_snes->apu->dsp->ram, 0x80);
  FILE *f = fopen(path, "wb");
  if (!f) Die("could not open SPC snapshot output");
  if (fwrite(image, 1, sizeof(image), f) != sizeof(image) || fclose(f) != 0)
    Die("could not write SPC snapshot output");
}

static void DumpSpcSnapshotWav(const char *path) {
  if (!path || !g_spc_snapshot_sample_start_valid) return;
  uint64_t produced = 0;
  audio_trace_sample_clocks(&produced, NULL);
  if (produced < g_spc_snapshot_sample_start ||
      audio_trace_dump_wav(path, (int64_t)g_spc_snapshot_sample_start,
                           produced - g_spc_snapshot_sample_start,
                           NULL, NULL) != 0)
    Die("could not write post-snapshot PCM WAV");
}

static void ResetSpcPCTrace(void) {
  memset(g_spc_pc_histogram, 0, sizeof(g_spc_pc_histogram));
  g_spc_pc_max_seen = 0;
}

static void ResetSpcExecutionTrace(void) {
  g_spc_exec_trace_count = 0;
  g_spc_port_trace_count = 0;
  g_spc_data_trace_count = 0;
  g_spc_state_trace_count = 0;
  g_spc_exec_trace_enabled = true;
}

static void DumpSpcPCTrace(const char *path) {
  if (!path) return;
  FILE *f = fopen(path, "w");
  if (!f) Die("could not open SPC PC trace output");
  for (unsigned pc = 0; pc < 0x10000; pc++) {
    if (g_spc_pc_histogram[pc])
      fprintf(f, "%04X %llu\n", pc,
              (unsigned long long)g_spc_pc_histogram[pc]);
  }
  if (fclose(f) != 0) Die("could not close SPC PC trace output");
}

static void DumpSpcExecutionTrace(const char *path) {
  if (!path) return;
  FILE *f = fopen(path, "w");
  if (!f) Die("could not open SPC execution trace output");
  const uint64_t count = g_spc_exec_trace_count;
  const uint64_t first = count > SPC_EXEC_TRACE_CAPACITY
      ? count - SPC_EXEC_TRACE_CAPACITY : 0;
  fprintf(f, "# instruction_count %llu retained_from %llu\n",
          (unsigned long long)count, (unsigned long long)first);
  for (uint64_t i = first; i < count; i++) {
    const SpcExecRecord *rec =
        &g_spc_exec_trace[i & (SPC_EXEC_TRACE_CAPACITY - 1)];
    fprintf(f, "%llu %04X %02X %02X %02X %02X %02X %02X\n",
            (unsigned long long)i, rec->pc, rec->opcode, rec->a, rec->x,
            rec->y, rec->sp, rec->flags);
  }
  if (fclose(f) != 0) Die("could not close SPC execution trace output");
}

static void DumpSpcPortTrace(const char *path) {
  if (!path) return;
  FILE *f = fopen(path, "w");
  if (!f) Die("could not open SPC port trace output");
  const uint64_t count = g_spc_port_trace_count;
  const uint64_t first = count > SPC_PORT_TRACE_CAPACITY
      ? count - SPC_PORT_TRACE_CAPACITY : 0;
  fprintf(f, "# read_count %llu retained_from %llu\n",
          (unsigned long long)count, (unsigned long long)first);
  for (uint64_t i = first; i < count; i++) {
    const SpcPortTraceRecord *rec =
        &g_spc_port_trace[i & (SPC_PORT_TRACE_CAPACITY - 1)];
    fprintf(f, "%llu %llu %04X %u %02X\n", (unsigned long long)i,
            (unsigned long long)rec->instruction_index, rec->pc_after_operand,
            rec->port, rec->value);
  }
  if (fclose(f) != 0) Die("could not close SPC port trace output");
}

static void DumpSpcDataTrace(const char *path) {
  if (!path) return;
  FILE *f = fopen(path, "w");
  if (!f) Die("could not open SPC data trace output");
  const uint64_t count = g_spc_data_trace_count;
  const uint64_t first = count > SPC_DATA_TRACE_CAPACITY
      ? count - SPC_DATA_TRACE_CAPACITY : 0;
  fprintf(f, "# read_count %llu retained_from %llu\n",
          (unsigned long long)count, (unsigned long long)first);
  for (uint64_t i = first; i < count; i++) {
    const SpcDataTraceRecord *rec =
        &g_spc_data_trace[i & (SPC_DATA_TRACE_CAPACITY - 1)];
    fprintf(f, "%llu %llu %04X %04X %02X\n", (unsigned long long)i,
            (unsigned long long)rec->instruction_index, rec->pc_after_operand,
            rec->address, rec->value);
  }
  if (fclose(f) != 0) Die("could not close SPC data trace output");
}

static void DumpSpcStateTrace(const char *path) {
  if (!path) return;
  FILE *f = fopen(path, "w");
  if (!f) Die("could not open SPC state trace output");
  const uint64_t count = g_spc_state_trace_count;
  const uint64_t first = count > SPC_STATE_TRACE_CAPACITY
      ? count - SPC_STATE_TRACE_CAPACITY : 0;
  fprintf(f, "# access_count %llu retained_from %llu\n",
          (unsigned long long)count, (unsigned long long)first);
  for (uint64_t i = first; i < count; i++) {
    const SpcStateTraceRecord *rec =
        &g_spc_state_trace[i & (SPC_STATE_TRACE_CAPACITY - 1)];
    fprintf(f, "%llu %llu %04X %c %04X %02X\n", (unsigned long long)i,
            (unsigned long long)rec->instruction_index, rec->pc_after_operand,
            rec->write ? 'W' : 'R', rec->address, rec->value);
  }
  if (fclose(f) != 0) Die("could not close SPC state trace output");
}

static uint16_t PhaseButtons(const char *phase) {
  if (!strcmp(phase, "title_start") || !strcmp(phase, "sound_enter") ||
      !strcmp(phase, "sound_exit"))
    return PHALANX_INPUT_START;
  if (!strcmp(phase, "menu_down_1") || !strcmp(phase, "menu_down_2"))
    return PHALANX_INPUT_DOWN;
  if (!strcmp(phase, "preview_select")) return PHALANX_INPUT_A;
  return 0;
}

static uint16_t ParseButtonNames(char *buttons) {
  uint16_t result = 0;
  char *save = NULL;
  for (char *button = strtok_r(buttons, "+", &save); button;
       button = strtok_r(NULL, "+", &save)) {
    if (!strcmp(button, "A")) result |= PHALANX_INPUT_A;
    else if (!strcmp(button, "B")) result |= PHALANX_INPUT_B;
    else if (!strcmp(button, "Start")) result |= PHALANX_INPUT_START;
    else if (!strcmp(button, "Select")) result |= PHALANX_INPUT_SELECT;
    else if (!strcmp(button, "Up")) result |= PHALANX_INPUT_UP;
    else if (!strcmp(button, "Down")) result |= PHALANX_INPUT_DOWN;
    else if (!strcmp(button, "Left")) result |= PHALANX_INPUT_LEFT;
    else if (!strcmp(button, "Right")) result |= PHALANX_INPUT_RIGHT;
    else if (!strcmp(button, "X")) result |= PHALANX_INPUT_X;
    else if (!strcmp(button, "Y")) result |= PHALANX_INPUT_Y;
  }
  return result;
}

static uint16_t ParseScript(unsigned frame, const char *script,
                            bool named_phases) {
  if (!script) return 0;
  char *copy = strdup(script);
  if (!copy) Die("out of memory");
  uint16_t result = 0;
  char *items = NULL;
  for (char *item = strtok_r(copy, ",", &items); item;
       item = strtok_r(NULL, ",", &items)) {
    char *colon = strchr(item, ':');
    char *equals = strchr(item, '=');
    if (named_phases && equals && !colon) {
      char *phase = item;
      char *range = equals + 1;
      *equals = 0;
      unsigned first = (unsigned)strtoul(range, NULL, 10);
      unsigned last = first;
      char *dash = strchr(range, '-');
      if (dash) {
        *dash = 0;
        first = (unsigned)strtoul(range, NULL, 10);
        last = (unsigned)strtoul(dash + 1, NULL, 10);
      }
      if (frame >= first && frame <= last) result |= PhaseButtons(phase);
      continue;
    }
    if (!colon) continue;
    *colon = 0;
    unsigned first = 0, last = 0;
    char *dash = strchr(item, '-');
    if (dash) {
      *dash = 0;
      first = (unsigned)strtoul(item, NULL, 10);
      last = (unsigned)strtoul(dash + 1, NULL, 10);
    } else {
      first = last = (unsigned)strtoul(item, NULL, 10);
    }
    if (frame < first || frame > last) continue;
    result |= named_phases ? PhaseButtons(colon + 1)
                           : ParseButtonNames(colon + 1);
  }
  free(copy);
  return result;
}

static uint16_t ScriptButtons(unsigned frame, const char *script) {
  return ParseScript(frame, script, false) |
         ParseScript(frame, getenv("PHALANX_ORACLE_PHASE_SCRIPT"), true);
}

enum {
  kBootFrames = 300,
  kNmiRecognitionInstructions = 3,
};

void RtlApuLock(void) {}
void RtlApuUnlock(void) {}
void rtl_accumulate_apu_catchup(void) {}
void RtlApuWrite(uint16 adr, uint8 val) {
  if (g_apu_log && g_apu_log_count < (1u << 20)) {
    fprintf(g_apu_log, "%llu %u %02X %06X\n",
            (unsigned long long)g_oracle_frames, (unsigned)(adr & 3), val,
            g_oracle_instruction_pc);
    g_apu_log_count++;
  }
  g_snes->apu->inPorts[adr & 3] = val;
}
void Die(const char *error) {
  fprintf(stderr, "oracle: %s\n", error ? error : "fatal error");
  exit(EXIT_FAILURE);
}
void ppudma_record_dma(int channel, int from_b, uint8_t bank, uint16_t address,
                       uint8_t b_address, uint16_t size) {
  (void)channel;
  (void)from_b;
  (void)bank;
  (void)address;
  (void)b_address;
  (void)size;
}
int interp816_opcode_hook(uint32_t address) {
  (void)address;
  return 0;
}
DspShadow *dsp_shadow_create(void) { return NULL; }
void dsp_shadow_free(DspShadow *shadow) { (void)shadow; }
void dsp_shadow_process(DspShadow *shadow, Dsp *dsp, int in_left,
                        int in_right, int *out_left, int *out_right) {
  (void)shadow;
  (void)dsp;
  *out_left = in_left;
  *out_right = in_right;
}
void dsp_shadow_verify_brr(const uint8_t *aram, uint16_t block_start,
                           int before, int after, const int16_t *cache) {
  (void)aram;
  (void)block_start;
  (void)before;
  (void)after;
  (void)cache;
}
void dsp_shadow_verify_echo(const int16_t *left, const int16_t *right,
                            const int8_t *coefficients, int index,
                            int sample_left, int sample_right) {
  (void)left;
  (void)right;
  (void)coefficients;
  (void)index;
  (void)sample_left;
  (void)sample_right;
}

static uint8_t BusRead(void *context, uint32_t address) {
  (void)context;
  const uint8_t value = snes_read(g_snes, address);
  /* $1f:8a64 is the verified IPL-style uploader. Its indirect-long reads
   * resolve the ROM ownership of the live SPC image. Keep this gated and
   * scoped to the uploader so normal oracle operation has no trace cost. */
  if (g_apu_source_log && g_oracle_instruction_pc >= 0x1f8a64 &&
      g_oracle_instruction_pc < 0x1f8ac7 &&
      (address & 0xffff) >= 0x8000 &&
      (address < g_oracle_instruction_pc ||
       address > g_oracle_instruction_pc + 3)) {
    fprintf(g_apu_source_log, "%llu %06X %06X %02X\n",
            (unsigned long long)g_oracle_frames, g_oracle_instruction_pc,
            address & 0xffffff, value);
  }
  return value;
}

static void BusWrite(void *context, uint32_t address, uint8_t value) {
  (void)context;
  snes_write(g_snes, address, value);
}

static void AdvanceBeam(void) {
  if (g_snes->vIrqEnabled && g_snes->hIrqEnabled) {
    if (g_snes->vPos == g_snes->vTimer + 1 &&
        g_snes->hPos == 4 * g_snes->hTimer) {
      g_snes->inIrq = true;
      g_oracle_cpu->irqWanted = true;
    }
  } else if (g_snes->vIrqEnabled) {
    if (g_snes->vPos == g_snes->vTimer + 1 && g_snes->hPos == 1024) {
      g_snes->inIrq = true;
      g_oracle_cpu->irqWanted = true;
    }
  } else if (g_snes->hIrqEnabled &&
             g_snes->hPos == 4 * g_snes->hTimer) {
    g_snes->inIrq = true;
    g_oracle_cpu->irqWanted = true;
  }

  if (g_snes->hPos == 0) {
    bool starts_vblank = false;
    if (g_snes->vPos == 0) {
      g_snes->inVblank = false;
      g_snes->inNmi = false;
      dma_startDma(g_snes->dma, 0, true);
    } else if (g_snes->vPos == 225) {
      starts_vblank = !ppu_checkOverscan(g_ppu);
    } else if (g_snes->vPos == 240 && !g_snes->inVblank) {
      starts_vblank = true;
    }
    if (starts_vblank) {
      ppu_handleVblank(g_ppu);
      g_snes->inVblank = true;
      g_snes->inNmi = true;
      /* The CPU recognizes NMI between instructions, not atomically with the
       * PPU edge. Preserve that window so Phalanx's pending RDNMI poll can
       * observe the latch before its NMI handler reads and clears it. */
      if (g_snes->nmiEnabled)
        g_nmi_delay = kNmiRecognitionInstructions;
      if (g_snes->autoJoyRead)
        g_snes->autoJoyTimer = 0;
    }
  } else if (g_snes->hPos == 1024 && !g_snes->inVblank) {
    dma_cycle(g_snes->dma);
  }

  g_snes->hPos += 2;
  if (g_snes->hPos == 1364) {
    g_snes->hPos = 0;
    g_snes->vPos++;
    if (g_snes->vPos == 262) {
      g_snes->vPos = 0;
      g_oracle_frames++;
    }
  }
}

static bool RunFrame(void) {
  const uint64_t target = g_oracle_frames + 1;
  long guard = 20000000;
  while (g_oracle_frames < target && guard-- > 0) {
    if (g_nmi_delay && --g_nmi_delay == 0)
      g_oracle_cpu->nmiWanted = true;
    g_oracle_instruction_pc =
        ((uint32_t)g_oracle_cpu->k << 16) | g_oracle_cpu->pc;
    int cpu_cycles = interp816_runOpcode(g_oracle_cpu);
    if (cpu_cycles <= 0)
      cpu_cycles = 1;
    const int master_clocks = cpu_cycles * 8;
    for (int clock = 0; clock < master_clocks; clock += 2)
      AdvanceBeam();
    g_snes->apuCatchupCycles +=
        (double)master_clocks * (32040.0 * 32.0) /
        (1364.0 * 262.0 * 60.0);
    snes_catchupApu(g_snes);
  }
  return guard > 0;
}

static uint8_t *ReadFile(const char *path, int *size_out) {
  FILE *file = fopen(path, "rb");
  if (!file)
    return NULL;
  fseek(file, 0, SEEK_END);
  const long size = ftell(file);
  fseek(file, 0, SEEK_SET);
  if (size <= 0) {
    fclose(file);
    return NULL;
  }
  uint8_t *data = malloc((size_t)size);
  if (!data || fread(data, 1, (size_t)size, file) != (size_t)size) {
    free(data);
    fclose(file);
    return NULL;
  }
  fclose(file);
  *size_out = (int)size;
  return data;
}

static int RunSpcTrace(const char *aram_path) {
  int size = 0;
  uint8_t *aram = ReadFile(aram_path, &size);
  if (!aram || size != 0x10000)
    Die("SPC trace requires a 65536-byte ARAM image");
  memcpy(g_snes->apu->ram, aram, 0x10000);
  free(aram);
  g_snes->apu->romReadable = false;
  g_snes->apu->spc->pc = 0x0800;
  g_snes->apu->spc->stopped = false;
  uint8_t dsp_prev[0x80];
  memcpy(dsp_prev, g_snes->apu->dsp->ram, sizeof(dsp_prev));
  unsigned max_steps = 20000;
  const char *steps_env = getenv("PHALANX_SPC_TRACE_STEPS");
  if (steps_env)
    max_steps = (unsigned)strtoul(steps_env, NULL, 10);
  for (unsigned i = 0; i < max_steps; i++) {
    if (i == 6000) {
      /* First non-bulk handoff observed after the captured upload session. */
      g_snes->apu->inPorts[0] = 0xA4;
      g_snes->apu->inPorts[1] = 0x00;
      g_snes->apu->inPorts[2] = 0x00;
      g_snes->apu->inPorts[3] = 0x08;
      fprintf(stderr, "spc trace: injected captured command $A4\n");
    }
    const uint16_t pc = g_snes->apu->spc->pc;
    const uint8_t opcode = g_snes->apu->ram[pc];
    printf("%05u %04X %02X\n", i, pc, opcode);
    const int cycles = spc_runOpcode(g_snes->apu->spc);
    for (int cycle = 0; cycle < cycles; cycle++)
      apu_cycle(g_snes->apu);
    for (unsigned reg = 0; reg < sizeof(dsp_prev); reg++) {
      const uint8_t value = g_snes->apu->dsp->ram[reg];
      if (value != dsp_prev[reg]) {
        printf("dsp %05u %02X %02X\n", i, reg, value);
        dsp_prev[reg] = value;
      }
    }
    if (g_snes->apu->spc->stopped)
      break;
  }
  return 0;
}

static void CheckJoypadRegisters(void) {
  static const struct {
    const char *name;
    uint16_t host;
    uint16_t joy;
  } cases[] = {
    {"R",      PHALANX_INPUT_R,      0x0010},
    {"L",      PHALANX_INPUT_L,      0x0020},
    {"X",      PHALANX_INPUT_X,      0x0040},
    {"A",      PHALANX_INPUT_A,      0x0080},
    {"Right",  PHALANX_INPUT_RIGHT,  0x0100},
    {"Left",   PHALANX_INPUT_LEFT,   0x0200},
    {"Down",   PHALANX_INPUT_DOWN,   0x0400},
    {"Up",     PHALANX_INPUT_UP,     0x0800},
    {"Start",  PHALANX_INPUT_START,  0x1000},
    {"Select", PHALANX_INPUT_SELECT, 0x2000},
    {"Y",      PHALANX_INPUT_Y,      0x4000},
    {"B",      PHALANX_INPUT_B,      0x8000},
  };

  for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
    g_snes->input1_currentState = PhalanxInputToCoreState(cases[i].host);
    const uint16_t actual = (uint16_t)snes_readReg(g_snes, 0x4218) |
                            ((uint16_t)snes_readReg(g_snes, 0x4219) << 8);
    if (actual != cases[i].joy) {
      fprintf(stderr, "oracle joypad %s: $%04X expected $%04X\n",
              cases[i].name, actual, cases[i].joy);
      exit(EXIT_FAILURE);
    }
  }
  g_snes->input1_currentState = 0;
}

int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "usage: phalanx_oracle ROM\n");
    return 2;
  }
  if (getenv("PHALANX_ORACLE_APU_LOG"))
    g_apu_log = stderr;
  const char *apu_source_log_path = getenv("PHALANX_ORACLE_APU_SOURCE_LOG");
  if (apu_source_log_path) {
    g_apu_source_log = fopen(apu_source_log_path, "w");
    if (!g_apu_source_log) Die("could not open APU upload-source log");
  }
  const char *diag_path = getenv("PHALANX_ORACLE_DIAGNOSTIC_LOG");
  if (diag_path) {
    g_diag_log = fopen(diag_path, "w");
    if (!g_diag_log) Die("could not open diagnostic log");
  }
  int rom_size = 0;
  uint8_t *rom = ReadFile(argv[1], &rom_size);
  if (!rom)
    Die("could not read ROM");

  g_snes = snes_init(g_ram);
  g_ppu = g_snes->ppu;
  g_dma = g_snes->dma;
  if (!snes_loadRom(g_snes, rom, rom_size))
    Die("could not load ROM");
  free(rom);
  snes_reset(g_snes, true);
  const char *spc_trace = getenv("PHALANX_SPC_TRACE_ARAM");
  if (spc_trace)
    return RunSpcTrace(spc_trace);
  const char *apu_snapshot_replay = getenv("PHALANX_ORACLE_APU_SNAPSHOT_REPLAY");
  if (apu_snapshot_replay) {
    const char *replay_wav = getenv("PHALANX_ORACLE_APU_SNAPSHOT_REPLAY_WAV");
    if (!replay_wav) Die("APU snapshot replay requires an output WAV path");
    uint64_t replay_samples = 4096;
    const char *samples_env = getenv("PHALANX_ORACLE_APU_SNAPSHOT_REPLAY_SAMPLES");
    if (samples_env) replay_samples = strtoull(samples_env, NULL, 10);
    return RunApuSnapshotReplay(apu_snapshot_replay, replay_wav, replay_samples);
  }
  CheckJoypadRegisters();
  g_oracle_cpu = interp816_init(NULL, BusRead, BusWrite);
  interp816_reset(g_oracle_cpu);

  unsigned start_frame = 0;
  const char *start_env = getenv("PHALANX_ORACLE_START_FRAME");
  if (start_env)
    start_frame = (unsigned)strtoul(start_env, NULL, 10);

  unsigned boot_frames = kBootFrames;
  const char *frames_env = getenv("PHALANX_ORACLE_FRAMES");
  const char *script_env = getenv("PHALANX_ORACLE_INPUT_SCRIPT");
  const char *screenshot_path = getenv("PHALANX_ORACLE_SCREENSHOT_PATH");
  unsigned screenshot_frame = 0;
  const char *screenshot_frame_env = getenv("PHALANX_ORACLE_SCREENSHOT_FRAME");
  if (screenshot_frame_env)
    screenshot_frame = (unsigned)strtoul(screenshot_frame_env, NULL, 10);
  const char *wram_dump_path = getenv("PHALANX_ORACLE_WRAM_DUMP_PATH");
  const char *spc_pc_log_path = getenv("PHALANX_ORACLE_SPC_PC_LOG");
  const char *spc_execution_log_path = getenv("PHALANX_ORACLE_SPC_EXEC_LOG");
  const char *spc_port_log_path = getenv("PHALANX_ORACLE_SPC_PORT_LOG");
  const char *spc_data_log_path = getenv("PHALANX_ORACLE_SPC_DATA_LOG");
  const char *spc_state_log_path = getenv("PHALANX_ORACLE_SPC_STATE_LOG");
  const char *spc_snapshot_path = getenv("PHALANX_ORACLE_SPC_SNAPSHOT");
  const char *apu_snapshot_path = getenv("PHALANX_ORACLE_APU_SNAPSHOT");
  const char *spc_snapshot_wav_path = getenv("PHALANX_ORACLE_SPC_SNAPSHOT_WAV");
  unsigned spc_snapshot_frame = 0;
  const char *spc_snapshot_frame_env = getenv("PHALANX_ORACLE_SPC_SNAPSHOT_FRAME");
  if (spc_snapshot_frame_env)
    spc_snapshot_frame = (unsigned)strtoul(spc_snapshot_frame_env, NULL, 10);
  unsigned wram_dump_frame = 0;
  const char *wram_dump_frame_env = getenv("PHALANX_ORACLE_WRAM_DUMP_FRAME");
  if (wram_dump_frame_env)
    wram_dump_frame = (unsigned)strtoul(wram_dump_frame_env, NULL, 10);
  unsigned audio_trace_clear_frame = 0;
  const char *audio_trace_clear_frame_env =
      getenv("PHALANX_ORACLE_AUDIO_TRACE_CLEAR_FRAME");
  if (audio_trace_clear_frame_env)
    audio_trace_clear_frame =
        (unsigned)strtoul(audio_trace_clear_frame_env, NULL, 10);
  if (frames_env)
    boot_frames = (unsigned)strtoul(frames_env, NULL, 10);
  for (unsigned frame = 1; frame <= boot_frames; frame++) {
    if (frame == audio_trace_clear_frame) {
      audio_trace_reset();
      if (spc_pc_log_path || spc_execution_log_path || spc_port_log_path || spc_data_log_path || spc_state_log_path)
        ResetSpcPCTrace();
      if (spc_execution_log_path || spc_port_log_path || spc_data_log_path || spc_state_log_path)
        ResetSpcExecutionTrace();
    }
    uint16_t scripted = ScriptButtons(frame, script_env);
    g_snes->input1_currentState =
        (start_frame && frame >= start_frame && frame < start_frame + 3)
            ? PhalanxInputToCoreState(PHALANX_INPUT_START | scripted)
            : PhalanxInputToCoreState(scripted);
    if (!RunFrame())
      Die("CPU failed to complete a frame");
    if (g_diag_log) {
      const uint16_t joypad = (uint16_t)snes_readReg(g_snes, 0x4218) |
          ((uint16_t)snes_readReg(g_snes, 0x4219) << 8);
      fprintf(g_diag_log,
              "%u input=%04X joypad=%04X inidisp=%02X tm=%02X screen=%u wram=%08X\n",
              frame, scripted, joypad, g_ppu->inidisp,
              g_ppu->screenEnabled[0], g_ppu->screenEnabled[0],
              WramFingerprint());
      fflush(g_diag_log);
    }
    if (screenshot_path && frame == screenshot_frame)
      DumpFramePpm(screenshot_path);
    if (wram_dump_path && frame == wram_dump_frame)
      DumpWram(wram_dump_path);
    if ((spc_snapshot_path || apu_snapshot_path) && frame == spc_snapshot_frame) {
      DumpSpcSnapshot(spc_snapshot_path);
      DumpApuSnapshot(apu_snapshot_path);
      audio_trace_sample_clocks(&g_spc_snapshot_sample_start, NULL);
      g_spc_snapshot_sample_start_valid = true;
    }
  }

  const char *wav_path = getenv("PHALANX_ORACLE_PCM_WAV");
  const char *dsp_log_path = getenv("PHALANX_ORACLE_DSP_LOG");
  const char *dsp_register_path = getenv("PHALANX_ORACLE_DSP_REGISTERS");
  DumpAudioEvents(dsp_log_path);
  DumpDspRegisters(dsp_register_path);
  DumpSpcPCTrace(spc_pc_log_path);
  DumpSpcExecutionTrace(spc_execution_log_path);
  DumpSpcPortTrace(spc_port_log_path);
  DumpSpcDataTrace(spc_data_log_path);
  DumpSpcStateTrace(spc_state_log_path);
  DumpSpcSnapshotWav(spc_snapshot_wav_path);
  g_spc_exec_trace_enabled = false;
  if (wav_path && audio_trace_dump_wav(wav_path, -1, 0, NULL, NULL) != 0)
    Die("could not write PCM WAV");
  if (PPU_forcedBlank(g_ppu) || g_ppu->screenEnabled[0] == 0) {
    fprintf(stderr,
            "oracle boot failed: frame=%d INIDISP=$%02X TM=$%02X\n",
            boot_frames, g_ppu->inidisp, g_ppu->screenEnabled[0]);
    return 1;
  }

  printf("oracle boot: %d frames, joypad registers passed, "
         "INIDISP=$%02X TM=$%02X\n",
         boot_frames, g_ppu->inidisp, g_ppu->screenEnabled[0]);
  if (g_diag_log) fclose(g_diag_log);
  if (g_apu_source_log) fclose(g_apu_source_log);
  return 0;
}
