# SHOBEATS FX

An analog-style multi-effects pedalboard plugin (VST3, Windows x64) built with C++ and JUCE.

![SHOBEATS FX user interface](docs/screenshot.png)

## Effects

Eight pedals, every control processes audio:

| Pedal | Controls |
| --- | --- |
| Saturation | Character (Soft / Warm / Hard / Fuzz / Fold), Drive, Tone, Output, Mix — 4x oversampled |
| Resampler | Rate (500 Hz – 48 kHz), Bits (2 – 24), Filter, Jitter, Mix |
| Tape | Saturation, Wow, Flutter, Noise (hiss), Age (head wear + dropouts), Mix |
| Chorus | Rate, Depth, Delay, Feedback, Width, Mix |
| Tremolo | Shape (Sine / Triangle / Square / Saw), Rate or BPM-synced division, Depth, Stereo (auto-pan) |
| Reverse | Length (ms or BPM-synced), Pitch (±12 st), forward/reverse Mix |
| Delay | Time (ms or BPM-synced), Feedback, Low Cut, High Cut, Ping-Pong, Width, Mix |
| Reverb | Size, Decay, Damping, Pre-Delay, Width, Mix |

Global: Input, Mix (whole chain wet/dry, latency-aligned), Output, peak meters, host tempo display.

## Using it

- **Stomp** a pedal's footswitch to switch it on/off (red LED = on). New instances start with every pedal off.
- **Turn** the two knobs on a pedal for its main controls. Shift-drag for fine control, double-click to reset.
- **Click** a pedal to open its *Advanced* panel with every control and a live display.
- **Drag** a pedal left/right to change the processing order (or use *Earlier / Later* in the panel).
  The order is saved with your project.
- Synced effects follow the host tempo (shown in the header).

## Status

| Milestone | Content | State |
| --- | --- | --- |
| M1 | Saturation, chorus, reverb, CMake, CI | Done |
| M2 | 8 effects, reorderable chain, global mix, BPM sync, pedalboard UI + advanced panel | Done |
| M3 | Global Drift / Unstable analog movement, UI polish | Next |
| M4 | Preset browser, factory presets, save/load user presets | Planned |
| M5 | FX Generator (Vintage, Dreamy, Dark, Lo-Fi, Space, Experimental) | Planned |

## Testing in FL Studio (Windows)

1. Open the latest green run of **Windows build** under the repository's *Actions* tab.
2. Download the artifact **ShobeatsFX-VST3-Windows-x64-Release** and unzip it.
3. Copy the whole `ShobeatsFX.vst3` folder to `C:\Program Files\Common Files\VST3\`.
4. In FL Studio: *Options → Manage plugins → Find more plugins* (or *Find installed plugins*).
5. Load **SHOBEATS FX** on a mixer insert.

## Building

Requirements: CMake ≥ 3.22, Visual Studio 2022 or newer, Git.

```bash
git clone --recurse-submodules <this repo>
```

```bash
cmake -S . -B build -A x64
```

```bash
cmake --build build --config Release --parallel
```

```bash
ctest --test-dir build -C Release --output-on-failure
```

The bundle is written to `build/ShobeatsFX_artefacts/Release/VST3/ShobeatsFX.vst3`.

| CMake option | Default | Meaning |
| --- | --- | --- |
| `SHOBEATS_BUILD_TESTS` | `ON` | Build the unit tests |
| `SHOBEATS_BUILD_STANDALONE` | `OFF` | Also build a standalone app |

Set `SHOBEATS_SNAPSHOT=path/prefix` when running the `[gui]` plugin tests to save UI renderings.

## Repository layout

```
source/dsp/       Plain C++ DSP for the eight effects (no JUCE, no allocation while processing)
source/ui/        Look-and-feel, pedals, pedalboard, advanced panel, displays
source/           Processor, parameters, chain order, editor
tests/            Catch2 tests: DSP behaviour, bypass, state recall, automation, editor
libs/JUCE         JUCE 8.0.17-security, pinned submodule (commit dabd5ad)
libs/Catch2       Catch2 v3.8.1, pinned submodule (commit 2b60af8)
.github/workflows Windows x64 Release build, tests, pluginval, VST3 artifact upload
```

## Continuous integration

Every push builds Release x64 on Windows, runs all unit tests, checks the VST3 bundle layout,
validates the plugin with pluginval at strictness level 10, and uploads the complete
`ShobeatsFX.vst3` bundle as an artifact.
