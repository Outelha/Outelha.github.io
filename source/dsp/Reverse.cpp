#include "Reverse.h"

namespace sfx::dsp
{
void Reverse::prepare (double newSampleRate, int)
{
    sampleRate = newSampleRate;
    // Worst case: longest segment read backwards at double speed reaches 3x its length into the past.
    const auto maxSamples = static_cast<std::size_t> (std::ceil (maxTimeMs * 0.001 * sampleRate * 3.2)) + 16;
    for (auto& b : buffers)
        b.prepare (maxSamples);
    mix.prepare (sampleRate, 20.0f);
    firstUpdate = true;
    reset();
}

void Reverse::reset() noexcept
{
    for (auto& b : buffers)
        b.reset();
    for (int h = 0; h < numHeads; ++h)
    {
        auto& head = heads[static_cast<std::size_t> (h)];
        head.length = targetSegment;
        head.rate = targetRate;
        head.t = static_cast<float> (h) * 0.5f * targetSegment;
    }
    mix.snapToTarget();
    isIdle = false;
}

void Reverse::setParameters (const Parameters& p, double bpm) noexcept
{
    const auto seconds = p.sync ? divisionSeconds (p.division, bpm)
                                : std::clamp (p.timeMs, minTimeMs, maxTimeMs) * 0.001f;
    targetSegment = std::clamp (seconds, minTimeMs * 0.001f, maxTimeMs * 0.001f) * static_cast<float> (sampleRate);
    targetRate = std::pow (2.0f, std::clamp (p.pitchSemitones, minPitch, maxPitch) / 12.0f);
    mix.setTarget (p.enabled ? std::clamp (p.mix, 0.0f, 1.0f) : 0.0f);

    if (firstUpdate)
    {
        mix.snapToTarget();
        firstUpdate = false;
        reset();
    }
}

void Reverse::process (float* const* channels, int numChannels, int numSamples) noexcept
{
    numChannels = std::min (numChannels, maxChannels);

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

    for (int i = 0; i < numSamples; ++i)
    {
        const auto m = mix.next();
        float dryGain, wetGain;
        equalPowerMix (m, dryGain, wetGain);

        for (int ch = 0; ch < numChannels; ++ch)
            buffers[static_cast<std::size_t> (ch)].push (channels[ch][i]);

        std::array<float, maxChannels> wet {};
        for (auto& head : heads)
        {
            // Hann window; the two heads are half a segment apart so the windows sum to one.
            const auto x = head.t / head.length;
            const auto window = std::sin (pi * x) * std::sin (pi * x);
            // Reading (1 + rate) * t samples into the past plays the audio backwards at 'rate'.
            const auto delay = (1.0f + head.rate) * head.t;
            for (int ch = 0; ch < numChannels; ++ch)
                wet[static_cast<std::size_t> (ch)] += window * buffers[static_cast<std::size_t> (ch)].readLinear (delay);
            head.t += 1.0f;
        }

        // Head 0 is the master: settings are latched when it starts a new segment, and
        // head 1 is (re)locked half a segment behind it so the windows always sum to one.
        auto& h0 = heads[0];
        auto& h1 = heads[1];
        if (h0.t >= h0.length)
        {
            h0.t = 0.0f;
            // Unstable: +-15 % segment length; Drift: up to +-30 cents per segment.
            h0.length = std::max (64.0f, targetSegment * (1.0f + 0.15f * movement.unstable * noise.nextBipolar()));
            h0.rate = targetRate * std::pow (2.0f, 0.3f * movement.drift * noise.nextBipolar() / 12.0f);
            h1.length = h0.length;
            h1.t = 0.5f * h0.length;
        }
        if (h1.t >= h1.length)
        {
            h1.t -= h1.length;
            h1.rate = h0.rate;
        }

        for (int ch = 0; ch < numChannels; ++ch)
            channels[ch][i] = channels[ch][i] * dryGain + wet[static_cast<std::size_t> (ch)] * wetGain;
    }
}
} // namespace sfx::dsp
