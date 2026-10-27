#!/bin/bash
# Builds the core natively together with src/amiga/undo.c and runs the undo test (build, undo, redo, history depth, tiles that changed since).
set -euo pipefail
cd "$(dirname "$0")/../.."
OUT=out/native/u; mkdir -p $OUT
CFLAGS="-DOSF1 -DIS_INTEL -std=gnu89 -w -Wno-return-mismatch -Wno-implicit-function-declaration -Wno-implicit-int -Wno-int-conversion -Wno-incompatible-pointer-types -Wno-error -fno-strict-aliasing -Isrc/core -Isrc/amiga -O1 -fsingle-precision-constant -ffp-contract=off"
for f in src/core/*.c src/amiga/ui_core.c src/amiga/undo.c tools/test/undo_test.c; do cc $CFLAGS -c "$f" -o "$OUT/$(basename "$f" .c).o"; done
cc -o out/native/undo_test $OUT/*.o -lm
mkdir -p out/native/run; ln -sfn "$PWD/upstream/res" out/native/run/res
( cd out/native/run && ../undo_test "$PWD/../../../upstream/cities/wetcity.cty" )
