# Changelog

Older versions: see the release notes on GitHub (v0.1 to v0.8.1).

## 0.8.10 (31 October 2026)

### Fixed
- **32 bit screens** (#43): on a card whose 32 bit mode could not be opened the program only said "cannot open the screen". It now tries
  other ways (the mode's own size, the other depth number 24/32, without the system pens) and writes the error code of every try to
  `micropolis.log` (option `debug`). Please report the log if it still fails.
- **Scrolling on chipset screens** (#45): after the blitter moved the picture the CPU wrote the new tiles into the bitplanes at once,
  which can corrupt colours (green flashes on roads) on real hardware. The program now waits for the blitter first.
  Not verified on real hardware.

### Changed
- **Autoscrolling at the top edge** (#44): the whole game status bar and the strip under it are now the scroll zone (as wished),
  not only the strip below the status bar. The dwell time of 0.3 s still keeps the way up to the menu bar quiet.

## 0.8.9 (27 October 2026)

### Added
- **Amiga chipset version, no graphics card needed** (#45): the screen mode requester at the start now also lists the chipset
  modes of at least 640x400 (e.g. *PAL:High Res Laced*, 640x512). On **AGA** machines the picture has all 256 colours; on
  **OCS/ECS** machines (16 colours) the game colours are reduced to a fixed palette of 16, built at the start from the colours
  the tile art uses most (grey, white, black and the dark blue of the panels are always in it). The panels and menus look the
  same as on RTG screens. Needs Kickstart 3.1 (graphics.library 40). Without `cybergraphics.library` the program now starts, too.
  Tested in the emulator on an A1200 (AGA) and an A3000 (ECS); real hardware not yet.
- Two screen tests (`aga`, `ecs`) in `tools/test/screens.py`.

## 0.8.8 (27 October 2026)

### Changed
- **Menus look the same on every screen depth** (#43): on 16 and 32 bit screens the menus used the system default pens (black
  background, white text, "OS 1.3 look"). The screen now gets its own system pens (black, grey, white, blue), so the menus are
  grey with black text and a highlighted title, like on palette screens and like the Workbench.
- **The tool palette is redrawn less when the mouse moves over the icons** (#30): only the label above the icons (name and
  price) is redrawn, no longer the whole palette with all icons. In the emulator profile A3000/030 a frame in which the hover
  changes every time took 6.5 ms before and 3.5 ms now. Whether this cures the jumping mouse pointer on a real 68030 has to be
  tested.

### Other
- `tools/emusys.sh` can send mouse input to the emulator (`XDO=...`), so that open menus can be photographed in tests.
- Debug options `hoverwalk` (the hover moves to another icon every frame) and `undotest`.

## 0.8.7 (27 October 2026)

### New
- **Undo / Redo** (#41): *Project > Undo* (Amiga+U) and *Redo* (Amiga+Z) take back and repeat the last 5 building actions. A road,
  rail or wire drawn with one drag is one step. Undo gives back the money the action cost and puts back only the tiles that still
  show what the action built, so what the simulation did in the meantime (a zone that grew) is not touched. A new city or a
  loaded city empties the history.
- **The sound output button has arrows** (#40): `<` goes back, `>` goes on, so that it looks like a selector.

### Changed
- **Scrolling at the edge** (#42, #44): the mouse has to stay at the edge for a third of a second before the map scrolls, so passing
  over it on the way to the menu bar does nothing; the top zone is only the strip under the status bar, not the status bar itself.
  The longer the mouse stays, the faster it scrolls (1 tile per 4 loops, then per 2, then 2 tiles per loop).
- **AHI needs less processor time** (#40): the loop chunks are twice as long and are looked after every third frame.

## 0.8.6 (26 October 2026)

### New
- **Sound through AHI** (#40): the sound window has a button *Output* that chooses the system sound (sound.datatype, Paula) or an
  AHI unit (0 to 3); it is saved. With AHI the effects and the three ambient loops are mixed by AHI: no fighting over Paula's
  channels, and a sound card with an AHI driver can be used. The 8SVX samples are read by the game itself, a loop is played as a
  stream of short chunks that follow each other without a gap (its volume follows the map within a fraction of a second). If
  `ahi.device` or the unit cannot be opened, the game stays with the system sound. Tested in the emulator (AHI 4.18 with the
  Paula driver); a real sound card is still to be tested.

## 0.8.5 (26 October 2026)

### Changed
- **City animation and world animation are separate switches** (#39): *Options > City animation* is the movement in the city itself
  (cars, smoke: the original game's animation, which looked jerky when the waves slowed the picture down), *Options > World
  animation* the surf and the wind. Both are on by default; each can be saved and switched off on its own.
- **The surf costs a fraction of the processor time** (#39): each picture of the surf is made once and then copied (a tile takes
  one 16x16 copy instead of a loop over its water pixels), the steps are 190 ms long (380 on a slow machine). In the emulator
  profile A3000/030 the time to draw a frame with the animation on dropped from about 10 ms to 2 ms.
- **The wind is visible again, but calm:** a gust lightens up to 7 of 16 bright pixels (14 was far too restless, 5 was not seen),
  the strength is worked out per 4x4 block, which is much cheaper.

## 0.8.4 (25 October 2026)

### Fixed
- **Scrolling is no longer slowed down by the animation** (#39): the surf and the wind stand still while the view scrolls and go on
  a moment after it stopped; a slow machine (a step that takes long to draw) gets half the animation speed; the surf is drawn
  from a list of the pixels it can touch instead of all 256.
- **The surf could be drawn wrongly:** the animation clock overflowed the 32 bit numbers of the Amiga, its steps became negative
  and the wave pattern read from outside its table (restless, wrong shapes). The clock is now always positive.
- **Gusts of wind are calmer** (#39): fewer lighter pixels, a wider and slower band, a colour closer to the green.
- **Menus use the "new look"** (#33): they follow the screen's pens like the Workbench (grey, 3D) instead of the 1.3 style with the
  game's colour 1 as background, which turned the highlighted entry black with unreadable text on 256 colour screens.
- **Autoscrolling at the left edge** (#34): the very left pixel columns of the screen scroll as well, not only the strip next to
  the tool palette.

### Other
- The log (option `debug`) lists the pens of the screen (`pens:`) and times the `scroll=N` test.

## 0.8.3 (25 October 2026)

### New
- **Surf along the coasts:** waves roll towards the shore and slow down on the way, their fronts follow the shape of the coast,
  they arrive in groups that drift along it, and after they break the foam runs back to sea in streaks. Quiet blues close to the
  water colour, twelve steps of 145 ms. Switched on and off with Options > Animation.
- **Gusts of wind over woods:** a slow band of lighter green passes over the trees (softly dithered, only the trees inside it are
  redrawn). Also part of Options > Animation.
- **Sound options window** (Options > Sound options...): sound on/off, ambient sounds and spoken tool names, and sliders for the
  volume of the sound effects and of the ambient sounds (saved in the preferences). The three menu items it replaces are gone from
  the Options menu.
- **Key M** opens and closes the overview map.
- **PORTING.md:** what is from the upstream code, what was replaced and what is left out on purpose (#14).

### Fixed
- **Autoscrolling** now also works at the left and the top edge of the map; the edge zones are 8 pixels wide (#34).
- **Menu colours:** the highlighted menu title and item were drawn in black on palette screens and hid the text; they are now
  readable (#33).
- **Amiga+M and Amiga+N** belong to the system (screen to back, Workbench to front): the menu shortcuts for the overview map and
  for New City are removed, the map has the key M instead (#35).
- **Ambient sounds** that stopped abruptly: a loop that has ended is started again at once, at most two loops play at the same time
  so that sound effects and announcements keep a channel, loops are not started while an effect plays, and they keep playing when
  the game is paused.

### Other
- Guides, ReadMe and README describe the new features in English, German and Polish; all new texts are translated.
- The release archive contains only the 24 original cities, and the version line of the ReadMe follows the release.

### Not verified
- The ambient sound fix and the pointer behaviour (#30) on real hardware; the speed of the surf and wind animation on a 68030.

## 0.8.2 (17 October 2026)
- The menu "Disasters" opens again on a 640 pixel wide screen (Level and Language became sub menus of Options).
- Quit asks first when the city has changed; the tax rate starts at 7 %; hovering over a tool icon redraws only the label (mouse pointer jumping on slow machines); a loaded city's options and level show in the menu check marks.

## 0.8.1 (16 October 2026)
- The simulation core is built with `-O1`: the `-O2` build ran the original code wrongly (almost no traffic, residents counted too low, demand valves stuck at their limits). Tools to compare the Amiga core with the native build.
