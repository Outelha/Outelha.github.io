#pragma once

#include "PedalArt.h"
#include "VisualFeed.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace sfx::ui
{
// Keeps one pedal's painter state in step with the plugin: parameters (every frame),
// the output level and whether audio is being processed (from the VisualFeed), and the
// animation phases that depend on them (reels, LFO windows).
class PedalLive
{
public:
    PedalLive (const juce::AudioProcessorValueTreeState& state, EffectId id, const VisualFeed& feed);

    // Fills `out` from the current parameter values only (no animation).
    static void readParameters (const juce::AudioProcessorValueTreeState&, EffectId, pedal::State& out);

    // Advances one frame. Returns true when the pedal must be repainted; `animating` is set
    // while something moves on its own (so the frame clock keeps full rate).
    bool advance (double dt, bool& animating);

    const pedal::State& state() const noexcept { return shown; }

private:
    const juce::AudioProcessorValueTreeState& parameters;
    EffectId effect;
    const VisualFeed& feed;
    pedal::State shown;
    double animTime = 0.0;
    float reelPhase = 0.0f, reelSpeed = 0.0f, level = 0.0f;
    std::uint64_t lastSamples = 0;
};
} // namespace sfx::ui
