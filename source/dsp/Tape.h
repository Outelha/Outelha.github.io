#pragma once

#include "DspCommon.h"

#include <array>

namespace sfx::dsp
{
// Tape machine emulation: asymmetric magnetic saturation, wow (slow pitch
// drift) and flutter (fast pitch wobble) via a modulated delay, hiss, and
// "age" degradation (high-frequency loss plus random level dropouts).
class Tape
{
public:
    static constexpr float maxDriveDb = 24.0f;
    static constexpr float maxWowMs = 3.0f;
    static constexpr float maxFlutterMs = 0.3f;

    struct Parameters
    {
        float driveDb = 6.0f;
        float wow = 0.3f;     // 0..1
        float flutter = 0.2f; // 0..1
        float noise = 0.0f;   // 0..1, hiss level up to about -30 dBFS
        float age = 0.2f;     // 0..1 degradation
        float mix = 1.0f;
        bool enabled = true;
    };

    void prepare (double sampleRate, int numChannels);
    void reset() noexcept;
    void setParameters (const Parameters& p) noexcept;
    void process (float* const* channels, int numChannels, int numSamples) noexcept;

    // Extra pitch/time instability added by the global Unstable control (0..1).
    void setInstability (float amount) noexcept { instability.setTarget (std::clamp (amount, 0.0f, 1.0f)); }

private:
    static constexpr int maxChannels = 2;
    static constexpr int coefficientInterval = 32;

    double sampleRate = 44100.0;
    bool firstUpdate = true;

    std::array<DelayLine, maxChannels> delays;
    std::array<DcBlocker, maxChannels> dc;
    std::array<Svf, maxChannels> headLoss;
    std::array<OnePoleLowpass, maxChannels> hissFilter;
    std::array<Noise, maxChannels> hiss { Noise { 0x1234567u }, Noise { 0x7654321u } };
    RandomDrift wowDrift, flutterDrift, dropout;

    float wowPhase = 0.0f, flutterPhase = 0.0f;
    Smoother drive, comp, wow, flutter, noiseLevel, age, mix, instability, baseDelay;
};
} // namespace sfx::dsp
