#pragma once

#include "DspCommon.h"

#include <array>

namespace sfx::dsp
{
// Stereo delay with tempo sync, filtered and gently saturated feedback,
// ping-pong mode and stereo width.
class Delay
{
public:
    static constexpr float minTimeMs = 1.0f, maxTimeMs = 2000.0f;
    static constexpr float maxFeedback = 0.95f;

    struct Parameters
    {
        float timeMs = 375.0f;
        bool sync = true;
        int division = 7; // 1/8
        float feedback = 0.4f;
        float lowCutHz = 120.0f;
        float highCutHz = 8000.0f;
        bool pingPong = false;
        float width = 1.0f;
        float mix = 0.3f;
        bool enabled = true;
    };

    void prepare (double sampleRate, int numChannels);
    void reset() noexcept;
    void setParameters (const Parameters& p, double bpm) noexcept;
    void process (float* const* channels, int numChannels, int numSamples) noexcept;

    void setInstability (float amount) noexcept { instability.setTarget (std::clamp (amount, 0.0f, 1.0f)); }

private:
    static constexpr int coefficientInterval = 32;

    double sampleRate = 44100.0;
    bool firstUpdate = true;
    bool isIdle = false;
    bool pingPong = false;

    std::array<DelayLine, 2> lines;
    std::array<Svf, 2> lowCut, highCut;
    RandomDrift wobble;
    Smoother time, feedback, lowCutHz, highCutHz, width, mix, instability;
};
} // namespace sfx::dsp
