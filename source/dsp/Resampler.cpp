#include "Resampler.h"

namespace sfx::dsp
{
void Resampler::prepare (double newSampleRate, int)
{
    sampleRate = newSampleRate;
    rate.prepare (sampleRate, 30.0f);
    bits.prepare (sampleRate, 30.0f);
    filterHz.prepare (sampleRate, 30.0f);
    jitter.prepare (sampleRate, 30.0f);
    mix.prepare (sampleRate, 20.0f);
    firstUpdate = true;
    reset();
}

void Resampler::reset() noexcept
{
    phase = 1.0f;
    held = {};
    for (auto& f : filters)
        f.reset();
    for (auto* s : { &rate, &bits, &filterHz, &jitter, &mix })
        s->snapToTarget();
}

void Resampler::setParameters (const Parameters& p) noexcept
{
    rate.setTarget (std::clamp (p.rateHz, minRateHz, maxRateHz));
    bits.setTarget (std::clamp (p.bits, minBits, maxBits));
    filterHz.setTarget (std::clamp (p.filterHz, minFilterHz, maxFilterHz));
    jitter.setTarget (std::clamp (p.jitter, 0.0f, 1.0f));
    mix.setTarget (p.enabled ? std::clamp (p.mix, 0.0f, 1.0f) : 0.0f);

    if (firstUpdate)
    {
        for (auto* s : { &rate, &bits, &filterHz, &jitter, &mix })
            s->snapToTarget();
        firstUpdate = false;
    }
}

void Resampler::process (float* const* channels, int numChannels, int numSamples) noexcept
{
    numChannels = std::min (numChannels, maxChannels);

    if (mix.getTarget() == 0.0f && mix.getCurrent() < 1.0e-6f)
    {
        mix.snapToTarget();
        return;
    }

    const auto sr = static_cast<float> (sampleRate);

    for (int i = 0; i < numSamples; ++i)
    {
        const auto r = rate.next();
        const auto b = bits.next();
        const auto fc = filterHz.next();
        const auto j = jitter.next();
        const auto m = mix.next();

        if (i % coefficientInterval == 0)
            for (int ch = 0; ch < numChannels; ++ch)
                filters[static_cast<std::size_t> (ch)].setCutoff (fc, sampleRate, 0.9f);

        // Sample clock: when the phase wraps, grab a new sample. Jitter randomises each period.
        phase += clockIncrement;
        const bool sampleNow = phase >= 1.0f;
        if (sampleNow)
        {
            phase -= std::floor (phase);
            clockIncrement = std::min (1.0f, r / sr) * (1.0f + 0.9f * j * noise.nextBipolar());
            clockIncrement = std::max (clockIncrement, 1.0e-4f);
        }

        // Quantiser with a continuous (fractional) bit depth.
        const auto levels = std::pow (2.0f, b - 1.0f);

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const auto c = static_cast<std::size_t> (ch);
            const auto dry = channels[ch][i];
            if (sampleNow)
                held[c] = std::round (std::clamp (dry, -1.0f, 1.0f) * levels) / levels;

            const auto wet = filters[c].lowpass (held[c]);
            channels[ch][i] = dry + m * (wet - dry);
        }
    }
}
} // namespace sfx::dsp
