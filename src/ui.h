#pragma once

#include <SDL.h>

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Where the menu window opens, in window points. Exposed so the test can say
 * where the overlay's pixels belong once the display's point-to-pixel scale
 * is applied. */
enum {
  PHALANX_UI_MENU_X = 24,
  PHALANX_UI_MENU_Y = 20,
  PHALANX_UI_MENU_WIDTH = 470,
  PHALANX_UI_MENU_HEIGHT = 620,
};

/* Everything the menu shows or changes. main.c owns the struct, applies the
 * request fields once per frame at a safe point, and clears them. The menu
 * never touches the machine itself. */
typedef struct PhalanxUiState {
  /* Live state, mirrored by the menu's controls. */
  bool paused;
  bool fullscreen;
  bool integer_scale;
  bool muted;
  int slot;             /* 0-based; the menu labels it 1..10 */
  unsigned long frames;

  /* Requests, consumed and cleared by main.c after the frame. */
  int request_save;     /* slot, or -1 */
  int request_load;     /* slot, or -1 */
  bool request_reset;
  bool request_quit;

  /* One-line result of the last action, shown under the slot list. */
  char status[96];
} PhalanxUiState;

/* Dear ImGui over the SDL renderer that already presents the frame. Returns
 * false and leaves the game running when the UI cannot be created. */
bool PhalanxUiInit(SDL_Window *window, SDL_Renderer *renderer);
void PhalanxUiShutdown(void);

/* Feed every SDL event here before the game handles it. Returns true when
 * the menu is open and consumed it. */
bool PhalanxUiProcessEvent(const SDL_Event *event);

void PhalanxUiToggle(void);
bool PhalanxUiIsOpen(void);

/* The controller reaches the app through GameController.framework, which the
 * Dear ImGui SDL backend cannot see. Pass the pad the game would have
 * received (enum PhalanxInputButton layout) so the menu can be navigated
 * with it while it is open. */
void PhalanxUiSetGamepad(uint16_t host_buttons);

/* Draw the menu over the presented frame. No-op while closed. */
void PhalanxUiRender(SDL_Renderer *renderer, PhalanxUiState *state);

/* The cursor position the menu is working from, in window points, as of the
 * last draw. False when the menu has never drawn or has no position. Exists
 * so the test can check that a mouse event lands where the cursor actually
 * is: SDL delivers those events in the renderer's logical coordinates, which
 * are not the coordinates the menu is laid out in. */
bool PhalanxUiMousePosition(float *x, float *y);

void PhalanxUiSetStatus(PhalanxUiState *state, const char *format, ...);

#ifdef __cplusplus
}
#endif
