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
| **Graphics** | RTG card with Picasso96 or CyberGraphX (`cybergraphics.library` 40+) and a screen mode between 640x480 and 800x600 (4:3 / 5:4 shape; 8, 15, 16, 24 or 32 bit; 8 bit is used when available) |
| **System files** | `mathieeedoubbas.library`, `mathieeedoubtrans.library` (in `LIBS:` of any Workbench), `gadtools.library`, `asl.library` |
| **Sound** | via the system `sound.datatype` (datatypes.library, OS 3.0+): the original sound effects as 8SVX samples; they are loaded when first played |
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

ToolTypes (icon information; remove the parentheses to switch one on): `CITY=cities/bruce.cty` city to open at start,
`SIZE=800x600` screen size (640x480 up to 800x600; the tile view fills the screen, so more of the city is visible),
`DEBUG` write `micropolis.log`.
Command line: `Micropolis [city] [SIZE=WxH] [ASK] [DEBUG]`.

**Screen mode:** by default Micropolis asks for the RTG screen mode at every start (a screen mode requester, then
"Always use this screen mode? - Always use it / Ask me next time"). With *Always use it* the mode is saved in
`ENVARC:Micropolis.prefs` and used without asking; the menu *Options > Ask for screen mode at start* switches the
question on again. `SIZE=` (ToolType or argument) skips the question and uses that size, `ASK` forces the question once. Startup problems (no RTG, no suitable screen mode, missing data) are shown in a requester.

## Looks and feels like an Amiga program

- The game runs on its own **screen with a title bar**: drag the title bar down to see the Workbench behind it, use the depth gadget
  (top right) to send the screen back. The title bar shows version, date, funds, population and speed.
- Messages, budget, city evaluation, history graphs, the query box and the overview map are **ordinary Amiga windows** on the game
  screen: drag them by their title bar, close them with the close gadget (the budget window's close gadget means *Continue*), bring
  them to the front with the depth gadget. They cannot be resized. Several can be open at once.
- The usual menus (right mouse button) work in every window.

## Controls

| Action | How |
|---|---|
| Query | the Query tool shows zone, density, land value, crime, pollution and growth in a box at the top right (click it or Esc to close) |
| Choose a tool | (hover an icon to see its name and cost at the top of the palette) click the button on the left (Query, Bulldozer, Roads, Rails, Wires, Parks, zones, power plants, ..., Chalk and Eraser for notes on the map) |
| Build | click or drag on the map; the white frame shows what will be built |
| Scroll | W A S D, mouse wheel, or move the mouse to the screen edge. With the overview map open: click or drag in it |
| Menus | right mouse button: Project, Speed, Options, Windows, Map, Disasters |
| Speed | `0` pause, `1` slow, `2` medium, `3` fast (or the Speed menu) |
| Budget | Windows > Budget (taxes, road/fire/police funding; Continue / Cancel) |
| Evaluation | Windows > Evaluation (public opinion, worst problems, score) |
| Graphs | Windows > Graphs (residential, commercial, industrial, money, crime, pollution; 10 or 120 years) |
| Overview map | M or Windows > Map opens/closes the overview map, Map > Next overlay cycles power, traffic, pollution, crime, land value, ...; click or drag in it to scroll |
| Scenarios | menu Scenarios: Dullsville 1900, San Francisco 1906, Hamburg 1944, Bern 1965, Tokyo 1957, Detroit 1972, Boston 2010, Rio de Janeiro 2047 (each with its story and goal) |
| Tool keys | as in the original: `b` bulldozer, `r` road, `p` park, `t` wire, `x` rail, `z` zone query; typed cheat words (e.g. `fund`) work too |
| Start | at launch a requester asks: New city, Load city, Scenario or the current (default) city; Options > Start menu at launch turns it off |
| Tool icons | uniform 32x32 icons by default; Options > Classic tool icons switches to the original pictures |
| Level | menu Level: Easy / Medium / Hard (start funds of a new city; saved in the preferences) |
| Ambient sounds | quiet loops that follow the visible part of the map: sea where it shows mostly water, traffic hum over a city, birds and wind over open land (Options > Sound options; CC0 recordings, see `tools/ambient/SOURCES.md`) |
| Options | Auto Goto, City animation, World animation, Messages, Message windows can be switched off in the Options menu; Options > Sound options: sound on/off, ambient sounds, "Speak tool names" (off by default), and volume sliders for effects and ambient sounds |
| Animation | Options > City animation: cars on the roads, smoke. Options > World animation: the surf along the coasts (waves that roll in, break and run back) and gusts of wind that pass over woods; separate so that a slow machine can switch it off |
| Help | `Help` key or Project > Help (AmigaGuide document `Micropolis.guide`) |
| Preferences | screen mode, options, speed, level and window positions are remembered (ENVARC:Micropolis.prefs) |
| Language | Options > ... Language: automatic (the system's locale), English, Deutsch or Polski (used from the next start; `lang=deutsch` on the command line, ToolType `LANGUAGE=`). Translations live in `l10n/*.ct` (FlexCat format, see `tools/l10n/l10n.py`); Polish is written without diacritics (the Amiga system fonts have no Latin-2 letters). **The German and Polish texts are machine translations and need review by native speakers.** |
| Quit | Esc or Project > Quit |
| Version | Project > About Micropolis (version, build, CPU, screen mode, free memory) |

The bars at the top right show what the city needs: **R**esidential, **C**ommercial, **I**ndustrial
(up = demand, down = too many). Events and warnings open a message window (Continue / Return / Space).

You can load and save cities (`.cty`, the original Micropolis format; `cities/` has the classic scenarios).

## Status

Works: simulation, building tools, map with animation and sprites, overview map with 14 overlays,
budget, evaluation window, history graphs, sound effects, scenarios, zone query box, earthquake shake, city load/save/new random city, menus, disasters, event messages.
Missing: looping sounds (bulldozer), terrain generator options for new cities.
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
`AMIGA`), `src/amiga` is the new front end. What is from upstream, what was replaced and what is left out on purpose is
listed in [PORTING.md](PORTING.md). `tools/emusys.sh` boots an installed Workbench disk image
headless in FS-UAE to test the program with RTG.

## License

Micropolis is free software under the **GNU GPL v3** (see `COPYING`), with the additional terms of the
original release: the name "SimCity" is a trademark of Electronic Arts and is not used or licensed here.
The graphics, sounds and texts come from the original Micropolis distribution
([osgcc/simcity](https://github.com/osgcc/simcity)) and are © Electronic Arts.
Original game design by Will Wright, Unix/X11 version and open-sourcing by Don Hopkins.
Amiga port by this repository's authors. No Kickstart or Workbench files are included.

## Tests

`tools/test/run_tests.sh [--quick]` (Docker must be running):

- **Notifications:** every `UI...` notification the simulation core sends must have a handler in `src/amiga/main.c` or a reason in `tools/test/ui_ignored.txt` (this would have caught the lost budget handler).
- **Screens:** the game is started in the emulator with debug arguments (`show=budget`, `lang=deutsch`, `speed=0`, `hover=N`, ...; `det` makes the picture deterministic) and writes its exact frame buffer (`dump=`); the test finds texts in it by rendering them with the game's own font. 10 cases, about 40 s each.
- **Core parity:** the simulation core is run for 12 months in the Amiga build and in a native build of the same code and the results are compared with tolerances (the original Micropolis code is only reliable with `-O1`, see the Makefile).
