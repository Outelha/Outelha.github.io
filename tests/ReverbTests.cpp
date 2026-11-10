#include "DspTestUtils.h"
#include "dsp/Reverb.h"

#include <catch2/catch_test_macros.hpp>

using namespace sfx;
using namespace sfx::test;
using dsp::Reverb;

namespace
{
Reverb makeReverb (Reverb::Parameters p)
{
    Reverb r;
    r.prepare (sampleRate, 2);
    r.setParameters (p);
    r.reset();
    return r;
}

// Energy (dB) in 10 ms windows of an impulse response.
double windowEnergyDb (const StereoBuffer& b, std::size_t start)
{
    const auto n = static_cast<std::size_t> (0.01 * sampleRate);
    const auto l = rms (b.left, start, start + n);
    const auto r = rms (b.right, start, start + n);
    return 20.0 * std::log10 (std::max (1.0e-12, std::sqrt (0.5 * (l * l + r * r))));
}
} // namespace

TEST_CASE ("Reverb: zero mix is bit-exact dry", "[reverb]")
{
    Reverb::Parameters p;
    p.mix = 0.0f;
    auto r = makeReverb (p);

    StereoBuffer buf (8192);
    buf.left = noise (8192, 0.5f, 5);
    buf.right = noise (8192, 0.5f, 6);
    const auto l = buf.left, rr = buf.right;
    processInBlocks (r, buf);
    CHECK (buf.left == l);
    CHECK (buf.right == rr);
}

TEST_CASE ("Reverb: impulse response respects the pre-delay", "[reverb]")
{
    Reverb::Parameters p;
    p.mix = 1.0f;
    p.predelayMs = 50.0f;
    auto r = makeReverb (p);

    StereoBuffer buf (48000);
    buf.left[0] = buf.right[0] = 1.0f;
    processInBlocks (r, buf);

    const auto preSamples = static_cast<std::size_t> (0.050 * sampleRate);
    CHECK (peak (buf.left, 1, preSamples) < 1.0e-6f); // nothing before pre-delay (dry is 0 at mix=1)
    CHECK (peak (buf.left, preSamples, buf.size()) > 1.0e-3f);
}

TEST_CASE ("Reverb: decay time roughly matches the RT60 setting", "[reverb]")
{
    for (float rt60 : { 1.0f, 3.0f })
    {
        Reverb::Parameters p;
        p.mix = 1.0f;
        p.decaySeconds = rt60;
        p.damping = 0.0f;
        p.predelayMs = 0.0f;
        auto r = makeReverb (p);

        StereoBuffer buf (static_cast<std::size_t> (sampleRate * (rt60 + 1.0f)));
        buf.left[0] = buf.right[0] = 1.0f;
        processInBlocks (r, buf);

        // Fit the slope between 100 ms and 100 ms + RT60/2 (well above the noise floor).
        const auto t0 = static_cast<std::size_t> (0.1 * sampleRate);
        const auto t1 = t0 + static_cast<std::size_t> (0.5 * rt60 * sampleRate);
        const auto drop = windowEnergyDb (buf, t0) - windowEnergyDb (buf, t1);
        const auto measuredRt60 = 60.0 * (static_cast<double> (t1 - t0) / sampleRate) / drop;

        INFO ("target " << rt60 << " s, measured " << measuredRt60 << " s");
        CHECK (measuredRt60 > rt60 * 0.7);
        CHECK (measuredRt60 < rt60 * 1.3);
    }
}

TEST_CASE ("Reverb: tail fully dies away", "[reverb]")
{
    Reverb::Parameters p;
    p.mix = 1.0f;
    p.decaySeconds = 0.5f;
    auto r = makeReverb (p);

    StereoBuffer buf (static_cast<std::size_t> (sampleRate * 3.0));
    for (std::size_t i = 0; i < 4800; ++i)
        buf.left[i] = buf.right[i] = 0.5f * std::sin (static_cast<float> (i) * 0.05f);
    processInBlocks (r, buf);

    CHECK (peak (buf.left, buf.size() - 4800) < 1.0e-4f);
    CHECK (peak (buf.right, buf.size() - 4800) < 1.0e-4f);
}

TEST_CASE ("Reverb: more damping darkens the tail", "[reverb]")
{
    auto highToLowRatio = [] (float damping) {
        Reverb::Parameters p;
        p.mix = 1.0f;
        p.decaySeconds = 2.0f;
        p.damping = damping;
        auto r = makeReverb (p);
        StereoBuffer buf (48000);
        buf.left[0] = buf.right[0] = 1.0f;
        processInBlocks (r, buf);
        const auto from = static_cast<std::size_t> (0.3 * sampleRate);
        return toneAmplitude (buf.left, 8000.0, from, 8192) / toneAmplitude (buf.left, 300.0, from, 8192);
    };
    CHECK (highToLowRatio (0.9f) < highToLowRatio (0.0f) * 0.5);
}

TEST_CASE ("Reverb: width 0 collapses the wet signal to mono", "[reverb]")
{
    Reverb::Parameters p;
    p.mix = 1.0f;
    p.width = 0.0f;
    auto r = makeReverb (p);

    StereoBuffer buf (24000);
    buf.left = noise (24000, 0.5f, 9);
    buf.right = noise (24000, 0.5f, 10);
    processInBlocks (r, buf);
    CHECK (maxAbsDiff (buf.left, buf.right) < 1.0e-5);

    // ...and full width gives a decorrelated stereo tail.
    p.width = 1.0f;
    auto wide = makeReverb (p);
    StereoBuffer b2 (24000);
    b2.left[0] = b2.right[0] = 1.0f;
    processInBlocks (wide, b2);
    CHECK (maxAbsDiff (b2.left, b2.right) > 1.0e-3);
}

TEST_CASE ("Reverb: stable and finite under extreme, changing settings", "[reverb]")
{
    Reverb::Parameters p;
    p.mix = 1.0f;
    p.decaySeconds = Reverb::maxDecaySeconds;
    p.size = 1.0f;
    p.damping = 0.0f;
    auto r = makeReverb (p);

    StereoBuffer buf (static_cast<std::size_t> (sampleRate * 4));
    const auto n = noise (buf.size(), 1.0f, 11);
    buf.left = n;
    buf.right = n;

    for (std::size_t start = 0, block = 0; start < buf.size(); start += 256, ++block)
    {
        p.size = static_cast<float> (block % 11) / 10.0f;
        p.predelayMs = static_cast<float> (block % 5) * 50.0f;
        r.setParameters (p);
        const auto len = static_cast<int> (std::min<std::size_t> (256, buf.size() - start));
        std::array<float*, 2> ptrs { buf.left.data() + start, buf.right.data() + start };
        r.process (ptrs.data(), 2, len);
    }

    CHECK (allFinite (buf.left));
    CHECK (allFinite (buf.right));
    CHECK (peak (buf.left) < 20.0f); // a 20 s reverb fed with full-scale noise builds up, but must not blow up
}

TEST_CASE ("Reverb: reset clears the tail", "[reverb]")
{
    Reverb::Parameters p;
    p.mix = 1.0f;
    p.decaySeconds = 10.0f;
    auto r = makeReverb (p);

    StereoBuffer buf (4800);
    buf.left = noise (4800, 0.5f, 12);
    buf.right = buf.left;
    processInBlocks (r, buf);
    r.reset();

    StereoBuffer silence (4800);
    processInBlocks (r, silence);
    CHECK (peak (silence.left) == 0.0f);
}

TEST_CASE ("Reverb: mono processing works", "[reverb]")
{
    Reverb::Parameters p;
    p.mix = 1.0f;
    auto r = makeReverb (p);
    StereoBuffer buf (24000);
    buf.left[0] = 1.0f;
    processInBlocks (r, buf, 256, 1);
    CHECK (allFinite (buf.left));
    CHECK (rms (buf.left) > 1.0e-4);
}
