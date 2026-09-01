#include "machine.h"
#include "platform_macos.h"
#include "build_config.h"
#include "input.h"
#include "savestate.h"
#include "ui.h"

#include "common_cpu_infra.h"
#include "common_rtl.h"
#include "keybinds.h"
#include "sha256.h"
#include "snes/snes.h"
#include "spc_player.h"

#include <SDL.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
  kFrameWidth = 256,
  kFrameHeight = 224,
  kAudioRate = 48000,
};

static const uint8_t kUsaRomSha256[32] = {
  0x06, 0x63, 0x33, 0x0b, 0xc0, 0x61, 0xf4, 0xb7,
  0x68, 0xfa, 0x18, 0x06, 0x61, 0x08, 0x78, 0xef,
  0x6e, 0x6c, 0xf5, 0x46, 0xf3, 0x60, 0x41, 0xae,
  0x08, 0x7c, 0x8e, 0x55, 0x70, 0x36, 0x93, 0xb8,
};

bool g_new_ppu = true;
bool g_ws_active = false;
int g_ws_extra = 0;
SpcPlayer *g_spc_player = NULL;

static SDL_mutex *g_audio_mutex;

void Die(const char *error) {
  const char *message = error ? error : "Unknown fatal error";
  fprintf(stderr, "Phalanx: %s\n", message);
  MacShowError("Phalanx stopped", message);
  exit(EXIT_FAILURE);
}

void RtlApuLock(void) {
  if (g_audio_mutex)
    SDL_LockMutex(g_audio_mutex);
}

void RtlApuUnlock(void) {
  if (g_audio_mutex)
    SDL_UnlockMutex(g_audio_mutex);
}

static const RtlGameInfo kPhalanxGameInfo = {
  .title = "phalanx",
  .initialize = NULL,
  .run_frame = NULL,
  .draw_ppu_frame = NULL,
  /* src/savestate.c names slot files from this prefix too; keep them equal. */
  .save_name_prefix = "phalanx",
  .state_save_extra = PhalanxStateSaveExtra,
  .state_load_extra = PhalanxStateLoadExtra,
  .on_state_loaded = PhalanxStateLoaded,
};

static uint8_t *ReadFile(const char *path, size_t *size_out) {
  FILE *file = fopen(path, "rb");
  if (!file)
    return NULL;
  if (fseek(file, 0, SEEK_END) != 0) {
    fclose(file);
    return NULL;
  }
  const long length = ftell(file);
  if (length <= 0 || fseek(file, 0, SEEK_SET) != 0) {
    fclose(file);
    return NULL;
  }
  uint8_t *data = (uint8_t *)malloc((size_t)length);
  if (!data || fread(data, 1, (size_t)length, file) != (size_t)length) {
    free(data);
    fclose(file);
    return NULL;
  }
  fclose(file);
  *size_out = (size_t)length;
  return data;
}

static bool LoadVerifiedRom(const char *path, uint8_t **rom_out,
                            size_t *size_out, char *error, size_t error_size) {
  size_t size = 0;
  uint8_t *rom = ReadFile(path, &size);
  if (!rom) {
    snprintf(error, error_size, "Could not read:\n%s", path);
    return false;
  }

  uint8_t digest[32];
  sha256_compute(rom, size, digest);
  if (size != 1024u * 1024u ||
      memcmp(digest, kUsaRomSha256, sizeof(digest)) != 0) {
    free(rom);
    snprintf(error, error_size,
             "This is not the supported clean Phalanx (USA) ROM.\n\n"
             "Expected a 1 MiB unheadered dump with SHA-256:\n"
             "0663330bc061f4b768fa1806610878ef6e6cf546f36041ae087c8e55703693b8");
    return false;
  }

  *rom_out = rom;
  *size_out = size;
  return true;
}

static bool ResolveRom(int argc, char **argv, uint8_t **rom_out,
                       size_t *size_out, char *path_out, size_t path_size) {
  const char *candidates[3] = {
    argc > 1 ? argv[1] : NULL,
    getenv("PHALANX_ROM"),
    PHALANX_DEV_ROM_PATH,
  };
  char error[1024] = {0};

  for (unsigned i = 0; i < 3; i++) {
    if (!candidates[i] || !candidates[i][0])
      continue;
    if (LoadVerifiedRom(candidates[i], rom_out, size_out,
                        error, sizeof(error))) {
      snprintf(path_out, path_size, "%s", candidates[i]);
      return true;
    }
  }

  char selected[4096];
  if (MacChoosePhalanxRom(selected, sizeof(selected)) &&
      LoadVerifiedRom(selected, rom_out, size_out, error, sizeof(error))) {
    snprintf(path_out, path_size, "%s", selected);
    return true;
  }

  if (error[0])
    MacShowError("Phalanx ROM required", error);
  return false;
}

static uint16_t KeyboardEventMask(SDL_Scancode scancode) {
  const PlayerBinds *binds = &keybinds_get()->p1;
  uint16_t mask = 0;
  if (scancode == binds->r)      mask |= PHALANX_INPUT_R;
  if (scancode == binds->l)      mask |= PHALANX_INPUT_L;
  if (scancode == binds->x)      mask |= PHALANX_INPUT_X;
  if (scancode == binds->a)      mask |= PHALANX_INPUT_A;
  if (scancode == binds->right)  mask |= PHALANX_INPUT_RIGHT;
  if (scancode == binds->left)   mask |= PHALANX_INPUT_LEFT;
  if (scancode == binds->down)   mask |= PHALANX_INPUT_DOWN;
  if (scancode == binds->up)     mask |= PHALANX_INPUT_UP;
  if (scancode == binds->start)  mask |= PHALANX_INPUT_START;
  if (scancode == binds->select) mask |= PHALANX_INPUT_SELECT;
  if (scancode == binds->y)      mask |= PHALANX_INPUT_Y;
  if (scancode == binds->b)      mask |= PHALANX_INPUT_B;
  return mask;
}

static void QueueFrameAudio(SDL_AudioDeviceID device, double *sample_accum,
                            bool muted) {
  if (!device)
    return;
  *sample_accum += (double)kAudioRate / 60.0988;
  const int samples = (int)*sample_accum;
  *sample_accum -= samples;
  int16_t buffer[1024 * 2];
  /* Muting silences the output only. The APU still renders every sample so
   * the guest's audio state advances identically either way. */
  RtlRenderAudio(buffer, samples, 2);
  if (muted)
    memset(buffer, 0, (size_t)samples * 2 * sizeof(int16_t));
  if (SDL_GetQueuedAudioSize(device) > (unsigned)(kAudioRate * 4 / 5))
    SDL_ClearQueuedAudio(device);
  SDL_QueueAudio(device, buffer, (Uint32)(samples * 2 * sizeof(int16_t)));
}

/* F1..F10 select save slots 1..10. F1 alone is the menu toggle, so slot 1
 * loads from the menu or Command-L instead. */
static int SlotFromKey(SDL_Keycode key) {
  if (key >= SDLK_F1 && key <= SDLK_F10)
    return (int)(key - SDLK_F1);
  return -1;
}

typedef struct AppliedState {
  bool paused;
  bool fullscreen;
  bool integer_scale;
} AppliedState;

/* Push the menu's view of the host settings onto SDL. Hotkeys mutate the same
 * PhalanxUiState, so both paths land here. */
static void ApplyUiState(const PhalanxUiState *ui, AppliedState *applied,
                         SDL_Window *window, SDL_Renderer *renderer,
                         SDL_AudioDeviceID audio) {
  if (ui->paused != applied->paused) {
    applied->paused = ui->paused;
    if (audio)
      SDL_PauseAudioDevice(audio, ui->paused);
  }
  if (ui->fullscreen != applied->fullscreen) {
    applied->fullscreen = ui->fullscreen;
    SDL_SetWindowFullscreen(window,
        ui->fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
  }
  if (ui->integer_scale != applied->integer_scale) {
    applied->integer_scale = ui->integer_scale;
    SDL_RenderSetIntegerScale(renderer,
        ui->integer_scale ? SDL_TRUE : SDL_FALSE);
  }
}

static void PresentFrame(SDL_Renderer *renderer, SDL_Texture *texture,
                         PhalanxUiState *ui) {
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
  SDL_RenderClear(renderer);
  SDL_RenderCopy(renderer, texture, NULL, NULL);
  PhalanxUiRender(renderer, ui);
  SDL_RenderPresent(renderer);
}

int main(int argc, char **argv) {
  SDL_SetMainReady();

  uint8_t *rom = NULL;
  size_t rom_size = 0;
  char rom_path[4096];
  if (!ResolveRom(argc, argv, &rom, &rom_size, rom_path, sizeof(rom_path)))
    return 1;

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
    MacShowError("Phalanx could not start", SDL_GetError());
    free(rom);
    return 1;
  }
  g_audio_mutex = SDL_CreateMutex();
  if (!g_audio_mutex) {
    MacShowError("Phalanx could not start", "Could not create the audio lock.");
    SDL_Quit();
    free(rom);
    return 1;
  }

  RtlRegisterGame(&kPhalanxGameInfo);
  if (!SnesInit(rom, (int)rom_size)) {
    MacShowError("Phalanx could not start", "SNESRecomp rejected the ROM image.");
    SDL_DestroyMutex(g_audio_mutex);
    SDL_Quit();
    free(rom);
    return 1;
  }
  free(rom);

  PhalanxMachine machine;
  if (!PhalanxMachineInit(&machine, g_rom, rom_size)) {
    MacShowError("Phalanx could not start", "The ROM has invalid interrupt vectors.");
    return 1;
  }
  PhalanxSaveStateBind(&machine);

  char support_dir[4096];
  if (!MacPrepareSupportDirectory(support_dir, sizeof(support_dir))) {
    MacShowError("Phalanx could not start",
                 "Could not create the Application Support folder.");
    SDL_DestroyMutex(g_audio_mutex);
    SDL_Quit();
    return 1;
  }
  char config_anchor[4096];
  snprintf(config_anchor, sizeof(config_anchor), "%s/Phalanx", support_dir);
  keybinds_init(config_anchor);
  const bool states_available = PhalanxSaveStatePrepare(support_dir);
  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
  SDL_Window *window = SDL_CreateWindow(
      "Phalanx (Recompiled)", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
      896, 672, SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
  SDL_Renderer *renderer = window ? SDL_CreateRenderer(
      window, -1, SDL_RENDERER_ACCELERATED) : NULL;
  SDL_Texture *texture = renderer ? SDL_CreateTexture(
      renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
      kFrameWidth, kFrameHeight) : NULL;
  if (!window || !renderer || !texture) {
    MacShowError("Phalanx could not start", SDL_GetError());
    return 1;
  }
  SDL_RenderSetLogicalSize(renderer, 896, 672);
  SDL_RenderSetIntegerScale(renderer, SDL_FALSE);

  SDL_AudioSpec desired;
  SDL_zero(desired);
  desired.freq = kAudioRate;
  desired.format = AUDIO_S16SYS;
  desired.channels = 2;
  desired.samples = 1024;
  SDL_AudioSpec obtained;
  SDL_zero(obtained);
  SDL_AudioDeviceID audio = SDL_OpenAudioDevice(
      NULL, 0, &desired, &obtained, SDL_AUDIO_ALLOW_SAMPLES_CHANGE);
  if (audio && (obtained.freq != kAudioRate || obtained.format != AUDIO_S16SYS ||
                obtained.channels != 2)) {
    SDL_CloseAudioDevice(audio);
    audio = 0;
  }
  if (audio)
    SDL_PauseAudioDevice(audio, 0);

  MacGamepadInitialize();

  const bool menu_available = PhalanxUiInit(window, renderer);
  if (!menu_available)
    fprintf(stderr, "Phalanx: the menu could not be created; "
                    "hotkeys still work\n");

  uint32_t pixels[kFrameWidth * kFrameHeight];
  PhalanxUiState ui;
  memset(&ui, 0, sizeof(ui));
  ui.request_save = -1;
  ui.request_load = -1;
  AppliedState applied = { false, false, false };
  bool running = true;
  uint16_t keyboard_latch = 0;
  double audio_samples = 0.0;
  const uint64_t frequency = SDL_GetPerformanceFrequency();
  const double ticks_per_frame = (double)frequency / 60.0988;
  double next_frame = (double)SDL_GetPerformanceCounter();

  if (!states_available)
    PhalanxUiSetStatus(&ui, "Save states are unavailable: %s/saves "
                            "could not be created.", support_dir);

  while (running) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_QUIT) {
        running = false;
        continue;
      }

      const bool key_down = event.type == SDL_KEYDOWN && !event.key.repeat;
      const SDL_Keycode key = key_down ? event.key.keysym.sym : SDLK_UNKNOWN;
      const SDL_Keymod mod = key_down ? (SDL_Keymod)event.key.keysym.mod : 0;
      const int slot = key_down ? SlotFromKey(key) : -1;

      /* Menu toggle and the state hotkeys are handled ahead of Dear ImGui so
       * they keep working while the menu has keyboard focus. */
      if (key_down && slot >= 0 && (mod & KMOD_SHIFT)) {
        ui.request_save = slot;
        continue;
      }
      if (key_down && key == SDLK_F1) {
        PhalanxUiToggle();
        continue;
      }
      if (key_down && slot > 0) {
        ui.request_load = slot;
        continue;
      }
      if (key_down && key == SDLK_ESCAPE && PhalanxUiIsOpen()) {
        PhalanxUiToggle();
        continue;
      }
      if (key_down && (mod & KMOD_GUI) && (key == SDLK_s || key == SDLK_l)) {
        if (key == SDLK_s)
          ui.request_save = ui.slot;
        else
          ui.request_load = ui.slot;
        continue;
      }

      /* An open menu owns the rest of the input, exactly as it owns the
       * controller below. */
      if (PhalanxUiProcessEvent(&event))
        continue;

      if (key_down) {
        keyboard_latch |= KeyboardEventMask(event.key.keysym.scancode);
        if (key == SDLK_ESCAPE)
          running = false;
        else if (key == SDLK_p && (mod & KMOD_GUI))
          ui.paused = !ui.paused;
        else if (key == SDLK_r && (mod & KMOD_GUI))
          ui.request_reset = true;
        else if ((key == SDLK_RETURN && (mod & KMOD_ALT)) ||
                 (key == SDLK_f && (mod & KMOD_GUI)))
          ui.fullscreen = !ui.fullscreen;
      }
    }

    if (ui.request_reset) {
      ui.request_reset = false;
      PhalanxMachineReset(&machine);
      if (audio)
        SDL_ClearQueuedAudio(audio);
      PhalanxUiSetStatus(&ui, "Reset.");
    }
    if (ui.request_save >= 0) {
      const int requested = ui.request_save;
      ui.request_save = -1;
      ui.slot = requested;
      if (PhalanxSaveStateSave(requested))
        PhalanxUiSetStatus(&ui, "Saved slot %d.", requested + 1);
      else
        PhalanxUiSetStatus(&ui, "Could not save slot %d.", requested + 1);
    }
    if (ui.request_load >= 0) {
      const int requested = ui.request_load;
      ui.request_load = -1;
      ui.slot = requested;
      if (PhalanxSaveStateLoad(requested)) {
        if (audio)
          SDL_ClearQueuedAudio(audio);
        PhalanxUiSetStatus(&ui, "Loaded slot %d.", requested + 1);
      } else if (PhalanxSaveStateSlotUsed(requested, NULL, 0)) {
        PhalanxUiSetStatus(&ui, "Slot %d could not be loaded.", requested + 1);
      } else {
        PhalanxUiSetStatus(&ui, "Slot %d is empty.", requested + 1);
      }
    }
    if (ui.request_quit) {
      running = false;
      continue;
    }

    ApplyUiState(&ui, &applied, window, renderer, audio);
    ui.frames = (unsigned long)machine.frames;

    if (ui.paused) {
      /* Keep presenting so the menu stays live — unpausing from it is the
       * whole point of the checkbox. */
      PresentFrame(renderer, texture, &ui);
      SDL_Delay(8);
      next_frame = (double)SDL_GetPerformanceCounter();
      continue;
    }

    const uint8_t *keys = SDL_GetKeyboardState(NULL);
    uint16_t host_buttons = keybinds_read_player(keys, 1) |
                            MacGamepadReadButtons() | keyboard_latch;
    keyboard_latch = 0;
    host_buttons = PhalanxInputRemoveOpposites(host_buttons);
    if (PhalanxUiIsOpen()) {
      PhalanxUiSetGamepad(host_buttons);
      host_buttons = 0;
    }

    if (!PhalanxMachineRunFrame(&machine, host_buttons)) {
      MacShowError("Phalanx stopped",
                   "The recompiled machine hit an invalid execution path. "
                   "See the log for the failing guest address.");
      break;
    }

    PhalanxMachineRender(pixels, kFrameWidth * sizeof(uint32_t));
    SDL_UpdateTexture(texture, NULL, pixels, kFrameWidth * sizeof(uint32_t));
    PresentFrame(renderer, texture, &ui);
    QueueFrameAudio(audio, &audio_samples, ui.muted);

    next_frame += ticks_per_frame;
    double now = (double)SDL_GetPerformanceCounter();
    if (next_frame < now - ticks_per_frame * 4.0)
      next_frame = now;
    while (now + (double)frequency / 1000.0 < next_frame) {
      SDL_Delay(1);
      now = (double)SDL_GetPerformanceCounter();
    }
    while ((double)SDL_GetPerformanceCounter() < next_frame) {
    }
  }

  PhalanxUiShutdown();
  MacGamepadShutdown();
  if (audio)
    SDL_CloseAudioDevice(audio);
  SDL_DestroyTexture(texture);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_DestroyMutex(g_audio_mutex);
  SDL_Quit();
  return machine.failed ? 1 : 0;
}
