#include "DspTestUtils.h"
#include "dsp/Chorus.h"
#include "dsp/Delay.h"
#include "dsp/Resampler.h"
#include "dsp/Reverb.h"
#include "dsp/Reverse.h"
#include "dsp/Saturation.h"
#include "dsp/Tape.h"
#include "dsp/Tremolo.h"

#include <catch2/catch_test_macros.hpp>

using namespace sfx;
using namespace sfx::test;

namespace
{
// Renders a module with a given movement and returns the left channel.
template <typename Module, typename Params, typename Setter>
std::vector<float> render (Params params, dsp::Movement movement, Setter&& setParams, std::size_t length = 96000)
{
    Module m;
    m.prepare (sampleRate, 2);
    setParams (m, params);
    m.setMovement (movement);
    m.reset();
    StereoBuffer buf (length);
    buf.left = sine (length, 440.0f, 0.4f);
    buf.right = sine (length, 440.0f, 0.4f);
    processInBlocks (m, buf);
    return buf.left;
}

double meanAbsDiff (const std::vector<float>& a, const std::vector<float>& b)
{
    double d = 0.0;
    for (std::size_t i = 0; i < a.size(); ++i)
        d += std::abs (a[i] - b[i]);
    return d / static_cast<double> (a.size());
}

template <typename Module, typename Params, typename Setter>
void checkMovement (const char* name, Params params, Setter&& setParams, double minChange = 1.0e-3)
{
    INFO (name);
    const auto still = render<Module> (params, {}, setParams);
    const auto stillAgain = render<Module> (params, {}, setParams);
    const auto drift = render<Module> (params, { 1.0f, 0.0f }, setParams);
    const auto unstable = render<Module> (params, { 0.0f, 1.0f }, setParams);

    // Zero movement is deterministic.
    CHECK (still == stillAgain);
    // Each control audibly changes the processing...
    CHECK (meanAbsDiff (still, drift) > minChange);
    CHECK (meanAbsDiff (still, unstable) > minChange);
    // ...without blowing anything up.
    CHECK (allFinite (drift));
    CHECK (allFinite (unstable));
    CHECK (peak (drift) < 3.0f);
    CHECK (peak (unstable) < 3.0f);
}
} // namespace

TEST_CASE ("Movement: Drift and Unstable affect every effect", "[movement]")
{
    checkMovement<dsp::Saturation> ("saturation", dsp::Saturation::Parameters {}, [] (auto& m, auto p) { m.setParameters (p); });

    dsp::Resampler::Parameters rp;
    rp.rateHz = 6000.0f;
    checkMovement<dsp::Resampler> ("resampler", rp, [] (auto& m, auto p) { m.setParameters (p); });

    dsp::Tape::Parameters tp;
    tp.wow = tp.flutter = 0.2f;
    tp.noise = 0.0f;
    checkMovement<dsp::Tape> ("tape", tp, [] (auto& m, auto p) { m.setParameters (p); });

    dsp::Chorus::Parameters cp;
    cp.mix = 1.0f;
    checkMovement<dsp::Chorus> ("chorus", cp, [] (auto& m, auto p) { m.setParameters (p); });

    dsp::Tremolo::Parameters trp;
    trp.depth = 0.8f;
    checkMovement<dsp::Tremolo> ("tremolo", trp, [] (auto& m, auto p) { m.setParameters (p, 120.0); });

    dsp::Reverse::Parameters rvp;
    rvp.mix = 1.0f;
    rvp.timeMs = 150.0f;
    checkMovement<dsp::Reverse> ("reverse", rvp, [] (auto& m, auto p) { m.setParameters (p, 120.0); });

    dsp::Delay::Parameters dp;
    dp.mix = 1.0f;
    dp.sync = false;
    dp.timeMs = 120.0f;
    checkMovement<dsp::Delay> ("delay", dp, [] (auto& m, auto p) { m.setParameters (p, 120.0); });

    dsp::Reverb::Parameters rbp;
    rbp.mix = 1.0f;
    checkMovement<dsp::Reverb> ("reverb", rbp, [] (auto& m, auto p) { m.setParameters (p); }, 1.0e-4);
}

TEST_CASE ("Movement: zero movement leaves the previous behaviour unchanged", "[movement]")
{
    // Explicitly setting zero must equal never calling setMovement at all.
    dsp::Delay::Parameters dp;
    dp.mix = 1.0f;
    dp.sync = false;
    auto withCall = render<dsp::Delay> (dp, {}, [] (auto& m, auto p) { m.setParameters (p, 120.0); });

    dsp::Delay d;
    d.prepare (sampleRate, 2);
    d.setParameters (dp, 120.0);
    d.reset();
    StereoBuffer buf (96000);
    buf.left = sine (96000, 440.0f, 0.4f);
    buf.right = buf.left;
    processInBlocks (d, buf);
    CHECK (buf.left == withCall);
}
