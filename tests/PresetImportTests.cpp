// Tests for the third-party preset importer. The fixtures below are written for these
// tests in the TONEFLX / FFWD FX file formats; they are not copies of commercial presets.

#include "PluginProcessor.h"

#include <catch2/catch_test_macros.hpp>

using sfx::Preset;
using sfx::PresetImporter;
using sfx::PresetManager;
using sfx::ShobeatsAudioProcessor;

namespace
{
struct Fixture
{
    juce::ScopedJuceInitialiser_GUI juce;
};

// TONEFLX format: explicit values, custom FXGraph order (Delay before Distortion), a pitch shift.
const char* toneflxPreset = R"(<?xml version="1.0" encoding="UTF-8"?>
<SonixAudio product="TONEFLX" version="1.5.1" PresetLocation="">
  <Core0 PresetLocation="">
    <FXGraph>
      <Module id="Delay"/><Module id="Distortion"/><Module id="Resampler"/><Module id="Tape"/><Module id="Chorus"/>
      <Module id="Modulation"/><Module id="ReverseDelay"/><Module id="Reverb"/><Module id="Pitch"/><Module id="EQ"/><Module id="Width"/>
    </FXGraph>
    <Parameters>
      <PARAM id="Core0_Delay_Active" value="1.0"/>
      <PARAM id="Core0_Delay_Sync" value="1.0"/>
      <PARAM id="Core0_Delay_NoteDivision" value="3.0"/>
      <PARAM id="Core0_Delay_NoteTiming" value="2.0"/>
      <PARAM id="Core0_Delay_Feedback" value="0.45"/>
      <PARAM id="Core0_Delay_HPF" value="300.0"/>
      <PARAM id="Core0_Delay_LPF" value="5000.0"/>
      <PARAM id="Core0_Delay_Mix" value="0.3"/>
      <PARAM id="Core0_Distortion_Active" value="1.0"/>
      <PARAM id="Core0_Distortion_Drive" value="10.0"/>
      <PARAM id="Core0_Distortion_Tone" value="-0.5"/>
      <PARAM id="Core0_Distortion_Mix" value="0.8"/>
      <PARAM id="Core0_Distortion_Mode" value="1.0"/>
      <PARAM id="Core0_Distortion_Frequency" value="500.0"/>
      <PARAM id="Core0_Distortion_Drift" value="0.2"/>
      <PARAM id="Core0_Chorus_Active" value="0.0"/>
      <PARAM id="Core0_Reverb_Active" value="0.0"/>
      <PARAM id="Core0_Pitch_Semitones" value="-4.0"/>
    </Parameters>
    <GlobalParameters Global_InputGain="-2.0" Global_OutputGain="1.5" Macro0="0.0"/>
  </Core0>
</SonixAudio>)";

// FFWD FX format: no FXGraph, parameters without values, a switch without a value.
const char* ffwdPreset = R"(<?xml version="1.0" encoding="UTF-8"?>
<FFWD product="FFWD FX" version="1.1.0" PresetLocation="">
  <Core0 PresetLocation="">
    <Parameters>
      <PARAM id="Core0_Chorus_Active" value="1.0"/>
      <PARAM id="Core0_Chorus_Depth" value="0.6"/>
      <PARAM id="Core0_Chorus_Mix"/>
      <PARAM id="Core0_Chorus_Rate" value="0.8"/>
      <PARAM id="Core0_Chorus_Stereo" value="1.0"/>
      <PARAM id="Core0_Chorus_Width" value="0.7"/>
      <PARAM id="Core0_Reverb_Active"/>
      <PARAM id="Core0_Reverb_Mix"/>
      <PARAM id="Core0_Reverb_Decay" value="80.0"/>
      <PARAM id="Core0_Resampler_Active" value="1.0"/>
      <PARAM id="Core0_Resampler_SampleRate" value="8000.0"/>
      <PARAM id="Core0_Resampler_PostFilter" value="1.0"/>
    </Parameters>
    <GlobalParameters Global_InputGain="0.0" Global_OutputGain="0.0"/>
  </Core0>
</FFWD>)";

// Single-module snippet (parameters as attributes).
const char* reverseSnippet = R"(<?xml version="1.0" encoding="UTF-8"?>
<Core0_ReverseDelay Core0_ReverseDelay_Active="1.0" Core0_ReverseDelay_Mix="0.6" Core0_ReverseDelay_NoteDivision="4.0"
  Core0_ReverseDelay_NoteTiming="0.0" Core0_ReverseDelay_Pitch="12.0" Core0_ReverseDelay_PitchActive="1.0" Core0_ReverseDelay_Feedback="0.0"
  Core0_ReverseDelay_HPF="10.0" Core0_ReverseDelay_LPF="9999.999" Core0_ReverseDelay_Type="0.0"/>)";

const char* resamplerModule = R"(<?xml version="1.0" encoding="UTF-8"?>
<Core0_Resampler Core0_Resampler_Active="1.0" Core0_Resampler_SampleRate="200.0" Core0_Resampler_Jitter="0.2"
  Core0_Resampler_Drive="1.0" Core0_Resampler_PostFilter="0.0" Core0_Resampler_PreFilter="0.0"/>)";

double num (const Preset& p, const char* id) { return static_cast<double> (p.params[id]); }

bool hasNote (const Preset& p, const juce::String& text)
{
    for (const auto& n : p.conversionNotes)
        if (n.containsIgnoreCase (text))
            return true;
    return false;
}

juce::File makeArchive (const juce::File& dir)
{
    juce::ZipFile::Builder builder;
    auto add = [&builder] (const juce::String& path, const juce::String& text) {
        builder.addEntry (std::make_unique<juce::MemoryInputStream> (text.toRawUTF8(), text.getNumBytesAsUTF8(), true), 9, path,
                          juce::Time::getCurrentTime());
    };
    add ("Presets/3. BASIC FX/DELAY (ECHOTEST)/@tester - Dotted Echo.fwd", toneflxPreset);
    add ("Presets/2. CREATIVE FX/AMBIENT/Soft Chorus Wash.fwd", ffwdPreset);
    add ("Presets/2. CREATIVE FX/MOVEMENT/Soft Chorus Wash.fwd", ffwdPreset); // same file in a second category
    add ("Presets/3. BASIC FX/REVERSE (BACKTRACK)/Octave Up.fwd", reverseSnippet);
    add ("Presets/3. BASIC FX/FILTER (LOFREQ)/Low Rate.fmod", resamplerModule);
    add ("Presets/1. SIGNATURE FX (START HERE)/broken.fwd", "this is not xml");
    add ("Presets/readme.txt", "ignored");
    add ("__MACOSX/Presets/._Octave Up.fwd", "ignored");

    auto zipFile = dir.getChildFile ("test-presets.zip");
    zipFile.deleteFile();
    juce::FileOutputStream out (zipFile);
    REQUIRE (out.openedOk());
    REQUIRE (builder.writeToStream (out, nullptr));
    return zipFile;
}
} // namespace

TEST_CASE_METHOD (Fixture, "Import: TONEFLX preset maps values, order, creator and category", "[import]")
{
    const auto r = PresetImporter::convert (toneflxPreset, "Presets/3. BASIC FX/DELAY (ECHOTEST)/! @tester - Dotted Echo.fwd");
    REQUIRE (r.status == PresetImporter::Status::partial); // the pitch shift cannot be reproduced
    const auto& p = r.preset;

    CHECK (p.name == "Dotted Echo");
    CHECK (p.creator == "@tester");
    CHECK (p.category == "Imported / Basic FX / Delay");
    CHECK (p.sourceProduct == "TONEFLX 1.5.1");
    CHECK (p.isImported);

    // Order follows FXGraph: Delay first, then Distortion; inactive modules are off.
    REQUIRE (p.chain.size() == 2);
    CHECK (p.chain[0] == static_cast<int> (sfx::EffectId::delay));
    CHECK (p.chain[1] == static_cast<int> (sfx::EffectId::saturation));

    // Linear drive 10 -> +20 dB, tone and mix kept, mode 1 -> Hard.
    CHECK (std::abs (num (p, sfx::ParamID::satDrive) - 20.0) < 1.0e-6);
    CHECK (std::abs (num (p, sfx::ParamID::satTone) + 0.5) < 1.0e-6);
    CHECK (std::abs (num (p, sfx::ParamID::satMix) - 0.8) < 1.0e-6);
    CHECK (p.params[sfx::ParamID::satMode].toString() == "Hard");

    // Division 3 (1/8) with timing 2 (dotted) -> "1/8 D".
    CHECK (static_cast<bool> (p.params[sfx::ParamID::delaySync]));
    CHECK (p.params[sfx::ParamID::delayDivision].toString() == "1/8 D");
    CHECK (std::abs (num (p, sfx::ParamID::delayFeedback) - 0.45) < 1.0e-6);
    CHECK (std::abs (num (p, sfx::ParamID::delayLowCut) - 300.0) < 1.0e-6);
    CHECK (std::abs (num (p, sfx::ParamID::delayHighCut) - 5000.0) < 1.0e-6);

    // Globals and movement.
    CHECK (std::abs (num (p, sfx::ParamID::inputGain) + 2.0) < 1.0e-6);
    CHECK (std::abs (num (p, sfx::ParamID::outputGain) - 1.5) < 1.0e-6);
    CHECK (std::abs (num (p, sfx::ParamID::drift) - 0.2) < 1.0e-6);

    CHECK (hasNote (p, "Missing: Pitch shift"));
    CHECK (hasNote (p, "Approx: Distortion: character mode"));
}

TEST_CASE_METHOD (Fixture, "Import: FFWD preset with missing values and no stored order", "[import]")
{
    const auto r = PresetImporter::convert (ffwdPreset, "Presets/2. CREATIVE FX/AMBIENT/Soft Chorus Wash.fwd");
    REQUIRE (r.status != PresetImporter::Status::failed);
    const auto& p = r.preset;

    CHECK (p.name == "Soft Chorus Wash");
    CHECK (p.creator.isEmpty());
    CHECK (p.category == "Imported / Creative FX / Ambient");

    // Reverb switch without a value: treated as off (and reported). Default order used.
    REQUIRE (p.chain.size() == 2);
    CHECK (p.chain[0] == static_cast<int> (sfx::EffectId::resampler));
    CHECK (p.chain[1] == static_cast<int> (sfx::EffectId::chorus));
    CHECK (hasNote (p, "Reverb: on/off state not stored"));
    CHECK (hasNote (p, "Processing order not stored"));

    // Chorus mix had no value: assumed source default 0.3, reported.
    CHECK (std::abs (num (p, sfx::ParamID::chorusMix) - 0.3) < 1.0e-6);
    CHECK (hasNote (p, "source defaults assumed"));
    CHECK (std::abs (num (p, sfx::ParamID::chorusWidth) - 0.7) < 1.0e-6);

    // Resampler: post filter at the target rate's Nyquist (0.45 x 8 kHz), no bit reduction.
    CHECK (std::abs (num (p, sfx::ParamID::rsRate) - 8000.0) < 1.0e-6);
    CHECK (std::abs (num (p, sfx::ParamID::rsFilter) - 3600.0) < 1.0e-6);
    CHECK (std::abs (num (p, sfx::ParamID::rsBits) - 24.0) < 1.0e-6);
}

TEST_CASE_METHOD (Fixture, "Import: module snippets and out-of-range values", "[import]")
{
    const auto rev = PresetImporter::convert (reverseSnippet, "Presets/3. BASIC FX/REVERSE (BACKTRACK)/Octave Up.fwd");
    REQUIRE (rev.status == PresetImporter::Status::converted);
    REQUIRE (rev.preset.chain.size() == 1);
    CHECK (rev.preset.chain[0] == static_cast<int> (sfx::EffectId::reverse));
    CHECK (rev.preset.params[sfx::ParamID::revDivision].toString() == "1/4");
    CHECK (std::abs (num (rev.preset, sfx::ParamID::revPitch) - 12.0) < 1.0e-6);
    CHECK (rev.preset.category == "Imported / Basic FX / Reverse");

    const auto rs = PresetImporter::convert (resamplerModule, "Presets/3. BASIC FX/FILTER (LOFREQ)/Low Rate.fmod");
    REQUIRE (rs.status != PresetImporter::Status::failed);
    CHECK (std::abs (num (rs.preset, sfx::ParamID::rsRate) - 500.0) < 1.0e-6); // clamped, reported
    CHECK (hasNote (rs.preset, "below the 500 Hz minimum"));
    CHECK (std::abs (num (rs.preset, sfx::ParamID::rsFilter) - 20000.0) < 1.0e-6); // post filter off

    const auto ffwd = PresetImporter::convert (ffwdPreset, "x/y.fwd");
    REQUIRE (! ffwd.preset.chain.empty());

    const auto bad = PresetImporter::convert ("this is not xml", "Presets/broken.fwd");
    CHECK (bad.status == PresetImporter::Status::failed);
    CHECK (bad.error.isNotEmpty());
    CHECK (PresetImporter::convert ("<Something/>", "a.fwd").status == PresetImporter::Status::failed);
}

TEST_CASE ("Import: folder names become browser categories", "[import]")
{
    CHECK (PresetImporter::categoryFromFolder ("Presets/1. SIGNATURE FX (START HERE)") == "Signature FX");
    CHECK (PresetImporter::categoryFromFolder ("Presets/2. CREATIVE FX/AMBIENT") == "Creative FX / Ambient");
    CHECK (PresetImporter::categoryFromFolder ("Presets/2. CREATIVE FX/ANALOG WARMTH") == "Creative FX / Analog Warmth");
    CHECK (PresetImporter::categoryFromFolder ("Presets/2. CREATIVE FX/MOVEMENT") == "Creative FX / Movement");
    CHECK (PresetImporter::categoryFromFolder ("Presets/2. CREATIVE FX/VINTAGE") == "Creative FX / Vintage");
    CHECK (PresetImporter::categoryFromFolder ("Presets/3. BASIC FX/CHORUS (CH-0RU5)") == "Basic FX / Chorus");
    CHECK (PresetImporter::categoryFromFolder ("Presets/3. BASIC FX/REVERB (RETROVRB)") == "Basic FX / Reverb");

    juce::String creator, name;
    PresetImporter::splitCreator ("! @someproducer - WARM KEYS II", creator, name);
    CHECK (creator == "@someproducer");
    CHECK (name == "WARM KEYS II");
    PresetImporter::splitCreator ("1-8TH DOTTED", creator, name);
    CHECK (creator.isEmpty());
    CHECK (name == "1-8TH DOTTED");
}

TEST_CASE_METHOD (Fixture, "Import: zip import is persistent, separate, deduplicated and loadable", "[import]")
{
    juce::TemporaryFile tmp;
    const auto root = tmp.getFile();
    root.createDirectory();
    const auto zip = makeArchive (root);
    const auto importDir = root.getChildFile ("Imported");
    const auto userDir = root.getChildFile ("User");

    ShobeatsAudioProcessor p;
    auto& pm = p.getPresetManager();
    pm.setUserDirectory (userDir);
    pm.setImportDirectory (importDir);
    REQUIRE (pm.getNumFactoryPresets() == 60);
    const auto factory = pm.getNumBuiltInPresets(); // factory + SHOBEATS Lab

    auto report = pm.importPresets (zip);
    CHECK (report.failed == 1);    // broken.fwd
    CHECK (report.imported() == 5); // incl. the same file in two categories
    CHECK (report.duplicates == 0);
    CHECK (report.reportFile.existsAsFile());
    CHECK (pm.getNumFactoryPresets() == 60);

    int imported = 0;
    for (const auto& preset : pm.getPresets())
        if (preset.isImported)
        {
            ++imported;
            CHECK (preset.category.startsWith ("Imported / "));
            CHECK (! preset.isFactory);
            CHECK (preset.file.isAChildOf (importDir));
        }
    CHECK (imported == 5);
    CHECK (pm.importedFolders().contains ("Creative FX / Ambient"));
    CHECK (pm.importedFolders().contains ("Creative FX / Movement"));
    CHECK (pm.importedFolders().contains ("Basic FX / Delay"));

    // Re-importing the same archive adds nothing.
    report = pm.importPresets (zip);
    CHECK (report.imported() == 0);
    CHECK (report.duplicates == 5);
    CHECK (static_cast<int> (pm.getPresets().size()) == factory + 5);

    // A fresh instance (e.g. after restarting FL Studio) finds them; loading applies the conversion.
    ShobeatsAudioProcessor q;
    q.getPresetManager().setUserDirectory (userDir);
    q.getPresetManager().setImportDirectory (importDir);
    const auto idx = q.getPresetManager().indexOf ("Dotted Echo", "Imported / Basic FX / Delay");
    REQUIRE (idx >= 0);
    REQUIRE (q.getPresetManager().load (idx));
    CHECK (q.getChainOrder().get()[0] == static_cast<int> (sfx::EffectId::delay));
    CHECK (q.getState().getRawParameterValue (sfx::ParamID::delayEnabled)->load() > 0.5f);
    CHECK (q.getState().getRawParameterValue (sfx::ParamID::reverbEnabled)->load() < 0.5f);
    CHECK (std::abs (q.getState().getRawParameterValue (sfx::ParamID::satDrive)->load() - 20.0f) < 0.02f);
    CHECK (q.getPresetManager().getPresets()[static_cast<size_t> (idx)].creator == "@tester");

    // The DAW project recalls the imported preset's name and settings.
    juce::MemoryBlock blob;
    q.getStateInformation (blob);
    ShobeatsAudioProcessor r;
    r.getPresetManager().setUserDirectory (userDir);
    r.getPresetManager().setImportDirectory (importDir);
    r.setStateInformation (blob.getData(), static_cast<int> (blob.getSize()));
    CHECK (r.getCurrentPresetLabel().first == "Dotted Echo");
    CHECK (r.getChainOrder().get() == q.getChainOrder().get());
    for (auto* id : sfx::allParameterIds)
    {
        INFO (id);
        CHECK (std::abs (r.getState().getParameter (id)->getValue() - q.getState().getParameter (id)->getValue()) < 1.0e-6f);
    }

    // Imported presets can be deleted; factory presets cannot.
    CHECK (pm.deleteUserPreset (static_cast<int> (pm.getPresets().size()) - 1));
    CHECK_FALSE (pm.deleteUserPreset (0));
    root.deleteRecursively();
}

TEST_CASE_METHOD (Fixture, "Import: converted presets render safely", "[import][audio]")
{
    constexpr double sr = 48000.0;
    constexpr int block = 512;
    for (const char* text : { toneflxPreset, ffwdPreset, reverseSnippet, resamplerModule })
    {
        const auto r = PresetImporter::convert (text, "Presets/X/test.fwd");
        REQUIRE (r.status != PresetImporter::Status::failed);
        ShobeatsAudioProcessor p;
        r.preset.applyTo (p.getState(), p.getChainOrder());
        p.setRateAndBufferSizeDetails (sr, block);
        p.prepareToPlay (sr, block);
        juce::AudioBuffer<float> buffer (2, block);
        juce::MidiBuffer midi;
        float peak = 0.0f;
        for (int b = 0; b < 200; ++b)
        {
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < block; ++i)
                    buffer.setSample (ch, i, 0.5f * std::sin (juce::MathConstants<float>::twoPi * 330.0f * static_cast<float> (b * block + i) / static_cast<float> (sr)));
            p.processBlock (buffer, midi);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < block; ++i)
                {
                    REQUIRE (std::isfinite (buffer.getSample (ch, i)));
                    peak = std::max (peak, std::abs (buffer.getSample (ch, i)));
                }
        }
        INFO (r.preset.name);
        CHECK (peak < 1.0f);
    }
}
