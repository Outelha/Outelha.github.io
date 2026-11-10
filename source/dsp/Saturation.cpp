#include "Saturation.h"

namespace trio::dsp
{
namespace
{
constexpr float warmBias = 0.35f;
constexpr float referenceLevel = 0.25f; // -12 dBFS
constexpr float toneSplitHz = 1000.0f;
constexpr float toneRangeDb = 6.0f;
} // namespace

float Saturation::shape (Mode mode, float x) noexcept
{
    switch (mode)
    {
        case Mode::soft:
            return std::tanh (x);
        case Mode::warm:
            return std::tanh (x + warmBias) - std::tanh (warmBias);
        case Mode::hard:
            return std::clamp (x, -1.0f, 1.0f);
    }
    return x;
}

float Saturation::compensationGain (Mode mode, float driveGain) noexcept
{
    const auto in = referenceLevel * driveGain;
    const auto out = 0.5f * (std::abs (shape (mode, in)) + std::abs (shape (mode, -in)));
    return out > 1.0e-6f ? referenceLevel / out : 1.0f;
}

void Saturation::prepare (double newSampleRate, int)
{
    sampleRate = newSampleRate;

    for (auto* s : { &driveGain, &compGain, &mix, &lowGain, &highGain })
        s->prepare (sampleRate, 20.0f);

    modeFadeLength = std::max (1, static_cast<int> (sampleRate * 0.01)); // 10 ms
    firstUpdate = true;
    reset();
}

void Saturation::reset() noexcept
{
    for (auto& ch : state)
    {
        ch.dc.prepare (sampleRate);
        ch.dc.reset();
        ch.toneSplit.setCutoff (toneSplitHz, sampleRate);
        ch.toneSplit.reset();
    }
    modeFadeRemaining = 0;
    previousMode = params.mode;
    for (auto* s : { &driveGain, &compGain, &mix, &lowGain, &highGain })
        s->snapToTarget();
}

void Saturation::setParameters (const Parameters& p) noexcept
{
    if (! firstUpdate && p.mode != params.mode)
    {
        previousMode = params.mode;
        modeFadeRemaining = modeFadeLength;
    }

    params = p;

    const auto drive = dbToGain (std::clamp (p.driveDb, minDriveDb, maxDriveDb));
    const auto tone = std::clamp (p.tone, -1.0f, 1.0f);

    driveGain.setTarget (drive);
    compGain.setTarget (compensationGain (p.mode, drive));
    mix.setTarget (p.enabled ? std::clamp (p.mix, 0.0f, 1.0f) : 0.0f);
    lowGain.setTarget (dbToGain (-tone * toneRangeDb));
    highGain.setTarget (dbToGain (tone * toneRangeDb));

    if (firstUpdate)
    {
        for (auto* s : { &driveGain, &compGain, &mix, &lowGain, &highGain })
            s->snapToTarget();
        previousMode = p.mode;
        firstUpdate = false;
    }
}

void Saturation::process (float* const* channels, int numChannels, int numSamples) noexcept
{
    numChannels = std::min (numChannels, maxChannels);

    // Fully bypassed and settled: leave the buffer untouched (bit-exact dry).
    if (mix.getTarget() == 0.0f && mix.getCurrent() < 1.0e-6f)
    {
        mix.snapToTarget();
        return;
    }

    for (int i = 0; i < numSamples; ++i)
    {
        const auto g = driveGain.next();
        const auto comp = compGain.next();
        const auto wetAmount = mix.next();
        const auto lo = lowGain.next();
        const auto hi = highGain.next();

        float fade = 0.0f;
        if (modeFadeRemaining > 0)
            fade = static_cast<float> (modeFadeRemaining--) / static_cast<float> (modeFadeLength);

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto& st = state[static_cast<std::size_t> (ch)];
            const auto dry = channels[ch][i];
            const auto driven = dry * g;

            auto wet = shape (params.mode, driven);
            if (fade > 0.0f)
                wet = wet * (1.0f - fade) + shape (previousMode, driven) * fade;

            wet = st.dc.process (wet) * comp;

            const auto low = st.toneSplit.process (wet);
            wet = low * lo + (wet - low) * hi;

            channels[ch][i] = dry + wetAmount * (wet - dry);
        }
    }
}
} // namespace trio::dsp
