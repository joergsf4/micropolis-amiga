#!/bin/bash
# Builds the simulation core natively (no Amiga, no UI) and runs a city for N months: compares with the Amiga build.
# usage: tools/native_test.sh city.cty [months] [report-every-months] [extra cc flags]
set -euo pipefail
cd "$(dirname "$0")/.."
OUT=out/native; mkdir -p $OUT
CITY="${1:?city file}"; MONTHS="${2:-24}"; EVERY="${3:-6}"; shift 3 2>/dev/null || true
# OSF1: QUAD is int (32 bit like on the Amiga); IS_INTEL: the core byte-swaps the big-endian .cty files
CFLAGS="-DOSF1 -DIS_INTEL -std=gnu89 -w -Wno-return-mismatch -Wno-implicit-function-declaration -Wno-implicit-int -Wno-int-conversion -Wno-incompatible-pointer-types -Wno-error -fno-strict-aliasing -Isrc/core -Isrc/amiga -O1 $*"
for f in src/core/*.c src/amiga/ui_core.c tools/native/headless_native.c; do
    cc $CFLAGS -c "$f" -o "$OUT/$(basename "$f" .c).o"
done
cc -o $OUT/headless_native $OUT/*.o -lm
mkdir -p $OUT/run; ln -sfn "$PWD/upstream/res" $OUT/run/res
( cd $OUT/run && ../headless_native "$PWD/../../../upstream/cities/$CITY" "$MONTHS" "$EVERY" )
