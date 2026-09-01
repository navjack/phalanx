#include "ui.h"

#include "input.h"
#include "savestate.h"

#include "imgui.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_sdlrenderer2.h"

#include <float.h>
#include <stdarg.h>
#include <stdio.h>

static bool s_open;
static bool s_ready;
static uint16_t s_gamepad;
static SDL_Renderer *s_renderer;

bool PhalanxUiInit(SDL_Window *window, SDL_Renderer *renderer) {
  if (s_ready)
    return true;
  IMGUI_CHECKVERSION();
  if (!ImGui::CreateContext())
    return false;

  ImGuiIO &io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  /* The gamepad arrives through GameController.framework, not SDL, so the
   * backend cannot discover it. PhalanxUiSetGamepad feeds nav keys instead;
   * this flag advertises that a pad is present. */
  io.BackendFlags |= ImGuiBackendFlags_HasGamepad;
  io.IniFilename = NULL;
  io.LogFilename = NULL;

  ImGui::StyleColorsDark();
  ImGuiStyle &style = ImGui::GetStyle();
  style.FontScaleMain = 1.25f;
  style.WindowRounding = 6.0f;
  style.FrameRounding = 4.0f;

  if (!ImGui_ImplSDL2_InitForSDLRenderer(window, renderer)) {
    ImGui::DestroyContext();
    return false;
  }
  if (!ImGui_ImplSDLRenderer2_Init(renderer)) {
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    return false;
  }
  s_renderer = renderer;
  s_ready = true;
  s_open = false;
  return true;
}

void PhalanxUiShutdown(void) {
  if (!ImGui::GetCurrentContext())
    return;
  if (s_ready) {
    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
  }
  ImGui::DestroyContext();
  s_renderer = NULL;
  s_ready = false;
  s_open = false;
}

/* SDL rewrites mouse event coordinates into the renderer's logical space
 * whenever a logical size is set, while the menu is laid out — and drawn — in
 * window points. The two agree only while the window happens to be exactly
 * its logical size, so a resized window used to click nowhere near the cursor.
 * Undo SDL's mapping before Dear ImGui sees it. That also settles a
 * disagreement inside the backend, whose own fallback path reads window
 * points straight from SDL_GetMouseState.
 *
 * Writes the corrected copy to `out` and returns true when `event` is a mouse
 * event with a position; leaves `out` alone otherwise. */
static bool MouseEventPosition(const SDL_Event *event, SDL_Event *out) {
  int x = 0;
  int y = 0;
  switch (event->type) {
  case SDL_MOUSEMOTION:
    SDL_RenderLogicalToWindow(s_renderer, (float)event->motion.x,
                              (float)event->motion.y, &x, &y);
    *out = *event;
    out->motion.x = x;
    out->motion.y = y;
    return true;
  case SDL_MOUSEBUTTONDOWN:
  case SDL_MOUSEBUTTONUP:
    SDL_RenderLogicalToWindow(s_renderer, (float)event->button.x,
                              (float)event->button.y, &x, &y);
    *out = *event;
    out->button.x = x;
    out->button.y = y;
    return true;
  default:
    /* Wheel deltas carry no position Dear ImGui reads, and SDL leaves them
     * in their own units either way. */
    return false;
  }
}

bool PhalanxUiProcessEvent(const SDL_Event *event) {
  /* Events reach Dear ImGui only while the menu is open. Feeding a closed
   * menu would queue input the library never drains, and its own NewFrame
   * re-reads the mouse when the menu opens. */
  if (!s_ready || !s_open || !event)
    return false;

  SDL_Event converted;
  if (s_renderer && MouseEventPosition(event, &converted))
    event = &converted;
  ImGui_ImplSDL2_ProcessEvent(event);
  return true;
}

void PhalanxUiToggle(void) {
  if (s_ready)
    s_open = !s_open;
}

bool PhalanxUiIsOpen(void) {
  return s_ready && s_open;
}

void PhalanxUiSetGamepad(uint16_t host_buttons) {
  s_gamepad = host_buttons;
}

/* Translate the pad the game would have received into Dear ImGui's nav keys.
 * main.c stops routing the pad to the machine while the menu is open, so the
 * two can never act on the same press. */
static void FeedGamepadNav(void) {
  ImGuiIO &io = ImGui::GetIO();
  static const struct {
    uint16_t mask;
    ImGuiKey key;
  } kNav[] = {
    { PHALANX_INPUT_UP,     ImGuiKey_GamepadDpadUp },
    { PHALANX_INPUT_DOWN,   ImGuiKey_GamepadDpadDown },
    { PHALANX_INPUT_LEFT,   ImGuiKey_GamepadDpadLeft },
    { PHALANX_INPUT_RIGHT,  ImGuiKey_GamepadDpadRight },
    { PHALANX_INPUT_B,      ImGuiKey_GamepadFaceDown },
    { PHALANX_INPUT_A,      ImGuiKey_GamepadFaceRight },
    { PHALANX_INPUT_L,      ImGuiKey_GamepadL1 },
    { PHALANX_INPUT_R,      ImGuiKey_GamepadR1 },
  };
  for (unsigned i = 0; i < IM_ARRAYSIZE(kNav); i++)
    io.AddKeyEvent(kNav[i].key, (s_gamepad & kNav[i].mask) != 0);
}

static void DrawSaveStates(PhalanxUiState *state) {
  ImGui::SeparatorText("Save states");
  for (int slot = 0; slot < PHALANX_SAVE_SLOTS; slot++) {
    char detail[32];
    const bool used = PhalanxSaveStateSlotUsed(slot, detail, sizeof(detail));

    ImGui::PushID(slot);
    char label[16];
    snprintf(label, sizeof(label), "Slot %d", slot + 1);
    if (ImGui::RadioButton(label, state->slot == slot))
      state->slot = slot;

    ImGui::SameLine(110.0f);
    if (ImGui::Button("Save"))
      state->request_save = slot;
    ImGui::SameLine();
    ImGui::BeginDisabled(!used);
    if (ImGui::Button("Load"))
      state->request_load = slot;
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::TextDisabled("%s", used ? detail : "empty");
    ImGui::PopID();
  }
  ImGui::TextWrapped("%s", state->status[0] ? state->status
                                            : "The selected slot is what "
                                              "Command-S and Command-L use.");
}

static void DrawSystem(PhalanxUiState *state) {
  ImGui::SeparatorText("System");
  ImGui::Checkbox("Paused", &state->paused);
  ImGui::SameLine();
  ImGui::Checkbox("Mute", &state->muted);

  ImGui::Checkbox("Fullscreen", &state->fullscreen);
  ImGui::SameLine();
  ImGui::Checkbox("Integer scaling", &state->integer_scale);

  if (ImGui::Button("Reset"))
    state->request_reset = true;
  ImGui::SameLine();
  if (ImGui::Button("Quit"))
    state->request_quit = true;
}

static void DrawHelp(unsigned long frames) {
  ImGui::SeparatorText("Controls");
  ImGui::TextWrapped("F1 opens this menu. Shift-F1 to Shift-F10 save slots 1 "
                     "to 10; F2 to F10 load slots 2 to 10.");
  ImGui::TextWrapped("Command-P pauses, Command-R resets, Command-F is "
                     "fullscreen.");
  ImGui::TextDisabled("Frame %lu", frames);
}

void PhalanxUiRender(SDL_Renderer *renderer, PhalanxUiState *state) {
  if (!PhalanxUiIsOpen() || !renderer || !state)
    return;

  FeedGamepadNav();
  ImGui_ImplSDLRenderer2_NewFrame();
  ImGui_ImplSDL2_NewFrame();
  ImGui::NewFrame();

  ImGui::SetNextWindowSize(
      ImVec2((float)PHALANX_UI_MENU_WIDTH, (float)PHALANX_UI_MENU_HEIGHT),
      ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowPos(
      ImVec2((float)PHALANX_UI_MENU_X, (float)PHALANX_UI_MENU_Y),
      ImGuiCond_FirstUseEver);
  bool open = true;
  if (ImGui::Begin("Phalanx (Recompiled)", &open)) {
    DrawSystem(state);
    DrawSaveStates(state);
    DrawHelp(state->frames);
  }
  ImGui::End();
  if (!open)
    s_open = false;

  ImGui::Render();

  /* The game presents through an emulated-pixel logical size; Dear ImGui
   * draws in window points. Render it with logical presentation off, then put
   * back every piece of renderer state that costs.
   *
   * SDL_RenderSetLogicalSize restores neither the viewport nor the
   * integer-scale flag, and Dear ImGui's own backup/restore records the
   * viewport while logical presentation is off — in pixels. Re-enabling
   * logical presentation reinterprets that rectangle in emulated pixels, so
   * skipping any of this leaves the game's frame drawn at the wrong scale
   * from the next present onward, menu open or not. tests/menu_test.c holds
   * the whole set. */
  int logical_width = 0;
  int logical_height = 0;
  SDL_RenderGetLogicalSize(renderer, &logical_width, &logical_height);
  const SDL_bool integer_scale = SDL_RenderGetIntegerScale(renderer);
  SDL_Rect viewport;
  SDL_RenderGetViewport(renderer, &viewport);
  const SDL_bool clip_enabled = SDL_RenderIsClipEnabled(renderer);
  SDL_Rect clip;
  SDL_RenderGetClipRect(renderer, &clip);

  SDL_RenderSetLogicalSize(renderer, 0, 0);
  /* The SDL_Renderer backend hands vertices to SDL_RenderGeometryRaw
   * unscaled and only projects its scissor rectangles, so on a Retina display
   * the caller owns the point-to-pixel scale — without this the menu draws at
   * half size into a quarter of the window while its clipping is computed for
   * the full one. Same call, same reason, as Dear ImGui's own SDL2 example. */
  const ImVec2 framebuffer_scale = ImGui::GetDrawData()->FramebufferScale;
  SDL_RenderSetScale(renderer, framebuffer_scale.x, framebuffer_scale.y);
  ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);
  SDL_RenderSetScale(renderer, 1.0f, 1.0f);

  SDL_RenderSetLogicalSize(renderer, logical_width, logical_height);
  SDL_RenderSetIntegerScale(renderer, integer_scale);
  /* Restore "no viewport of its own" as exactly that. Handing SDL the
   * equivalent explicit rectangle looks identical until presentation changes
   * again, at which point the rectangle is reinterpreted in the new
   * coordinate space and starts cropping. */
  SDL_RenderSetViewport(renderer, NULL);
  SDL_Rect full_viewport;
  SDL_RenderGetViewport(renderer, &full_viewport);
  if (SDL_memcmp(&full_viewport, &viewport, sizeof(full_viewport)) != 0)
    SDL_RenderSetViewport(renderer, &viewport);
  SDL_RenderSetClipRect(renderer, clip_enabled ? &clip : NULL);
}

bool PhalanxUiMousePosition(float *x, float *y) {
  if (!s_ready || !ImGui::GetCurrentContext())
    return false;
  const ImVec2 position = ImGui::GetIO().MousePos;
  if (position.x <= -FLT_MAX || position.y <= -FLT_MAX)
    return false;
  if (x)
    *x = position.x;
  if (y)
    *y = position.y;
  return true;
}

void PhalanxUiSetStatus(PhalanxUiState *state, const char *format, ...) {
  if (!state)
    return;
  va_list args;
  va_start(args, format);
  vsnprintf(state->status, sizeof(state->status), format, args);
  va_end(args);
}
