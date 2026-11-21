# Redesign progress log

Each milestone is one commit on `claude/hardware-ui-redesign`. Renders are produced with
`tools/ui_snapshot.sh` (headless Linux, Barlow fonts embedded, so text metrics match Windows).

## M1 — audit (22f54ac)
See `M1_AUDIT.md`.

## M2 — design system (0398933)
Fonts, textures, knob/key/panel/tube painters, `PedalArt` for the eight pedals.
Sheet: `m2_design_sheet.jpg`.

## M3 — main screen
Complete:
* New 1260 x 840 (3:2) design size matching the references; same whole-UI scaling.
* Header: chrome logo pill with silver SHOBEATS and lit FX badge, preset well with warm
  underglow, orange chevron keys, SAVE, illuminated ADVANCED/MAIN key.
* Drift / Unstable: chamfered machined modules, large numbered knobs with an illuminated
  ring whose brightness follows the value, value windows, glass tubes whose glow follows
  the parameter, vent slots.
* Central display: preset name, category, selected vibe, tempo (link icon only when the
  tempo really comes from the host), chain status (active pedals in order or
  "ALL PEDALS BYPASSED"), Drift / Unstable values, and a **live waveform of the plugin's
  real output**, read from a new lock-free `VisualFeed` (no fake animation: silence draws a
  flat line).
* Six icon vibe keys (lit = selected vibe), illuminated GENERATE key with chevrons and
  speaker grilles; same generation code path as before.
* FX chain: eight miniature pedals using the shared pedal art; knob positions, LEDs,
  windows follow real parameters; tape reels turn only while audio is processed and the
  pedal is on; resampler bars and tape VU follow the real output level. Click toggles,
  double-click opens the pedal (unchanged behaviour).
* Master: Input / Mix / Output machined knobs and stereo LED ladders.
* Static art (plates, trays, grilles, labels) is a separate component cached as an image.
* One `FrameClock` per editor (VBlank, 60 fps cap, 15 fps idle, stops when hidden).

Visible differences from reference 01 still open (for M8 polish):
* Overall warmth: the reference has more orange light reflections on the chassis edges.
* Pedal enclosures are a bit flatter (less glossy clear-coat highlight) than the renders.
* Master knobs are smaller than in the reference.

## M4 — shared Advanced layout + Saturation
Complete:
* One `DetailPanel` layout for all eight effects (rebuilt on selection): PEDAL SETTINGS,
  italic accent title, description, ON key with LED, EARLIER / LATER keys, illustration
  corner, effect display, mode segments / switches, machined knobs with value windows.
  The panel's light seams take the selected pedal's accent colour.
* Advanced pedalboard: eight pedals drawn with the shared pedal art at full size; real
  pedal knobs (tape: the reels are the Wow/Age controls), real footswitch, selection glow
  in the pedal's accent, slot badges. Drag-and-drop reordering, Earlier/Later, click to
  select unchanged. Delay tap LED kept.
* `PedalLive` drives main-screen and Advanced pedals from the same parameter/feed logic.
* `EffectArt`: bundled artwork if present, otherwise procedural; its glow follows the
  effect (Saturation tubes follow Drive and on/off).
* Saturation display: transfer curve computed from the real DSP (`Saturation::shape`,
  `compensationGain`, Drive, Output, Mix, mode), neon styling, grid, caption.

Removed on purpose: the small LCD read-outs on the resampler / reverse / delay pedals and
the "SHOBEATS" silkscreen on the name plates (not in the approved designs; the same values
are shown in the detail panel).

## M5 + M6 — Resampler, Tape, Chorus, Tremolo, Reverse, Delay, Reverb
Complete:
* `EffectDisplay` is now a `FrameClock` client with access to the `VisualFeed`.
  Modulation displays (chorus, tremolo, reverse) advance only while their effect is on;
  static displays repaint only when a parameter or the tempo changes; nothing repaints
  while the Advanced view is hidden.
* Shared caption pill (accent outline, sine glyph, read-out) as in references 03–08.
* Resampler: reference tone sample-and-held at Rate and quantised to Bits (real maths of
  the effect), the clean tone faintly behind it. Pill "kHz / bit".
* Tape: transport deck with two independent reels (`pedal::paintReel`), tape path, guides
  and head. Reels spin up only while audio is being processed and the effect is on, coast
  to a stop when bypassed or when the host stops calling the plugin. Pack size follows Age.
  Wow/flutter trace below, hiss from Noise, "AGE %" read-out.
* Chorus: four voices on their own LFO phases; depth = sweep, base delay = band height.
* Tremolo: gain curve from `Tremolo::shapeValue` (shape, depth), right channel shown when
  Stereo > 0, live LFO playhead, rate or synced division.
* Reverse: forward segment (faint) vs reversed segment (bright) per window, left arrows,
  window boundaries scrolling at the reverse time.
* Delay: L / R lanes, dry hit, repeats at Time spaced and Feedback-scaled, ping-pong
  alternates lanes, accent bars with glow. Pill "time / FB %".
* Reverb: decay envelope reaching silence at RT60 after the pre-delay, damping envelope,
  early reflections spread by Size, diffuse speckle, blue→violet fill.
* All remaining hard-coded `Palette::` colours in the displays replaced by accent colours.

Open / differences from references 03–09:
* Reverse reference draws plain sines; ours draws enveloped segments because that is what
  the effect does (kept on purpose).
* Lower-left illustrations are procedural and simpler than the reference renders (the
  optional `assets/images/art_<effect>.png` slot can take licensed art later).
* Chassis warmth / reflections still for M8.

## M7 — audio-reactive integration and rendering cost
Real data behind every moving element (summary):

| Element | Driven by |
|---|---|
| Main display waveform | `VisualFeed` output peaks (lock-free ring written at the end of `processChunk`) |
| Tape reels (main, Advanced pedal, Advanced display) | `VisualFeed::samplesProcessed()` changing + tape on; coast down when either stops |
| Resampler bars / Tape VU on pedals | smoothed output level from the feed |
| Chorus / Tremolo / Reverse / Delay animation | parameter values; time advances only while the effect is on |
| Saturation tubes, Drift/Unstable tubes, reverb dome, delay rings | parameter values |
| LED meters | processor meters (30 Hz editor timer, repaint only when a segment changes) |

Rendering cost:
* Static artwork (chassis + header, pedalboard frame and badges, Advanced panel frame and
  title) moved into cached `Backdrop` components. Animated children now repaint over a
  bitmap instead of re-running the panel gradients and textures each frame.
* Measured with the new `[perf]` test (Linux, software renderer, RelWithDebInfo; Windows
  uses Direct2D and is faster). Full-editor repaint, worst case:

| Scale | Main before → after | Advanced before → after |
|---|---|---|
| 100 % | 35 → 26 ms | 63 → 30 ms |
| 125 % | 72 → 46 ms | 162 → 65 ms |
| 150 % | 99 → 62 ms | 204 → 59 ms |
| 200 % | 172 → 88 ms | 353 → 99 ms |

  A normal animation frame only repaints the moving parts: about 1.5–2.3 ms per pedal or
  display at 100 %.

New regression tests:
* `UI: tape reels turn only while audio is processed and the pedal is on`: still when the
  host isn't processing, turning while it is, settling after audio stops, still when
  bypassed, and still when the Advanced view is hidden.
* `UI: full-editor render cost stays within budget` (prints the timings above).

Also fixed the MSVC shadowing warnings (C4458/C4459) introduced in M2–M4.

## M8 — polish, scaling, overlays
Complete:
* Warmth: bezels are smoked bronze instead of neutral steel, with orange light reflected
  in their lower edges; module plates catch orange in the lower chamfers; extra light
  seams along the chassis (top of the hardware plate, bottom of the chain plate).
* Pedal finish: clear-coat diagonal reflection band, warm rim light on the right edge,
  more metal flake / less hammer-tone on the sparkle finishes (resampler, chorus,
  tremolo, delay, reverb).
* Master section: larger Input / Mix / Output knobs (cells overlap slightly).
* Combo-box and pop-up menu text use the condensed value face.
* Preset browser and Save panel sit in the same bezel panel as the rest of the hardware
  (separate commit, 2 small hunks in `PresetBrowser.cpp` paint code only).
* Scaling test at 100 / 125 / 150 / 200 %: editor size, single view transform, every
  visible control inside its parent in both views, renders at each size
  (`SHOBEATS_SNAPSHOT` also writes `scale<percent>_{main,adv}.png`). Visual check of the
  crops: identical layout, crisp text and strokes at every scale.

Open (honest list):
* Pedal enclosures are still less glossy / saturated than the reference renders.
* Lower-left illustrations in the Advanced panel are procedural and simpler than the
  reference images; licensed art can be dropped into `assets/images/art_<effect>.png`.
* The GENERATE key and hex grilles are smaller and flatter than on reference 01.
* Not tested inside FL Studio by me: only pluginval + the lifecycle host run on CI.

## DELAY skin — first pedal drawn from high-resolution artwork
Complete (details in `DELAY_ASSETS.md`):
* 20 supplied files bundled unchanged in `assets/images/delay/`. Each was checked for
  exact @2x size, alpha, transparent corners, glass transparency and template alignment.
* `Skin` loader: decodes on first use, keeps an exact @1x copy, and picks the copy from
  the physical pixel scale. It is owned by the editors (no static images), and the
  whole skin falls back to the procedural drawing if any file is missing.
* Main tile, Advanced pedal, footswitch, LEDs, pedal knobs, settings-panel background,
  display frame and glass, panel knobs and corner illustration use the images. Echo
  rings, display graph, value arcs, ticks, value text, keys and badges stay in code.
* Selection glow fade (150 ms), status LED cross-fade (80 ms), tap-tempo LED blink.
* New tests `[skin]`: assets, knob rotation (-135 / 0 / +135 degrees measured on the
  rendered pointer), image lifetime, the 10 skinned sliders and the footswitch still
  drive their parameters, and rendering at 85–200 %.
