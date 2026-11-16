#pragma once

#include "Preset.h"

namespace sfx
{
// Offline rendering and measurement of a preset through the real processor.
// Used by the import tool (--audio-check), the Sound Lab quality control and tests.
// Never used on the audio thread.
struct AudioMetrics
{
    bool finite = true;
    float peakDb = -200.0f;        // output peak, dBFS
    double levelChangeDb = 0.0;    // output RMS vs input RMS over the signal part
    double tailGrowthDb = 0.0;     // last tail window vs first tail window (positive = growing = unstable)
    double realtimeFactor = 0.0;   // processing time / audio time (single instance, this machine)
    std::vector<float> features;   // per-50 ms frame: level L, level R, side, brightness (dB)
};

namespace audit
{
inline constexpr double sampleRate = 48000.0;
inline constexpr int blockSize = 512;

// Decaying chord + drum-like noise hits for 'signalSeconds', peaking near -6 dBFS, then silence.
juce::AudioBuffer<float> testSignal (double signalSeconds = 2.0, double totalSeconds = 3.0);

// Applies the preset to a fresh processor instance, renders the buffer and measures it.
AudioMetrics measure (const Preset& preset, const juce::AudioBuffer<float>& input, double signalSeconds = 2.0);

// Mean absolute difference between two feature vectors (dB).
double featureDistance (const std::vector<float>& a, const std::vector<float>& b);
} // namespace audit
} // namespace sfx
