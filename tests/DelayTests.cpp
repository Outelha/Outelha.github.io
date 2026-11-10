#include "DspTestUtils.h"
#include "dsp/Delay.h"

#include <catch2/catch_test_macros.hpp>

using namespace sfx;
using namespace sfx::test;
using dsp::Delay;

namespace
{
Delay make (Delay::Parameters p, double bpm = 120.0)
{
    Delay d;
    d.prepare (sampleRate, 2);
    d.setParameters (p, bpm);
    d.reset();
    return d;
}

std::size_t argmaxFrom (const std::vector<float>& v, std::size_t from, std::size_t to)
{
    std::size_t best = from;
    for (auto i = from; i < std::min (to, v.size()); ++i)
        if (std::abs (v[i]) > std::abs (v[best]))
            best = i;
    return best;
}
} // namespace

TEST_CASE ("Delay: zero mix is bit-exact dry", "[delay]")
{
    Delay::Parameters p;
    p.mix = 0.0f;
    auto d = make (p);
    StereoBuffer buf (8192);
    buf.left = noise (8192, 0.5f, 1);
    const auto l = buf.left;
    processInBlocks (d, buf);
    CHECK (buf.left == l);
}

TEST_CASE ("Delay: echo arrives after the set time", "[delay]")
{
    Delay::Parameters p;
    p.sync = false;
    p.timeMs = 100.0f;
    p.feedback = 0.0f;
    p.lowCutHz = 20.0f;
    p.highCutHz = 20000.0f;
    p.mix = 1.0f;
    auto d = make (p);

    StereoBuffer buf (24000);
    buf.left[0] = buf.right[0] = 1.0f;
    processInBlocks (d, buf);
    const auto expected = static_cast<std::size_t> (0.1 * sampleRate);
    const auto at = argmaxFrom (buf.left, 1, buf.size());
    CHECK (std::abs (static_cast<long> (at) - static_cast<long> (expected)) <= 2);
}

TEST_CASE ("Delay: synced time follows the host tempo", "[delay]")
{
    Delay::Parameters p;
    p.sync = true;
    p.division = 7; // 1/8 at 150 BPM = 0.2 s
    p.feedback = 0.0f;
    p.lowCutHz = 20.0f;
    p.highCutHz = 20000.0f;
    p.mix = 1.0f;
    auto d = make (p, 150.0);
    StereoBuffer buf (24000);
    buf.left[0] = buf.right[0] = 1.0f;
    processInBlocks (d, buf);
    const auto at = argmaxFrom (buf.left, 1, buf.size());
    CHECK (std::abs (static_cast<long> (at) - static_cast<long> (0.2 * sampleRate)) <= 2);
}

TEST_CASE ("Delay: feedback repeats decay and stay bounded at maximum", "[delay]")
{
    Delay::Parameters p;
    p.sync = false;
    p.timeMs = 50.0f;
    p.feedback = Delay::maxFeedback;
    p.mix = 1.0f;
    auto d = make (p);
    StereoBuffer buf (static_cast<std::size_t> (sampleRate * 10));
    buf.left = noise (4800, 1.0f, 2);
    buf.left.resize (buf.size(), 0.0f);
    buf.right = buf.left;
    processInBlocks (d, buf);
    CHECK (allFinite (buf.left));
    CHECK (peak (buf.left) < 3.0f);

    // Moderate feedback: the 3rd repeat is quieter than the 1st.
    p.feedback = 0.5f;
    p.lowCutHz = 20.0f;
    p.highCutHz = 20000.0f;
    auto d2 = make (p);
    StereoBuffer imp (24000);
    imp.left[0] = imp.right[0] = 1.0f;
    processInBlocks (d2, imp);
    const auto step = static_cast<std::size_t> (0.05 * sampleRate);
    CHECK (peak (imp.left, step * 3 - 20, step * 3 + 20) < peak (imp.left, step - 20, step + 20) * 0.5f);
}

TEST_CASE ("Delay: ping-pong alternates sides", "[delay]")
{
    Delay::Parameters p;
    p.sync = false;
    p.timeMs = 100.0f;
    p.feedback = 0.7f;
    p.pingPong = true;
    p.lowCutHz = 20.0f;
    p.highCutHz = 20000.0f;
    p.mix = 1.0f;
    auto d = make (p);
    StereoBuffer buf (48000);
    buf.left[0] = buf.right[0] = 1.0f;
    processInBlocks (d, buf);

    const auto step = static_cast<std::size_t> (0.1 * sampleRate);
    // First repeat on the left, second on the right.
    CHECK (peak (buf.left, step - 10, step + 10) > 4.0f * peak (buf.right, step - 10, step + 10));
    CHECK (peak (buf.right, 2 * step - 10, 2 * step + 10) > 4.0f * peak (buf.left, 2 * step - 10, 2 * step + 10));
}
