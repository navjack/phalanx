#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum PhalanxInputButton {
  PHALANX_INPUT_R      = 1u << 0,
  PHALANX_INPUT_L      = 1u << 1,
  PHALANX_INPUT_X      = 1u << 2,
  PHALANX_INPUT_A      = 1u << 3,
  PHALANX_INPUT_RIGHT  = 1u << 4,
  PHALANX_INPUT_LEFT   = 1u << 5,
  PHALANX_INPUT_DOWN   = 1u << 6,
  PHALANX_INPUT_UP     = 1u << 7,
  PHALANX_INPUT_START  = 1u << 8,
  PHALANX_INPUT_SELECT = 1u << 9,
  PHALANX_INPUT_Y      = 1u << 10,
  PHALANX_INPUT_B      = 1u << 11,
};

uint16_t PhalanxInputRemoveOpposites(uint16_t host_buttons);
uint16_t PhalanxInputToCoreState(uint16_t host_buttons);

#ifdef __cplusplus
}
#endif
