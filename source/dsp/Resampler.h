#pragma once

#include "DspCommon.h"

#include <array>

namespace sfx::dsp
{
// Lo-fi resampler: sample-and-hold rate reduction with clock jitter, bit-depth
// quantisation and a resonant low-pass "reconstruction" filter.
class Resampler
{
public:
    static constexpr float minRateHz = 500.0f, maxRateHz = 48000.0f;
    static constexpr float minBits = 2.0f, maxBits = 24.0f;
    static constexpr float minFilterHz = 200.0f, maxFilterHz = 20000.0f;

    struct Parameters
    {
        float rateHz = 11025.0f; // target sample rate
        float bits = 10.0f;      // quantiser resolution (fractional values morph smoothly)
        float filterHz = 8000.0f;// post low-pass cutoff
        float jitter = 0.0f;     // 0..1 random variation of the sample clock
        float mix = 1.0f;
        bool enabled = true;
    };

    void prepare (double sampleRate, int numChannels);
    void reset() noexcept;
    void setParameters (const Parameters& p) noexcept;
    void process (float* const* channels, int numChannels, int numSamples) noexcept;

private:
    static constexpr int maxChannels = 2;
    static constexpr int coefficientInterval = 16;

    double sampleRate = 44100.0;
    bool firstUpdate = true;
    float phase = 1.0f;
    float clockIncrement = 1.0f;
    std::array<float, maxChannels> held {};
    std::array<Svf, maxChannels> filters;
    Noise noise { 0x51f15eedu };

    Smoother rate, bits, filterHz, jitter, mix;
};
} // namespace sfx::dsp
