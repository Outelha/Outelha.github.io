#include "DspTestUtils.h"
#include "dsp/Chorus.h"

#include <catch2/catch_test_macros.hpp>

using namespace trio;
using namespace trio::test;
using dsp::Chorus;

namespace
{
Chorus makeChorus (Chorus::Parameters p)
{
    Chorus c;
    c.prepare (sampleRate, 2);
    c.setParameters (p);
    c.reset();
    return c;
}
} // namespace

TEST_CASE ("Chorus: zero mix is bit-exact dry", "[chorus]")
{
    Chorus::Parameters p;
    p.mix = 0.0f;
    p.depth = 1.0f;
    auto c = makeChorus (p);

    StereoBuffer buf (8192);
    buf.left = noise (8192, 0.5f, 1);
    buf.right = noise (8192, 0.5f, 2);
    const auto l = buf.left, r = buf.right;
    processInBlocks (c, buf);
    CHECK (buf.left == l);
    CHECK (buf.right == r);
}

TEST_CASE ("Chorus: zero depth and full mix is a pure delay of the centre time", "[chorus]")
{
    Chorus::Parameters p;
    p.depth = 0.0f;
    p.mix = 1.0f;
    p.delayMs = 10.0f;
    auto c = makeChorus (p);

    StereoBuffer buf (4096);
    buf.left[100] = 1.0f;
    buf.right[100] = 1.0f;
    processInBlocks (c, buf);

    const auto expected = 100 + static_cast<std::size_t> (0.010 * sampleRate);
    std::size_t argmax = 0;
    for (std::size_t i = 0; i < buf.size(); ++i)
        if (std::abs (buf.left[i]) > std::abs (buf.left[argmax]))
            argmax = i;

    CHECK (argmax == expected);
    CHECK (std::abs (buf.left[argmax] - 1.0f) < 1.0e-3f);
    CHECK (peak (buf.left, 0, expected - 2) < 1.0e-6f);
}

TEST_CASE ("Chorus: decorrelates a mono source into stereo", "[chorus]")
{
    Chorus::Parameters p;
    p.depth = 0.8f;
    p.rateHz = 1.0f;
    p.mix = 0.5f;
    auto c = makeChorus (p);

    StereoBuffer buf (96000);
    buf.left = sine (96000, 440.0f, 0.5f);
    buf.right = buf.left;
    processInBlocks (c, buf);

    CHECK (maxAbsDiff (buf.left, buf.right) > 0.05);
}

TEST_CASE ("Chorus: modulation actually moves (output not a static filter)", "[chorus]")
{
    Chorus::Parameters p;
    p.depth = 1.0f;
    p.rateHz = 2.0f;
    p.mix = 1.0f;
    auto c = makeChorus (p);

    StereoBuffer buf (96000);
    buf.left = sine (96000, 1000.0f, 0.5f);
    buf.right = buf.left;
    processInBlocks (c, buf);

    // With two voices sweeping in opposite directions the summed wet level
    // fluctuates; a static delay would keep it constant.
    double minRms = 1.0, maxRms = 0.0;
    for (std::size_t start = 4800; start + 480 <= buf.size(); start += 480)
    {
        const auto r = rms (buf.left, start, start + 480);
        minRms = std::min (minRms, r);
        maxRms = std::max (maxRms, r);
    }
    CHECK (maxRms - minRms > 0.02);
}

TEST_CASE ("Chorus: bounded and finite at extreme settings", "[chorus]")
{
    Chorus::Parameters p;
    p.depth = 1.0f;
    p.rateHz = Chorus::maxRateHz;
    p.delayMs = Chorus::minDelayMs;
    p.mix = 1.0f;
    auto c = makeChorus (p);

    StereoBuffer buf (48000);
    buf.left = noise (48000, 1.0f, 3);
    buf.right = noise (48000, 1.0f, 4);
    processInBlocks (c, buf, 64);
    CHECK (allFinite (buf.left));
    // Cubic interpolation of full-scale white noise can overshoot a little; anything
    // well beyond that would indicate a broken read position or runaway gain.
    CHECK (peak (buf.left) <= 1.3f);

    // Sweep parameters every block: must stay finite and bounded.
    for (int block = 0; block < 200; ++block)
    {
        p.delayMs = Chorus::minDelayMs + (Chorus::maxDelayMs - Chorus::minDelayMs) * static_cast<float> (block % 17) / 16.0f;
        p.rateHz = 0.05f + static_cast<float> (block % 7);
        c.setParameters (p);
        std::array<float*, 2> ptrs { buf.left.data() + block * 128, buf.right.data() + block * 128 };
        c.process (ptrs.data(), 2, 128);
    }
    CHECK (allFinite (buf.left));
    CHECK (allFinite (buf.right));
    CHECK (peak (buf.left) <= 1.3f);
}

TEST_CASE ("Chorus: mono processing works", "[chorus]")
{
    Chorus::Parameters p;
    auto c = makeChorus (p);
    StereoBuffer buf (4800);
    buf.left = sine (4800, 220.0f, 0.5f);
    processInBlocks (c, buf, 256, 1);
    CHECK (allFinite (buf.left));
    CHECK (rms (buf.left) > 0.1);
}
