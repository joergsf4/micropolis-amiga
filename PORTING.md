# Porting notes: what is from upstream, what was replaced, what is missing on purpose

The simulation in `src/core` is the original Micropolis C code (`upstream/src/sim`). Almost all of it is byte for byte the
same: of the 28 `.c` files only a handful differ, and every change is marked `AMIGA` in a comment. Everything the original
did with Tcl/Tk and X11 is replaced by the Amiga front end in `src/amiga`.

Compare with `diff upstream/src/sim/<file> src/core/<file>`.

## How the core talks to the front end

Upstream drives its user interface by calling `Eval("UIxxx args")` on a Tcl interpreter. `Eval()` is the single entry
point for that here (`src/amiga/ui_core.c`): it passes the string to the front end (`ui_eval()` in `main.c` or the headless
test), which handles the notification natively. `tools/test/check_handlers.py` checks that every `UI...` notification of
the core is either handled in `src/amiga` or listed in `tools/test/ui_ignored.txt` with a reason.

## Files of upstream `src/sim`

| Upstream file | Here | Note |
|---|---|---|
| `s_eval.c` | `src/core/s_eval.c` | one line: `VoteProblems()` wrapped around at `> PROBNUM`, so it read `ProblemTable[10]` and wrote `ProblemVotes[10]`, one past the end of both arrays; now `>= PROBNUM`. The result depended on what the linker placed behind the arrays. Simulation usually unchanged (the vote loop mostly ends after 600 tries either way), the problem votes are about 7 % higher |
| other `s_*.c` (simulation), `g_ani.c`, `rand.c`, `random.c`, `w_budget.c`, `w_con.c`, `w_eval.c`, `w_keys.c`, `w_resrc.c`, `w_sound.c`, `w_stubs.c` | `src/core`, unchanged | the game itself |
| `sim.c` | `src/core/sim.c` | `main()` and the X11/Tk option parsing removed (no `getopt`); the simulation loop stays |
| `w_graph.c` | `src/core/w_graph.c` | only the history data logic; the Tk graph widget is gone, `graph_ui.c` draws the graphs |
| `w_sprite.c` | `src/core/w_sprite.c` | the Tcl `sprite` command interface removed; movement code unchanged |
| `w_tool.c`, `w_update.c`, `w_util.c` | `src/core` | 1 to 6 changed lines: no X event time, float `SetDemand` (the ieee double library was unreliable), negative amounts printed as `-$1,281` |
| `headers/sim.h` | `src/core/sim.h` | the X11/Tcl/Tk/POSIX include block replaced by `amiga_compat.h`, a few prototypes |
| `headers/view.h`, `macros.h` | `src/core`, unchanged | the view structs keep their fields; their window-system types are dummies |
| `w_tk.c`, `w_x.c`, `w_inter.c`, `w_sim.c`, `w_map.c`, `w_editor.c`, `w_date.c`, `w_piem.c` | removed | Tcl/Tk commands, X11 drawing, pie menus. The Amiga front end does this: `main.c` (screen, windows, menus, tool palette, map drawing), `mapview.c` (overview map), `gfx.c` |
| `g_setup.c`, `g_map.c`, `g_bigmap.c`, `g_smmaps.c` | removed | X11 map drawing and map overlays: replaced by `main.c` (tile drawing) and `mapview.c` (overlays) |
| `w_print.c` | removed | printing |
| `w_net.c`, `w_cam.c`, `g_cam.c`, `headers/cam.h` | removed or stubbed | multiplayer and the X11 "cam" window: not supported, `cam.h` stays only for the types |
| `terrain/` | not needed | the map generator is in `s_gen.c`, which is part of the core |
| `res/*.tcl` (the Tk user interface) | not used | replaced by C. The game's texts and numbers are converted by `tools/convert_assets.py` (`res/stri.*`, `hexa.*`, `snro.*` are used as they are; the message window texts come from `micropolis.tcl` and become `notices.txt`) |

## What the front end replaces (`src/amiga/ui_core.c`)

These are functions that lived in the removed upstream files. They are small on purpose:

- `MakeNewSim()`, `Eval()`, `tkMustExit`, `UpdateDelayed`
- redraw hooks (`InvalidateMaps`, `InvalidateEditors`, `EventuallyRedrawView`, `RedrawMaps`, `Kick`, `UpdateFlush`, ...): they
  tell the front end that something changed; the front end decides what to draw (`ui_invalidate`)
- timer and event hooks (`DoTimeoutListen`, `StartMicropolisTimer`, ...): empty, the front end runs its own loop
- the earthquake (`DoEarthQuake`, `StopEarthquake`): sends the `UIEarthQuake` notification and shakes the view
- the **chalk / ink overlay** (`NewInk`, `FreeInk`, `StartInk`, `AddInk`, `EraseOverlay`): notes on the map, kept in world
  pixel coordinates like in the original. The tools themselves (`ChalkTool`, `EraserTool`) are the original ones in `w_tool.c`

## Left out on purpose

- **Multiplayer and the network** (`w_net.c`, shared views): the original could show several editor windows and map windows of
  one city on several X displays; here there is one game window and one overview map.
- **The "cam" window** (`w_cam.c`, `g_cam.c`).
- **Printing** (`w_print.c`).
- **The Tk help, the pie menus and the demo/"frob" windows**: AmigaGuide help instead; the dynamic zone filter of the map
  (`drawDynamic`) is not built, the original had it switched off, too ("Disabled dynamic filter for now", see issue #24).
- **Extra editor and map windows**: one editor view and one overview map.

## Other deviations of the build

- **`-O2 -fno-strict-aliasing -fno-peephole2` for the core:** with plain `-O2` the cross compiler (Bebbo amiga-gcc) miscompiled
  parts of the core (almost no traffic, residents counted too low, demand valves stuck at their limits), so it was built with
  `-O1` up to 0.8.17. Built file by file and compared in hash runs, two causes showed up: the m68k `peephole2` pass changes
  `s_traf.c` and `s_zone.c` (switched off), and `s_eval.c` read past an array (fixed, see above), so its result depended on the
  link order. These were found with GCC 6.5; with GCC 16 (the Docker image since November 2026) even plain `-O2` computes
  correctly, `peephole2` stays off for older images. The core is bit-identical to `-O1` and to the native build (4 cities,
  2 seeds, 5 game years); on a cycle-exact emulated 68030 the simulation is only 2-4 % faster. `-O3` was about 7 % faster
  there, but makes the core code 75 % bigger, so it is not used until it is measured on real 68030 and 68060 hardware.
  `tools/native_test.sh` builds the core natively and compares.
- **Reproducible test runs:** `headless <city> <steps> hash [seed]` (Amiga) and `headless_native` with the same arguments print
  state hashes of all simulation tables after every step (one step = 16 `sim_loop` calls = one `CityTime` tick, about a week).
  The core seeds its random numbers from the clock while a city loads (`RandomlySeedRand`), so both test programs are linked
  with `-Wl,--wrap=gettimeofday` and give the core a fixed time in hash runs (`tools/native/statehash.h`); the game itself is
  not affected. With the same seed the Amiga build and the native build give identical hashes (checked over 96 steps with
  Kobe and Senri on an emulated A3000); `tools/test/core_parity.py` compares them and names the first step and table that
  differ. Single runs of a city are chaotic: to judge a change of the game rules, compare several seeds.
- **Floats:** the core uses `float` and no FPU code is generated (`-m68030`/`-m68020`, software float).
- **Byte order:** the Amiga is big endian like the `.cty` files, so nothing is swapped. Only the native test build
  (`tools/native_test.sh`, defines `IS_INTEL`) byte-swaps when it reads a city.
- **Language:** all texts of the front end and the message texts go through `locale.library` catalogs
  (`l10n/`, `tools/l10n/l10n.py`); texts that come from the core are listed in `tools/l10n/extra.txt`.
