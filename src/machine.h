#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct PhalanxMachine {
  uint32_t resume_pc24;
  uint32_t reset_pc24;
  uint32_t nmi_pc24_native;
  uint32_t nmi_pc24_emulation;
  uint32_t irq_pc24_native;
  uint32_t irq_pc24_emulation;
  uint64_t frame_start_master;
  uint64_t frames;
  bool failed;
} PhalanxMachine;

bool PhalanxMachineInit(PhalanxMachine *machine,
                        const uint8_t *rom, size_t rom_size);
void PhalanxMachineReset(PhalanxMachine *machine);
/* host_buttons uses enum PhalanxInputButton's canonical 12-bit layout. */
bool PhalanxMachineRunFrame(PhalanxMachine *machine, uint16_t host_buttons);
void PhalanxMachineRender(uint32_t *pixels, size_t pitch);

#ifdef __cplusplus
}
#endif
