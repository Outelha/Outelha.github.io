#include "PresetAudit.h"

#include "PluginProcessor.h"

namespace sfx::audit
{
juce::AudioBuffer<float> testSignal (double signalSeconds, double totalSeconds)
{
    const auto n = static_cast<int> (sampleRate * totalSeconds);
    juce::AudioBuffer<float> b (2, n);
    b.clear();
    juce::Random rng (42);
    for (int i = 0; i < static_cast<int> (sampleRate * signalSeconds); ++i)
    {
        const auto t = static_cast<float> (i) / static_cast<float> (sampleRate);
        float chord = 0.0f;
        for (auto f : { 220.0f, 277.18f, 329.63f, 440.0f })
            for (int h = 1; h <= 4; ++h)
                chord += std::sin (juce::MathConstants<float>::twoPi * f * static_cast<float> (h) * t) / static_cast<float> (h * h);
        chord *= 0.09f * std::exp (-1.6f * std::fmod (t, 1.0f));
        const auto hit = (rng.nextFloat() * 2.0f - 1.0f) * 0.35f * std::exp (-40.0f * std::fmod (t, 0.25f));
        b.setSample (0, i, chord + hit);
        b.setSample (1, i, chord * 0.9f + hit);
    }
    return b;
}

namespace
{
double rmsDb (const juce::AudioBuffer<float>& b, int from, int to)
{
    double acc = 0.0;
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
        for (int i = from; i < to; ++i)
            acc += b.getSample (ch, i) * b.getSample (ch, i);
    return 10.0 * std::log10 (acc / (b.getNumChannels() * std::max (1, to - from)) + 1.0e-12);
}

std::vector<float> features (const juce::AudioBuffer<float>& b)
{
    constexpr int frame = 2400; // 50 ms
    std::vector<float> f;
    for (int start = 0; start + frame <= b.getNumSamples(); start += frame)
    {
        double l = 0, r = 0, side = 0, bright = 0;
        for (int i = start; i < start + frame; ++i)
        {
            const auto a = b.getSample (0, i), c = b.getSample (1, i);
            l += a * a;
            r += c * c;
            side += (a - c) * (a - c);
            const auto d = i > 0 ? a - b.getSample (0, i - 1) : 0.0f;
            bright += d * d;
        }
        for (auto v : { l, r, side, bright })
            f.push_back (static_cast<float> (10.0 * std::log10 (v / frame + 1.0e-9)));
    }
    return f;
}
} // namespace

AudioMetrics measure (const Preset& preset, const juce::AudioBuffer<float>& input, double signalSeconds)
{
    ShobeatsAudioProcessor p;
    preset.applyTo (p.getState(), p.getChainOrder());
    p.setRateAndBufferSizeDetails (sampleRate, blockSize);
    p.prepareToPlay (sampleRate, blockSize);

    juce::AudioBuffer<float> out (input);
    juce::MidiBuffer midi;
    const auto n = out.getNumSamples();
    const auto t0 = juce::Time::getMillisecondCounterHiRes();
    for (int start = 0; start < n; start += blockSize)
    {
        juce::AudioBuffer<float> chunk (out.getArrayOfWritePointers(), 2, start, std::min (blockSize, n - start));
        p.processBlock (chunk, midi);
    }
    const auto elapsed = (juce::Time::getMillisecondCounterHiRes() - t0) * 0.001;

    AudioMetrics m;
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < n; ++i)
            m.finite = m.finite && std::isfinite (out.getSample (ch, i));
    m.peakDb = juce::Decibels::gainToDecibels (out.getMagnitude (0, n), -200.0f);
    const auto signalEnd = std::min (n, static_cast<int> (sampleRate * signalSeconds));
    m.levelChangeDb = rmsDb (out, 0, signalEnd) - rmsDb (input, 0, signalEnd);

    // Tail: compare the first and last quarter-second after the input stops.
    const auto window = static_cast<int> (sampleRate * 0.25);
    if (n - signalEnd >= 2 * window)
        m.tailGrowthDb = rmsDb (out, n - window, n) - rmsDb (out, signalEnd, signalEnd + window);

    m.realtimeFactor = elapsed / (static_cast<double> (n) / sampleRate);
    m.features = features (out);
    return m;
}

double featureDistance (const std::vector<float>& a, const std::vector<float>& b)
{
    const auto n = std::min (a.size(), b.size());
    if (n == 0)
        return 0.0;
    double d = 0.0;
    for (size_t i = 0; i < n; ++i)
        d += std::abs (a[i] - b[i]);
    return d / static_cast<double> (n);
}
} // namespace sfx::audit
