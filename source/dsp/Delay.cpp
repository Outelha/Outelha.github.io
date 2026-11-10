#include "Delay.h"

namespace sfx::dsp
{
void Delay::prepare (double newSampleRate, int)
{
    sampleRate = newSampleRate;
    const auto maxSamples = static_cast<std::size_t> (std::ceil (maxTimeMs * 0.0011 * sampleRate)) + 16;
    for (auto& l : lines)
        l.prepare (maxSamples);
    time.prepare (sampleRate, 120.0f); // tape-style glide when the time changes
    for (auto* s : { &feedback, &lowCutHz, &highCutHz, &width, &mix })
        s->prepare (sampleRate, 30.0f);
    instability.prepare (sampleRate, 100.0f);
    wobble.prepare (sampleRate, 1.3f, 0xdeadbeu);
    firstUpdate = true;
    reset();
}

void Delay::reset() noexcept
{
    for (auto& l : lines)
        l.reset();
    for (auto& f : lowCut)
        f.reset();
    for (auto& f : highCut)
        f.reset();
    for (auto* s : { &time, &feedback, &lowCutHz, &highCutHz, &width, &mix, &instability })
        s->snapToTarget();
    isIdle = false;
}

void Delay::setParameters (const Parameters& p, double bpm) noexcept
{
    const auto seconds = p.sync ? divisionSeconds (p.division, bpm) : std::clamp (p.timeMs, minTimeMs, maxTimeMs) * 0.001f;
    time.setTarget (std::clamp (seconds, minTimeMs * 0.001f, maxTimeMs * 0.001f) * static_cast<float> (sampleRate));
    feedback.setTarget (std::clamp (p.feedback, 0.0f, maxFeedback));
    lowCutHz.setTarget (std::clamp (p.lowCutHz, 20.0f, 2000.0f));
    highCutHz.setTarget (std::clamp (p.highCutHz, 1000.0f, 20000.0f));
    width.setTarget (std::clamp (p.width, 0.0f, 1.0f));
    mix.setTarget (p.enabled ? std::clamp (p.mix, 0.0f, 1.0f) : 0.0f);
    pingPong = p.pingPong;

    if (firstUpdate)
    {
        for (auto* s : { &time, &feedback, &lowCutHz, &highCutHz, &width, &mix })
            s->snapToTarget();
        firstUpdate = false;
    }
}

void Delay::process (float* const* channels, int numChannels, int numSamples) noexcept
{
    numChannels = std::min (numChannels, 2);

    if (mix.getTarget() == 0.0f && mix.getCurrent() < 1.0e-6f)
    {
        mix.snapToTarget();
        if (! isIdle)
        {
            reset();
            isIdle = true;
        }
        return;
    }
    isIdle = false;

    const bool stereo = numChannels == 2;
    const auto wobbleDepth = 0.002f * static_cast<float> (sampleRate); // up to 2 ms at full instability

    for (int i = 0; i < numSamples; ++i)
    {
        const auto t = time.next();
        const auto fb = feedback.next();
        const auto lc = lowCutHz.next();
        const auto hc = highCutHz.next();
        const auto w = width.next();
        const auto m = mix.next();
        const auto unstable = instability.next();

        if (i % coefficientInterval == 0)
            for (std::size_t c = 0; c < 2; ++c)
            {
                lowCut[c].setCutoff (lc, sampleRate);
                highCut[c].setCutoff (hc, sampleRate);
            }

        const auto readDelay = std::max (1.0f, t - 1.0f + unstable * wobbleDepth * wobble.next());

        const auto inL = channels[0][i];
        const auto inR = stereo ? channels[1][i] : inL;

        std::array<float, 2> echo { lines[0].readCubic (readDelay), lines[1].readCubic (readDelay) };

        // Filter + soft-saturate the feedback so high settings stay musical and bounded.
        std::array<float, 2> fbSig {};
        for (std::size_t c = 0; c < 2; ++c)
            fbSig[c] = std::tanh (fb * highCut[c].lowpass (lowCut[c].highpass (echo[c])));

        if (pingPong && stereo)
        {
            // Mono input enters the left line; each repeat crosses to the other side.
            lines[0].push (0.5f * (inL + inR) + fbSig[1]);
            lines[1].push (fbSig[0]);
        }
        else
        {
            lines[0].push (inL + fbSig[0]);
            lines[1].push (inR + fbSig[1]);
        }

        float dryGain, wetGain;
        equalPowerMix (m, dryGain, wetGain);

        if (stereo)
        {
            const auto mid = 0.5f * (echo[0] + echo[1]);
            const auto side = 0.5f * (echo[0] - echo[1]) * w;
            channels[0][i] = inL * dryGain + (mid + side) * wetGain;
            channels[1][i] = inR * dryGain + (mid - side) * wetGain;
        }
        else
        {
            channels[0][i] = inL * dryGain + echo[0] * wetGain;
        }
    }
}
} // namespace sfx::dsp
