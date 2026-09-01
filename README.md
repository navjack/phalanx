# Phalanx (Recompiled) for macOS

Personal-use Apple Silicon macOS runner for the USA release of **Phalanx**,
built on SNESRecomp's whole-program LLE/AOT pipeline.

The repository and app bundle do not contain a ROM. At runtime the app accepts
only the clean, unheadered 1 MiB USA dump with SHA-256:

`0663330bc061f4b768fa1806610878ef6e6cf546f36041ae087c8e55703693b8`

## Build

Place the ROM at `ROMS/Phalanx (USA).sfc`, then run:

```sh
bash tools/build-macos.sh
```

Open `Phalanx.app`. If the development ROM is not present, the app asks you to
choose it. The ROM is loaded externally and is never copied into the bundle.

## Controls

The app creates `~/Library/Application Support/Phalanx Recompiled/keybinds.ini`
on first launch. On macOS, controllers are read directly through Apple's
GameController framework; SDL is not involved in controller discovery or
mapping. The physical D-pad and left stick both drive the SNES D-pad. Face
buttons are mapped by diamond position, Menu is Start, and Options is Select.

- Arrow keys: D-pad
- `Z` / `X`: SNES B / A
- `A` / `S`: SNES Y / X
- `C` / `V`: SNES L / R
- Return: Start
- Right Shift: Select

- `F1`: open and close the menu
- Command-P: pause
- Command-R: reset
- Option-Return or Command-F: fullscreen
- Escape: close the menu, or quit when it is closed

## Menu and save states

`F1` opens a Dear ImGui overlay with pause, mute, fullscreen, integer scaling,
reset, quit, and the ten save-state slots. The game keeps running underneath
it, and the menu takes over keyboard and controller input while it is open, so
a press never reaches both. It can be navigated with the mouse, the keyboard,
or the controller, though only `F1` opens it.

Save states live in `~/Library/Application Support/Phalanx Recompiled/saves`
and never go inside the app bundle. Slots also have hotkeys:

- Shift-`F1` to Shift-`F10`: save slots 1 to 10
- `F2` to `F10`: load slots 2 to 10
- Command-S / Command-L: save or load the slot selected in the menu
  (`F1` is the menu toggle, so slot 1 loads from here or from the menu)

On a Mac keyboard the function keys need `Fn` held unless *Use F1, F2, etc. as
standard function keys* is turned on in System Settings; the Command shortcuts
never do.

A state carries more than SNESRecomp's device snapshot. Because this build
executes the whole program on the LLE tier, the live 65816 register file is
the runtime's `CpuState` and the frame scheduler's continuation is Phalanx's
own; both are written into a chunk appended to the framework blob. A file that
does not carry that chunk intact — truncated, foreign, or older than the
format — is refused, and the machine is rolled back to the state it had before
the load was attempted rather than left half-restored.

## Correctness model

SNESRecomp still generates the statically proven native function set, but this
build keeps whole-program execution on its byte-accurate LLE tier by default.
Phalanx waits for later hardware events from inside interrupt work; allowing an
arbitrarily long AOT body to cross a frame deadline can inject interrupts in
the past and corrupt the live continuation. The LLE tier yields at exact
instruction boundaries, so cycle-driven NMI/IRQ, PPU, DMA, SPC700, and DSP
state remain coherent. AOT bounce can be re-enabled for development only after
deadline-safe differential validation.

The boot regression runs the clean ROM for 300 interpreter-driven frames and
requires the PPU to leave forced blank with an enabled screen. The input test
checks all twelve buttons against the SNES auto-joypad wire format. The
save-state test boots the ROM, snapshots it, runs on, reloads, and requires the
resumed run to reproduce the original frames exactly; it also requires damaged
slots to be refused without disturbing the running machine. The menu test
drives the overlay against SDL's dummy video driver, with no ROM and no
display. The macOS build script runs all four, bundles SDL, signs the app
locally, verifies the native arm64 executable and GameController linkage,
rejects accidental SDL controller imports, and rejects any bundle containing a
ROM or save states.
