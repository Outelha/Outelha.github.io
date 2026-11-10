#include "DspTestUtils.h"
#include "dsp/Reverse.h"

#include <catch2/catch_test_macros.hpp>

using namespace sfx;
using namespace sfx::test;
using dsp::Reverse;

namespace
{
Reverse make (Reverse::Parameters p, double bpm = 120.0)
{
    Reverse r;
    r.prepare (sampleRate, 2);
    r.setParameters (p, bpm);
    r.reset();
    return r;
}
} // namespace

TEST_CASE ("Reverse: zero mix is bit-exact dry", "[reverse]")
{
    Reverse::Parameters p;
    p.mix = 0.0f;
    auto r = make (p);
    StereoBuffer buf (8192);
    buf.left = noise (8192, 0.5f, 1);
    const auto l = buf.left;
    processInBlocks (r, buf);
    CHECK (buf.left == l);
}

TEST_CASE ("Reverse: a rising sawtooth comes out as a falling one", "[reverse]")
{
    // Forward: slow rise, sharp drop (period 50 ms). Played backwards, the slow
    // parts fall, so falling samples must outnumber rising ones.
    auto countSlopes = [] (const std::vector<float>& v, std::size_t from) {
        int falling = 0, rising = 0;
        for (std::size_t i = from; i + 1 < v.size(); ++i)
            (v[i + 1] < v[i] ? falling : rising)++;
        return std::pair<int, int> { falling, rising };
    };

    StereoBuffer buf (static_cast<std::size_t> (sampleRate * 3));
    const auto period = static_cast<std::size_t> (0.05 * sampleRate);
    for (std::size_t i = 0; i < buf.size(); ++i)
        buf.left[i] = static_cast<float> (i % period) / static_cast<float> (period) - 0.5f;

    const auto [fwdFalling, fwdRising] = countSlopes (buf.left, 0);
    REQUIRE (fwdRising > fwdFalling * 10);

    Reverse::Parameters p;
    p.timeMs = 200.0f;
    p.mix = 1.0f;
    auto r = make (p);
    processInBlocks (r, buf, 256, 1);

    const auto [falling, rising] = countSlopes (buf.left, static_cast<std::size_t> (0.5 * sampleRate));
    INFO ("falling " << falling << " rising " << rising);
    CHECK (falling > rising * 3);
}

TEST_CASE ("Reverse: windows sum to unity (constant input stays constant)", "[reverse]")
{
    Reverse::Parameters p;
    p.timeMs = 300.0f;
    p.mix = 1.0f;
    auto r = make (p);
    StereoBuffer buf (static_cast<std::size_t> (sampleRate * 3), 0.5f);
    processInBlocks (r, buf, 256, 1);
    // After the buffer has filled (pitch 0: reads up to 2x segment into the past).
    const auto start = static_cast<std::size_t> (0.7 * sampleRate);
    const auto [mn, mx] = std::minmax_element (buf.left.begin() + static_cast<long> (start), buf.left.end());
    CHECK (*mn > 0.49f);
    CHECK (*mx < 0.51f);
}

TEST_CASE ("Reverse: tempo-synced segment length", "[reverse]")
{
    Reverse::Parameters p;
    p.sync = true;
    p.division = 4; // 1/4
    auto r = make (p, 100.0);
    CHECK (std::abs (r.getSegmentSamples() - static_cast<float> (sampleRate * 0.6)) < 1.0f);
}

TEST_CASE ("Reverse: pitch extremes stay finite and bounded", "[reverse]")
{
    for (float pitch : { Reverse::minPitch, Reverse::maxPitch })
    {
        Reverse::Parameters p;
        p.pitchSemitones = pitch;
        p.timeMs = Reverse::maxTimeMs;
        p.mix = 1.0f;
        auto r = make (p);
        StereoBuffer buf (static_cast<std::size_t> (sampleRate * 8));
        buf.left = noise (buf.size(), 1.0f, 3);
        buf.right = noise (buf.size(), 1.0f, 4);
        processInBlocks (r, buf, 333);
        CHECK (allFinite (buf.left));
        CHECK (peak (buf.left) <= 1.05f);
    }
}
