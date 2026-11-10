#include "DspTestUtils.h"
#include "dsp/Tape.h"

#include <catch2/catch_test_macros.hpp>

using namespace sfx;
using namespace sfx::test;
using dsp::Tape;

namespace
{
Tape make (Tape::Parameters p)
{
    Tape t;
    t.prepare (sampleRate, 2);
    t.setParameters (p);
    t.reset();
    return t;
}

// Average period (in samples) between rising zero crossings in a window.
std::vector<double> crossingIntervals (const std::vector<float>& v, std::size_t from)
{
    std::vector<double> intervals;
    double last = -1.0;
    for (std::size_t i = from + 1; i < v.size(); ++i)
        if (v[i - 1] < 0.0f && v[i] >= 0.0f)
        {
            const auto frac = static_cast<double> (-v[i - 1] / (v[i] - v[i - 1]));
            const auto t = static_cast<double> (i - 1) + frac;
            if (last >= 0.0)
                intervals.push_back (t - last);
            last = t;
        }
    return intervals;
}
} // namespace

TEST_CASE ("Tape: zero mix is bit-exact dry", "[tape]")
{
    Tape::Parameters p;
    p.mix = 0.0f;
    p.noise = 1.0f;
    auto t = make (p);
    StereoBuffer buf (4096);
    buf.left = noise (4096, 0.5f, 1);
    const auto l = buf.left;
    processInBlocks (t, buf);
    CHECK (buf.left == l);
}

TEST_CASE ("Tape: hiss appears on silence only when noise is up", "[tape]")
{
    for (float n : { 0.0f, 1.0f })
    {
        Tape::Parameters p;
        p.noise = n;
        p.age = 0.0f;
        auto t = make (p);
        StereoBuffer buf (24000);
        processInBlocks (t, buf);
        INFO ("noise " << n);
        if (n == 0.0f)
            CHECK (peak (buf.left) < 1.0e-6f);
        else
            CHECK (rms (buf.left, 4800) > 1.0e-3);
    }
}

TEST_CASE ("Tape: wow and flutter modulate pitch", "[tape]")
{
    auto spread = [] (float wow, float flutter) {
        Tape::Parameters p;
        p.wow = wow;
        p.flutter = flutter;
        p.noise = 0.0f;
        p.age = 0.0f;
        p.driveDb = 0.0f;
        auto t = make (p);
        StereoBuffer buf (static_cast<std::size_t> (sampleRate * 4));
        buf.left = sine (buf.size(), 1000.0f, 0.3f);
        processInBlocks (t, buf, 256, 1);
        const auto iv = crossingIntervals (buf.left, 9600);
        const auto [mn, mx] = std::minmax_element (iv.begin(), iv.end());
        return *mx - *mn;
    };
    const auto steady = spread (0.0f, 0.0f);
    const auto wobbly = spread (1.0f, 1.0f);
    INFO ("steady " << steady << " wobbly " << wobbly);
    CHECK (steady < 0.05);   // a clean transport: constant period
    CHECK (wobbly > 0.15);   // audible pitch movement
}

TEST_CASE ("Tape: age darkens the sound", "[tape]")
{
    auto highs = [] (float age) {
        Tape::Parameters p;
        p.age = age;
        p.wow = p.flutter = p.noise = 0.0f;
        auto t = make (p);
        StereoBuffer buf (16384);
        buf.left = sine (16384, 9000.0f, 0.2f);
        processInBlocks (t, buf, 256, 1);
        return rms (buf.left, 8192);
    };
    CHECK (highs (1.0f) < highs (0.0f) * 0.3);
}

TEST_CASE ("Tape: bounded and finite with everything maxed", "[tape]")
{
    Tape::Parameters p;
    p.driveDb = Tape::maxDriveDb;
    p.wow = p.flutter = p.noise = p.age = 1.0f;
    auto t = make (p);
    t.setMovement ({ 1.0f, 1.0f });
    StereoBuffer buf (48000);
    buf.left = noise (48000, 1.5f, 7);
    buf.right = noise (48000, 1.5f, 8);
    processInBlocks (t, buf, 100);
    CHECK (allFinite (buf.left));
    CHECK (allFinite (buf.right));
    CHECK (peak (buf.left) < 2.5f);
}
