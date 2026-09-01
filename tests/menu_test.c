/*
 * Menu smoke test. No ROM and no machine; it needs only a window.
 *
 * It covers what breaks silently: a backend that fails to initialize, a draw
 * that asserts, the renderer state PhalanxUiRender switches around Dear ImGui
 * (which rescales the game's own frame for every later present if any of it
 * is left behind), and where the overlay's pixels actually land.
 *
 * It prefers the real video driver so a high-DPI display is exercised — the
 * point-to-pixel scale is the whole reason the overlay has to set a render
 * scale at all, and a renderer whose output matches its window cannot show
 * that being wrong. SDL's dummy driver is the fallback for a headless run,
 * where the extent check still holds at 1:1.
 */
#include "savestate.h"
#include "ui.h"

#include <SDL.h>

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
  kWindowWidth = 896,
  kWindowHeight = 672,
  /* Deliberately not the window size. The app presents an emulated-pixel
   * logical size into a larger window — and on a Retina display the renderer
   * output is larger again — so the renderer's pixel and logical coordinate
   * spaces differ. State that survives the overlay's round trip in the wrong
   * one is only wrong when they differ, which is exactly the case a window
   * sized 1:1 with its logical size cannot reproduce. */
  kLogicalWidth = 256,
  kLogicalHeight = 224,
};

static int Fail(const char *message) {
  fprintf(stderr, "menu: %s\n", message);
  return 1;
}

/* Everything about the renderer that decides where the game's frame lands. */
typedef struct RendererState {
  int logical_width;
  int logical_height;
  int integer_scale;
  int clip_enabled;
  SDL_Rect viewport;
  SDL_Rect clip;
  float scale_x;
  float scale_y;
} RendererState;

static void CaptureRendererState(SDL_Renderer *renderer, RendererState *out) {
  memset(out, 0, sizeof(*out));
  SDL_RenderGetLogicalSize(renderer, &out->logical_width, &out->logical_height);
  out->integer_scale = SDL_RenderGetIntegerScale(renderer) ? 1 : 0;
  out->clip_enabled = SDL_RenderIsClipEnabled(renderer) ? 1 : 0;
  SDL_RenderGetViewport(renderer, &out->viewport);
  SDL_RenderGetClipRect(renderer, &out->clip);
  SDL_RenderGetScale(renderer, &out->scale_x, &out->scale_y);
}

static void ReportRendererState(const char *label, const RendererState *s) {
  fprintf(stderr,
          "  %s logical=%dx%d scale=%.2f,%.2f viewport=%d,%d %dx%d "
          "clip=%d integer=%d\n",
          label, s->logical_width, s->logical_height, s->scale_x, s->scale_y,
          s->viewport.x, s->viewport.y, s->viewport.w, s->viewport.h,
          s->clip_enabled, s->integer_scale);
}

/* Read the whole render target in raw pixels. SDL_RenderReadPixels works in
 * the renderer's logical coordinates, so with a logical size active it would
 * hand back only the letterboxed sub-rect, rescaled — useless for judging
 * where something landed on screen. */
static SDL_Surface *ReadTarget(SDL_Renderer *renderer, int width, int height) {
  SDL_Surface *shot = SDL_CreateRGBSurfaceWithFormat(
      0, width, height, 32, SDL_PIXELFORMAT_ARGB8888);
  if (!shot)
    return NULL;

  int logical_width = 0;
  int logical_height = 0;
  SDL_RenderGetLogicalSize(renderer, &logical_width, &logical_height);
  SDL_RenderSetLogicalSize(renderer, 0, 0);
  SDL_RenderSetViewport(renderer, NULL);
  SDL_RenderSetClipRect(renderer, NULL);
  const int result = SDL_RenderReadPixels(renderer, NULL,
                                          SDL_PIXELFORMAT_ARGB8888,
                                          shot->pixels, shot->pitch);
  SDL_RenderSetLogicalSize(renderer, logical_width, logical_height);
  SDL_RenderSetViewport(renderer, NULL);

  if (result != 0) {
    SDL_FreeSurface(shot);
    return NULL;
  }
  return shot;
}

/* Bounding box of everything drawn over the cleared background, in renderer
 * pixels. Empty box (w or h of 0) when nothing was drawn. */
static bool DrawnExtent(SDL_Renderer *renderer, int width, int height,
                        SDL_Rect *out) {
  SDL_Surface *shot = ReadTarget(renderer, width, height);
  if (!shot)
    return false;

  int min_x = width;
  int min_y = height;
  int max_x = -1;
  int max_y = -1;
  for (int y = 0; y < height; y++) {
    const uint32_t *row =
        (const uint32_t *)((const uint8_t *)shot->pixels + (size_t)y * shot->pitch);
    for (int x = 0; x < width; x++) {
      if ((row[x] & 0x00FFFFFFu) == 0)
        continue;
      if (x < min_x) min_x = x;
      if (y < min_y) min_y = y;
      if (x > max_x) max_x = x;
      if (y > max_y) max_y = y;
    }
  }
  SDL_FreeSurface(shot);

  out->x = max_x < 0 ? 0 : min_x;
  out->y = max_y < 0 ? 0 : min_y;
  out->w = max_x < 0 ? 0 : max_x - min_x + 1;
  out->h = max_y < 0 ? 0 : max_y - min_y + 1;
  return true;
}

/* One iteration of the app's present: the game's frame through the logical
 * size, then the overlay on top. Mirrors PresentFrame in src/main.c. */
static void PresentGameFrame(SDL_Renderer *renderer, SDL_Texture *frame,
                             PhalanxUiState *state) {
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
  SDL_RenderClear(renderer);
  SDL_RenderCopy(renderer, frame, NULL, NULL);
  PhalanxUiRender(renderer, state);
}

/* Optional: dump what the overlay drew, so a layout change can be looked at
 * without a display. Off unless a path is passed on the command line. */
static bool DumpFrame(SDL_Renderer *renderer, int width, int height,
                      const char *path) {
  SDL_Surface *shot = ReadTarget(renderer, width, height);
  if (!shot)
    return false;
  const bool ok = SDL_SaveBMP(shot, path) == 0;
  SDL_FreeSurface(shot);
  return ok;
}

int main(int argc, char **argv) {
  SDL_SetMainReady();
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
      fprintf(stderr, "menu: SDL_Init: %s\n", SDL_GetError());
      return 2;
    }
  }

  /* Hidden, but a real window on a real display: SDL still reports the
   * display's pixel density, which is what the overlay has to scale by. */
  SDL_Window *window = SDL_CreateWindow("menu", SDL_WINDOWPOS_CENTERED,
                                        SDL_WINDOWPOS_CENTERED, kWindowWidth,
                                        kWindowHeight, SDL_WINDOW_HIDDEN |
                                        SDL_WINDOW_ALLOW_HIGHDPI);
  SDL_Renderer *renderer = window
      ? SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE) : NULL;
  if (!window || !renderer) {
    fprintf(stderr, "menu: could not create a window: %s\n", SDL_GetError());
    SDL_Quit();
    return 2;
  }
  SDL_RenderSetLogicalSize(renderer, kLogicalWidth, kLogicalHeight);

  int output_width = 0;
  int output_height = 0;
  int window_width = 0;
  int window_height = 0;
  SDL_GetRendererOutputSize(renderer, &output_width, &output_height);
  SDL_GetWindowSize(window, &window_width, &window_height);
  const float pixel_scale = window_width > 0
      ? (float)output_width / (float)window_width : 1.0f;
  printf("menu: %s driver, window %dx%d, output %dx%d\n",
         SDL_GetCurrentVideoDriver(), window_width, window_height,
         output_width, output_height);

  if (!PhalanxUiInit(window, renderer))
    return Fail("the menu could not be created");
  if (PhalanxUiIsOpen())
    return Fail("the menu started open");

  PhalanxUiState state;
  memset(&state, 0, sizeof(state));
  state.request_save = -1;
  state.request_load = -1;

  /* Closed: nothing draws and no event is consumed. */
  SDL_Event event;
  memset(&event, 0, sizeof(event));
  event.type = SDL_KEYDOWN;
  event.key.keysym.sym = SDLK_a;
  if (PhalanxUiProcessEvent(&event))
    return Fail("a closed menu consumed an event");
  PhalanxUiRender(renderer, &state);

  /* Where the game's own frame lands before the menu has ever drawn. This is
   * the regression the renderer-state juggling above exists to prevent: the
   * frame is presented through the logical size, so anything the overlay
   * leaves behind moves or rescales it — including after the menu is closed
   * again. */
  SDL_Texture *frame = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                         SDL_TEXTUREACCESS_STREAMING,
                                         kLogicalWidth, kLogicalHeight);
  if (!frame)
    return Fail("could not create the frame texture");
  {
    uint32_t *pixels = (uint32_t *)malloc(sizeof(uint32_t) * kLogicalWidth *
                                          kLogicalHeight);
    if (!pixels)
      return Fail("out of memory");
    for (int i = 0; i < kLogicalWidth * kLogicalHeight; i++)
      pixels[i] = 0xFFFFFFFFu;
    SDL_UpdateTexture(frame, NULL, pixels, kLogicalWidth * sizeof(uint32_t));
    free(pixels);
  }

  SDL_Rect game_before;
  PresentGameFrame(renderer, frame, &state);
  if (!DrawnExtent(renderer, output_width, output_height, &game_before))
    return Fail("could not read back the game frame");
  if (game_before.w <= 0 || game_before.h <= 0)
    return Fail("the game frame drew nothing");

  PhalanxUiToggle();
  if (!PhalanxUiIsOpen())
    return Fail("the menu did not open");
  if (!PhalanxUiProcessEvent(&event))
    return Fail("an open menu ignored an event");

  /* The overlay draws in window pixels and the game presents through a
   * logical size, so PhalanxUiRender has to switch presentation off and back
   * on around Dear ImGui. Every field it touches is checked here: a viewport
   * or integer-scale flag that survives that round trip in the wrong units
   * rescales the game's own frame for every present afterwards, which is not
   * visible in the overlay itself. Both presentation shapes are exercised
   * because they are separate SDL states, and several frames each because
   * Dear ImGui only settles window layout and its font atlas after the
   * first one. */
  for (int integer_scale = 0; integer_scale < 2; integer_scale++) {
    SDL_RenderSetIntegerScale(renderer, integer_scale ? SDL_TRUE : SDL_FALSE);

    RendererState before;
    CaptureRendererState(renderer, &before);

    for (int frame = 0; frame < 4; frame++) {
      PhalanxUiSetGamepad(frame == 2 ? 0xFFFFu : 0u);
      SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
      SDL_RenderClear(renderer);
      PhalanxUiRender(renderer, &state);

      RendererState after;
      CaptureRendererState(renderer, &after);
      if (memcmp(&before, &after, sizeof(before)) != 0) {
        fprintf(stderr, "menu: the overlay changed the game's presentation "
                        "(integer scaling %s)\n", integer_scale ? "on" : "off");
        ReportRendererState("before", &before);
        ReportRendererState("after ", &after);
        return 1;
      }
    }
  }
  SDL_RenderSetIntegerScale(renderer, SDL_FALSE);

  /* SDL hands mouse events to the app in the renderer's logical coordinates,
   * not window points — SDL_GetMouseState stays in window points, so the two
   * spaces disagree the moment the window is not exactly its logical size.
   * The menu is laid out and drawn in window points, so the cursor it sees
   * has to come back to that space or every click lands somewhere else.
   *
   * The centre of the logical area is the centre of the window's letterboxed
   * viewport, whatever the scale. */
  {
    int expected_x = 0;
    int expected_y = 0;
    SDL_RenderLogicalToWindow(renderer, (float)kLogicalWidth / 2.0f,
                              (float)kLogicalHeight / 2.0f,
                              &expected_x, &expected_y);

    SDL_Event motion;
    memset(&motion, 0, sizeof(motion));
    motion.type = SDL_MOUSEMOTION;
    motion.motion.windowID = SDL_GetWindowID(window);
    motion.motion.x = kLogicalWidth / 2;
    motion.motion.y = kLogicalHeight / 2;
    if (!PhalanxUiProcessEvent(&motion))
      return Fail("the open menu ignored a mouse event");
    PhalanxUiRender(renderer, &state);

    float mouse_x = 0.0f;
    float mouse_y = 0.0f;
    if (!PhalanxUiMousePosition(&mouse_x, &mouse_y))
      return Fail("the menu has no cursor position");
    if (fabsf(mouse_x - (float)expected_x) > 1.0f ||
        fabsf(mouse_y - (float)expected_y) > 1.0f) {
      fprintf(stderr, "menu: the cursor lands in the wrong place\n");
      fprintf(stderr, "  event at logical %d,%d is window point %d,%d\n",
              kLogicalWidth / 2, kLogicalHeight / 2, expected_x, expected_y);
      fprintf(stderr, "  the menu thinks the cursor is at %.1f,%.1f\n",
              mouse_x, mouse_y);
      return 1;
    }
  }

  /* Where the overlay's pixels actually landed. Dear ImGui lays the window
   * out in window points; the renderer draws in pixels. If the point-to-pixel
   * scale is not applied, this box comes back at 1/scale of its size in the
   * corner of the window — which is what a high-DPI display shows when the
   * overlay forgets to set a render scale. */
  SDL_Rect drawn;
  if (!DrawnExtent(renderer, output_width, output_height, &drawn))
    return Fail("could not read back the overlay");

  const SDL_Rect expected = {
    (int)(PHALANX_UI_MENU_X * pixel_scale),
    (int)(PHALANX_UI_MENU_Y * pixel_scale),
    (int)(PHALANX_UI_MENU_WIDTH * pixel_scale),
    (int)(PHALANX_UI_MENU_HEIGHT * pixel_scale),
  };
  /* Rounded corners pull the extreme rows and columns in by the corner
   * radius, so the box is allowed to be slightly small — never large, and
   * never displaced. */
  const int slack = (int)(8.0f * pixel_scale) + 2;
  if (abs(drawn.x - expected.x) > slack || abs(drawn.y - expected.y) > slack ||
      drawn.w > expected.w + slack || drawn.w < expected.w - slack ||
      drawn.h > expected.h + slack || drawn.h < expected.h - slack) {
    fprintf(stderr, "menu: the overlay drew at the wrong scale or place\n");
    fprintf(stderr, "  expected %d,%d %dx%d (scale %.2f, slack %d)\n",
            expected.x, expected.y, expected.w, expected.h, pixel_scale, slack);
    fprintf(stderr, "  drawn    %d,%d %dx%d\n",
            drawn.x, drawn.y, drawn.w, drawn.h);
    return 1;
  }

  if (argc > 1 && !DumpFrame(renderer, output_width, output_height, argv[1]))
    return Fail("the frame dump failed");

  if (state.request_save != -1 || state.request_load != -1 ||
      state.request_reset || state.request_quit)
    return Fail("the menu raised a request nobody asked for");

  PhalanxUiSetStatus(&state, "Saved slot %d.", 7);
  if (strcmp(state.status, "Saved slot 7.") != 0)
    return Fail("the status line did not format");

  /* Slot naming is what the menu labels and what main.c saves through. */
  char path[64];
  if (!PhalanxSaveStateSlotPath(0, path, sizeof(path)) ||
      strcmp(path, "saves/phalanx0.sav") != 0)
    return Fail("slot 1 resolved to an unexpected file");
  if (PhalanxSaveStateSlotPath(PHALANX_SAVE_SLOTS, path, sizeof(path)) ||
      PhalanxSaveStateSlotPath(-1, path, sizeof(path)))
    return Fail("an out-of-range slot resolved to a file");

  char detail[32] = "stale";
  if (PhalanxSaveStateSlotUsed(0, detail, sizeof(detail)) || detail[0])
    return Fail("a missing slot reported contents");

  PhalanxUiToggle();
  if (PhalanxUiIsOpen())
    return Fail("the menu did not close");

  /* The frame the player is left with once the menu is closed again. */
  SDL_Rect game_after;
  PresentGameFrame(renderer, frame, &state);
  if (!DrawnExtent(renderer, output_width, output_height, &game_after))
    return Fail("could not read back the game frame");
  if (SDL_memcmp(&game_before, &game_after, sizeof(game_before)) != 0) {
    fprintf(stderr, "menu: the game's frame moved after the menu was used\n");
    fprintf(stderr, "  before %d,%d %dx%d\n", game_before.x, game_before.y,
            game_before.w, game_before.h);
    fprintf(stderr, "  after  %d,%d %dx%d\n", game_after.x, game_after.y,
            game_after.w, game_after.h);
    return 1;
  }
  SDL_DestroyTexture(frame);

  PhalanxUiShutdown();
  PhalanxUiShutdown();

  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();

  puts("menu: overlay init, draw, gamepad nav, and slot naming passed");
  return 0;
}
