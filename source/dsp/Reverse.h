#pragma once

#include "DspCommon.h"

#include <array>

namespace sfx::dsp
{
// Reverse effect: two overlapping read heads play the most recent audio
// backwards in windowed segments ("grains"). Pitch changes the backwards
// playback speed. Mix blends forward (dry) and reversed audio.
class Reverse
{
public:
    static constexpr float minTimeMs = 80.0f, maxTimeMs = 2000.0f;
    static constexpr float minPitch = -12.0f, maxPitch = 12.0f;

    struct Parameters
    {
        float timeMs = 500.0f;
        bool sync = false;
        int division = 4; // 1/4
        float pitchSemitones = 0.0f;
        float mix = 0.5f;
        bool enabled = true;
    };

    void prepare (double sampleRate, int numChannels);
    void reset() noexcept;
    void setParameters (const Parameters& p, double bpm) noexcept;
    void process (float* const* channels, int numChannels, int numSamples) noexcept;

    // Global Drift detunes each segment slightly; Unstable varies segment lengths.
    void setMovement (Movement m) noexcept
    {
        movement.drift = std::clamp (m.drift, 0.0f, 1.0f);
        movement.unstable = std::clamp (m.unstable, 0.0f, 1.0f);
    }

    // Exact segment length currently used (samples), for tests.
    float getSegmentSamples() const noexcept { return targetSegment; }

private:
    static constexpr int maxChannels = 2;
    static constexpr int numHeads = 2;

    struct Head
    {
        float t = 0.0f;      // position inside the current segment (samples)
        float length = 1.0f; // segment length latched at segment start
        float rate = 1.0f;   // backwards speed latched at segment start
    };

    double sampleRate = 44100.0;
    bool firstUpdate = true;
    bool isIdle = false;
    std::array<DelayLine, maxChannels> buffers;
    std::array<Head, numHeads> heads {};
    float targetSegment = 1.0f, targetRate = 1.0f;
    Smoother mix;
    Movement movement;
    Noise noise { 0x4e7e45u };
};
} // namespace sfx::dsp
