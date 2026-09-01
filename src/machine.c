#include "machine.h"
#include "input.h"

#include "common_cpu_infra.h"
#include "common_rtl.h"
#include "cpu_state.h"
#include "snes/cart.h"
#include "snes/dma.h"
#include "snes/interp_bridge.h"
#include "snes/ppu.h"
#include "snes/snes.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
  kMasterClocksPerLine = 1364,
  kLinesPerFrame = 262,
  kVisibleLines = 224,
  kNmiRecognitionWindow = 64,
};

static const uint64_t kMasterClocksPerFrame =
    (uint64_t)kMasterClocksPerLine * kLinesPerFrame;

static uint16_t ReadVector(const uint8_t *rom, size_t size, unsigned offset) {
  const size_t address = 0x7FE0u + offset;
  if (address + 1 >= size)
    return 0;
  return (uint16_t)(rom[address] | ((uint16_t)rom[address + 1] << 8));
}

static uint64_t NextIrqMasterClock(uint64_t now, uint64_t limit) {
  if (!g_snes || (!g_snes->hIrqEnabled && !g_snes->vIrqEnabled))
    return limit;

  const uint64_t frame_base = now -
      ((uint64_t)g_snes->vPos * kMasterClocksPerLine + g_snes->hPos);
  uint64_t candidate = limit;

  if (g_snes->vIrqEnabled) {
    const uint32_t line = g_snes->vTimer < kLinesPerFrame
                            ? g_snes->vTimer : kLinesPerFrame - 1;
    const uint32_t horizontal = g_snes->hIrqEnabled
        ? (uint32_t)g_snes->hTimer * 4u : 0u;
    uint64_t event = frame_base + (uint64_t)line * kMasterClocksPerLine +
                     (horizontal < kMasterClocksPerLine ? horizontal : 0u);
    if (event <= now)
      event += kMasterClocksPerFrame;
    if (event < candidate)
      candidate = event;
  } else {
    const uint32_t horizontal = (uint32_t)g_snes->hTimer * 4u;
    if (horizontal < kMasterClocksPerLine) {
      uint64_t event = frame_base +
          (uint64_t)g_snes->vPos * kMasterClocksPerLine + horizontal;
      if (event <= now)
        event += kMasterClocksPerLine;
      if (event < candidate)
        candidate = event;
    }
  }

  return candidate > limit ? limit : candidate;
}

static void AdvanceIdleTo(uint64_t target) {
  if (target <= g_cpu.master_cycles)
    return;
  g_cpu.master_cycles = target;
  snes_sync_master_clock(g_snes, target);
  if (g_snes->cart)
    cart_sync_coprocessors(g_snes->cart, target);
}

static bool EnterInterrupt(PhalanxMachine *machine, bool nmi) {
  const uint32_t vector = nmi
      ? (g_cpu.emulation ? machine->nmi_pc24_emulation
                         : machine->nmi_pc24_native)
      : (g_cpu.emulation ? machine->irq_pc24_emulation
                         : machine->irq_pc24_native);
  if ((vector & 0xFFFFu) == 0 || (vector & 0xFFFFu) == 0xFFFFu)
    return true;

  /* Keep interrupt execution in the same event-driven continuation as the
   * interrupted program. Phalanx deliberately waits for a later NMI inside
   * its frame service routine; treating a handler as an atomic host call
   * deadlocks in that hardware wait. A real return address makes RTI resume
   * the suspended continuation naturally, including nested NMIs. */
  cpu_mirrors_to_p(&g_cpu);
  const uint16_t return_pc = (uint16_t)machine->resume_pc24;
  if (!g_cpu.emulation) {
    cpu_write8(&g_cpu, 0x00, g_cpu.S,
               (uint8_t)(machine->resume_pc24 >> 16));
    g_cpu.S = (uint16_t)(g_cpu.S - 1);
  }
  cpu_write8(&g_cpu, 0x00, g_cpu.S, (uint8_t)(return_pc >> 8));
  g_cpu.S = (uint16_t)(g_cpu.S - 1);
  cpu_write8(&g_cpu, 0x00, g_cpu.S, (uint8_t)return_pc);
  g_cpu.S = (uint16_t)(g_cpu.S - 1);
  cpu_write8(&g_cpu, 0x00, g_cpu.S, g_cpu.P);
  g_cpu.S = (uint16_t)(g_cpu.S - 1);

  g_cpu._flag_I = 1;
  g_cpu._flag_D = 0;
  cpu_mirrors_to_p(&g_cpu);
  g_cpu.PB = (uint8_t)(vector >> 16);
  g_cpu.host_return_valid = 0;
  machine->resume_pc24 = vector;
  return true;
}

static bool RunCpuUntil(PhalanxMachine *machine, uint64_t deadline) {
  unsigned guard = 0;
  while (g_cpu.master_cycles < deadline) {
    if (++guard > 100000u) {
      fprintf(stderr, "Phalanx: event scheduler made no forward progress\n");
      machine->failed = true;
      return false;
    }

    if (g_snes->inIrq && !g_cpu._flag_I) {
      if (!EnterInterrupt(machine, false))
        return false;
      continue;
    }

    interp_bridge_set_master_deadline(deadline);
    if (!interp_bridge_run_until_quiescent(&g_cpu, machine->resume_pc24)) {
      fprintf(stderr, "Phalanx: LLE continuation bailed at $%06X\n",
              machine->resume_pc24);
      machine->failed = true;
      return false;
    }
    machine->resume_pc24 = interp_bridge_lle_resume_pc();

    if (g_snes->inIrq && !g_cpu._flag_I)
      continue;

    if (g_cpu.master_cycles < deadline) {
      const uint64_t event = NextIrqMasterClock(g_cpu.master_cycles, deadline);
      AdvanceIdleTo(event);
    }
  }
  return true;
}

bool PhalanxMachineInit(PhalanxMachine *machine,
                        const uint8_t *rom, size_t rom_size) {
  if (!machine || !rom || rom_size < 0x8000u)
    return false;
  memset(machine, 0, sizeof(*machine));

  machine->reset_pc24 = ReadVector(rom, rom_size, 0x1C);
  machine->nmi_pc24_native = ReadVector(rom, rom_size, 0x0A);
  machine->irq_pc24_native = ReadVector(rom, rom_size, 0x0E);
  machine->nmi_pc24_emulation = ReadVector(rom, rom_size, 0x1A);
  machine->irq_pc24_emulation = ReadVector(rom, rom_size, 0x1E);
  if ((machine->reset_pc24 & 0xFFFFu) < 0x8000u)
    return false;

  cpu_state_init(&g_cpu, g_ram);
  g_snes->beamMasterLast = 0;
  machine->resume_pc24 = machine->reset_pc24;
  return true;
}

void PhalanxMachineReset(PhalanxMachine *machine) {
  if (!machine)
    return;
  snes_reset(g_snes, true);
  cpu_state_init(&g_cpu, g_ram);
  g_snes->beamMasterLast = 0;
  machine->resume_pc24 = machine->reset_pc24;
  machine->frame_start_master = 0;
  machine->frames = 0;
  machine->failed = false;
}

bool PhalanxMachineRunFrame(PhalanxMachine *machine, uint16_t host_buttons) {
  if (!machine || machine->failed)
    return false;

  /* Convert the shared host layout once to the SNESRecomp core's 12-bit
   * serial order. Keeping the result intact is required for every button;
   * notably, directions occupy core bits 7..4. */
  g_snes->input1_currentState = PhalanxInputToCoreState(host_buttons);
  g_snes->input2_currentState = 0;

  static int input_trace = -1;
  static uint16_t last_host_buttons = UINT16_MAX;
  if (input_trace < 0)
    input_trace = getenv("PHALANX_INPUT_TRACE") != NULL;
  const bool input_changed =
      input_trace && host_buttons != last_host_buttons;
  if (input_changed) {
    const uint16_t joy_register =
        SwapInputBits(g_snes->input1_currentState);
    fprintf(stderr,
            "Phalanx input: host=$%03X serial=$%04X joy=$%04X\n",
            host_buttons, g_snes->input1_currentState, joy_register);
  }

  const bool overscan = ppu_checkOverscan(g_ppu);
  const uint64_t vblank = machine->frame_start_master +
      (uint64_t)(overscan ? 240 : 225) * kMasterClocksPerLine;
  const uint64_t frame_end = machine->frame_start_master +
      kMasterClocksPerFrame;

  if (!RunCpuUntil(machine, vblank))
    return false;

  ppu_handleVblank(g_ppu);
  g_snes->inVblank = true;
  g_snes->inNmi = true;
  if (g_snes->nmiEnabled) {
    /* The whole-program bridge parks a stable hardware-poll loop at a
     * canonical instruction boundary. Preserve the small CPU recognition
     * window at the vblank edge before entering NMI so a pending $4210 read
     * can observe RDNMI. Entering the handler immediately lets its own $4210
     * read consume the latch first and deadlocks software polling RDNMI. */
    uint64_t recognition_end = g_cpu.master_cycles + kNmiRecognitionWindow;
    if (recognition_end > frame_end)
      recognition_end = frame_end;
    if (!RunCpuUntil(machine, recognition_end) ||
        !EnterInterrupt(machine, true))
      return false;
  }

  if (!RunCpuUntil(machine, frame_end))
    return false;

  machine->frame_start_master = frame_end;
  machine->frames++;
  snes_frame_counter = (int)machine->frames;
  g_snes->inVblank = false;
  g_snes->inNmi = false;

  if (input_changed) {
    const uint16_t raw_game =
        (uint16_t)(g_ram[0x0200] | ((uint16_t)g_ram[0x0201] << 8));
    const uint16_t current =
        (uint16_t)(g_ram[0x10A2] | ((uint16_t)g_ram[0x10A3] << 8));
    const uint16_t pressed =
        (uint16_t)(g_ram[0x10A4] | ((uint16_t)g_ram[0x10A5] << 8));
    const uint16_t gameplay =
        (uint16_t)(g_ram[0x021E] | ((uint16_t)g_ram[0x021F] << 8));
    fprintf(stderr,
            "Phalanx input commit: menu=$%04X current=$%04X pressed=$%04X "
            "gameplay=$%04X\n",
            raw_game, current, pressed, gameplay);
    last_host_buttons = host_buttons;
  }

  static int trace = -1;
  if (trace < 0)
    trace = getenv("PHALANX_MACHINE_TRACE") != NULL;
  if (trace && (machine->frames <= 10 || machine->frames % 60 == 0)) {
    fprintf(stderr,
            "[machine] frame=%llu pc=$%06X master=%llu beam=%u,%u "
            "P=$%02X E=%u I=%u nmi=%u irq=%u INIDISP=$%02X TM=$%02X\n",
            (unsigned long long)machine->frames, machine->resume_pc24,
            (unsigned long long)g_cpu.master_cycles, g_snes->vPos,
            g_snes->hPos, g_cpu.P, g_cpu.emulation, g_cpu._flag_I,
            g_snes->nmiEnabled, g_snes->inIrq, g_ppu->inidisp,
            g_ppu->screenEnabled[0]);
  }
  return true;
}

void PhalanxMachineRender(uint32_t *pixels, size_t pitch) {
  if (!pixels || !g_ppu || !g_dma)
    return;

  memset(pixels, 0, pitch * kVisibleLines);
  PpuBeginDrawing(g_ppu, (uint8_t *)pixels, pitch,
                  kPpuRenderFlags_NewRenderer);

  SimpleHdma channels[8];
  memset(channels, 0, sizeof(channels));
  dma_startDma(g_dma, g_snesrecomp_last_hdmaen, true);
  for (unsigned channel = 0; channel < 8; channel++) {
    if (g_snesrecomp_last_hdmaen & (1u << channel))
      SimpleHdma_Init(&channels[channel], &g_dma->channel[channel]);
  }

  ppu_runLine(g_ppu, 0);
  for (int line = 1; line <= kVisibleLines; line++) {
    ppu_runLine(g_ppu, line);
    for (unsigned channel = 0; channel < 8; channel++) {
      if (g_snesrecomp_last_hdmaen & (1u << channel))
        SimpleHdma_DoLine(&channels[channel]);
    }
  }
}
