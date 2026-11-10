#include "DspTestUtils.h"
#include "dsp/Resampler.h"

#include <catch2/catch_test_macros.hpp>

#include <set>

using namespace sfx;
using namespace sfx::test;
using dsp::Resampler;

namespace
{
Resampler make (Resampler::Parameters p)
{
    Resampler r;
    r.prepare (sampleRate, 2);
    r.setParameters (p);
    r.reset();
    return r;
}
} // namespace

TEST_CASE ("Resampler: disabled or zero mix is bit-exact dry", "[resampler]")
{
    for (bool enabled : { false, true })
    {
        Resampler::Parameters p;
        p.enabled = enabled;
        p.mix = enabled ? 0.0f : 1.0f;
        auto r = make (p);
        StereoBuffer buf (4096);
        buf.left = noise (4096, 0.5f, 1);
        buf.right = noise (4096, 0.5f, 2);
        const auto l = buf.left;
        processInBlocks (r, buf);
        CHECK (buf.left == l);
    }
}

TEST_CASE ("Resampler: low bit depth quantises to few levels", "[resampler]")
{
    Resampler::Parameters p;
    p.bits = 3.0f;               // 2^(3-1) = 4 steps per polarity
    p.rateHz = Resampler::maxRateHz;
    p.filterHz = Resampler::maxFilterHz;
    p.mix = 1.0f;
    auto r = make (p);

    StereoBuffer buf (4800);
    buf.left = sine (4800, 50.0f, 0.9f); // slow sine so the filter barely smooths the steps
    processInBlocks (r, buf, 256, 1);

    // The output should cluster around the quantiser levels: count distinct rounded values.
    std::set<int> distinct;
    for (std::size_t i = 2400; i < 4800; ++i)
        distinct.insert (static_cast<int> (std::lround (buf.left[i] * 4.0f)));
    CHECK (distinct.size() <= 9);
    CHECK (allFinite (buf.left));
}

TEST_CASE ("Resampler: rate reduction holds samples", "[resampler]")
{
    Resampler::Parameters p;
    p.rateHz = 4800.0f; // one new sample every 10 input samples
    p.bits = 24.0f;
    p.filterHz = Resampler::maxFilterHz;
    p.mix = 1.0f;
    auto r = make (p);

    // Raw hold behaviour: with the filter wide open, consecutive equal pairs dominate.
    StereoBuffer buf (4800);
    buf.left = noise (4800, 0.5f, 3);
    const auto in = buf.left;
    processInBlocks (r, buf, 256, 1);

    double changeEnergy = 0.0, inputChangeEnergy = 0.0;
    for (std::size_t i = 1; i < buf.size(); ++i)
    {
        changeEnergy += std::pow (buf.left[i] - buf.left[i - 1], 2.0);
        inputChangeEnergy += std::pow (in[i] - in[i - 1], 2.0);
    }
    // Held noise changes far less often than the input.
    CHECK (changeEnergy < inputChangeEnergy * 0.35);
}

TEST_CASE ("Resampler: filter removes high frequencies", "[resampler]")
{
    auto highBand = [] (float cutoff) {
        Resampler::Parameters p;
        p.rateHz = Resampler::maxRateHz;
        p.bits = 24.0f;
        p.filterHz = cutoff;
        p.mix = 1.0f;
        auto r = make (p);
        StereoBuffer buf (16384);
        buf.left = sine (16384, 10000.0f, 0.5f);
        processInBlocks (r, buf, 256, 1);
        return rms (buf.left, 8192);
    };
    CHECK (highBand (1000.0f) < highBand (20000.0f) * 0.1);
}

TEST_CASE ("Resampler: jitter keeps output finite and bounded", "[resampler]")
{
    Resampler::Parameters p;
    p.rateHz = Resampler::minRateHz;
    p.bits = Resampler::minBits;
    p.jitter = 1.0f;
    p.mix = 1.0f;
    auto r = make (p);
    StereoBuffer buf (48000);
    buf.left = noise (48000, 1.0f, 4);
    buf.right = noise (48000, 1.0f, 5);
    processInBlocks (r, buf, 64);
    CHECK (allFinite (buf.left));
    CHECK (peak (buf.left) < 2.0f);
}
