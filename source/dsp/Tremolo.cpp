#include "Tremolo.h"

namespace sfx::dsp
{
float Tremolo::shapeValue (Shape s, float p) noexcept
{
    p -= std::floor (p);
    switch (s)
    {
        case Shape::sine:
            return 0.5f + 0.5f * std::sin (twoPi * p);
        case Shape::triangle:
            return p < 0.5f ? 2.0f * p : 2.0f - 2.0f * p;
        case Shape::square:
            // Softened edges avoid clicks while keeping the choppy character.
            return 0.5f + 0.5f * std::tanh (8.0f * std::sin (twoPi * p));
        case Shape::saw:
        {
            // Falling ramp with a short rounded reset.
            const auto ramp = 1.0f - p;
            return p < 0.97f ? ramp : ramp + (p - 0.97f) / 0.03f * (1.0f - ramp) * 1.0f;
        }
    }
    return 0.0f;
}

void Tremolo::prepare (double newSampleRate, int)
{
    sampleRate = newSampleRate;
    rate.prepare (sampleRate, 30.0f);
    depth.prepare (sampleRate, 20.0f);
    stereo.prepare (sampleRate, 30.0f);
    drift.prepare (sampleRate, 100.0f);
    instability.prepare (sampleRate, 100.0f);
    rateWander.prepare (sampleRate, 0.25f, 0x7e3e1u);
    depthFlicker.prepare (sampleRate, 5.0f, 0x7e3e2u);
    shapeFadeLength = std::max (1, static_cast<int> (sampleRate * 0.02));
    firstUpdate = true;
    reset();
}

void Tremolo::reset() noexcept
{
    phase = 0.0f;
    shapeFade = 0;
    previousShape = shape;
    for (auto* s : { &rate, &depth, &stereo, &drift, &instability })
        s->snapToTarget();
}

void Tremolo::setParameters (const Parameters& p, double bpm) noexcept
{
    if (! firstUpdate && p.shape != shape)
    {
        previousShape = shape;
        shapeFade = shapeFadeLength;
    }
    shape = p.shape;

    const auto hz = p.sync ? 1.0f / divisionSeconds (p.division, bpm) : std::clamp (p.rateHz, minRateHz, maxRateHz);
    rate.setTarget (hz);
    depth.setTarget (p.enabled ? std::clamp (p.depth, 0.0f, 1.0f) : 0.0f);
    stereo.setTarget (std::clamp (p.stereo, 0.0f, 1.0f));

    if (firstUpdate)
    {
        for (auto* s : { &rate, &depth, &stereo })
            s->snapToTarget();
        previousShape = shape;
        firstUpdate = false;
    }
}

void Tremolo::process (float* const* channels, int numChannels, int numSamples) noexcept
{
    numChannels = std::min (numChannels, 2);
    const auto invSr = 1.0f / static_cast<float> (sampleRate);

    if (depth.getTarget() == 0.0f && depth.getCurrent() < 1.0e-6f)
    {
        // Keep the LFO running so re-enabling stays in time.
        phase += rate.getTarget() * invSr * static_cast<float> (numSamples);
        phase -= std::floor (phase);
        depth.snapToTarget();
        return;
    }

    for (int i = 0; i < numSamples; ++i)
    {
        const auto r = rate.next() * (1.0f + 0.25f * drift.next() * rateWander.next());
        const auto d = depth.next() * (1.0f - 0.35f * instability.next() * std::max (0.0f, depthFlicker.next()));
        const auto st = stereo.next();

        float fade = 0.0f;
        if (shapeFade > 0)
            fade = static_cast<float> (shapeFade--) / static_cast<float> (shapeFadeLength);

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const auto p = phase + (ch == 1 ? 0.5f * st : 0.0f);
            auto lfo = shapeValue (shape, p);
            if (fade > 0.0f)
                lfo = lfo * (1.0f - fade) + shapeValue (previousShape, p) * fade;
            channels[ch][i] *= 1.0f - d * (1.0f - lfo);
        }

        phase += r * invSr;
        if (phase >= 1.0f)
            phase -= 1.0f;
    }
}
} // namespace sfx::dsp
