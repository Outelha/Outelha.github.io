#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

// Small, dependency-free DSP building blocks shared by the effect modules.
// Everything here is real-time safe once prepared: no allocation, no locks.
namespace trio::dsp
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
} // namespace trio::dsp
