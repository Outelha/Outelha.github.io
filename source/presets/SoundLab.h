#pragma once

#include "Preset.h"
#include "PresetAudit.h"

#include <juce_audio_processors/juce_audio_processors.h>

// SHOBEATS SOUND LAB - rule-based generation of original preset candidates.
//
// Offline / message-thread code only (used by tools/sound_lab and tests); it never runs
// in the audio callback. Every candidate is reproducible from (category, seed) and the
// generator version. The rules are documented in docs/SOUND_LAB.md.
namespace sfx::lab
{
inline constexpr int generatorVersion = 1;
inline constexpr int numCategories = 10;

const juce::StringArray& categoryNames();

struct Candidate
{
    Preset preset;
    juce::String id;           // e.g. "B2-C04-07"
    juce::String architecture; // architecture id
    juce::String purpose;      // musical purpose
    juce::String instruments;  // suggested sources
    int category = 0;
    int batch = 0;
    juce::uint32 seed = 0;
};

// One candidate for a category; deterministic for (category, seed).
Candidate generate (int category, juce::uint32 seed);

// A batch: 'perCategory' candidates in every category. Seeds derive from (baseSeed, batch,
// category, slot). Names are unique against 'taken' (lower-case), which is updated.
std::vector<Candidate> generateBatch (int batch, int perCategory, juce::uint32 baseSeed, juce::StringArray* taken = nullptr);

// Stable fingerprint of a preset's sound settings (chain + quantised parameters).
juce::String fingerprint (const Preset&);

// Structural distance between two presets in [0, 1]: active-effect set, order and
// normalised parameter values (ranges taken from the plugin's parameter layout).
double structuralDistance (const Preset& a, const Preset& b, juce::AudioProcessorValueTreeState& ranges);

// Unique original name for the category; 'taken' names (lower-case) are avoided.
juce::String makeName (int category, juce::uint32 seed, const juce::StringArray& taken);

// --- Quality control -------------------------------------------------------------
// Thresholds (documented in docs/SOUND_LAB.md).
inline constexpr double maxPeakDb = -1.0;          // after level matching
inline constexpr double silentChangeDb = -30.0;    // output this far below input = silent/broken
inline constexpr double minLevelChangeDb = -6.0;   // after level matching
inline constexpr double maxLevelChangeDb = 3.0;
inline constexpr double maxTailGrowthDb = 1.0;     // a growing tail = runaway feedback
inline constexpr double maxRealtimeFactor = 0.25;  // CPU: one instance, offline measurement
inline constexpr double minOutputGainDb = -12.0, maxOutputGainDb = 6.0;
// Similarity (structural distance in [0,1], audio feature distance in dB).
// Calibrated on generated batches: two variants of one architecture typically measure
// 0.02-0.10 structurally while sounding 1-13 dB apart, so the audio test decides; settings
// that are near-identical are rejected whatever they sound like.
inline constexpr double duplicateStructural = 0.02, duplicateStructuralWithAudio = 0.20, duplicateAudioDb = 1.5;
inline constexpr double referenceStructural = 0.05, referenceStructuralWithAudio = 0.25, referenceAudioDb = 1.5;

// Checks every parameter exists and lies inside its range (choices by name). Empty = valid.
juce::String validateParameters (const Preset&, juce::AudioProcessorValueTreeState& ranges);

struct QcResult
{
    bool passed = false;
    juce::String reason;   // first failed check
    AudioMetrics metrics;  // after level matching
    double outputGainDb = 0.0;
};

// Renders the candidate, level-matches it through outputGain (written into the preset),
// re-renders and applies the technical checks above.
QcResult qualityCheck (Preset& preset, const juce::AudioBuffer<float>& input, juce::AudioProcessorValueTreeState& ranges);

// True when two candidates are too similar to keep both.
bool isDuplicate (double structural, double audioDb);
bool isTooCloseToReference (double structural, double audioDb);

// Human-readable summary of the chain built from the actual values.
juce::String describeChain (const Preset&);
} // namespace sfx::lab
