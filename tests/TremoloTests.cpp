#include "DspTestUtils.h"
#include "dsp/Tremolo.h"

#include <catch2/catch_test_macros.hpp>

using namespace sfx;
using namespace sfx::test;
using dsp::Tremolo;

namespace
{
Tremolo make (Tremolo::Parameters p, double bpm = 120.0)
{
    Tremolo t;
    t.prepare (sampleRate, 2);
    t.setParameters (p, bpm);
    t.reset();
    return t;
}

// Period (in samples) of the amplitude envelope of a constant (DC) input.
double envelopePeriod (const std::vector<float>& v)
{
    // Find successive local minima of the gain curve.
    std::vector<std::size_t> minima;
    for (std::size_t i = 1; i + 1 < v.size(); ++i)
        if (v[i] < v[i - 1] && v[i] <= v[i + 1] && v[i] < 0.5f)
            if (minima.empty() || i - minima.back() > 100)
                minima.push_back (i);
    if (minima.size() < 3)
        return 0.0;
    return static_cast<double> (minima.back() - minima.front()) / static_cast<double> (minima.size() - 1);
}
} // namespace

TEST_CASE ("Tremolo: zero depth or disabled is bit-exact dry", "[tremolo]")
{
    for (bool enabled : { false, true })
    {
        Tremolo::Parameters p;
        p.enabled = enabled;
        p.depth = enabled ? 0.0f : 1.0f;
        auto t = make (p);
        StereoBuffer buf (4096);
        buf.left = noise (4096, 0.5f, 1);
        const auto l = buf.left;
        processInBlocks (t, buf);
        CHECK (buf.left == l);
    }
}

TEST_CASE ("Tremolo: full depth sine swings from silence to unity", "[tremolo]")
{
    Tremolo::Parameters p;
    p.depth = 1.0f;
    p.rateHz = 5.0f;
    auto t = make (p);
    StereoBuffer buf (48000, 1.0f);
    processInBlocks (t, buf, 256, 1);
    const auto [mn, mx] = std::minmax_element (buf.left.begin(), buf.left.end());
    CHECK (*mn < 0.01f);
    CHECK (*mx > 0.99f);
}

TEST_CASE ("Tremolo: free rate sets the modulation period", "[tremolo]")
{
    Tremolo::Parameters p;
    p.depth = 1.0f;
    p.rateHz = 4.0f;
    auto t = make (p);
    StereoBuffer buf (96000, 1.0f);
    processInBlocks (t, buf, 256, 1);
    CHECK (std::abs (envelopePeriod (buf.left) - sampleRate / 4.0) < 50.0);
}

TEST_CASE ("Tremolo: tempo sync follows the host BPM", "[tremolo]")
{
    Tremolo::Parameters p;
    p.depth = 1.0f;
    p.sync = true;
    p.division = 4; // quarter note
    auto t = make (p, 120.0);
    StereoBuffer buf (static_cast<std::size_t> (sampleRate * 3), 1.0f);
    processInBlocks (t, buf, 256, 1);
    // 1/4 at 120 BPM = 0.5 s.
    CHECK (std::abs (envelopePeriod (buf.left) - sampleRate * 0.5) < 100.0);
}

TEST_CASE ("Tremolo: stereo setting puts L and R out of phase", "[tremolo]")
{
    Tremolo::Parameters p;
    p.depth = 1.0f;
    p.stereo = 1.0f;
    auto t = make (p);
    StereoBuffer buf (48000, 1.0f);
    processInBlocks (t, buf);
    double correlation = 0.0;
    for (std::size_t i = 0; i < buf.size(); ++i)
        correlation += (buf.left[i] - 0.5) * (buf.right[i] - 0.5);
    CHECK (correlation < 0.0); // auto-pan: one side up while the other is down
}

TEST_CASE ("Tremolo: every shape stays within [0, 1] gain", "[tremolo]")
{
    for (int s = 0; s < Tremolo::numShapes; ++s)
        for (float ph = 0.0f; ph < 1.0f; ph += 0.001f)
        {
            const auto v = Tremolo::shapeValue (static_cast<Tremolo::Shape> (s), ph);
            REQUIRE (v >= -1.0e-4f);
            REQUIRE (v <= 1.0001f);
        }
}
