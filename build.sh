#!/bin/bash
# Baut das Amiga-Programm im Docker-Devkit aus ~/dev/dnd-amiga (Bebbo amiga-gcc).
# Usage: ./build.sh [make-Ziele]   (Ausgabe: out/ und ~/dev/AmigaProjekt/workbench/share/Micropolis)
set -euo pipefail
cd "$(dirname "$0")"
python3 tools/convert_assets.py upstream/images out/data
python3 tools/make_icons.py out/icons >/dev/null
IMAGE=sacredbanana/amiga-compiler:m68k-amigaos
TTY=""; [ -t 1 ] && TTY="-t"
docker run --rm $TTY -v "$PWD":/project -v "$HOME/dev/AmigaProjekt/workbench/share":/share -w /project "$IMAGE" make "$@"
