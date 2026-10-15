#!/bin/bash
# Boots a COPY of the installed Workbench hard disk (workbench/hd/system.hdf) headless in FS-UAE (Docker) with the RTG
# card, runs a command from User-Startup and prints what it wrote to Work:result.txt. A screenshot lands in out/shot.png.
# defaults = the workbench.fs-uae setup (A4000/040, uaegfx-z3 16 MB). usage: [MODEL=A3000 ROMFILE=kicka3000.rom] [GFX=..] [GFXMEM=..] tools/emusys.sh "<command>" [seconds] [wait-for-file-text]
set -euo pipefail
cd "$(dirname "$0")/.."
CMD="${1:?command}"; TMO="${2:-60}"
ROM="$HOME/dev/AmigaProjekt/Amiga OS3/ROM/${ROMFILE:-kicka4000.rom}"
mkdir -p out/sys out/syswork
[ -f out/sys/clean.hdf ] || cp -c "$HOME/dev/AmigaProjekt/workbench/hd/system.hdf" out/sys/clean.hdf
cp -c out/sys/clean.hdf out/sys/system.hdf
rm -rf out/syswork; mkdir -p out/syswork; cp -R "${SHAREDIR:-$HOME/dev/AmigaProjekt/workbench/share}/." out/syswork/
rm -f out/syswork/result.txt* out/syswork/Micropolis/micropolis.log* out/syswork/Micropolis/Micropolis.prefs
[ -n "${PREFS:-}" ] && printf "$PREFS" > out/syswork/Micropolis/Micropolis.prefs   # PREFS="ASK=0\nMODE=50051000\nSIZE=1024x768\nDEPTH=8\n"
cp "$ROM" out/kick.rom
printf '%s\n' "$CMD" > out/syswork/cmd.script
printf 'FailAt 21\nAssign Picasso96: "Work:Picasso96"\nExecute Work:cmd.script >Work:result.txt\necho DONE >>Work:result.txt\n' > out/sys/us.txt
docker run --rm -v "$PWD/out/sys":/s -e USE_SM="${USE_SM:-}" sacredbanana/amiga-compiler:m68k-amigaos sh -c 'xdftool /s/system.hdf delete s/user-startup 2>/dev/null; xdftool /s/system.hdf write /s/us.txt s/user-startup; [ -f /s/sm.prefs ] && [ -n "$USE_SM" ] && { xdftool /s/system.hdf delete prefs/env-archive/sys/screenmode.prefs; xdftool /s/system.hdf write /s/sm.prefs prefs/env-archive/sys/screenmode.prefs; }; true' >/dev/null
docker run --rm -i -v "$PWD":/project -w /project -e HOME=/tmp -e TMO="$TMO" -e M="${MODEL:-A4000/040}" -e G="${GFX:-uaegfx-z3}" -e GM="${GFXMEM:-16384}" -e X="${EXTRA:-}" -e SHOTS="${SHOTS:-}" -e SOUND="${SOUND:-}" -e WW="${WW:-1280}" -e WH="${WH:-800}" dnd-amiga-emutest bash -s <<'EOS'
Xvfb :99 -screen 0 1600x1200x24 >/dev/null 2>&1 &
export DISPLAY=:99; sleep 1
if [ -n "$SOUND" ]; then printf '[general]\ndrivers = wave\n[wave]\nfile = /project/out/audio.wav\n' > /tmp/alsoft.conf; export ALSOFT_CONF=/tmp/alsoft.conf; fi
fs-uae --amiga_model="$M" --kickstart_file=/project/out/kick.rom --hard_drive_0=/project/out/sys/system.hdf \
  --hard_drive_1=/project/out/syswork --hard_drive_1_label=Work --graphics_card="$G" --graphics_card_memory="$GM" \
  --zorro_iii_memory=65536 --fullscreen=0 --window_width=${WW:-1280} --window_height=${WH:-800} --floppy_drive_speed=0 $X \
  > /project/out/fs-uae.log 2>&1 &
n=0
for _ in $(seq 1 $TMO); do
  grep -q DONE /project/out/syswork/result.txt 2>/dev/null && break
  sleep 1; n=$((n+1))
  for t in ${SHOTS:-}; do [ "$n" = "$t" ] && import -window root /project/out/shot_$t.png 2>/dev/null; done
done
sleep 1
import -window root /project/out/shot.png 2>/dev/null || true
EOS
cat out/syswork/result.txt 2>/dev/null || echo "(no result.txt)"
