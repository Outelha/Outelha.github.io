#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

// Small, dependency-free DSP building blocks shared by the effect modules.
// Everything here is real-time safe once prepared: no allocation, no locks.
namespace sfx::dsp
{
constexpr float pi = 3.14159265358979323846f;
constexpr float twoPi = 2.0f * pi;

inline float dbToGain (float db) noexcept { return std::pow (10.0f, db * 0.05f); }
inline float gainToDb (float g) noexcept { return 20.0f * std::log10 (std::max (g, 1.0e-9f)); }

// Exponential (one-pole) parameter smoother.
class Smoother
{
public:
    void prepare (double sampleRate, float timeMs) noexcept
    {
        const auto samples = std::max (1.0, sampleRate * static_cast<double> (timeMs) * 0.001);
        coeff = static_cast<float> (std::exp (-1.0 / samples));
    }

    void setTarget (float t) noexcept { target = t; }
    void snap (float v) noexcept { current = target = v; }
    void snapToTarget() noexcept { current = target; }
    float getTarget() const noexcept { return target; }
    float getCurrent() const noexcept { return current; }

    float next() noexcept
    {
        current = target + coeff * (current - target);
        return current;
    }

private:
    float coeff = 0.0f, current = 0.0f, target = 0.0f;
};

// One-pole low-pass, used for tone/damping.
class OnePoleLowpass
{
public:
    void setCutoff (float hz, double sampleRate) noexcept
    {
        const auto x = std::exp (-twoPi * hz / static_cast<float> (sampleRate));
        a = 1.0f - x;
    }
    void setCoefficient (float newA) noexcept { a = newA; }
    void reset() noexcept { z = 0.0f; }
    float getState() const noexcept { return z; }
    float process (float x) noexcept
    {
        z += a * (x - z);
        return z;
    }

private:
    float a = 1.0f, z = 0.0f;
};

// First-order DC blocking filter.
class DcBlocker
{
public:
    void prepare (double sampleRate) noexcept
    {
        r = static_cast<float> (std::exp (-twoPi * 10.0 / sampleRate)); // ~10 Hz corner
    }
    void reset() noexcept { x1 = y1 = 0.0f; }
    float process (float x) noexcept
    {
        const auto y = x - x1 + r * y1;
        x1 = x;
        y1 = y;
        return y;
    }

private:
    float r = 0.995f, x1 = 0.0f, y1 = 0.0f;
};

// Circular delay line with fractional (cubic Hermite / linear) reads.
class DelayLine
{
public:
    void prepare (std::size_t maxDelaySamples)
    {
        // +4 guard samples for interpolation.
        buffer.assign (maxDelaySamples + 4, 0.0f);
        writePos = 0;
    }

    void reset() noexcept
    {
        std::fill (buffer.begin(), buffer.end(), 0.0f);
        writePos = 0;
    }

    std::size_t capacity() const noexcept { return buffer.size() - 4; }

    void push (float x) noexcept
    {
        buffer[writePos] = x;
        if (++writePos == buffer.size())
            writePos = 0;
    }

    // Delay is measured relative to the most recently pushed sample (delay 0 == last sample).
    float readInteger (std::size_t delay) const noexcept
    {
        const auto size = buffer.size();
        auto idx = writePos + size - 1 - std::min (delay, capacity());
        if (idx >= size)
            idx -= size;
        return buffer[idx];
    }

    float readLinear (float delay) const noexcept
    {
        delay = std::clamp (delay, 0.0f, static_cast<float> (capacity() - 1));
        const auto i = static_cast<std::size_t> (delay);
        const auto f = delay - static_cast<float> (i);
        const auto a = readInteger (i);
        const auto b = readInteger (i + 1);
        return a + f * (b - a);
    }

    float readCubic (float delay) const noexcept
    {
        delay = std::clamp (delay, 1.0f, static_cast<float> (capacity() - 2));
        const auto i = static_cast<std::size_t> (delay);
        const auto f = delay - static_cast<float> (i);
        const auto xm1 = readInteger (i - 1);
        const auto x0 = readInteger (i);
        const auto x1 = readInteger (i + 1);
        const auto x2 = readInteger (i + 2);
        // 4-point, 3rd-order Hermite.
        const auto c1 = 0.5f * (x1 - xm1);
        const auto c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const auto c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * f + c2) * f + c1) * f + x0;
    }

private:
    std::vector<float> buffer;
    std::size_t writePos = 0;
};

// Equal-power dry/wet crossfade gains.
inline void equalPowerMix (float mix, float& dryGain, float& wetGain) noexcept
{
    mix = std::clamp (mix, 0.0f, 1.0f);
    dryGain = std::cos (mix * pi * 0.5f);
    wetGain = std::sin (mix * pi * 0.5f);
}

// Fast deterministic white-noise source (xorshift32), real-time safe.
class Noise
{
public:
    explicit Noise (unsigned seed = 0x9e3779b9u) noexcept : state (seed == 0 ? 1u : seed) {}
    void seed (unsigned s) noexcept { state = s == 0 ? 1u : s; }
    // Uniform in [0, 1).
    float nextUnipolar() noexcept
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return static_cast<float> (state >> 8) * (1.0f / 16777216.0f);
    }
    // Uniform in [-1, 1).
    float nextBipolar() noexcept { return nextUnipolar() * 2.0f - 1.0f; }

private:
    unsigned state;
};

// Topology-preserving-transform state variable filter (Simper). Low/high/band outputs.
class Svf
{
public:
    void setCutoff (float hz, double sampleRate, float q = 0.7071f) noexcept
    {
        hz = std::clamp (hz, 10.0f, static_cast<float> (sampleRate) * 0.49f);
        g = std::tan (pi * hz / static_cast<float> (sampleRate));
        k = 1.0f / q;
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
    void reset() noexcept { ic1 = ic2 = 0.0f; }

    struct Outputs
    {
        float low, band, high;
    };

    Outputs process (float x) noexcept
    {
        const auto v3 = x - ic2;
        const auto v1 = a1 * ic1 + a2 * v3;
        const auto v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        return { v2, v1, x - k * v1 - v2 };
    }
    float lowpass (float x) noexcept { return process (x).low; }
    float highpass (float x) noexcept { return process (x).high; }

private:
    float g = 0.0f, k = 1.4142f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;
    float ic1 = 0.0f, ic2 = 0.0f;
};

// Smoothed random walk in [-1, 1], used for analog-style drift.
class RandomDrift
{
public:
    void prepare (double sampleRate, float rateHz, unsigned seed) noexcept
    {
        noise.seed (seed);
        step = static_cast<float> (rateHz / sampleRate);
        smoothing.prepare (sampleRate, 1000.0f / std::max (0.01f, rateHz) * 0.5f);
    }
    float next() noexcept
    {
        phase += step;
        if (phase >= 1.0f)
        {
            phase -= 1.0f;
            smoothing.setTarget (noise.nextBipolar());
        }
        return smoothing.next();
    }

private:
    Noise noise;
    Smoother smoothing;
    float phase = 1.0f, step = 0.0f;
};

// Global analog movement amounts shared by every effect (0..1 each).
//  drift:    slow, wandering variation (pitch, rate, time) like warm-up and component tolerance
//  unstable: faster, irregular imperfections (flutter, jitter, level flicker)
struct Movement
{
    float drift = 0.0f;
    float unstable = 0.0f;
};

// Tempo-synced note divisions, expressed in quarter-note beats.
inline constexpr int numDivisions = 14;
inline constexpr std::array<float, numDivisions> divisionBeats {
    4.0f, 2.0f, 3.0f, 4.0f / 3.0f, 1.0f, 1.5f, 2.0f / 3.0f, 0.5f, 0.75f, 1.0f / 3.0f, 0.25f, 0.375f, 1.0f / 6.0f, 0.125f
};
inline constexpr std::array<const char*, numDivisions> divisionNames {
    "1/1", "1/2", "1/2 D", "1/2 T", "1/4", "1/4 D", "1/4 T", "1/8", "1/8 D", "1/8 T", "1/16", "1/16 D", "1/16 T", "1/32"
};
inline float divisionSeconds (int index, double bpm) noexcept
{
    index = std::clamp (index, 0, numDivisions - 1);
    return divisionBeats[static_cast<std::size_t> (index)] * 60.0f / static_cast<float> (std::max (20.0, bpm));
}
} // namespace sfx::dsp
