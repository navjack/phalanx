#pragma once

#include "machine.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct SaveLoadInfo;

enum {
  /* Slot numbering is 0-based internally; the menu presents 1..10. */
  PHALANX_SAVE_SLOTS = 10,
};

/* Bind the machine every snapshot reads and writes. Call before the first
 * save or load; until then both refuse to run. */
void PhalanxSaveStateBind(PhalanxMachine *machine);

/* RtlGameInfo hooks. They extend a framework snapshot with the host-side
 * execution state snes_saveload does not model — the LLE tier's CpuState
 * register file, the frame scheduler's continuation, and the APU pacing
 * anchors — and reconcile it after a load. */
void PhalanxStateSaveExtra(struct SaveLoadInfo *sli);
void PhalanxStateLoadExtra(struct SaveLoadInfo *sli, uint32_t version);
void PhalanxStateLoaded(uint32_t version);

/* Anchor state files under `directory` (the app's Application Support
 * folder). RtlSaveLoad names slots relative to the process working
 * directory, so this changes the working directory and creates the
 * `saves` subfolder inside it. Returns false if either step fails. */
bool PhalanxSaveStatePrepare(const char *directory);

/* Save/load slot 0..PHALANX_SAVE_SLOTS-1. Both refuse to touch a machine
 * that has already failed, and load leaves the machine untouched when the
 * slot file is missing or unreadable. */
bool PhalanxSaveStateSave(int slot);
bool PhalanxSaveStateLoad(int slot);

/* ── Slot naming and metadata (src/save_slots.c) ──────────────────────────
 * Kept apart from the snapshot itself: the menu asks what a slot holds
 * without linking against the machine. */

/* Path of a slot's file, relative to the directory PhalanxSaveStatePrepare
 * anchored. False for an out-of-range slot. */
bool PhalanxSaveStateSlotPath(int slot, char *path, size_t path_size);

/* True when the slot holds a state file. On success `detail` receives a
 * short human-readable timestamp ("2026-08-10 19:04"); on failure it
 * receives an empty string. `detail` may be NULL. */
bool PhalanxSaveStateSlotUsed(int slot, char *detail, size_t detail_size);

#ifdef __cplusplus
}
#endif
