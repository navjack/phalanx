#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BUILD="$ROOT/build-macos"
STAGE=$(mktemp -d "${TMPDIR:-/tmp}/phalanx-build.XXXXXX")
trap 'rm -rf "$STAGE"' EXIT HUP INT TERM

git -C "$ROOT" submodule update --init snesrecomp third_party/imgui
"$ROOT/tools/regen.sh"

cmake -S "$ROOT" -B "$BUILD" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build "$BUILD" --target Phalanx phalanx_oracle phalanx_input_test \
  phalanx_menu_test phalanx_savestate_test
ctest --test-dir "$BUILD" --output-on-failure
"$BUILD/phalanx_oracle" "$ROOT/ROMS/Phalanx (USA).sfc"
"$BUILD/phalanx_savestate_test" "$ROOT/ROMS/Phalanx (USA).sfc"

# A developer may have run the CMake bundle directly before this project moved
# mutable settings to Application Support. Never propagate that stale state.
rm -f "$BUILD/Phalanx.app/Contents/MacOS/keybinds.ini"
rm -rf "$BUILD/Phalanx.app/Contents/MacOS/saves"
ditto "$BUILD/Phalanx.app" "$STAGE/Phalanx.app"
APP="$STAGE/Phalanx.app"
BINARY="$APP/Contents/MacOS/Phalanx"
FRAMEWORKS="$APP/Contents/Frameworks"
mkdir -p "$FRAMEWORKS"

SDL2_SOURCE=$(otool -L "$BINARY" | awk '/libSDL2/{print $1; exit}')
if [ -z "$SDL2_SOURCE" ] || [ ! -f "$SDL2_SOURCE" ]; then
  echo "Could not locate the SDL2 compatibility library." >&2
  exit 1
fi
SDL3_SOURCE=/opt/homebrew/opt/sdl3/lib/libSDL3.dylib
if [ ! -f "$SDL3_SOURCE" ]; then
  SDL3_SOURCE=$(brew --prefix sdl3)/lib/libSDL3.dylib
fi
if [ ! -f "$SDL3_SOURCE" ]; then
  echo "Could not locate SDL3, required by sdl2-compat." >&2
  exit 1
fi

ditto "$SDL2_SOURCE" "$FRAMEWORKS/libSDL2-2.0.0.dylib"
ditto "$SDL3_SOURCE" "$FRAMEWORKS/libSDL3.dylib"
chmod u+w "$BINARY" "$FRAMEWORKS/libSDL2-2.0.0.dylib" \
  "$FRAMEWORKS/libSDL3.dylib"
install_name_tool -change "$SDL2_SOURCE" \
  "@executable_path/../Frameworks/libSDL2-2.0.0.dylib" "$BINARY"

codesign --force --sign - "$FRAMEWORKS/libSDL3.dylib"
codesign --force --sign - "$FRAMEWORKS/libSDL2-2.0.0.dylib"
codesign --force --deep --sign - "$STAGE/Phalanx.app"
codesign --verify --deep --strict "$APP"
file "$BINARY" | grep -q 'arm64'
otool -L "$BINARY" | grep -q \
  '@executable_path/../Frameworks/libSDL2-2.0.0.dylib'
otool -L "$BINARY" | grep -q '/GameController.framework/'
if nm -u "$BINARY" | grep -Eq \
    'SDL_(GameController|IsGameController|NumJoysticks|Joystick)'; then
  echo "The macOS build still imports SDL controller APIs." >&2
  exit 1
fi
if find "$APP" -type f \( -iname '*.sfc' -o -iname '*.smc' \) | grep -q .; then
  echo "A ROM was copied into the app bundle; refusing to install it." >&2
  exit 1
fi
if find "$APP" -type f -name 'keybinds.ini' | grep -q .; then
  echo "Mutable keybinds.ini was copied into the app bundle." >&2
  exit 1
fi
if find "$APP" -name 'saves' | grep -q .; then
  echo "Save states were copied into the app bundle." >&2
  exit 1
fi

DESTINATION="$ROOT/Phalanx.app"
BACKUP="$STAGE/previous.app"
if [ -e "$DESTINATION" ]; then
  mv "$DESTINATION" "$BACKUP"
fi
if ! mv "$APP" "$DESTINATION"; then
  if [ -e "$BACKUP" ]; then
    mv "$BACKUP" "$DESTINATION"
  fi
  exit 1
fi
rm -rf "$BACKUP"

echo "Built and verified $DESTINATION"
