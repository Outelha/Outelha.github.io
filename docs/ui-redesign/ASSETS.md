# SHOBEATS FX UI assets

## What is in the repository

| Asset | Location | Licence | Used for |
|---|---|---|---|
| Barlow Condensed SemiBold / Bold / SemiBold Italic / Bold Italic | `assets/fonts/` | SIL Open Font License 1.1 (`assets/fonts/OFL-Barlow.txt`) | labels, keys, values, titles |
| Barlow Medium / Medium Italic / SemiBold | `assets/fonts/` | SIL OFL 1.1 | descriptions |
| Procedural textures: noise, brushed, conic (spun aluminium), hammered, sparkle | generated in `source/ui/Textures.cpp` | original | chassis, knob caps, pedal finishes |

Fonts are embedded with `juce_add_binary_data` (`ShobeatsAssets`) so the plugin looks the
same on every Windows machine regardless of installed fonts.

Everything else (knobs, keys, pedals, reels, tubes, displays, panels, glows) is drawn in
code in `source/ui/Hardware.cpp` and `source/ui/PedalArt.cpp`. Vector drawing stays sharp at
100–200 % display scaling and lets every lit element follow real parameter values.

## Optional artwork (not yet produced)

The bottom-left corner of each Advanced panel in the references shows a photographic
illustration. These are decorative, non-interactive, and cannot be reproduced convincingly
with procedural drawing. The code loads them by name through `Textures::artwork()`; when a
file is absent a procedural fallback is drawn instead, so the plugin is complete without
them.

To add one: export a PNG with transparency, put it in `assets/images/` with the exact name
below, rebuild. No code change is needed.

| File name | Size (px, 2x) | Content (original SHOBEATS art only) |
|---|---|---|
| `art_saturation.png` | 700 x 360 | two glowing vacuum tubes on a dark chassis, warm orange light |
| `art_resampler.png` | 700 x 360 | a chip on a dark circuit board, cyan light |
| `art_tape.png` | 700 x 360 | reel-to-reel transport detail (capstan, reel), bronze |
| `art_chorus.png` | 700 x 360 | flowing teal light ribbons |
| `art_tremolo.png` | 700 x 360 | flowing pink/magenta light ribbons |
| `art_reverse.png` | 700 x 360 | metal push button with a violet rewind symbol |
| `art_delay.png` | 700 x 360 | metal puck with golden echo rings |
| `art_reverb.png` | 700 x 360 | blue concentric ripples / subtle nebula |

Budget: each PNG at most ~400 KB; all eight together under 3 MB of plugin size. They are
decoded once with the first editor and released with the last one (same lifetime rule as
the textures).

Do not use artwork from commercial plugins, stock sites without a licence, or crops of the
reference images (those are concept renders, not production art).

## DELAY bitmap skin

The DELAY pedal (main tile, Advanced pedal and its settings panel) is drawn from 20
high-resolution files in `assets/images/delay/`. They were made from the DELAY asset
specification and its positioning templates. Loading, layer order, validation results
and the differences from the specification are in `DELAY_ASSETS.md`. The other seven
pedals are unchanged.
