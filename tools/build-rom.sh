#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BUILD="$ROOT/build-decomp"
ASAR_BUILD="$BUILD/asar"
ASAR="$ASAR_BUILD/asar/bin/asar"

if [ ! -x "$ASAR" ]; then
  cmake -S "$ROOT/third_party/asar/src" -B "$ASAR_BUILD" \
    -DCMAKE_BUILD_TYPE=Release
  cmake --build "$ASAR_BUILD" --target asar-standalone --parallel
fi

go run ./tools/phalanx-decomp build \
  --asar "$ASAR" \
  --work "$BUILD"

ORACLE="$ROOT/build-macos/phalanx_oracle"
ROM="$BUILD/Phalanx (USA).sfc"
if [ -x "$ORACLE" ]; then
  "$ORACLE" "$ROM"
fi

echo "Built and verified $ROM"
