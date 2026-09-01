#pragma once

#include <stddef.h>
#include <stdint.h>

int MacChoosePhalanxRom(char *path, size_t path_size);
/* Create (if needed) and return the app's Application Support folder — the
 * home of keybinds.ini and the saves folder. Mutable state never goes in the
 * signed bundle. */
int MacPrepareSupportDirectory(char *path, size_t path_size);
void MacShowError(const char *title, const char *message);

void MacGamepadInitialize(void);
void MacGamepadShutdown(void);
uint16_t MacGamepadReadButtons(void);
