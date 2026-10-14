# Micropolis for AmigaOS 3.2 (RTG)

Port of [Micropolis](https://github.com/osgcc/simcity) (Don Hopkins' open-source SimCity, Linux/X11 + Tcl/Tk)
to the Amiga: the simulation core is kept, the Tcl/Tk/X11 front end is replaced by a native
Intuition + cybergraphics (Picasso96 / RTG) one.

Target: 68040, Workbench 3.2, RTG screen (FS-UAE: A4000/040, `uaegfx-z3`; see `../AmigaProjekt/workbench`).

## Status
Works: the core simulates on the 68040, map view with animated tiles and sprites, scrolling, status bar,
speed control. Missing: mouse tools, menus, load/save, budget/evaluation windows, mini map, sound
(see the issues).

## Build
```
tools/fetch_upstream.sh   # clones the original sources (not part of this repo)
./build.sh                # Docker (sacredbanana/amiga-compiler), output -> ../AmigaProjekt/workbench/share/Micropolis
```
Run in the emulator: `cd Work:Micropolis`, `micropolis [cities/haight.cty]`. Keys: cursors scroll, 0-3 speed, P pause, Esc quit.

## Layout
- `src/core/` simulation core, copied from upstream `src/sim` and stripped of Tcl/Tk/X11 (Tcl hooks go through `Eval("UI...")`, see `src/amiga/ui_core.c`).
  Changes to the originals are marked `AMIGA` or are in `amiga_compat.h`. `tools/extract.py` pulls functions out of upstream files.
- `src/amiga/` frontend: `main.c` (screen, rendering, input), `ui_core.c` (glue), `stack.c` (own 256 KB stack: the shell's 4 KB is too small), `headless.c` (console test), `rtgtest.c`.
- `tools/convert_assets.py` XPM tiles/sprites -> indexed binaries; `tools/emutest.sh` runs console programs headless in FS-UAE (Docker).

## Notes
- Needs `mathieeedoubbas.library` (floating point) and `locale.library` from Workbench 3.2.
- License: code GPL-3 (see upstream COPYING, plus its additional terms); graphics/sounds are © Electronic Arts. Keep this repo private.
