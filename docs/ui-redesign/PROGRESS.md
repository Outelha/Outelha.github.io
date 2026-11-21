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
