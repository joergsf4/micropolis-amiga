#!/bin/bash
# Runs a console program headlessly in FS-UAE (Docker, Kickstart 3.2 A1200, no Workbench) and prints
# what it wrote to result.txt. The program dir is out/testhd, booted as a hard-drive folder.
# usage: tools/emutest.sh "<command line, run from the program dir>" [timeout-seconds]
set -euo pipefail
cd "$(dirname "$0")/.."
ROM="$HOME/dev/AmigaProjekt/Amiga OS3/ROM/kicka4000.rom"
CMD="${1:?command}"; TMO="${2:-120}"
rm -rf out/testhd; mkdir -p out/testhd/S out/testhd/Libs
cp -R "$HOME/dev/AmigaProjekt/workbench/share/Micropolis/." out/testhd/
printf 'Assign ENV: SYS:ENV\nAssign C: SYS:C\nCD SYS:\n%s >result.txt\necho DONE >>result.txt\n' "$CMD" > out/testhd/S/Startup-Sequence
rm -f out/testhd/micropolis.log*; mkdir -p out/testhd/C out/testhd/ENV; cp out/libs/*.library out/testhd/Libs/; cp out/libs/Assign out/libs/MakeDir out/testhd/C/   # from Workbench3.2.adf (see README)
cp "$ROM" out/kick.rom
docker run --rm -i -v "$PWD":/project -w /project -e HOME=/tmp -e TMO="$TMO" -e EXTRA="${EXTRA:-}" dnd-amiga-emutest bash -s <<'EOS'
Xvfb :99 -screen 0 1024x800x24 >/dev/null 2>&1 &
export DISPLAY=:99; sleep 1
fs-uae --amiga_model=A4000/040 --kickstart_file=/project/out/kick.rom --hard_drive_0=/project/out/testhd \
  --fullscreen=0 --window_width=640 --window_height=480 --floppy_drive_speed=0 \
  --fast_memory=8192 $EXTRA > /project/out/fs-uae.log 2>&1 &
for _ in $(seq 1 $TMO); do
  grep -q DONE /project/out/testhd/result.txt 2>/dev/null && break; sleep 1
done
sleep 1
import -window root /project/out/shot.png 2>/dev/null || true
EOS
cat out/testhd/result.txt 2>/dev/null || echo "(no result.txt)"
