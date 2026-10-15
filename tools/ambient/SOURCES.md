# Sources of the ambient sounds

All recordings are CC0 (public domain dedication). The loops in `assets/ambient/` are cut from them by `make_ambient.py`
(steadiest window, crossfaded loop, 8 bit mono 11025 Hz). The downloaded originals (`src/`, about 14 MB) are not part of the repository.

| Loop | Source | Author | Licence | Download |
|---|---|---|---|---|
| `amb_sea.8svx` | "Sea and river wave sounds" (VistulaShort.mp3, waves on the river Vistula) | RandomMind | CC0 | https://opengameart.org/content/sea-and-river-wave-sounds , https://opengameart.org/sites/default/files/VistulaShort_0.mp3 |
| `amb_city.8svx` | "High traffic road sounds" (gatve Varniu.ogg) | IgnasD | CC0 | https://opengameart.org/content/high-traffic-road-sounds , https://opengameart.org/sites/default/files/gatve%20Varniu_2.ogg |
| `amb_nature.8svx` | "Birds and Wind - Ambient, Birds, Wind and Synth" (Birds and Wind - Ambient.ogg) | Spring Spring (bird sounds public domain by isaiah658, syncopika, pauliuw) | CC0 | https://opengameart.org/content/birds-and-wind-ambient-birds-wind-and-synth , https://opengameart.org/sites/default/files/Birds%20and%20Wind%20-%20Ambient_1.ogg |

To rebuild: put the three files into `tools/ambient/src/` under the names `vistula_short.mp3`, `traffic.ogg`, `birds_wind.ogg`, then `python3 tools/ambient/make_ambient.py`.

Other candidates (CC0, but their server blocks scripted downloads, so they would have to be saved by hand in a browser):
bigsoundbank.com (Joseph Sardin): "Street and Roads", "City center #1/#2", "Highway", many more.
