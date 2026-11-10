#pragma once

#include "DspCommon.h"

namespace sfx::dsp
{
// Amplitude modulation with selectable LFO shape, stereo phase offset (auto-pan
// at full stereo) and optional tempo sync.
class Tremolo
{
public:
    enum class Shape
    {
        sine = 0,
        triangle,
        square,
        saw
    };
    static constexpr int numShapes = 4;
    static constexpr float minRateHz = 0.1f, maxRateHz = 20.0f;

    struct Parameters
    {
        float rateHz = 4.0f;
        bool sync = false;
        int division = 7; // index into divisionBeats (1/8 by default)
        float depth = 0.6f;
        Shape shape = Shape::sine;
        float stereo = 0.0f; // 0 = mono tremolo, 1 = right channel 180 degrees out (auto-pan)
        bool enabled = true;
    };

    // LFO value in [0, 1] for a phase in [0, 1). Exposed for the UI display.
    static float shapeValue (Shape shape, float phase) noexcept;

    void prepare (double sampleRate, int numChannels);
    void reset() noexcept;
    void setParameters (const Parameters& p, double bpm) noexcept;
    void process (float* const* channels, int numChannels, int numSamples) noexcept;

    // Global Drift wanders the rate; Unstable makes the depth breathe irregularly.
    void setMovement (Movement m) noexcept
    {
        drift.setTarget (std::clamp (m.drift, 0.0f, 1.0f));
        instability.setTarget (std::clamp (m.unstable, 0.0f, 1.0f));
    }

private:
    double sampleRate = 44100.0;
    bool firstUpdate = true;
    Shape shape = Shape::sine, previousShape = Shape::sine;
    int shapeFade = 0, shapeFadeLength = 1;
    float phase = 0.0f;
    Smoother rate, depth, stereo, drift, instability;
    RandomDrift rateWander, depthFlicker;
};
} // namespace sfx::dsp
