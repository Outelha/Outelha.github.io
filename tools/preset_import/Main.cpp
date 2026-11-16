// Converts third-party presets with the same code the plugin uses, prints a summary and
// optionally renders every converted preset through the real processor (--audio-check)
// to report peak level and level change for a fixed test signal.

#include "PluginProcessor.h"

#include <cstdio>

int main (int argc, char* argv[])
{
    if (argc < 3)
    {
        std::fprintf (stderr, "usage: %s <zip|folder|file> <output folder> [--audio-check]\n", argv[0]);
        return 1;
    }
    juce::ScopedJuceInitialiser_GUI juceInit;
    const juce::File source (juce::File::getCurrentWorkingDirectory().getChildFile (juce::String::fromUTF8 (argv[1])));
    const juce::File out (juce::File::getCurrentWorkingDirectory().getChildFile (juce::String::fromUTF8 (argv[2])));
    const bool audioCheck = argc > 3 && juce::String (argv[3]) == "--audio-check";

    auto report = sfx::PresetImporter::importInto (source, out);
    std::printf ("Converted: %d\nPartially converted: %d\nFailed: %d\nSkipped duplicates: %d\nReport: %s\n",
                 report.converted, report.partial, report.failed, report.duplicates, report.reportFile.getFullPathName().toRawUTF8());

    if (! audioCheck)
        return report.failed > 0 ? 2 : 0;

    // Test signal: decaying chord + noise hits, peaking near -6 dBFS.
    constexpr double sr = 48000.0;
    constexpr int block = 512;
    const int n = static_cast<int> (sr * 3.0);
    juce::AudioBuffer<float> input (2, n);
    input.clear();
    juce::Random rng (42);
    for (int i = 0; i < static_cast<int> (sr * 2.0); ++i)
    {
        const auto t = static_cast<float> (i) / static_cast<float> (sr);
        float chord = 0.0f;
        for (auto f : { 220.0f, 277.18f, 329.63f, 440.0f })
            for (int h = 1; h <= 4; ++h)
                chord += std::sin (juce::MathConstants<float>::twoPi * f * static_cast<float> (h) * t) / static_cast<float> (h * h);
        chord *= 0.09f * std::exp (-1.6f * std::fmod (t, 1.0f));
        const auto hit = (rng.nextFloat() * 2.0f - 1.0f) * 0.35f * std::exp (-40.0f * std::fmod (t, 0.25f));
        input.setSample (0, i, chord + hit);
        input.setSample (1, i, chord * 0.9f + hit);
    }
    auto rmsDb = [] (const juce::AudioBuffer<float>& b, int to) {
        double acc = 0.0;
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < to; ++i)
                acc += b.getSample (ch, i) * b.getSample (ch, i);
        return 10.0 * std::log10 (acc / (2.0 * to) + 1.0e-12);
    };
    const auto inDb = rmsDb (input, static_cast<int> (sr * 2.0));

    sfx::ShobeatsAudioProcessor probe;
    probe.getPresetManager().setUserDirectory (out.getChildFile ("__none__"));
    probe.getPresetManager().setImportDirectory (out);
    const auto& presets = probe.getPresetManager().getPresets();
    int checked = 0, nonFinite = 0, over0 = 0;
    double worstPeak = -200.0, minChange = 1e9, maxChange = -1e9;
    juce::String worstName;
    for (size_t i = 0; i < presets.size(); ++i)
    {
        if (! presets[i].isImported)
            continue;
        sfx::ShobeatsAudioProcessor p;
        presets[i].applyTo (p.getState(), p.getChainOrder());
        p.setRateAndBufferSizeDetails (sr, block);
        p.prepareToPlay (sr, block);
        juce::AudioBuffer<float> buf (input);
        juce::MidiBuffer midi;
        for (int start = 0; start < n; start += block)
        {
            juce::AudioBuffer<float> chunk (buf.getArrayOfWritePointers(), 2, start, std::min (block, n - start));
            p.processBlock (chunk, midi);
        }
        bool finite = true;
        for (int ch = 0; ch < 2; ++ch)
            for (int s = 0; s < n; ++s)
                finite = finite && std::isfinite (buf.getSample (ch, s));
        const auto peakDb = juce::Decibels::gainToDecibels (buf.getMagnitude (0, n), -200.0f);
        const auto change = rmsDb (buf, static_cast<int> (sr * 2.0)) - inDb;
        ++checked;
        nonFinite += finite ? 0 : 1;
        over0 += peakDb > 0.0f ? 1 : 0;
        if (peakDb > worstPeak)
        {
            worstPeak = peakDb;
            worstName = presets[i].category + " / " + presets[i].name;
        }
        minChange = std::min (minChange, change);
        maxChange = std::max (maxChange, change);
        std::printf ("AUDIO %+6.1f dBFS peak  %+6.1f dB level  %s / %s\n", peakDb, change, presets[i].category.toRawUTF8(), presets[i].name.toRawUTF8());
    }
    std::printf ("Audio check: %d presets, non-finite output: %d, peaks above 0 dBFS: %d, loudest peak %.1f dBFS (%s), level change %.1f..%.1f dB\n",
                 checked, nonFinite, over0, worstPeak, worstName.toRawUTF8(), minChange, maxChange);
    return 0;
}
