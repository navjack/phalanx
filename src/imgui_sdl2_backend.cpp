/*
 * Dear ImGui's SDL2 platform backend, compiled through this file rather than
 * built straight from third_party/imgui/backends.
 *
 * Controllers on this build are read through Apple's GameController framework
 * (src/platform_macos.m); SDL discovers and maps nothing, and
 * tools/build-macos.sh asserts the shipped binary imports no SDL controller
 * API. The backend's optional gamepad-navigation path is the only code in the
 * menu stack that would break that assertion. It is already inert here — the
 * menu never sets ImGuiConfigFlags_NavEnableGamepad and feeds nav keys from
 * the GameController pad itself (src/ui.cpp) — so redirecting its six SDL
 * entry points to constants costs no behavior and keeps the build check
 * meaningful instead of relaxed.
 */
#include <SDL.h>

#define SDL_NumJoysticks()                     0
#define SDL_IsGameController(index)            SDL_FALSE
#define SDL_GameControllerOpen(index)          ((SDL_GameController *)NULL)
#define SDL_GameControllerClose(gamepad)       ((void)(gamepad))
#define SDL_GameControllerGetButton(pad, btn)  ((Uint8)0)
#define SDL_GameControllerGetAxis(pad, axis)   ((Sint16)0)

#include "backends/imgui_impl_sdl2.cpp"
