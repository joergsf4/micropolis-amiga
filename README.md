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
| **Graphics** | RTG card with Picasso96 or CyberGraphX (`cybergraphics.library` 40+) and a 640x480 8-bit screen mode |
| **System files** | `mathieeedoubbas.library`, `mathieeedoubtrans.library` (in `LIBS:` of any Workbench), `gadtools.library`, `asl.library` |
| **CPU libraries** | the right `680x0.library` for your CPU (68030/68040/68060), as AmigaOS 3.2 asks for |

On an emulator: FS-UAE with `graphics_card = uaegfx-z3` (Zorro III) and Picasso96 installed.
There is no AGA/OCS version.

## Install

Unpack the archive anywhere, e.g. `lha x Micropolis.lha Work:`, then double-click the `Micropolis`
icon. From a shell:

```
cd Work:Micropolis
Micropolis                      ; opens cities/haight.cty
Micropolis cities/bruce.cty
```

The program looks for its data (`res/`, `cities/`) in the **current directory**, so start it from its own drawer.

## Controls

| Action | How |
|---|---|
| Choose a tool | click the button on the left (Query, Bulldozer, Roads, Rails, Wires, Parks, zones, power plants, ...) |
| Build | click or drag on the map; the white frame shows what will be built |
| Scroll | W A S D, mouse wheel, or move the mouse to the screen edge. With the overview map open: click or drag in it |
| Menus | right mouse button: Project, Speed, Options, Windows, Map, Disasters |
| Speed | `0` pause, `1` slow, `2` medium, `3` fast, `P` pause on/off |
| Budget | `B` or Windows > Budget (taxes, road/fire/police funding; Continue / Cancel) |
| Overview map | `M` on/off, `C` next overlay (power, traffic, pollution, crime, land value, ...) |
| Quit | Esc or Project > Quit |

The bars at the top right show what the city needs: **R**esidential, **C**ommercial, **I**ndustrial
(up = demand, down = too many). Events and warnings open a message window (Continue / Return / Space).

You can load and save cities (`.cty`, the original Micropolis format; `cities/` has the classic scenarios).

## Status

Works: simulation, building tools, map with animation and sprites, overview map with 14 overlays,
budget, city load/save/new random city, menus, disasters, event messages.
Missing: evaluation window and graphs, sound, scenario selection, earthquake shake, tooltypes.
See the [issues](../../issues).

## Troubleshooting

- *Nothing happens / the program ends at once:* start it from a shell to see the message. Typical causes:
  `cybergraphics.library` missing (Picasso96 not active), no 640x480x8 RTG mode, or `res/` not found.
- *Software failure when quitting:* please report the error number.
- The program writes `micropolis.log` into its drawer. That is a debug log; you can delete it.

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
