#pragma once

#include "DspCommon.h"

#include <array>

namespace trio::dsp
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

    struct Parameters
    {
        float rateHz = 0.8f;
        float depth = 0.5f;    // 0..1, scales the modulation swing
        float delayMs = 12.0f; // centre delay of the voices
        float mix = 0.5f;
        bool enabled = true;
    };

    void prepare (double sampleRate, int numChannels);
    void reset() noexcept;
    void setParameters (const Parameters& p) noexcept;

    void process (float* const* channels, int numChannels, int numSamples) noexcept;

private:
    static constexpr int maxChannels = 2;
    static constexpr int voicesPerChannel = 2;

    std::array<DelayLine, maxChannels> delays;
    double sampleRate = 44100.0;
    float lfoPhase = 0.0f;
    bool firstUpdate = true;

    Smoother rate, depthMs, delaySamples, mix;
};
} // namespace trio::dsp
