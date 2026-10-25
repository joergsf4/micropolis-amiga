# Changelog

Older versions: see the release notes on GitHub (v0.1 to v0.8.1).

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
