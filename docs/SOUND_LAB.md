# SHOBEATS SOUND LAB

The Sound Lab creates **original** SHOBEATS FX presets with documented rules, checks them
automatically and lets you curate them in the plugin. Generated presets are
**technically validated, not artistically approved**: a person still has to listen.

## What ships

| Category | Generated | Accepted | Duplicates | Too close to reference |
| --- | ---: | ---: | ---: | ---: |
| Dark & Cinematic | 50 | 48 | 2 | 0 |
| Melody Transformations | 50 | 46 | 4 | 0 |
| Vintage & Analog | 50 | 32 | 4 | 14 |
| Dreamy & Atmospheric | 50 | 46 | 4 | 0 |
| Experimental & Glitch | 50 | 43 | 7 | 0 |
| Trap & R&B Textures | 50 | 32 | 10 | 8 |
| Rhythmic & Reverse | 50 | 47 | 3 | 0 |
| Space & Ambient | 50 | 47 | 3 | 0 |
| Drums & Percussion | 50 | 18 | 27 | 5 |
| Original Wildcards | 50 | 50 | 0 | 0 |
| **Total** | **500** | **409** | **64** | **27** |

No candidate failed the technical checks in the shipped run (generator version 1, base seed
2026, 5 batches × 100). `presets/lab/manifest.csv` / `manifest.json` list **all 500** candidates
with their status, rejection reason, seed, architecture, chain and QC metrics. The accepted
ones are in `presets/lab/NN_<category>.json` and compiled into the plugin (browser group
*SHOBEATS LAB*), separately from the 60 factory presets.

Drums & Percussion has the fewest accepted candidates. Its architectures are short chains
with narrow, mix-safe ranges, so many variants sound alike, and the duplicate filter
removes them.

## How a candidate is built (`source/presets/SoundLab.cpp`)

1. **Category → architecture.** Each category has five or six hand-written *architectures*.
   An architecture fixes:
   - a pedal chain **in a deliberate order**, often a non-canonical one;
   - a musical purpose;
   - suggested instruments;
   - parameter rules.

   Examples:

   | Architecture | Chain | Why this order |
   | --- | --- | --- |
   | Sub-Octave Shadow | octave-down reverse → dark drive → long damped hall | The reversed octave is distorted before it reaches the hall. |
   | Gated Hall | reverb → square tremolo | The reverb tail itself is gated. |
   | Reverse Echo Ladder | delay → reverse | Every repeat plays backwards. |
   | Inside-Out Room | reverb → wavefolder → echoes | The room is distorted, not the source. |
   | Tape After Space | delay → reverb → aged tape | The whole space is printed to worn tape. |
   | Chopped Reverser | gate → reverse | Chopped envelopes swell in backwards. |

2. **Parameter rules.** Values come from musically chosen ranges per architecture. Examples:
   - tempo-synced divisions;
   - reverse pitch from {0, ±12, +7, −5, −7};
   - dark tone with heavy drive;
   - log-scaled rates and frequencies.
3. **Variation axes.** These make variants differ in character, not only numerically:
   - **Intensity**: subtle (×0.65), as designed, or bold (×1.3) on every mix or depth control.
   - **Colour pedal** (45 % chance): tape in front, chorus before the last pedal, saturation in front, or a room or light crush at the end. The 808 architecture never gets one, to protect its low end.
4. **Coupling rules.** These apply to every candidate:
   - **Long reverbs:** decay > 8 s → reverb mix ≤ 0.6 and damping ≥ 0.3. Decay > 6 s → size ≥ 0.6.
   - **Delay:** feedback > 0.6 → high cut ≤ 6.5 kHz and delay mix ≤ 0.45.
   - **Chorus:** feedback > 0.6 → chorus mix ≤ 0.55.
   - **Saturation:**
     - drive > 18 dB → saturation mix ≤ 0.85;
     - a bright tone gets a negative output trim;
     - Hard/Fuzz above 12 dB drive gets up to −4 dB of trim.
5. **Movement.** Drift and Unstable ranges are set per architecture. For example, wide on Glitch, near zero on Drums.
6. **Names** are drawn from category-specific word lists (adjective + noun). They are unique across the whole run.
7. **Reproducibility.** The seed is a hash of (base seed, batch, category, slot). Each preset stores:
   - `lab.seed`, `lab.generatorVersion`, `lab.architecture`;
   - `lab.purpose`, `lab.instruments`, `lab.chainSummary`;
   - `lab.fingerprint` and `lab.qc`.

   `generate(category, seed)` rebuilds the same preset. A test checks this against the shipped banks.

These rules are SHOBEATS's own. They were informed only by **aggregate statistics** from a
private, local reference library, never by individual presets:

- about 90 % of reference presets use the default pedal order;
- some pedal pairs are rarely combined;
- the typical delay division, reverb decay and feedback distributions.

No reference preset is copied, renamed or varied. Features SHOBEATS does not have, such as
LFO matrices and extra delay modes, are not imitated.

## Quality control (`qualityCheck`)

Each candidate is rendered through the real `ShobeatsAudioProcessor`, offline, at 48 kHz.
The test signal is a decaying chord plus drum-like hits, followed by silence.

1. **Parameter validity.** Every ID must exist, numbers must be in range, and choices must be valid names.
2. **Non-finite or silent output:** rejected. Silent means more than 30 dB below the input.
3. **Level match.** `outputGain` is set to cancel the level change, from −12 to +6 dB in 0.5 dB steps, and capped so the peak stays ≤ −1.5 dBFS. The candidate is then rendered again.
4. **After matching**, it is rejected for any of these:
   - the peak is above −1 dBFS;
   - the level change is outside −6…+3 dB;
   - the tail grows by more than 1 dB (runaway feedback). This is confirmed on a 6-second tail first, so long reverse windows are not flagged by mistake;
   - the CPU real-time factor of one instance is above 0.25.
5. **Duplicates.** Each candidate is compared with every accepted one using two measures:
   - **structural distance** (0–1): 45 % effect set, 15 % pair order, 40 % normalised values of the parameters the presets set;
   - **audio distance**: the mean difference, in dB, of per-50 ms level, stereo-side and brightness features.

   It is a duplicate if the structure is < 0.02, or if the structure is < 0.20 **and** the audio is < 1.5 dB.
6. **Reference similarity.** This runs only when the tool is given `--reference <folder>`, a
   private local library that is never written anywhere. A candidate is too close if the
   structure is < 0.05, or if the structure is < 0.25 **and** the audio is < 1.5 dB.

## Curation in the plugin

Open **PRESETS**. The groups are:

- ALL
- FACTORY
- SHOBEATS LAB
- USER
- IMPORTED
- APPROVED
- FAVORITES

It also has a sub-category menu, search (names, descriptions, categories, creators, instruments)
and a rating filter. Each row has 5 stars (click the same star again to clear it), a heart
(favourite) and × (hide/reject).

- Hidden presets disappear from the list and from **< >** navigation. Tick *Show hidden* to see them again.
- Ctrl/Cmd/Shift-click selects several presets. Ctrl/Cmd+A selects everything shown.
  - **Export…** writes a `.zip` with one `.sfxpreset` per preset and a `collection.json` with ratings.
  - **Approve** copies the selection into `%APPDATA%\SHOBEATS\SHOBEATS FX\Approved`, listed under *APPROVED*.
- Ratings, favourites and hidden flags are saved in `%APPDATA%\SHOBEATS\SHOBEATS FX\curation.json`, with each preset's fingerprint.

## Generating new batches (`tools/sound_lab`)

```
cmake --build build --target shobeats_sound_lab
build/tools/sound_lab/shobeats_sound_lab generate --out presets/lab --batches 5 --per-category 10 --seed 2026 [--reference <private folder>]
```

- The tool is offline and single-threaded, and never part of the audio callback.
- The same arguments and generator version give byte-identical banks. The CPU timings in the manifest vary by machine.
- Presets you hid in the browser are excluded by fingerprint: the tool reads `curation.json`, or `--exclude <file>`. Use `--include-rejected` to allow them again.
- A rebuild compiles the new banks into the plugin.

Change the rules in `SoundLab.cpp` and bump `generatorVersion` so older seeds stay meaningful.
