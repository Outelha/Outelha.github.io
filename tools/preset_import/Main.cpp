// Converts third-party presets with the same code the plugin uses, prints a summary and
// optionally renders every converted preset through the real processor (--audio-check)
// to report peak level and level change for a fixed test signal.

#include "PluginProcessor.h"
#include "presets/PresetAudit.h"

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

    const auto input = sfx::audit::testSignal();
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
        const auto m = sfx::audit::measure (presets[i], input);
        const bool finite = m.finite;
        const auto peakDb = m.peakDb;
        const auto change = m.levelChangeDb;
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
