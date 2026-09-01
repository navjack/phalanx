#include "input.h"

#include <stdint.h>
#include <stdio.h>

typedef struct ButtonCase {
  const char *name;
  uint16_t host;
  uint16_t core;
} ButtonCase;

int main(void) {
  static const ButtonCase cases[] = {
    {"R",      PHALANX_INPUT_R,      1u << 11},
    {"L",      PHALANX_INPUT_L,      1u << 10},
    {"X",      PHALANX_INPUT_X,      1u << 9},
    {"A",      PHALANX_INPUT_A,      1u << 8},
    {"Right",  PHALANX_INPUT_RIGHT,  1u << 7},
    {"Left",   PHALANX_INPUT_LEFT,   1u << 6},
    {"Down",   PHALANX_INPUT_DOWN,   1u << 5},
    {"Up",     PHALANX_INPUT_UP,     1u << 4},
    {"Start",  PHALANX_INPUT_START,  1u << 3},
    {"Select", PHALANX_INPUT_SELECT, 1u << 2},
    {"Y",      PHALANX_INPUT_Y,      1u << 1},
    {"B",      PHALANX_INPUT_B,      1u << 0},
  };

  for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
    const uint16_t actual = PhalanxInputToCoreState(cases[i].host);
    if (actual != cases[i].core) {
      fprintf(stderr, "%s: core=$%04X expected=$%04X\n",
              cases[i].name, actual, cases[i].core);
      return 1;
    }
  }

  const uint16_t all_directions =
      PHALANX_INPUT_RIGHT | PHALANX_INPUT_LEFT |
      PHALANX_INPUT_DOWN | PHALANX_INPUT_UP;
  if (PhalanxInputRemoveOpposites(all_directions) != 0) {
    fprintf(stderr, "opposing directions were not neutralized\n");
    return 1;
  }

  puts("input mapping: 12 buttons and opposite-direction filtering passed");
  return 0;
}
