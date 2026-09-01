/*
 * Where save states live and what is in them. Deliberately free of any
 * dependency on the machine or the SNESRecomp runtime: the menu draws a slot
 * list every frame it is open, and the menu's own test links only this.
 */
#include "savestate.h"

#include <stdio.h>
#include <sys/stat.h>
#include <time.h>

/* Relative to the directory PhalanxSaveStatePrepare anchored. The prefix
 * must match RtlGameInfo.save_name_prefix in src/main.c so the framework's
 * own RtlSaveLoad path names the same files. */
static const char kSlotPathFormat[] = "saves/phalanx%d.sav";

bool PhalanxSaveStateSlotPath(int slot, char *path, size_t path_size) {
  if (!path || slot < 0 || slot >= PHALANX_SAVE_SLOTS)
    return false;
  const int written = snprintf(path, path_size, kSlotPathFormat, slot);
  return written > 0 && (size_t)written < path_size;
}

bool PhalanxSaveStateSlotUsed(int slot, char *detail, size_t detail_size) {
  if (detail && detail_size)
    detail[0] = '\0';

  char path[64];
  struct stat info;
  if (!PhalanxSaveStateSlotPath(slot, path, sizeof(path)) ||
      stat(path, &info) != 0 || info.st_size <= 0)
    return false;

  if (detail && detail_size) {
    struct tm local;
    if (localtime_r(&info.st_mtime, &local))
      strftime(detail, detail_size, "%Y-%m-%d %H:%M", &local);
  }
  return true;
}
