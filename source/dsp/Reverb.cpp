#include "Reverb.h"

namespace sfx::dsp
{
namespace
{
// Mutually prime-ish base lengths (ms) for the FDN lines at size scale 1.0.
constexpr std::array<float, Reverb::numLines> baseLineMs { 31.1f, 37.3f, 41.9f, 47.3f, 53.9f, 59.1f, 67.7f, 73.3f };

// Input diffusers, per channel (ms). Slightly different between L and R.
constexpr std::array<std::array<float, 4>, 2> diffuserMs { { { 4.71f, 3.59f, 12.73f, 9.31f },
                                                              { 4.93f, 3.37f, 13.11f, 8.87f } } };

constexpr float modulationDepthSamplesAt48k = 6.0f;
constexpr float modulationBaseHz = 0.3f;

// Output tap signs: two mutually orthogonal patterns for L and R.
constexpr std::array<float, Reverb::numLines> tapL { 1, -1, 1, -1, 1, -1, 1, -1 };
constexpr std::array<float, Reverb::numLines> tapR { 1, 1, -1, -1, 1, 1, -1, -1 };
constexpr std::array<float, Reverb::numLines> inputSign { 1, 1, -1, 1, -1, -1, 1, -1 };
} // namespace

float Reverb::sizeToScale (float size) const noexcept
{
    return minSizeScale + (maxSizeScale - minSizeScale) * std::clamp (size, 0.0f, 1.0f);
}

void Reverb::prepare (double newSampleRate, int)
{
    sampleRate = newSampleRate;
    const auto msToSamples = sampleRate * 0.001;

    for (auto& p : predelay)
        p.prepare (static_cast<std::size_t> (std::ceil (maxPredelayMs * msToSamples)) + 2);

    for (std::size_t ch = 0; ch < 2; ++ch)
        for (std::size_t d = 0; d < numDiffusers; ++d)
        {
            auto& ap = diffusers[ch][d];
            ap.length = std::max<std::size_t> (1, static_cast<std::size_t> (diffuserMs[ch][d] * msToSamples));
            ap.line.prepare (ap.length + 1);
        }

    const auto modDepth = modulationDepthSamplesAt48k * static_cast<float> (sampleRate / 48000.0);
    for (std::size_t i = 0; i < numLines; ++i)
    {
        const auto maxLen = baseLineMs[i] * maxSizeScale * msToSamples + modDepth + 4.0;
        lines[i].prepare (static_cast<std::size_t> (std::ceil (maxLen)));
        lineLength[i].prepare (sampleRate, 250.0f);
        modPhase[i] = static_cast<float> (i) / static_cast<float> (numLines);
    }

    predelaySamples.prepare (sampleRate, 100.0f);
    mix.prepare (sampleRate, 20.0f);
    width.prepare (sampleRate, 20.0f);

    firstUpdate = true;
    reset();
}

void Reverb::reset() noexcept
{
    for (auto& p : predelay)
        p.reset();
    for (auto& ch : diffusers)
        for (auto& ap : ch)
            ap.line.reset();
    for (auto& l : lines)
        l.reset();
    for (auto& d : damping)
        d.reset();
    for (auto& s : lineLength)
        s.snapToTarget();
    for (auto* s : { &predelaySamples, &mix, &width })
        s->snapToTarget();
    isIdle = false;
}

void Reverb::setParameters (const Parameters& p) noexcept
{
    params = p;
    params.decaySeconds = std::clamp (p.decaySeconds, minDecaySeconds, maxDecaySeconds);

    const auto msToSamples = static_cast<float> (sampleRate * 0.001);
    const auto scale = sizeToScale (p.size);
    for (std::size_t i = 0; i < numLines; ++i)
        lineLength[i].setTarget (baseLineMs[i] * scale * msToSamples);

    predelaySamples.setTarget (std::clamp (p.predelayMs, 0.0f, maxPredelayMs) * msToSamples);
    mix.setTarget (p.enabled ? std::clamp (p.mix, 0.0f, 1.0f) : 0.0f);
    width.setTarget (std::clamp (p.width, 0.0f, 1.0f));

    // Damping: in-loop low-pass from ~18 kHz (bright) down to ~1.2 kHz (dark).
    const auto nyquistSafe = static_cast<float> (sampleRate * 0.45);
    const auto cutoff = std::min (nyquistSafe, 18000.0f * std::pow (1200.0f / 18000.0f, std::clamp (p.damping, 0.0f, 1.0f)));
    for (auto& d : damping)
        d.setCutoff (cutoff, sampleRate);

    if (firstUpdate)
    {
        for (auto& s : lineLength)
            s.snapToTarget();
        for (auto* s : { &predelaySamples, &mix, &width })
            s->snapToTarget();
        firstUpdate = false;
    }
}

void Reverb::updateLoopGains() noexcept
{
    // Per-line gain so every line decays by 60 dB in decaySeconds.
    const auto rt60Samples = params.decaySeconds * static_cast<float> (sampleRate);
    for (std::size_t i = 0; i < numLines; ++i)
        lineGain[i] = std::pow (10.0f, -3.0f * lineLength[i].getCurrent() / rt60Samples);
}

void Reverb::process (float* const* channels, int numChannels, int numSamples) noexcept
{
    numChannels = std::min (numChannels, 2);
    if (numChannels <= 0)
        return;

    if (mix.getTarget() == 0.0f && mix.getCurrent() < 1.0e-6f)
    {
        // Disabled: free the CPU and clear the tail once so it does not reappear later.
        mix.snapToTarget();
        if (! isIdle)
        {
            reset();
            isIdle = true;
        }
        return;
    }
    isIdle = false;

    updateLoopGains();

    const auto modDepth = modulationDepthSamplesAt48k * static_cast<float> (sampleRate / 48000.0);
    const auto modInc = modulationBaseHz / static_cast<float> (sampleRate);
    const auto householder = 2.0f / static_cast<float> (numLines);
    const auto outputScale = 1.0f / std::sqrt (static_cast<float> (numLines));
    const bool stereo = numChannels > 1;

    std::array<float, numLines> y {};

    for (int i = 0; i < numSamples; ++i)
    {
        const auto pd = predelaySamples.next();
        const auto m = mix.next();
        const auto w = width.next();

        const auto inL = channels[0][i];
        const auto inR = stereo ? channels[1][i] : inL;

        // Pre-delay + diffusion.
        std::array<float, 2> diffused {};
        for (std::size_t ch = 0; ch < 2; ++ch)
        {
            auto& pre = predelay[ch];
            pre.push (ch == 0 ? inL : inR);
            auto x = pre.readLinear (pd);
            for (auto& ap : diffusers[ch])
                x = ap.process (x);
            diffused[ch] = x;
        }

        // Read the network.
        float sum = 0.0f;
        for (std::size_t l = 0; l < numLines; ++l)
        {
            const auto len = lineLength[l].next();
            modPhase[l] += modInc * (1.0f + 0.13f * static_cast<float> (l));
            if (modPhase[l] >= 1.0f)
                modPhase[l] -= 1.0f;
            const auto mod = modDepth * std::sin (twoPi * modPhase[l]);
            y[l] = lines[l].readLinear (len - 1.0f + mod);
            sum += lineGain[l] * damping[l].process (y[l]);
        }

        // Householder feedback: f = g*d - (2/N) * sum(g*d). Lossless, fully mixing.
        const auto reflect = householder * sum;
        float wetL = 0.0f, wetR = 0.0f;
        for (std::size_t l = 0; l < numLines; ++l)
        {
            const auto fb = lineGain[l] * damping[l].getState() - reflect;
            const auto in = inputSign[l] * diffused[l & 1u] * 0.5f;
            lines[l].push (fb + in);
            wetL += tapL[l] * y[l];
            wetR += tapR[l] * y[l];
        }
        wetL *= outputScale;
        wetR *= outputScale;

        // Width (mid/side).
        const auto mid = 0.5f * (wetL + wetR);
        const auto side = 0.5f * (wetL - wetR) * w;
        wetL = mid + side;
        wetR = mid - side;

        float dryGain, wetGain;
        equalPowerMix (m, dryGain, wetGain);

        if (stereo)
        {
            channels[0][i] = inL * dryGain + wetL * wetGain;
            channels[1][i] = inR * dryGain + wetR * wetGain;
        }
        else
        {
            channels[0][i] = inL * dryGain + mid * wetGain;
        }
    }
}
} // namespace sfx::dsp
