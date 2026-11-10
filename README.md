# Trio FX

An audio effect plugin (VST3) with three modules in series:

**Saturation → Chorus → Reverb**

![Trio FX user interface](docs/screenshot.png)

| Module | What it does | Controls |
| --- | --- | --- |
| Saturation | Waveshaper running at 4x oversampling, with automatic level compensation | Character (Soft / Warm / Hard), Drive, Tone (tilt), Mix |
| Chorus | Stereo chorus: two modulated delay voices per channel, LFOs 90° apart between L and R | Rate, Depth, Delay, Mix |
| Reverb | Pre-delay → allpass diffusion → 8-line feedback delay network with damping and gentle modulation | Size, Decay (RT60), Damping, Pre-Delay, Width, Mix |

Global Input/Output gain and peak meters. Each module has its own power button,
and switching it on or off crossfades without clicks. The window can be resized
(75%–200%) with a fixed aspect ratio. Double-click a knob to reset it,
shift-drag for fine control, or click the value to type a number.

The plugin reports the oversampling latency to the host, and host bypass keeps
the audio aligned by applying the same delay.

## Repository layout

```
source/dsp/       Plain C++ DSP (Saturation, Chorus, Reverb) — no JUCE dependency
source/ui/        Look-and-feel and UI components
source/           AudioProcessor, parameters, editor
tests/            Catch2 unit tests (DSP + plugin/processor/editor)
libs/JUCE         JUCE 8.0.17-security, pinned submodule (commit dabd5ad)
libs/Catch2       Catch2 v3.8.1, pinned submodule (commit 2b60af8)
.github/workflows Windows x64 Release build, tests, pluginval, VST3 artifact upload
```

## Building

Requirements: CMake ≥ 3.22, a C++20 compiler (Visual Studio 2022 or newer on Windows).

```bash
git clone --recurse-submodules <this repo>
```

If you already cloned without submodules:

```bash
git submodule update --init --recursive
```

Windows (Visual Studio, x64):

```bash
cmake -S . -B build -A x64
```

```bash
cmake --build build --config Release --parallel
```

```bash
ctest --test-dir build -C Release --output-on-failure
```

The VST3 bundle is written to `build/TrioFX_artefacts/Release/VST3/TrioFX.vst3`.
Copy the whole `TrioFX.vst3` folder to `C:\Program Files\Common Files\VST3\`.

CMake options:

| Option | Default | Meaning |
| --- | --- | --- |
| `TRIOFX_BUILD_TESTS` | `ON` | Build the unit tests |
| `TRIOFX_BUILD_STANDALONE` | `OFF` | Also build a standalone app |

Set the environment variable `TRIOFX_SNAPSHOT=path/to/file.png` while running the
plugin tests to save a rendering of the UI.

## Continuous integration

`.github/workflows/build-windows.yml` runs on every push and pull request:

1. Builds Release x64 with Visual Studio
2. Runs all unit tests
3. Checks the VST3 bundle layout (`Contents/x86_64-win/TrioFX.vst3`, `Contents/Resources/moduleinfo.json`)
4. Validates the plugin with [pluginval](https://github.com/Tracktion/pluginval) at strictness level 10
5. Uploads the complete `TrioFX.vst3` bundle as the artifact `TrioFX-VST3-Windows-x64-Release`

## Naming

The product name, company, bundle ID and plugin codes are set at the top of
`CMakeLists.txt` (`TrioFX`, `NerdyProd`, `Nrdy`, `Trfx`). These are placeholders.
Change them **before** the first public release: hosts identify the plugin by these
codes, so changing them later breaks saved sessions.
