#include "Chorus.h"

namespace trio::dsp
{
void Chorus::prepare (double newSampleRate, int)
{
    sampleRate = newSampleRate;

    const auto maxSamples = static_cast<std::size_t> (
        std::ceil ((maxDelayMs + maxModulationMs + 2.0f) * 0.001 * sampleRate));

    for (auto& d : delays)
        d.prepare (maxSamples);

    rate.prepare (sampleRate, 50.0f);
    depthMs.prepare (sampleRate, 50.0f);
    delaySamples.prepare (sampleRate, 80.0f);
    mix.prepare (sampleRate, 20.0f);

    firstUpdate = true;
    reset();
}

void Chorus::reset() noexcept
{
    for (auto& d : delays)
        d.reset();
    lfoPhase = 0.0f;
    for (auto* s : { &rate, &depthMs, &delaySamples, &mix })
        s->snapToTarget();
}

void Chorus::setParameters (const Parameters& p) noexcept
{
    const auto centreMs = std::clamp (p.delayMs, minDelayMs, maxDelayMs);
    // Never let the swing reach the write head.
    const auto swingMs = std::clamp (p.depth, 0.0f, 1.0f) * std::min (maxModulationMs, centreMs - 1.0f);

    rate.setTarget (std::clamp (p.rateHz, minRateHz, maxRateHz));
    depthMs.setTarget (swingMs);
    delaySamples.setTarget (centreMs * 0.001f * static_cast<float> (sampleRate));
    mix.setTarget (p.enabled ? std::clamp (p.mix, 0.0f, 1.0f) : 0.0f);

    if (firstUpdate)
    {
        for (auto* s : { &rate, &depthMs, &delaySamples, &mix })
            s->snapToTarget();
        firstUpdate = false;
    }
}

void Chorus::process (float* const* channels, int numChannels, int numSamples) noexcept
{
    numChannels = std::min (numChannels, maxChannels);

    if (mix.getTarget() == 0.0f && mix.getCurrent() < 1.0e-6f)
    {
        // Keep the delay lines warm so re-enabling does not start from silence.
        for (int ch = 0; ch < numChannels; ++ch)
            for (int i = 0; i < numSamples; ++i)
                delays[static_cast<std::size_t> (ch)].push (channels[ch][i]);
        mix.snapToTarget();
        return;
    }

    const auto msToSamples = 0.001f * static_cast<float> (sampleRate);
    const auto invSampleRate = 1.0f / static_cast<float> (sampleRate);

    for (int i = 0; i < numSamples; ++i)
    {
        const auto r = rate.next();
        const auto swing = depthMs.next() * msToSamples;
        const auto centre = delaySamples.next();
        const auto m = mix.next();

        float dryGain, wetGain;
        equalPowerMix (m, dryGain, wetGain);

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto& line = delays[static_cast<std::size_t> (ch)];
            const auto dry = channels[ch][i];
            line.push (dry);

            float wet = 0.0f;
            for (int v = 0; v < voicesPerChannel; ++v)
            {
                // Voices 180 degrees apart within a channel, channels 90 degrees apart.
                const auto phase = lfoPhase + 0.5f * static_cast<float> (v) + 0.25f * static_cast<float> (ch);
                const auto lfo = std::sin (twoPi * phase);
                wet += line.readCubic (centre + swing * lfo);
            }
            wet *= 1.0f / static_cast<float> (voicesPerChannel);

            channels[ch][i] = dry * dryGain + wet * wetGain;
        }

        lfoPhase += r * invSampleRate;
        if (lfoPhase >= 1.0f)
            lfoPhase -= 1.0f;
    }
}
} // namespace trio::dsp
