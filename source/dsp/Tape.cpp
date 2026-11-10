#include "Tape.h"

namespace sfx::dsp
{
namespace
{
constexpr float bias = 0.12f;
float magnetise (float x) noexcept { return std::tanh (x + bias) - std::tanh (bias); }
} // namespace

void Tape::prepare (double newSampleRate, int)
{
    sampleRate = newSampleRate;
    const auto maxDelay = static_cast<std::size_t> (std::ceil ((maxWowMs * 2.0f + maxFlutterMs * 2.0f + 2.0f) * 0.001 * sampleRate)) + 8;
    for (auto& d : delays)
        d.prepare (maxDelay);

    for (auto* s : { &drive, &comp, &noiseLevel, &mix })
        s->prepare (sampleRate, 30.0f);
    for (auto* s : { &wow, &flutter, &age, &instability })
        s->prepare (sampleRate, 80.0f);
    baseDelay.prepare (sampleRate, 300.0f);

    wowDrift.prepare (sampleRate, 0.7f, 0xabcdefu);
    flutterDrift.prepare (sampleRate, 9.0f, 0x13579bu);
    dropout.prepare (sampleRate, 3.0f, 0x2468acu);

    firstUpdate = true;
    reset();
}

void Tape::reset() noexcept
{
    for (auto& d : delays)
        d.reset();
    for (std::size_t c = 0; c < maxChannels; ++c)
    {
        dc[c].prepare (sampleRate);
        dc[c].reset();
        headLoss[c].reset();
        hissFilter[c].setCutoff (7000.0f, sampleRate);
        hissFilter[c].reset();
    }
    wowPhase = flutterPhase = 0.0f;
    for (auto* s : { &drive, &comp, &wow, &flutter, &noiseLevel, &age, &mix, &instability, &baseDelay })
        s->snapToTarget();
}

void Tape::setParameters (const Parameters& p) noexcept
{
    const auto g = dbToGain (std::clamp (p.driveDb, 0.0f, maxDriveDb));
    drive.setTarget (g);
    // Keep perceived level roughly constant as drive increases (reference -12 dBFS).
    comp.setTarget (0.25f / std::max (1.0e-4f, 0.5f * (std::abs (magnetise (0.25f * g)) + std::abs (magnetise (-0.25f * g)))));
    wow.setTarget (std::clamp (p.wow, 0.0f, 1.0f));
    flutter.setTarget (std::clamp (p.flutter, 0.0f, 1.0f));
    noiseLevel.setTarget (std::clamp (p.noise, 0.0f, 1.0f));
    age.setTarget (std::clamp (p.age, 0.0f, 1.0f));
    mix.setTarget (p.enabled ? std::clamp (p.mix, 0.0f, 1.0f) : 0.0f);

    // The delay centre only needs to be as large as the modulation swing.
    const auto swingMs = std::clamp (p.wow, 0.0f, 1.0f) * maxWowMs + std::clamp (p.flutter, 0.0f, 1.0f) * maxFlutterMs;
    baseDelay.setTarget ((swingMs + 0.3f) * 0.001f * static_cast<float> (sampleRate));

    if (firstUpdate)
    {
        for (auto* s : { &drive, &comp, &wow, &flutter, &noiseLevel, &age, &mix, &instability, &baseDelay })
            s->snapToTarget();
        firstUpdate = false;
    }
}

void Tape::process (float* const* channels, int numChannels, int numSamples) noexcept
{
    numChannels = std::min (numChannels, maxChannels);

    if (mix.getTarget() == 0.0f && mix.getCurrent() < 1.0e-6f)
    {
        mix.snapToTarget();
        for (int ch = 0; ch < numChannels; ++ch)
            for (int i = 0; i < numSamples; ++i)
                delays[static_cast<std::size_t> (ch)].push (channels[ch][i]);
        return;
    }

    const auto sr = static_cast<float> (sampleRate);
    const auto msToSamples = 0.001f * sr;

    for (int i = 0; i < numSamples; ++i)
    {
        const auto g = drive.next();
        const auto c = comp.next();
        const auto w = wow.next();
        const auto f = flutter.next();
        const auto n = noiseLevel.next();
        const auto a = age.next();
        const auto m = mix.next();
        const auto unstable = instability.next();
        const auto centre = baseDelay.next();

        if (i % coefficientInterval == 0)
        {
            // Head loss: 18 kHz when new, down to ~3.5 kHz when worn.
            const auto cutoff = 18000.0f * std::pow (3500.0f / 18000.0f, a);
            for (int ch = 0; ch < numChannels; ++ch)
                headLoss[static_cast<std::size_t> (ch)].setCutoff (cutoff, sampleRate, 0.75f);
        }

        // Wow: slow sine with a drifting rate; flutter: faster sine plus random scrape.
        const auto wowRate = 0.55f * (1.0f + 0.3f * wowDrift.next());
        wowPhase += wowRate / sr;
        wowPhase -= std::floor (wowPhase);
        flutterPhase += 6.5f / sr;
        flutterPhase -= std::floor (flutterPhase);

        const auto wowMod = (w + 0.5f * unstable) * maxWowMs * std::sin (twoPi * wowPhase);
        const auto flutterMod = (f + 0.5f * unstable) * maxFlutterMs
                                * (0.7f * std::sin (twoPi * flutterPhase) + 0.3f * flutterDrift.next());
        const auto modSamples = (wowMod + flutterMod) * msToSamples;

        // Dropouts: random dips in level, only with age.
        const auto dip = 1.0f - a * a * 0.6f * std::max (0.0f, dropout.next());

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const auto cc = static_cast<std::size_t> (ch);
            const auto dry = channels[ch][i];
            auto x = dc[cc].process (magnetise (dry * g)) * c;
            delays[cc].push (x);
            x = delays[cc].readCubic (std::max (1.0f, centre + modSamples));
            x = headLoss[cc].lowpass (x) * dip;
            x += n * 0.03f * hissFilter[cc].process (hiss[cc].nextBipolar());
            channels[ch][i] = dry + m * (x - dry);
        }
    }
}
} // namespace sfx::dsp
