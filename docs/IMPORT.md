# Importing presets from other plugins

SHOBEATS FX can convert presets from **TONEFLX** and **FFWD FX** (`.fwd` preset files and
`.fmod` module files, or a `.zip` containing them) into native SHOBEATS presets.

- **How:** PRESETS window → **IMPORT…** → choose a `.zip`, `.fwd` or `.fmod` file.
- **Where they go:** `%APPDATA%\SHOBEATS\SHOBEATS FX\Imported\` (macOS/Linux: the user application-data
  folder), separate from the 60 factory presets and from your own saved presets (`…\Presets\`).
  They persist across restarts and are listed under **IMPORTED**, keeping the archive's folders
  (e.g. *Signature FX*, *Creative FX / Ambient*, *Basic FX / Chorus*).
- **Duplicates:** importing the same archive again skips every file already imported
  (same path and same content). The same file stored in two folders is kept in both.
- **Report:** each import writes `import-report-<date>.txt` in the Imported folder listing every
  preset as CONVERTED / PARTIAL / FAILED with the reasons.
- **The original plugin is not needed.** Only the preset files are read.
- **Rights:** the converter runs on your own computer from your own files. No third-party presets
  are included in this repository or in SHOBEATS FX builds.

## What "converted" and "partial" mean

The conversion maps each source effect onto the closest SHOBEATS pedal. **Converted presets
approximate the original sound; they are not identical** – the algorithms and parameter units
differ, and no audio comparison with the original plugin has been made.

- **Converted** – every active source effect was mapped; only approximations were needed.
- **Partial** – the preset uses at least one source feature that SHOBEATS cannot reproduce
  (listed in the preset's notes and in the report, e.g. *"Missing: Pitch shift (-4.0 st)"*).
- **Failed** – the file is not a supported preset (e.g. not XML).

## File format notes

| Element | Meaning |
| --- | --- |
| `<SonixAudio product="TONEFLX">` / `<FFWD product="FFWD FX">` | Preset roots (versions 0.8.1 – 1.7.0 seen) |
| `<Core0_<Module> …attributes…>` | Single-module snippet (`.fwd` / `.fmod`) |
| `<FXGraph><Module id="…"/>…` | Processing order. Missing in some files → default order (reported) |
| `<PARAM id="Core0_<Module>_<Param>" value="…"/>` | Parameter values |
| `<PARAM id="…"/>` (no value) | FFWD FX omits values left at default. Defaults are not published, so the most common explicit value in a 650-preset sample is assumed (reported) |
| `<GlobalParameters Global_InputGain Global_OutputGain …>` | Global gain in dB, macros |
| `Core0_Tape_Tape*` | Older TONEFLX tape parameter names (treated as aliases) |

A module whose `…_Active` switch has no value is treated as **off**: in the sample, every such
module was also completely untouched, so switching it on would add an effect the author did not set up.

## Parameter mapping

Evidence for note values comes from presets named after their setting (e.g. "1-8TH DOTTED").

| Source | SHOBEATS | Notes |
| --- | --- | --- |
| Note division 0…6, timing 0/1/2 | 1/64 · 1/32 · 1/16 · 1/8 · 1/4 · 1/2 · 1/1, straight / triplet / dotted | Nearest available value when SHOBEATS lacks it (reported) |
| Distortion Drive 1–35 | Saturation Drive 20·log10(x) dB | Linear gain (minimum 1.0 = unity) |
| Distortion Tone, Mix | Tone, Mix | |
| Distortion Mode 0/1 | Soft / Hard | Best guess, reported |
| Distortion Frequency | – | Missing when not at 500 Hz |
| Resampler SampleRate, Jitter | Rate (≥ 500 Hz), Jitter | Bits = 24 (source has no bit depth) |
| Resampler PostFilter | Filter = 0.45 × rate, or open | PreFilter not available (reported); Drive missing |
| Tape Range × (1−Balance) / × Balance | Wow / Flutter (×2.5) | Approximate; rates fixed in SHOBEATS |
| Tape NoiseGain dB, NoiseMode | Noise (mode 0 = off) | Noise types approximated |
| Tape LPF, DropOutDepth | Age | Approximate |
| Tape HPF, Flux, Scrape | – | Missing when used |
| Chorus Rate, Depth, Mix, Width, Stereo | Rate (≥ 0.05 Hz), Depth, Mix, Width | Saturation, Clock missing; alt. waveform approximated |
| Modulation (tremolo) Depth, Rate, Sync, division, Stereo, Shape | Tremolo | Shape → Triangle / Sine / Square; Mode not interpreted |
| ReverseDelay division, Pitch (+PitchActive), Mix | Reverse (synced) | Feedback, filters, Type missing |
| Delay Sync, division, Time × Adjust, Feedback, HPF, LPF, Mix | Delay | Feedback ≤ 90 %, time ≤ 2 s, high-cut ≥ 1 kHz; Type, Freeze, modulation, saturation, Clock missing |
| Reverb Decay, Size, PreDelay, Mix | Reverb | Decay ≤ 20 s |
| Reverb HighDamp (Hz) | Damping | Inverse of the SHOBEATS damping curve; LowDamp, Blend, saturation missing; Type approximated |
| Per-module Drift / Instability | Global Drift / Unstable (maximum of the active modules) | Approximate |
| Global Input / Output gain (dB) | Input / Output | Output ≤ +6 dB |
| Pitch, Shifter, EQ, Width, LFO matrix (MOD1/MOD2), Macros, Mixer | – | Missing when used |

### Level safety

Measured on a 650-preset sample, two cases came out louder in SHOBEATS than intended, so the
converter trims them (reported in the notes): positive Tone and Hard mode at very high drive on
the saturation (Saturation Output reduced), and two or more effects fully wet (Output −3 dB).
Loading any preset also uses the click-free fade-out / swap / fade-in transition.
