#include "DspTestUtils.h"
#include "dsp/Saturation.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace trio;
using namespace trio::test;
using Sat = dsp::Saturation;

namespace
{
Sat makeSat (Sat::Parameters p)
{
    Sat s;
    s.prepare (sampleRate, 2);
    s.setParameters (p);
    s.reset();
    return s;
}
} // namespace

TEST_CASE ("Saturation: zero mix is bit-exact dry", "[saturation]")
{
    Sat::Parameters p;
    p.driveDb = 30.0f;
    p.mix = 0.0f;
    auto s = makeSat (p);

    StereoBuffer buf (4096);
    buf.left = sine (4096, 440.0f, 0.8f);
    buf.right = noise (4096, 0.8f);
    const auto l = buf.left, r = buf.right;
    processInBlocks (s, buf);

    CHECK (buf.left == l);
    CHECK (buf.right == r);
}

TEST_CASE ("Saturation: disabled module passes audio through", "[saturation]")
{
    Sat::Parameters p;
    p.driveDb = 36.0f;
    p.enabled = false;
    auto s = makeSat (p);

    StereoBuffer buf (2048);
    buf.left = sine (2048, 1000.0f, 0.5f);
    const auto l = buf.left;
    processInBlocks (s, buf);
    CHECK (buf.left == l);
}

TEST_CASE ("Saturation: silence in, silence out", "[saturation]")
{
    for (int mode = 0; mode < Sat::numModes; ++mode)
    {
        Sat::Parameters p;
        p.mode = static_cast<Sat::Mode> (mode);
        p.driveDb = 24.0f;
        auto s = makeSat (p);
        StereoBuffer buf (4096);
        processInBlocks (s, buf);
        CHECK (peak (buf.left) < 1.0e-6f);
    }
}

TEST_CASE ("Saturation: output stays bounded and finite for hot input", "[saturation]")
{
    for (int mode = 0; mode < Sat::numModes; ++mode)
    {
        Sat::Parameters p;
        p.mode = static_cast<Sat::Mode> (mode);
        p.driveDb = Sat::maxDriveDb;
        p.tone = 1.0f;
        auto s = makeSat (p);
        StereoBuffer buf (48000);
        buf.left = sine (48000, 100.0f, 4.0f); // +12 dBFS
        buf.right = noise (48000, 4.0f);
        processInBlocks (s, buf);
        INFO ("mode " << mode);
        CHECK (allFinite (buf.left));
        CHECK (allFinite (buf.right));
        CHECK (peak (buf.left, 4800) < 2.0f);
        CHECK (peak (buf.right, 4800) < 2.0f);
    }
}

TEST_CASE ("Saturation: more drive means more harmonics", "[saturation]")
{
    auto thirdHarmonicRatio = [] (float driveDb) {
        Sat::Parameters p;
        p.mode = Sat::Mode::soft;
        p.driveDb = driveDb;
        auto s = makeSat (p);
        StereoBuffer buf (16384);
        buf.left = sine (16384, 500.0f, 0.5f);
        buf.right = buf.left;
        processInBlocks (s, buf);
        const auto fundamental = toneAmplitude (buf.left, 500.0, 8192, 8192);
        const auto third = toneAmplitude (buf.left, 1500.0, 8192, 8192);
        return third / fundamental;
    };

    const auto low = thirdHarmonicRatio (0.0f);
    const auto mid = thirdHarmonicRatio (12.0f);
    const auto high = thirdHarmonicRatio (30.0f);
    CHECK (low < mid);
    CHECK (mid < high);
    CHECK (high > 0.1); // clearly audible distortion at high drive
}

TEST_CASE ("Saturation: warm mode adds even harmonics, soft mode does not", "[saturation]")
{
    auto secondHarmonicRatio = [] (Sat::Mode mode) {
        Sat::Parameters p;
        p.mode = mode;
        p.driveDb = 18.0f;
        auto s = makeSat (p);
        StereoBuffer buf (16384);
        buf.left = sine (16384, 500.0f, 0.5f);
        processInBlocks (s, buf, 256, 1);
        return toneAmplitude (buf.left, 1000.0, 8192, 8192) / toneAmplitude (buf.left, 500.0, 8192, 8192);
    };

    CHECK (secondHarmonicRatio (Sat::Mode::soft) < 0.005);
    CHECK (secondHarmonicRatio (Sat::Mode::warm) > 0.02);
}

TEST_CASE ("Saturation: warm mode output has no DC offset", "[saturation]")
{
    Sat::Parameters p;
    p.mode = Sat::Mode::warm;
    p.driveDb = 24.0f;
    auto s = makeSat (p);
    StereoBuffer buf (96000);
    buf.left = sine (96000, 200.0f, 0.7f);
    processInBlocks (s, buf, 512, 1);

    double mean = 0.0;
    for (std::size_t i = 48000; i < 96000; ++i)
        mean += buf.left[i];
    mean /= 48000.0;
    CHECK (std::abs (mean) < 1.0e-3);
}

TEST_CASE ("Saturation: auto gain keeps the reference level roughly constant", "[saturation]")
{
    auto outputRms = [] (float driveDb) {
        Sat::Parameters p;
        p.driveDb = driveDb;
        auto s = makeSat (p);
        StereoBuffer buf (24000);
        buf.left = sine (24000, 300.0f, 0.25f); // -12 dBFS reference
        processInBlocks (s, buf, 256, 1);
        return rms (buf.left, 12000);
    };

    const auto clean = outputRms (0.0f);
    for (float drive : { 6.0f, 12.0f, 24.0f, 36.0f })
    {
        const auto ratioDb = 20.0 * std::log10 (outputRms (drive) / clean);
        INFO ("drive " << drive << " dB -> level change " << ratioDb << " dB");
        CHECK (std::abs (ratioDb) < 3.0);
    }
}

TEST_CASE ("Saturation: shaper curves are odd/monotonic where expected", "[saturation]")
{
    for (float x = -3.0f; x <= 3.0f; x += 0.01f)
    {
        CHECK (Sat::shape (Sat::Mode::soft, -x) == Catch::Approx (-Sat::shape (Sat::Mode::soft, x)).margin (1e-6));
        CHECK (Sat::shape (Sat::Mode::hard, x) <= 1.0f);
        CHECK (Sat::shape (Sat::Mode::warm, x + 0.01f) >= Sat::shape (Sat::Mode::warm, x));
    }
    CHECK (Sat::shape (Sat::Mode::warm, 0.0f) == Catch::Approx (0.0f).margin (1e-6));
}
