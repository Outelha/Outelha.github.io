#pragma once

#include "DspCommon.h"

#include <array>

namespace trio::dsp
{
// Waveshaping saturator with drive, tilt tone control, automatic level
// compensation and a phase-coherent dry/wet mix.
//
// The processor runs this at an oversampled rate; it has no latency itself.
class Saturation
{
public:
    enum class Mode
    {
        soft = 0, // symmetric tanh: smooth, tape-like, odd harmonics
        warm,     // biased tanh: asymmetric, tube-like, adds even harmonics
        hard      // hard clip: aggressive, bright
    };

    static constexpr int numModes = 3;
    static constexpr float minDriveDb = 0.0f;
    static constexpr float maxDriveDb = 36.0f;

    struct Parameters
    {
        float driveDb = 12.0f; // input gain into the shaper
        float tone = 0.0f;     // -1 (dark) .. +1 (bright), tilt around 1 kHz
        float mix = 1.0f;      // 0 = dry, 1 = fully saturated
        Mode mode = Mode::soft;
        bool enabled = true;
    };

    // Raw transfer function (no drive, no compensation). Exposed for the UI curve display.
    static float shape (Mode mode, float x) noexcept;

    // Static gain that keeps a -12 dBFS sine at roughly the same peak level for a given drive.
    static float compensationGain (Mode mode, float driveGain) noexcept;

    void prepare (double sampleRate, int numChannels);
    void reset() noexcept;
    void setParameters (const Parameters& p) noexcept;

    void process (float* const* channels, int numChannels, int numSamples) noexcept;

private:
    static constexpr int maxChannels = 2;

    struct ChannelState
    {
        DcBlocker dc;
        OnePoleLowpass toneSplit;
    };

    std::array<ChannelState, maxChannels> state {};
    double sampleRate = 44100.0;

    Parameters params;
    Mode previousMode = Mode::soft;
    int modeFadeRemaining = 0, modeFadeLength = 1;

    Smoother driveGain, compGain, mix, lowGain, highGain;
    bool firstUpdate = true;
};
} // namespace trio::dsp
