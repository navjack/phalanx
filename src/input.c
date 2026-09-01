#include "input.h"

static uint16_t ReverseBits16(uint16_t value) {
  uint16_t result = 0;
  for (unsigned bit = 0; bit < 16; bit++)
    result |= ((value >> bit) & 1u) << (15u - bit);
  return result;
}

uint16_t PhalanxInputRemoveOpposites(uint16_t host_buttons) {
  const uint16_t horizontal = PHALANX_INPUT_RIGHT | PHALANX_INPUT_LEFT;
  const uint16_t vertical = PHALANX_INPUT_DOWN | PHALANX_INPUT_UP;
  if ((host_buttons & horizontal) == horizontal)
    host_buttons &= (uint16_t)~horizontal;
  if ((host_buttons & vertical) == vertical)
    host_buttons &= (uint16_t)~vertical;
  return host_buttons;
}

uint16_t PhalanxInputToCoreState(uint16_t host_buttons) {
  /* keybinds and the native macOS backend share the architectural 12-bit
   * layout R,L,X,A,Right,Left,Down,Up,Start,Select,Y,B. The SNESRecomp core
   * expects the controller's serial order instead: B first through R last.
   * That is exactly a reversal of the twelve meaningful bits. Keep this
   * single conversion at the machine boundary so every host input source
   * reaches the core through the same representation. */
  return (uint16_t)(ReverseBits16(host_buttons & 0x0FFFu) >> 4);
}
