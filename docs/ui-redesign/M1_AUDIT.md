# SHOBEATS FX hardware UI redesign — M1 audit

Branch: `claude/hardware-ui-redesign`, created from `claude/clever-darwin-1ualx2` at `73e6c00`
("SHOBEATS SOUND LAB: rule-based original preset generator, audio QC, curation browser").
`main` (the nerdyprod.me website) is never touched by this work.

The Sound Lab / 500-preset work happens on `claude/clever-darwin-1ualx2` in another thread.
This branch only changes `source/ui/*`, `source/PluginEditor.*`, a small read-only
visualisation feed in the processor (M7), new assets, tests and docs. See
"Merge strategy" at the end.

## 1. Architecture inventory

### Editor and views

| Piece | File | What it does today |
|---|---|---|
| `ShobeatsAudioProcessorEditor` | `source/PluginEditor.*` | Owns the look-and-feel, tooltip window and `MainView`. Resizable with fixed aspect; the whole view is drawn at a fixed design size (1240 x 800) and scaled with an `AffineTransform`. A 30 Hz timer pushes meter levels and BPM into the view. |
| `MainView` | `source/PluginEditor.*` | Top bar (logo, `PresetBar`, view switch). Shows either `MainPanel` (MAIN) or `Pedalboard` + `DetailPanel` (ADVANCED). Hosts the `PresetBrowser` and `SavePresetPanel` overlays. |
| `MainPanel` | `source/ui/MainPanel.*` | Drift / Unstable knobs (`Knob::Style::alu`), `CentralDisplay`, six vibe `KeyCap`s, GENERATE, eight `EffectTile`s (click = toggle, double-click = open in Advanced), Input / Mix / Output knobs, IN / OUT `LevelMeter`s. 10 Hz timer re-orders the tiles when the chain changes. |
| `CentralDisplay` | `source/ui/MainPanel.*` | Preset name / category / description, chain chips, Drift / Unstable values, BPM. 10 Hz timer polls the processor. **No audio visualisation yet.** |
| `Pedalboard` / `Pedal` / `Footswitch` | `source/ui/Pedalboard.*` | Eight pedals in chain order, each with two `Knob::Style::pedal` knobs (or a `SyncSwap`), name plate, LED, stomp switch. Drag-and-drop reordering through `juce::DragAndDropContainer`; `nudgeSelected(±1)`. Animated slide when the order changes (10 Hz board timer, 20 Hz per-pedal timer for live readouts). |
| `DetailPanel` | `source/ui/DetailPanel.*` | One shared panel for the selected effect: name, description, ON chip, Earlier / Later, `EffectDisplay`, segmented mode choices / toggles, all DSP knobs. Rebuilt in `showEffect()`. |
| `EffectDisplay` | `source/ui/EffectDisplay.*` | Parameter-driven visualisations for all eight effects (transfer curve, stepped wave, tape wobble, chorus LFOs, tremolo shape, reverse segments, delay taps L/R, reverb envelope). 30 Hz timer, repaints only while animated or values change. |
| `PresetBar`, `PresetBrowser`, `SavePresetPanel` | `source/ui/PresetBrowser.*` | Preset navigation, factory / user / imported / Lab categories, save dialog. 8 Hz timer. |
| Shared controls | `source/ui/Controls.*` | `Knob` (panel / pedal / alu), `SyncSwap`, `ToggleChip` (bool param), `SegmentedChoice` (choice param), `LevelMeter`. All use APVTS attachments. |
| Look-and-feel | `source/ui/ShobeatsLookAndFeel.*` | Rotary slider rendering (pedal and arc knobs), text boxes, buttons, popup bubble. |
| Hardware painter | `source/ui/Hardware.*` | Procedural brushed plate, recess, screws, engraved text, glass display, aluminium knob, LED. |
| Textures | `source/ui/Textures.*` | 128 px noise and 512 px brushed tiles generated in code, owned through a `SharedResourcePointer` held by each editor (lifetime rule, see 4). |
| Theme | `source/ui/Theme.h` | Palette, per-effect enclosure colours, `uiFont()`. |

No raster or SVG assets exist in the repository today: everything is drawn procedurally.
`juce_add_binary_data` is already used for presets, so adding an asset bundle is a small,
well-understood CMake change.

### Parameters and state

* 61 parameters in one `AudioProcessorValueTreeState` (`source/Parameters.*`). Ids are
  stable strings; `parameterVersion = 1`. Effects are `EffectId` 0–7 with
  `effectInfo[].enabledId` for each on/off switch.
* Chain order: `ChainOrder` packs the eight slots into one atomic word (UI writes, audio reads).
  Stored in the state tree.
* All UI controls are bound by `SliderAttachment`, `ButtonAttachment` or `ParameterAttachment`.
  There is no UI-only duplicate of any parameter. The redesign must keep it that way.

### DSP and audio thread

* `processBlock` → `updateDspParameters(bpm)` → `processChunk` in prepared-size chunks.
  Saturation runs 4x oversampled; dry path latency-matched; click-free fades on preset load
  and reordering.
* Audio → UI data today: `LevelMeters` (atomic peak per channel, CAS max on the audio
  thread, `exchange(0)` on the UI) and `currentBpm` (atomic double). Nothing else.
* Global Drift / Unstable are DSP parameters handled in `DspCommon.h` (`MovementTests`).

### Animation sources today

| Timer | Rate | Purpose |
|---|---|---|
| Editor | 30 Hz | meters + BPM into the view |
| `EffectDisplay` | 30 Hz | animated displays |
| `Pedal` (resampler, tape, reverse, delay) | 20 Hz | live readouts / small animation |
| `Pedalboard` | 10 Hz | chain order polling, slide animation |
| `MainPanel`, `CentralDisplay` | 10 Hz | tile order, display text |
| `PresetBrowser` | 8 Hz | list refresh |

All timers belong to components and stop when the editor is destroyed. There is no
central frame clock and no visibility-based throttling.

## 2. Stability fixes that must be preserved

From commits `6320604`, `e30d7c2`, `d42ae81`:

1. **Process before prepare** (the FL Studio removal crash): `prepared` atomic; until it is
   set, `processBlock` / `processBlockBypassed` pass audio through. The redesign adds nothing
   to the audio path before that check.
2. **No graphics objects in static storage.** Images on Windows are Direct2D/COM resources;
   they must be owned by something whose lifetime ends with the last editor (today:
   `SharedResourcePointer<Textures>` in the look-and-feel). Every new asset cache in the
   redesign follows the same rule, and `LifecycleTests` checks `Textures::liveInstances()`.
3. **Async callbacks use `SafePointer`** (preset delete dialog). Any new async UI callback
   must do the same.
4. CI runs the VST3 lifecycle stress host and two full pluginval passes at strictness 10.

## 3. Regression protection already in place

`tests/PluginTests.cpp`, `LifecycleTests.cpp`, `SoundLabTests.cpp` drive the editor directly:

* editor size defaults to 85 % of `MainView::designWidth/Height`;
* `MainView::getMainPanel().refreshDisplay()`, `getPedalboard().select()/nudgeSelected()`,
  `getDetailPanel().getEffect()`, `getBrowser().showCategory()/clickRow()`, `setAdvanced()`,
  `showBrowser()`;
* 100x editor create/paint/destroy, editors opened on a background audio load, two instances.

These public view APIs are the contract the redesign keeps, so the existing tests keep
exercising the new UI. `SHOBEATS_SNAPSHOT=<prefix>` renders main, the eight Advanced
panels, browser and Lab views at 2x; `tools/ui_snapshot.sh` (added in M1) runs it headless
and builds side-by-side comparison sheets against the nine references.

## 4. Reference images: what each needs

All references are 3:2 (1536 x 1024; Reverse and Delay are 1448 x 1086). The current design
size is 1240 x 800 (1.55:1). The redesign moves to **1260 x 840 (3:2)** so the layout can
follow the references proportionally.

| Area | Reference detail | Interactive | Animated / data | Drawing approach |
|---|---|---|---|---|
| Top bar | chrome logo plate with orange "FX" badge, preset well with orange underglow, `<` `>` keys, SAVE, big orange ADVANCED/MAIN key | presets, browse, save, view switch | — | procedural |
| Drift / Unstable | machined plate with screws, large spun-aluminium knob, orange ring, 0–10 scale, value window, vertical glass tube + vent slots at the outer edges | knobs | ring + tube glow track the parameter | procedural knob; tube procedural |
| Central display | bevelled glass, preset name in italic orange, vibe line, BPM + link icon, live orange waveform, status line | — | **real** output waveform / spectrum | procedural + audio feed (M7) |
| Vibe keys | six keys with icons (cassette, cloud, moon, waveform, planet, flask) | yes | selected glow | procedural icons (paths) |
| GENERATE | wide orange illuminated key, chevrons, speaker grilles each side | yes | press state | procedural |
| FX chain | eight miniature pedals with individual faces (tubes, bargraph, reels+VU, sine windows, ◀◀, rings, starry dome), name plate, LED, footswitch, slot number | toggle, select, double-click, drag reorder | tape reels, bargraph, sine windows | procedural per-pedal faces |
| Master | Input / Mix / Output knobs, vertical LED ladders IN/OUT | knobs | meters (real) | procedural |
| Advanced top | eight larger pedals, selected one outlined in the effect accent, slot badge | as above | as above | shared `PedalFace` painter |
| Advanced bottom | PEDAL SETTINGS, italic accent name, description, ON pill, EARLIER/LATER, display, mode segments, metal knobs with accent ring and value pill; panel trim glows in the selected accent | all | effect display | shared `DetailPanel` |
| Bottom-left illustration | photo-like vacuum tubes / chip / tape deck / waves / echo rings / nebula per effect | no | optional | **raster asset candidates** (see 5) |

## 5. Asset strategy (decided for M2)

* **Procedural vector rendering stays the primary technique** for every control, pedal,
  display and panel. It is resolution-independent, which is what makes 100–200 % Windows
  scaling sharp, and it lets every lit element follow real parameter values.
* Knobs: procedural multi-layer rendering (drop shadow, knurled skirt, conical-brushed cap
  using a cached conic texture, specular highlight, indicator, accent arc), not filmstrips:
  filmstrips would need 8 accent colours x several sizes x 2 scales.
* Textures (noise, brushed, conic brush, pedal finishes such as hammered/sparkle/
  starfield) are generated in code once and cached in the shared `Textures` object, keeping
  the Direct2D lifetime rule.
* The photo-like bottom-left illustrations in references 02–09 cannot be convincingly drawn
  procedurally. They are decorative and non-interactive. M2 adds an `assets/` directory, a
  `ShobeatsAssets` binary-data target and an `AssetImage` component that draws a PNG if
  present and a tasteful procedural fallback otherwise. The list of required artwork goes
  in `docs/ui-redesign/ASSETS.md`. No commercial plugin artwork and no parts of the
  reference images are copied into the plugin.

## 6. Real-time-safe visual data plan (M7)

* Add a `VisualFeed` owned by the processor: a single-producer/single-consumer lock-free
  ring of downsampled min/max pairs of the output (and input) per channel, written at the
  end of `processChunk` with no allocation, plus an atomic block counter and the host's
  "is playing" flag.
* The editor reads it from one central `FrameClock` (`juce::VBlankAttachment`,
  capped at 60 Hz, dropping to 15 Hz when nothing animates, stopped when the editor is not
  showing). Components register with the clock instead of running their own timers.
* Tape reels rotate from the block counter while the tape pedal is enabled and audio is
  being processed; they settle when bypassed or when processing stops.

## 7. Risks

| Risk | Mitigation |
|---|---|
| Merge conflicts with the Sound Lab branch | That work lives in `source/presets/*`, `presets/lab`, `tools/sound_lab`, `PresetBrowser`. The redesign leaves those alone except for restyling `PresetBrowser` in M8, done last and isolated in its own commit. |
| Changing the design size breaks tests | Tests use `MainView::designWidth/Height` symbolically; only the constants change. |
| Paint cost of procedural realism | Static layers (chassis, plates, pedal bodies) are rendered once into cached images per scale (`setBufferedToImage` or explicit caches); only dynamic layers repaint. Measured in M8. |
| Direct2D image lifetime | All caches live in editor-owned objects; covered by `LifecycleTests`. |

## 8. M1 status

Complete:
* Repository, branch history, UI / DSP / state / lifecycle architecture audited (this file).
* Feature branch `claude/hardware-ui-redesign` created from `73e6c00`.
* `tools/ui_snapshot.sh`: headless rendering of all views + side-by-side comparison with the
  references; baseline renders of the current UI saved under `docs/ui-redesign/baseline/`.
* Linux build and full test suite run on the unmodified code as the baseline.

Not part of M1: any visual change.
