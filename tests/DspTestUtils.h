#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <numeric>
#include <random>
#include <vector>

namespace trio::test
{
constexpr double sampleRate = 48000.0;

struct StereoBuffer
{
    std::vector<float> left, right;

    explicit StereoBuffer (std::size_t n, float value = 0.0f) : left (n, value), right (n, value) {}

    std::size_t size() const { return left.size(); }
    std::array<float*, 2> pointers() { return { left.data(), right.data() }; }
};

inline std::vector<float> sine (std::size_t n, float freqHz, float amplitude, double sr = sampleRate)
{
    std::vector<float> v (n);
    for (std::size_t i = 0; i < n; ++i)
        v[i] = amplitude * static_cast<float> (std::sin (2.0 * 3.14159265358979323846 * freqHz * static_cast<double> (i) / sr));
    return v;
}

inline std::vector<float> noise (std::size_t n, float amplitude, unsigned seed = 42)
{
    std::mt19937 rng (seed);
    std::uniform_real_distribution<float> dist (-amplitude, amplitude);
    std::vector<float> v (n);
    for (auto& x : v)
        x = dist (rng);
    return v;
}

inline float peak (const std::vector<float>& v, std::size_t from = 0, std::size_t to = SIZE_MAX)
{
    to = std::min (to, v.size());
    float p = 0.0f;
    for (auto i = from; i < to; ++i)
        p = std::max (p, std::abs (v[i]));
    return p;
}

inline double rms (const std::vector<float>& v, std::size_t from = 0, std::size_t to = SIZE_MAX)
{
    to = std::min (to, v.size());
    if (to <= from)
        return 0.0;
    double acc = 0.0;
    for (auto i = from; i < to; ++i)
        acc += static_cast<double> (v[i]) * v[i];
    return std::sqrt (acc / static_cast<double> (to - from));
}

inline bool allFinite (const std::vector<float>& v)
{
    return std::all_of (v.begin(), v.end(), [] (float x) { return std::isfinite (x); });
}

inline double maxAbsDiff (const std::vector<float>& a, const std::vector<float>& b)
{
    double d = 0.0;
    for (std::size_t i = 0; i < std::min (a.size(), b.size()); ++i)
        d = std::max (d, static_cast<double> (std::abs (a[i] - b[i])));
    return d;
}

// Magnitude of a single frequency bin (Goertzel-style DFT), normalised to sine amplitude.
inline double toneAmplitude (const std::vector<float>& v, double freqHz, std::size_t from, std::size_t count, double sr = sampleRate)
{
    double re = 0.0, im = 0.0;
    for (std::size_t i = 0; i < count; ++i)
    {
        const auto w = 0.5 - 0.5 * std::cos (2.0 * 3.14159265358979323846 * static_cast<double> (i) / static_cast<double> (count - 1)); // Hann
        const auto phase = 2.0 * 3.14159265358979323846 * freqHz * static_cast<double> (i) / sr;
        re += w * v[from + i] * std::cos (phase);
        im -= w * v[from + i] * std::sin (phase);
    }
    return 2.0 * std::sqrt (re * re + im * im) / (static_cast<double> (count) * 0.5);
}

// Runs a module over a buffer in host-sized blocks.
template <typename Module>
void processInBlocks (Module& m, StereoBuffer& buf, int blockSize = 256, int channels = 2)
{
    for (std::size_t start = 0; start < buf.size(); start += static_cast<std::size_t> (blockSize))
    {
        const auto n = static_cast<int> (std::min<std::size_t> (static_cast<std::size_t> (blockSize), buf.size() - start));
        std::array<float*, 2> ptrs { buf.left.data() + start, buf.right.data() + start };
        m.process (ptrs.data(), channels, n);
    }
}
} // namespace trio::test
