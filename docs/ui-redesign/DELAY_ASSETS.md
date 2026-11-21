# DELAY pedal: bitmap skin integration

The DELAY pedal is the first SHOBEATS FX pedal drawn from high-resolution artwork. The
other seven pedals keep their procedural drawing. This document records what was
delivered, how it was checked, and how each file is used. The asset specification that
the artwork was made from is the Claude Doc "SHOBEATS FX — DELAY pedal asset
specification" (Markdown copy in `SHOBEATS_DELAY_TEMPLATES.zip`).

## Files

20 files in `assets/images/delay/`, copied byte for byte from `SHOBEATS_DELAY_ASSETS_v2.zip`
(manifest: `docs/ui-redesign/delay-assets-manifest.json`). They are embedded in the plugin
by the `ShobeatsAssets` binary-data target (the CMake glob includes `assets/images/delay/*.png`
and `*.jpg`) and found by their original file names.

| File | @2x size | Mode | Used by |
|---|---|---|---|
| `delay_adv_enclosure@2x.png` | 308 x 544 | RGBA | `pedal::paint` (Advanced pedal) |
| `delay_adv_window_glass@2x.png` | 212 x 92 | RGBA, max alpha 23 | `pedal::paint` (Advanced pedal) |
| `delay_adv_selected_glow@2x.png` | 308 x 544 | RGBA, white | `pedal::paint`, tinted with the DELAY accent, 150 ms fade |
| `delay_main_enclosure@2x.png` | 208 x 448 | RGBA | `pedal::paint` (main tile) |
| `delay_main_window_glass@2x.png` | 160 x 80 | RGBA, max alpha 23 | `pedal::paint` (main tile) |
| `pedal_knob_shadow@2x.png` | 88 x 88 | RGBA | `pedal::paint` (under the knobs) |
| `pedal_knob_body@2x.png` | 88 x 88 | RGBA, rotates | `ShobeatsLookAndFeel::drawSkinnedPedalKnob`; main tile: `pedal::paint` |
| `pedal_knob_gloss@2x.png` | 88 x 88 | RGBA | same |
| `footswitch_up@2x.png`, `footswitch_down@2x.png` | 88 x 88 | RGBA | `Footswitch::paintButton`; main tile: `pedal::paint` |
| `led_amber_off@2x.png`, `led_amber_on@2x.png` | 48 x 48 | RGBA | status LED (80 ms cross-fade) and tap-tempo LED (blink) |
| `panel_background@2x.jpg` | 2480 x 816 | RGB (JPEG) | `DetailPanel::paintPanel` (cached backdrop) |
| `display_frame@2x.png`, `display_glass@2x.png` | 700 x 688 | RGBA | `EffectDisplay::paint` (below / above the live graph) |
| `panel_knob_shadow`, `_skirt`, `_cap`, `_pointer@2x.png` | 144 x 144 | RGBA | `ShobeatsLookAndFeel::drawSkinnedPanelKnob` |
| `art_delay@2x.png` | 600 x 324 | RGBA | `EffectArt::paint` (panel corner) |

## Validation of the delivered files

Checked with the automated test *DELAY skin: all 20 assets are bundled at their exact @2x
size* and by inspection over the six positioning templates.

- **Names and sizes:** all 20 present, every one at its exact @2x size. Rotating layers are square.
- **Transparency:** every PNG has an alpha channel and fully transparent corners. The JPEG
  panel background is opaque with chassis-black corners (#0B0B0C).
- **Glass overlays:** at most 23/255 alpha (window glass) and 20/255 (display glass), so the
  live echo rings and graph stay fully visible (spec: at least 80 % transparent).
- **Alignment:** the enclosures, windows, plates, LED bezels, footswitch collar, knob
  recesses, display frame and knob layers line up with the templates. The dark window glass
  of both enclosures matches the window rectangle horizontally within about 1 design pixel.
- **Colour profile:** none embedded.

Observations for the next revision of the artwork (none blocks use):

| File | Observation | Effect today |
|---|---|---|
| `panel_knob_shadow` | Reaches the canvas border: alpha 190 on the bottom edge, about 60 on the sides | Not visible: the cut lies on the dark groove ring. It is drawn below the value arc so it never dims it (see below) |
| `pedal_knob_shadow` | Alpha 99 on the bottom edge | Not visible at the current sizes |
| `panel_knob_pointer` | Pointer centred 1.5 px (@2x) right of the canvas centre | Up to 0.4 design px off-axis while turning; not visible |
| `delay_*_enclosure` | Body alpha 254 instead of 255 | 0.4 % see-through; not visible |
| `delay_main_enclosure` | Shadow touches the left and right canvas edges (alpha 31) | Within the 2 px tile margin; not visible |
| `art_delay` | Echo rings are part of the illustration; the art fills the lower 55 % of the canvas | Decorative corner as specified (#20). It follows ON by opacity only, as no `art_delay_glow` was supplied |

## How it is drawn

`source/ui/Skin.h/.cpp` loads the files. It follows the Direct2D lifetime rule from the FL
Studio crash fix:

- No static images and no JUCE `ImageCache`. Each editor's look-and-feel owns a
  `SharedResourcePointer<Skin>`, so the images are released when the last editor closes. The
  test *DELAY skin: images live only while an editor exists* checks this.
- Files are decoded on first use, on the message thread. Each layer keeps the @2x master and
  an exact 2:1 box-filtered @1x copy.
- Every draw picks the copy from the context's physical pixel scale. At the default 85 % size
  the pedal art is drawn from the @1x copy instead of down-sampling the master by 0.42, so
  fine detail does not shimmer.
- The skin is all-or-nothing: if any of the 20 files is missing, DELAY keeps its procedural
  drawing. `pedal::hasSkin (EffectId)` is true for DELAY only.

Only painting changed. Every slider, button, attachment, hit area and parameter ID is the same
as before; the test *DELAY skin: controls keep driving their parameters* covers the 10 skinned
sliders and the footswitch.

### Layer order as implemented (bottom to top)

**Advanced pedal** (`Pedal::paint` → `pedal::paint`, then child components):

1. `delay_adv_selected_glow`, tinted with the DELAY accent, fading in and out over 150 ms.
2. Light spill under the pedal when ON (code).
3. `delay_adv_enclosure`.
4. Light inside the window when ON, and the echo rings: one per audible repeat, from Feedback (code).
5. `delay_adv_window_glass`.
6. Status LED: `led_amber_off`, then `led_amber_on` cross-faded over 80 ms.
7. `pedal_knob_shadow` under both knobs.
8. Tap-tempo LED: `led_amber_off`, then `led_amber_on` blinking at the delay time (120 BPM reference when synced).
9. Knob components (real sliders): `pedal_knob_body` rotated, then `pedal_knob_gloss`.
10. Footswitch component (real button): `footswitch_up` / `footswitch_down`, 6 % brighter on hover.
11. Drag shade while the pedal is moved (code).

**Main tile** (`EffectTile::paintButton` → `pedal::paint`): light spill, `delay_main_enclosure`
(1 px lower while pressed), window light and echo rings, `delay_main_window_glass`, status LED,
both display-only knobs (shadow, rotated body, gloss), footswitch (pressed frame while the tile
is held), then the slot badge (code).

**Settings panel:** `panel_background` with the accent light seams, texts and title in code
(cached backdrop); `art_delay`; display = `display_frame`, live graph and caption pill in code,
`display_glass`; keys in code; each knob = scale ticks and groove (code),
`panel_knob_shadow`, value arc (code), `panel_knob_skirt` rotated, `panel_knob_cap`,
`panel_knob_pointer` rotated, value window and text (code).

### Differences from the specification

- **Panel knob shadow below the value arc.** The specification put the shadow above the arc.
  The delivered shadow reaches the arc's radius with up to 75 % opacity and would dim the lit
  arc, so it is drawn just before the arc.
- **Pedal knob shadow painted by the pedal.** The shadow canvas (44 px) is larger than the knob
  component (37 px). Drawing it from the component would clip it, and enlarging the component
  would change the hit area. The pedal paints it underneath instead.

Rotation: every rotating layer is drawn as supplied (pointing to 12 o'clock) and turned
clockwise by the slider's angle, -135 degrees at the minimum and +135 degrees at the maximum.
The test *DELAY skin: knob layers turn with the value* measures the rendered pointer at 0, 50
and 100 %.

## Checks

- `ctest` / `shobeats_plugin_tests "[skin]"`: assets, rotation, lifetime, controls, and
  rendering at 85, 100, 125, 150 and 200 % (Main and Advanced, ON and OFF).
- `SHOBEATS_SNAPSHOT_DELAY=<prefix> shobeats_plugin_tests "DELAY skin: Main and Advanced render at every editor size"`
  writes those renderings as PNG files (needs a display, for example `xvfb-run`).
- The existing render-cost, scaling, lifecycle and pluginval checks run unchanged in CI.

## CI result (commit 6e39cb8)

- Windows build run 44923172419: Release build, 114/114 tests (including the five
  `DELAY skin` tests), lifecycle stress host (exit 0), pluginval strictness 10, two
  passes (exit 0). VST3 artifact `ShobeatsFX-VST3-Windows-x64-Release` (id 14715014193).
- Linux AddressSanitizer run 44923172460: success.
- Not tested inside FL Studio: pluginval and the lifecycle host are not a DAW.
