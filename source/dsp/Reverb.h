#pragma once

#include "DspCommon.h"

#include <array>

namespace trio::dsp
{
// Algorithmic stereo reverb: pre-delay -> input allpass diffusion -> 8-line
// feedback delay network (Householder mixing, per-line damping and gentle
// delay modulation) -> width control.
class Reverb
{
public:
    static constexpr float minDecaySeconds = 0.2f, maxDecaySeconds = 20.0f;
    static constexpr float maxPredelayMs = 250.0f;

    struct Parameters
    {
        float size = 0.6f;          // 0..1, scales the delay network
        float decaySeconds = 2.5f;  // RT60
        float damping = 0.4f;       // 0 = bright, 1 = dark
        float predelayMs = 20.0f;
        float width = 1.0f;         // 0 = mono, 1 = full stereo
        float mix = 0.3f;
        bool enabled = true;
    };

    void prepare (double sampleRate, int numChannels);
    void reset() noexcept;
    void setParameters (const Parameters& p) noexcept;

    void process (float* const* channels, int numChannels, int numSamples) noexcept;

    static constexpr int numLines = 8;

private:
    static constexpr int numDiffusers = 4;
    static constexpr float minSizeScale = 0.35f, maxSizeScale = 1.6f;

    struct Allpass
    {
        DelayLine line;
        std::size_t length = 1;
        float process (float x) noexcept
        {
            constexpr float g = 0.6f;
            const auto delayed = line.readInteger (length - 1);
            const auto v = x + g * delayed;
            line.push (v);
            return delayed - g * v;
        }
    };

    void updateLoopGains() noexcept;
    float sizeToScale (float size) const noexcept;

    double sampleRate = 44100.0;
    bool firstUpdate = true;
    bool isIdle = false;
    Parameters params;

    std::array<DelayLine, 2> predelay;
    std::array<std::array<Allpass, numDiffusers>, 2> diffusers;

    std::array<DelayLine, numLines> lines;
    std::array<OnePoleLowpass, numLines> damping;
    std::array<float, numLines> lineGain {};
    std::array<float, numLines> modPhase {};
    std::array<Smoother, numLines> lineLength;

    Smoother predelaySamples, mix, width;
};
} // namespace trio::dsp
