# SHOBEATS FX hardware UI redesign: delivery report

Branch: `claude/hardware-ui-redesign` (based on the Sound Lab branch `claude/clever-darwin-1ualx2` at `73e6c00`).
Nothing was pushed to `main`, nothing merged, nothing deployed.

## 1. What was implemented

* **Design system (M2).** Embedded fonts, procedural metal textures, and one library of hardware painters:
  * machined knobs and pedal knobs
  * keys, footswitches, bezels and trays
  * light seams, glass and vacuum tubes
  * speaker grilles, value windows and LED ladders

  `PedalArt` draws the eight individually styled pedals from a single state struct.
* **Main screen (M3), as in reference 01.**
  * Header: logo, preset well with BROWSE / ‹ › / SAVE, and an illuminated MAIN / ADVANCED key.
  * DRIFT and UNSTABLE modules with numbered knobs and glass tubes.
  * Central display: preset, category, vibe, tempo with host link, chain status, Drift/Unstable read-out and a live waveform.
  * FX Generator vibe keys, GENERATE, and an 8-pedal FX chain.
  * MASTER section: Input, Mix and Output knobs with LED meters.
* **Advanced screen (M4–M6), as in references 02–09.**
  * One shared layout for all eight effects: PEDAL SETTINGS, title and description, ON key, EARLIER / LATER, illustration corner, effect display, mode segments and switches, and machined knobs with value windows.
  * The panel's light seams take the selected pedal's colour.
  * The full-size pedalboard keeps drag-and-drop reordering.
* **Effect displays (M4–M6).** All eight are computed from the effect's real parameters; see section 4.
* **Audio-reactive feed and performance (M3, M7).**
  * `VisualFeed` is a lock-free, allocation-free audio-to-UI ring buffer.
  * `FrameClock` is one VBlank clock per editor, capped at 60 fps, with a 15 fps idle rate. It stops when hidden or closed.
  * Static artwork is cached in `Backdrop` images.
* **Polish and scaling (M8).**
  * Warm bronze bezels with reflected orange light, a clear-coat pedal finish and larger master knobs.
  * The preset browser and save panel now sit in the same hardware bezel.
  * Verified at 100 / 125 / 150 / 200 %.

**Unchanged on purpose:**
* all DSP, the 61 parameters and their IDs, and automation
* DAW state save/restore and presets, including factory, imported and Sound Lab presets
* FX Generator and Drift/Unstable logic
* chain order (a lock-free atomic) and the FL Studio plugin-removal fix (`prepared` flag)

## 2. Assets

See `ASSETS.md`.

* **Fonts:** Barlow and Barlow Condensed, licensed under the SIL OFL. They are embedded in the plugin.
* **Everything else:** drawn in code (vector and procedural). This keeps it sharp at every scale, and every lit element follows real data.
* **Not used:** no commercial-plugin graphics and no images taken from the references. The references are used only as design targets and for compare sheets in `docs/ui-redesign`.
* **Optional artwork:** `assets/images/art_<effect>.png` (lower-left illustrations). These files are not produced. A procedural fallback is drawn when they are absent.

## 3. Interactive components (all bound to real JUCE parameters)

| Component | Binding |
|---|---|
| All knobs (header, Drift/Unstable, master, Advanced, pedal knobs) | `SliderAttachment` |
| Tape pedal reels (Wow / Age) | real pedal knobs under the painted reels |
| ON keys, footswitches, toggle chips | `ButtonAttachment` / `ParameterAttachment` |
| Mode segments (saturation mode, tremolo shape, ...) | `ParameterAttachment` |
| Sync swaps (Time ↔ Division) | parameter-driven |
| Pedal drag-and-drop, EARLIER / LATER, chain tiles | `ChainOrder` (atomic) |
| Vibe keys, GENERATE, preset ‹ › BROWSE SAVE | existing preset manager / generator |
| MAIN / ADVANCED key | view switch |

## 4. Animations and their real data source

| Animation | Source |
|---|---|
| Main display waveform | output peaks from `VisualFeed` |
| Tape reels (main tile, Advanced pedal, Advanced display) | spin only while `processBlock` is running and tape is on; coast to a stop when bypassed or stopped; frozen when hidden |
| Resampler bars, Tape VU on the pedals | smoothed output level from the feed |
| Saturation transfer curve | `dsp::Saturation::shape` + `compensationGain` with Drive / Output / Mix / mode |
| Resampler display | a reference tone held at Rate and quantised to Bits |
| Chorus voices / Tremolo gain curve / Reverse segments | Rate, Depth, Delay, Shape, Stereo, Time / Division, Pitch, Mix; time advances only while the effect is on |
| Delay taps | Time / Division, Feedback, Mix, Ping-pong |
| Reverb envelope | Decay (RT60), Pre-delay, Damping, Size |
| Tubes, dome, echo rings, pedal windows | parameter values |
| LED meters | processor meters; repaint only when a segment changes |

## 5. Open improvements

* Pedal enclosures are still less glossy and less saturated than the reference renders.
* The lower-left illustrations are procedural and simpler than the references. Licensed art can be dropped in without code changes.
* The GENERATE key and hex grilles are smaller and flatter than on reference 01.
* Rendering on Windows uses Direct2D. The cost figures in `PROGRESS.md` come from the Linux software renderer, which is an upper bound.

## 6. Differences from the designs

* The references are AI-generated images, so some details are physically inconsistent. We follow the layout, materials and colours, not pixel-exact shapes.
* Values shown are the plugin's real values. For example, the reference shows "AGE 25 %"; ours shows whatever the preset sets.
* Reverse display: we draw the reversed segments (a swell then a cut) rather than plain sines, because that is what the effect actually does.
* Removed on purpose:
  * the small LCD read-outs on the resampler / reverse / delay pedals
  * the "SHOBEATS" silkscreen on the pedal plates

  Neither is in the approved designs. The same values are shown in the Advanced panel.

## 7. Test results (Linux, this branch)

* Plugin tests: **57 / 57 test cases pass** (859 040 assertions). New in this work:
  * design-system render at 1x and 2x
  * tape reels move only while audio runs and the pedal is on, settle when bypassed or stopped, and freeze when hidden
  * render cost at 100–200 %
  * scaling at 100 / 125 / 150 / 200 %
* DSP tests: **52 / 52 pass**.
* Not tested in FL Studio by me. pluginval passing does not prove FL Studio behaviour; please do a manual check: open, close and remove the plugin, multiple instances, and resizing.

## 8. Windows GitHub Actions result

**Windows build: success** on commit `a742db4` (the last code commit), run #21:
https://github.com/Outelha/Outelha.github.io/actions/runs/44912629814

| Step | Result |
|---|---|
| Release build (MSVC, x64) | passed |
| Unit tests (ctest, DSP + plugin) | passed |
| VST3 bundle layout check | passed |
| Lifecycle stress test (load, process, editor, remove, unload many times) | passed |
| pluginval strictness 10 | passed |

**Linux AddressSanitizer: success** on the same commit:
https://github.com/Outelha/Outelha.github.io/actions/runs/44912629793

Earlier runs:
* The M4 run failed with a link error: `PedalLive.cpp` was missing from `CMakeLists.txt`. M5/M6 fixed it.
* The intermediate runs were cancelled by newer pushes.

## 9. VST3 artifact

`ShobeatsFX-VST3-Windows-x64-Release` (complete `ShobeatsFX.vst3` bundle, 25 MB zipped; kept until 2026-12-21):
https://github.com/Outelha/Outelha.github.io/actions/runs/44912629814/artifacts/14710923733

Debug symbols for crash dumps from a DAW: `ShobeatsFX-PDB-Windows-x64-Release`:
https://github.com/Outelha/Outelha.github.io/actions/runs/44912629814/artifacts/14711003232

To install: unzip the artifact and copy the `ShobeatsFX.vst3` folder to `C:\Program Files\Common Files\VST3\`. Then rescan plugins in FL Studio.

## 10. Branch and commits

`claude/hardware-ui-redesign`, on top of `73e6c00` (Sound Lab):

| Commit | Milestone |
|---|---|
| 22f54ac | M1 audit, references, snapshot/compare tooling |
| 0398933 | M2 design system |
| d013c05 | M3 main screen |
| 969f133 | M4 shared Advanced layout + Saturation |
| 3f5d6b7 | M5/M6 remaining seven effect displays |
| c114539 | M7 cached backdrops, animation and render-cost tests |
| badecf0 | M8 warmth, pedal finish, master knobs, scaling test |
| a742db4 | M8 preset browser / save panel bezel (isolated) |

## 11. Integrating with the Sound Lab work

* This branch starts exactly at the Sound Lab head (`73e6c00`), and Sound Lab has no newer commits. Merging this branch into the Sound Lab branch is therefore a **fast-forward with no conflicts** today.
* Sound Lab files that were **not touched**:
  * `SoundLab.*`, `PresetImporter.*`, `PresetAudit.*`, `PresetManager.*`, `Generator.*`
  * preset data and lab data
  * all DSP
* Shared files that were touched, and only in paint/layout code:
  * `source/ui/PresetBrowser.cpp`: `PresetBar` paint/layout, plus the two panel-background hunks in commit `a742db4`.
  * `source/PluginEditor.*`: the view layout.
  * `source/PluginProcessor.*`: adds `VisualFeed` and the host-tempo flag. No DSP or state changes.
  * `CMakeLists.txt`: the new UI sources and the `ShobeatsAssets` font target.
  * `tests/PluginTests.cpp`: new UI tests appended at the end, plus the snapshot block.
* If Sound Lab moves on before the merge:
  1. Rebase this branch onto it: `git rebase origin/claude/clever-darwin-1ualx2`.
  2. Expect conflicts, if any, only in the files above. Keep Sound Lab's logic and this branch's paint code.
  3. If the browser conflicts, `a742db4` can be dropped on its own.
* After merging:
  1. Run `tools/ui_snapshot.sh <build-dir> <out-dir>` to re-render and compare against the references.
  2. Run the test suites.
* Merging into `main` (the website) is not part of this work and needs your approval.
