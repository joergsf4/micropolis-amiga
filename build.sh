#!/bin/bash
# Baut das Amiga-Programm im Docker-Devkit aus ~/dev/dnd-amiga (Bebbo amiga-gcc).
# Usage: ./build.sh [make-Ziele]   (Ausgabe: out/ und ~/dev/AmigaProjekt/workbench/share/Micropolis)
set -euo pipefail
cd "$(dirname "$0")"
python3 tools/convert_assets.py upstream/images out/data
python3 tools/make_icons.py out/icons >/dev/null
IMAGE=sacredbanana/amiga-compiler:m68k-amigaos
TTY=""; [ -t 1 ] && TTY="-t"
REV=$(git rev-parse --short HEAD 2>/dev/null || echo nogit)
[ -n "$(git status --porcelain 2>/dev/null)" ] && REV="$REV+"          # '+' = built from uncommitted changes
mkdir -p out
printf '#define BUILD_REV "%s"\n#define BUILD_TIME "%s"\n' "$REV" "$(date '+%Y-%m-%d %H:%M')" > out/buildinfo.h
docker run --rm $TTY -v "$PWD":/project -v "$HOME/dev/AmigaProjekt/workbench/share":/share -w /project "$IMAGE" make "$@"
