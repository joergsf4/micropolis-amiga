#!/bin/bash
# Builds a release archive in LHA format (out/release/Micropolis-<version>.lha):
#   Micropolis.info          drawer icon
#   Micropolis/              Micropolis (program) + icon, ReadMe, COPYING, res/, cities/, Tools/
# usage: tools/release.sh [version]     (default: read from src/amiga/main.c $VER)
set -euo pipefail
cd "$(dirname "$0")/.."
VER="${1:-$(sed -n 's/#define VERSION  "\(.*\)"/\1/p' src/amiga/version.h)}"
SHARE="$HOME/dev/AmigaProjekt/workbench/share/Micropolis"
OUT=out/release; D=$OUT/stage
./build.sh >/dev/null                              # fresh build + data
rm -rf "$OUT/stage" "$OUT/icons" "$OUT/verify"; mkdir -p "$D/Micropolis/Tools" "$D/Micropolis/res" "$D/Micropolis/cities"
python3 tools/make_icons.py "$OUT/icons" >/dev/null
cp "$OUT/icons/Micropolis-drawer.info" "$D/Micropolis.info"
cp out/micropolis "$D/Micropolis/Micropolis"; cp "$OUT/icons/Micropolis.info" "$D/Micropolis/Micropolis.info"
cp release/ReadMe "$D/Micropolis/ReadMe"; cp "$OUT/icons/ReadMe.info" "$D/Micropolis/ReadMe.info"
cp COPYING "$D/Micropolis/COPYING"; cp release/Micropolis.guide "$D/Micropolis/Micropolis.guide"
cp out/rtgtest "$D/Micropolis/Tools/RTGTest"; cp out/listmodes "$D/Micropolis/Tools/ListModes"
cp -R "$SHARE"/res/. "$D/Micropolis/res/"
cp "$SHARE"/cities/*.cty "$D/Micropolis/cities/"
cp -R "$SHARE"/Catalogs "$D/Micropolis/Catalogs"
ARC="Micropolis-$VER.lha"
docker run --rm -v "$PWD/$D":/stage -v "$PWD/$OUT":/out -w /stage sacredbanana/amiga-compiler:m68k-amigaos \
    sh -c "rm -f /out/$ARC; lha a1o5 /out/$ARC Micropolis.info Micropolis >/dev/null 2>&1 && lha lv /out/$ARC | tail -45"
ls -la "$OUT/$ARC"
