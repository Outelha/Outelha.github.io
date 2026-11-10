#pragma once

#include "DspCommon.h"

#include <array>

namespace sfx::dsp
{
// Stereo two-voice chorus. Each channel has two modulated delay taps driven by
// sine LFOs in quadrature across channels, which gives a wide image even from
// a mono source.
class Chorus
{
public:
    static constexpr float minRateHz = 0.05f, maxRateHz = 5.0f;
    static constexpr float minDelayMs = 5.0f, maxDelayMs = 30.0f;
    static constexpr float maxModulationMs = 5.0f;
    static constexpr float maxFeedback = 0.9f;

    struct Parameters
    {
        float rateHz = 0.8f;
        float depth = 0.5f;    // 0..1, scales the modulation swing
        float delayMs = 12.0f; // centre delay of the voices
        float feedback = 0.0f; // 0..0.9, wet signal fed back into the delay line
        float width = 1.0f;    // 0 = mono wet, 1 = full stereo
        float mix = 0.5f;
        bool enabled = true;
    };

    void prepare (double sampleRate, int numChannels);
    void reset() noexcept;
    void setParameters (const Parameters& p) noexcept;

    void process (float* const* channels, int numChannels, int numSamples) noexcept;

    // Global Drift wanders rate and centre delay; Unstable adds irregular voice jitter.
    void setMovement (Movement m) noexcept
    {
        drift.setTarget (std::clamp (m.drift, 0.0f, 1.0f));
        instability.setTarget (std::clamp (m.unstable, 0.0f, 1.0f));
    }

private:
    static constexpr int maxChannels = 2;
    static constexpr int voicesPerChannel = 2;

    std::array<DelayLine, maxChannels> delays;
    double sampleRate = 44100.0;
    float lfoPhase = 0.0f;
    bool firstUpdate = true;

    Smoother rate, depthMs, delaySamples, mix, feedback, width, drift, instability;
    RandomDrift rateWander, delayWander, jitterL, jitterR;
    std::array<float, maxChannels> lastWet {};
};
} // namespace sfx::dsp
