# Micropolis for AmigaOS 3

Micropolis (the open-source version of the original SimCity) for the Amiga with a native
Intuition front end and **RTG** graphics (Picasso96 / CyberGraphX). The simulation is the original
C code; Tcl/Tk and X11 were replaced by an Amiga user interface.

![status](https://img.shields.io/badge/status-early%20alpha-orange)

## What you need

| | |
|---|---|
| **OS** | AmigaOS 3.x (developed and tested on 3.2 only). Kickstart 3.2 recommended |
| **CPU** | 68020 or better (tested on an emulated 68040; a 68030/50 MHz is the performance target) |
| **RAM** | 2 MB Chip + 2 MB Fast or more (the program itself takes about 1.5 MB) |
| **Graphics** | RTG card with Picasso96 or CyberGraphX (`cybergraphics.library` 40+) and any screen mode of at least 640x480 (8, 15, 16, 24 or 32 bit; 8 bit is used when available) |
| **System files** | `mathieeedoubbas.library`, `mathieeedoubtrans.library` (in `LIBS:` of any Workbench), `gadtools.library`, `asl.library` |
| **Sound** | Paula via `audio.device` (stereo, the original sound effects at 11 kHz); needs about 300 KB Chip RAM while playing |
| **CPU libraries** | the right `680x0.library` for your CPU (68030/68040/68060), as AmigaOS 3.2 asks for |

On an emulator: FS-UAE with `graphics_card = uaegfx-z3` (Zorro III) and Picasso96 installed.
There is no AGA/OCS version.

## Install

Unpack the archive anywhere, e.g. `lha x Micropolis-0.2.lha Work:`, then double-click the `Micropolis`
icon. Nothing else has to be installed (besides the RTG system and the standard Workbench libraries above).
The program finds its data (`res/`, `cities/`) next to itself, so it can be started from anywhere:

```
Work:Micropolis/Micropolis                 ; opens cities/haight.cty
Work:Micropolis/Micropolis my.cty          ; opens a city file (path relative to the current directory)
```

ToolTypes (icon information): `CITY=cities/bruce.cty` city to open at start, `DEBUG` write `micropolis.log`
(also a command line argument). Startup problems (no RTG, no suitable screen mode, missing data) are shown in a requester.

## Controls

| Action | How |
|---|---|
| Choose a tool | click the button on the left (Query, Bulldozer, Roads, Rails, Wires, Parks, zones, power plants, ...) |
| Build | click or drag on the map; the white frame shows what will be built |
| Scroll | W A S D, mouse wheel, or move the mouse to the screen edge. With the overview map open: click or drag in it |
| Menus | right mouse button: Project, Speed, Options, Windows, Map, Disasters |
| Speed | `0` pause, `1` slow, `2` medium, `3` fast, `P` pause on/off |
| Budget | `B` or Windows > Budget (taxes, road/fire/police funding; Continue / Cancel) |
| Evaluation | `E` or Windows > Evaluation (public opinion, worst problems, score) |
| Graphs | `G` or Windows > Graphs (residential, commercial, industrial, money, crime, pollution; 10 or 120 years) |
| Overview map | `M` on/off, `C` next overlay (power, traffic, pollution, crime, land value, ...) |
| Quit | Esc or Project > Quit |
| Version | Project > About Micropolis (version, build, CPU, screen mode, free memory) |

The bars at the top right show what the city needs: **R**esidential, **C**ommercial, **I**ndustrial
(up = demand, down = too many). Events and warnings open a message window (Continue / Return / Space).

You can load and save cities (`.cty`, the original Micropolis format; `cities/` has the classic scenarios).

## Status

Works: simulation, building tools, map with animation and sprites, overview map with 14 overlays,
budget, evaluation window, history graphs, sound effects, city load/save/new random city, menus, disasters, event messages.
Missing: scenario selection, earthquake shake, looping sounds (bulldozer).
See the [issues](../../issues).

## Troubleshooting

- *A requester says no RTG / no screen mode:* Picasso96 or CyberGraphX is not running, or the card offers no mode of at least 640x480.
- *"Cannot find the program data":* keep the `Micropolis` program together with its `res` and `cities` drawers.
- *Software failure when quitting:* an early crash on quit seems fixed in 0.2; if you see it, please report the error number.
- `micropolis.log` is only written with the DEBUG option; attach it to bug reports.

## Building from source

Needs Docker (uses the `sacredbanana/amiga-compiler` image: Bebbo's amiga-gcc).

```
tools/fetch_upstream.sh      # clones the original sources (needed for graphics and message texts)
./build.sh                   # -> out/ and ../AmigaProjekt/workbench/share/Micropolis (adjust the path in build.sh)
tools/release.sh             # -> out/release/Micropolis-<version>.lha
```

`src/core` is the original simulation (copied from upstream `src/sim`, Tcl/Tk/X11 removed, changes marked
`AMIGA`), `src/amiga` is the new front end. `tools/emusys.sh` boots an installed Workbench disk image
headless in FS-UAE to test the program with RTG.

## License

Micropolis is free software under the **GNU GPL v3** (see `COPYING`), with the additional terms of the
original release: the name "SimCity" is a trademark of Electronic Arts and is not used or licensed here.
The graphics, sounds and texts come from the original Micropolis distribution
([osgcc/simcity](https://github.com/osgcc/simcity)) and are © Electronic Arts.
Original game design by Will Wright, Unix/X11 version and open-sourcing by Don Hopkins.
Amiga port by this repository's authors. No Kickstart or Workbench files are included.
