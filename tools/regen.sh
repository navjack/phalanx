#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ROM="$ROOT/ROMS/Phalanx (USA).sfc"
EXPECTED=0663330bc061f4b768fa1806610878ef6e6cf546f36041ae087c8e55703693b8

if [ ! -f "$ROM" ]; then
  echo "Missing private ROM: $ROM" >&2
  exit 1
fi

ACTUAL=$(shasum -a 256 "$ROM" | awk '{print $1}')
if [ "$ACTUAL" != "$EXPECTED" ]; then
  echo "Unsupported ROM hash: $ACTUAL" >&2
  exit 1
fi

python3 "$ROOT/snesrecomp/tools/v2_sync_funcs_h.py" \
  --cfg-dir "$ROOT/recomp" \
  --out "$ROOT/recomp/funcs.h"

python3 "$ROOT/snesrecomp/tools/v2_emit.py" \
  --rom "$ROM" \
  --cfg-dir "$ROOT/recomp" \
  --out-dir "$ROOT/src/gen" \
  --source-root "$ROOT/src"

