#include "Parameters.h"

#include "dsp/Chorus.h"
#include "dsp/Delay.h"
#include "dsp/Resampler.h"
#include "dsp/Reverb.h"
#include "dsp/Reverse.h"
#include "dsp/Saturation.h"
#include "dsp/Tape.h"
#include "dsp/Tremolo.h"

namespace sfx
{
const std::array<const char*, numParameters> allParameterIds {
    ParamID::inputGain, ParamID::outputGain, ParamID::globalMix,
    ParamID::satEnabled, ParamID::satDrive, ParamID::satTone, ParamID::satMode, ParamID::satOutput, ParamID::satMix,
    ParamID::rsEnabled, ParamID::rsRate, ParamID::rsBits, ParamID::rsFilter, ParamID::rsJitter, ParamID::rsMix,
    ParamID::tapeEnabled, ParamID::tapeDrive, ParamID::tapeWow, ParamID::tapeFlutter, ParamID::tapeNoise, ParamID::tapeAge, ParamID::tapeMix,
    ParamID::chorusEnabled, ParamID::chorusRate, ParamID::chorusDepth, ParamID::chorusDelay, ParamID::chorusFeedback, ParamID::chorusWidth, ParamID::chorusMix,
    ParamID::tremEnabled, ParamID::tremRate, ParamID::tremSync, ParamID::tremDivision, ParamID::tremDepth, ParamID::tremShape, ParamID::tremStereo,
    ParamID::revEnabled, ParamID::revTime, ParamID::revSync, ParamID::revDivision, ParamID::revPitch, ParamID::revMix,
    ParamID::delayEnabled, ParamID::delayTime, ParamID::delaySync, ParamID::delayDivision, ParamID::delayFeedback,
    ParamID::delayLowCut, ParamID::delayHighCut, ParamID::delayPingPong, ParamID::delayWidth, ParamID::delayMix,
    ParamID::reverbEnabled, ParamID::reverbSize, ParamID::reverbDecay, ParamID::reverbDamping, ParamID::reverbPredelay,
    ParamID::reverbWidth, ParamID::reverbMix,
};

ParameterReader::ParameterReader (juce::AudioProcessorValueTreeState& state)
{
    for (std::size_t i = 0; i < allParameterIds.size(); ++i)
    {
        values[i] = state.getRawParameterValue (allParameterIds[i]);
        jassert (values[i] != nullptr);
    }
}

namespace
{
using namespace juce;
using Attributes = AudioParameterFloatAttributes;

ParameterID pid (const char* id) { return { id, parameterVersion }; }

NormalisableRange<float> range (float min, float max, float interval = 0.0f) { return { min, max, interval }; }

NormalisableRange<float> skewed (float min, float max, float centre, float interval = 0.0f)
{
    NormalisableRange<float> r { min, max, interval };
    r.setSkewForCentre (centre);
    return r;
}

String trimmed (float v, int decimals) { return String (v, decimals); }

Attributes decibels()
{
    return Attributes {}
        .withLabel ("dB")
        .withStringFromValueFunction ([] (float v, int) { return (v > 0.0f ? "+" : "") + trimmed (v, 1) + " dB"; })
        .withValueFromStringFunction ([] (const String& s) { return s.retainCharacters ("-+0123456789.").getFloatValue(); });
}

Attributes percent()
{
    return Attributes {}
        .withLabel ("%")
        .withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; })
        .withValueFromStringFunction ([] (const String& s) { return s.retainCharacters ("-0123456789.").getFloatValue() / 100.0f; });
}

Attributes bipolarPercent()
{
    return Attributes {}
        .withLabel ("%")
        .withStringFromValueFunction ([] (float v, int) {
            const auto p = roundToInt (v * 100.0f);
            return (p > 0 ? "+" : "") + String (p) + " %";
        })
        .withValueFromStringFunction ([] (const String& s) { return s.retainCharacters ("-+0123456789.").getFloatValue() / 100.0f; });
}

Attributes unit (const String& u, int decimals)
{
    return Attributes {}
        .withLabel (u)
        .withStringFromValueFunction ([u, decimals] (float v, int) { return trimmed (v, decimals) + " " + u; })
        .withValueFromStringFunction ([] (const String& s) { return s.retainCharacters ("-0123456789.").getFloatValue(); });
}

Attributes hertz()
{
    return Attributes {}
        .withLabel ("Hz")
        .withStringFromValueFunction ([] (float v, int) {
            return v >= 1000.0f ? trimmed (v / 1000.0f, v >= 10000.0f ? 1 : 2) + " kHz" : String (roundToInt (v)) + " Hz";
        })
        .withValueFromStringFunction ([] (const String& s) {
            auto v = s.retainCharacters ("0123456789.").getFloatValue();
            return s.containsIgnoreCase ("k") ? v * 1000.0f : v;
        });
}

Attributes milliseconds()
{
    return Attributes {}
        .withLabel ("ms")
        .withStringFromValueFunction ([] (float v, int) {
            return v >= 1000.0f ? trimmed (v / 1000.0f, 2) + " s" : String (v, v < 10.0f ? 1 : 0) + " ms";
        })
        .withValueFromStringFunction ([] (const String& s) {
            auto v = s.retainCharacters ("0123456789.").getFloatValue();
            return s.endsWithIgnoreCase (" s") ? v * 1000.0f : v;
        });
}

Attributes semitones()
{
    return Attributes {}
        .withLabel ("st")
        .withStringFromValueFunction ([] (float v, int) { return (v > 0.0f ? "+" : "") + trimmed (v, 1) + " st"; })
        .withValueFromStringFunction ([] (const String& s) { return s.retainCharacters ("-+0123456789.").getFloatValue(); });
}

StringArray divisionChoices()
{
    StringArray names;
    for (auto* n : dsp::divisionNames)
        names.add (n);
    return names;
}

void addFloat (AudioProcessorParameterGroup& g, const char* id, const String& name, NormalisableRange<float> r, float def, Attributes a)
{
    g.addChild (std::make_unique<AudioParameterFloat> (pid (id), name, r, def, a));
}
void addBool (AudioProcessorParameterGroup& g, const char* id, const String& name, bool def)
{
    g.addChild (std::make_unique<AudioParameterBool> (pid (id), name, def));
}
void addChoice (AudioProcessorParameterGroup& g, const char* id, const String& name, const StringArray& choices, int def)
{
    g.addChild (std::make_unique<AudioParameterChoice> (pid (id), name, choices, def));
}

std::unique_ptr<AudioProcessorParameterGroup> group (const char* key, const String& name)
{
    return std::make_unique<AudioProcessorParameterGroup> (key, name, " | ");
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto global = group ("global", "Global");
    addFloat (*global, ParamID::inputGain, "Input", range (-24.0f, 24.0f, 0.01f), 0.0f, decibels());
    addFloat (*global, ParamID::globalMix, "Global Mix", range (0.0f, 1.0f, 0.001f), 1.0f, percent());
    addFloat (*global, ParamID::outputGain, "Output", range (-24.0f, 24.0f, 0.01f), 0.0f, decibels());
    layout.add (std::move (global));

    // All effects start switched off: a new instance is transparent until you choose a sound.
    auto sat = group ("saturation", "Saturation");
    addBool (*sat, ParamID::satEnabled, "Saturation On", false);
    addFloat (*sat, ParamID::satDrive, "Sat Drive", range (dsp::Saturation::minDriveDb, dsp::Saturation::maxDriveDb, 0.01f), 12.0f, decibels());
    addFloat (*sat, ParamID::satTone, "Sat Tone", range (-1.0f, 1.0f, 0.001f), 0.0f, bipolarPercent());
    addChoice (*sat, ParamID::satMode, "Sat Character", { "Soft", "Warm", "Hard", "Fuzz", "Fold" }, 0);
    addFloat (*sat, ParamID::satOutput, "Sat Output", range (dsp::Saturation::minOutputDb, dsp::Saturation::maxOutputDb, 0.01f), 0.0f, decibels());
    addFloat (*sat, ParamID::satMix, "Sat Mix", range (0.0f, 1.0f, 0.001f), 1.0f, percent());
    layout.add (std::move (sat));

    auto rs = group ("resampler", "Resampler");
    addBool (*rs, ParamID::rsEnabled, "Resampler On", false);
    addFloat (*rs, ParamID::rsRate, "Rs Rate", skewed (dsp::Resampler::minRateHz, dsp::Resampler::maxRateHz, 8000.0f, 1.0f), 11025.0f, hertz());
    addFloat (*rs, ParamID::rsBits, "Rs Bits", range (dsp::Resampler::minBits, dsp::Resampler::maxBits, 0.01f), 10.0f, unit ("bit", 1));
    addFloat (*rs, ParamID::rsFilter, "Rs Filter", skewed (dsp::Resampler::minFilterHz, dsp::Resampler::maxFilterHz, 3000.0f, 1.0f), 9000.0f, hertz());
    addFloat (*rs, ParamID::rsJitter, "Rs Jitter", range (0.0f, 1.0f, 0.001f), 0.0f, percent());
    addFloat (*rs, ParamID::rsMix, "Rs Mix", range (0.0f, 1.0f, 0.001f), 1.0f, percent());
    layout.add (std::move (rs));

    auto tape = group ("tape", "Tape");
    addBool (*tape, ParamID::tapeEnabled, "Tape On", false);
    addFloat (*tape, ParamID::tapeDrive, "Tape Drive", range (0.0f, dsp::Tape::maxDriveDb, 0.01f), 6.0f, decibels());
    addFloat (*tape, ParamID::tapeWow, "Tape Wow", range (0.0f, 1.0f, 0.001f), 0.3f, percent());
    addFloat (*tape, ParamID::tapeFlutter, "Tape Flutter", range (0.0f, 1.0f, 0.001f), 0.2f, percent());
    addFloat (*tape, ParamID::tapeNoise, "Tape Noise", range (0.0f, 1.0f, 0.001f), 0.1f, percent());
    addFloat (*tape, ParamID::tapeAge, "Tape Age", range (0.0f, 1.0f, 0.001f), 0.25f, percent());
    addFloat (*tape, ParamID::tapeMix, "Tape Mix", range (0.0f, 1.0f, 0.001f), 1.0f, percent());
    layout.add (std::move (tape));

    auto chorus = group ("chorus", "Chorus");
    addBool (*chorus, ParamID::chorusEnabled, "Chorus On", false);
    addFloat (*chorus, ParamID::chorusRate, "Chorus Rate", skewed (dsp::Chorus::minRateHz, dsp::Chorus::maxRateHz, 0.8f, 0.001f), 0.8f, unit ("Hz", 2));
    addFloat (*chorus, ParamID::chorusDepth, "Chorus Depth", range (0.0f, 1.0f, 0.001f), 0.5f, percent());
    addFloat (*chorus, ParamID::chorusDelay, "Chorus Delay", range (dsp::Chorus::minDelayMs, dsp::Chorus::maxDelayMs, 0.01f), 12.0f, unit ("ms", 1));
    addFloat (*chorus, ParamID::chorusFeedback, "Chorus Feedback", range (0.0f, dsp::Chorus::maxFeedback, 0.001f), 0.0f, percent());
    addFloat (*chorus, ParamID::chorusWidth, "Chorus Width", range (0.0f, 1.0f, 0.001f), 1.0f, percent());
    addFloat (*chorus, ParamID::chorusMix, "Chorus Mix", range (0.0f, 1.0f, 0.001f), 0.5f, percent());
    layout.add (std::move (chorus));

    auto trem = group ("tremolo", "Tremolo");
    addBool (*trem, ParamID::tremEnabled, "Tremolo On", false);
    addFloat (*trem, ParamID::tremRate, "Trem Rate", skewed (dsp::Tremolo::minRateHz, dsp::Tremolo::maxRateHz, 4.0f, 0.001f), 4.0f, unit ("Hz", 2));
    addBool (*trem, ParamID::tremSync, "Trem Sync", false);
    addChoice (*trem, ParamID::tremDivision, "Trem Division", divisionChoices(), 7);
    addFloat (*trem, ParamID::tremDepth, "Trem Depth", range (0.0f, 1.0f, 0.001f), 0.6f, percent());
    addChoice (*trem, ParamID::tremShape, "Trem Shape", { "Sine", "Triangle", "Square", "Saw" }, 0);
    addFloat (*trem, ParamID::tremStereo, "Trem Stereo", range (0.0f, 1.0f, 0.001f), 0.0f, percent());
    layout.add (std::move (trem));

    auto rev = group ("reverse", "Reverse");
    addBool (*rev, ParamID::revEnabled, "Reverse On", false);
    addFloat (*rev, ParamID::revTime, "Rev Time", skewed (dsp::Reverse::minTimeMs, dsp::Reverse::maxTimeMs, 500.0f, 0.1f), 500.0f, milliseconds());
    addBool (*rev, ParamID::revSync, "Rev Sync", false);
    addChoice (*rev, ParamID::revDivision, "Rev Division", divisionChoices(), 4);
    addFloat (*rev, ParamID::revPitch, "Rev Pitch", range (dsp::Reverse::minPitch, dsp::Reverse::maxPitch, 0.01f), 0.0f, semitones());
    addFloat (*rev, ParamID::revMix, "Rev Mix", range (0.0f, 1.0f, 0.001f), 0.5f, percent());
    layout.add (std::move (rev));

    auto delay = group ("delay", "Delay");
    addBool (*delay, ParamID::delayEnabled, "Delay On", false);
    addFloat (*delay, ParamID::delayTime, "Delay Time", skewed (dsp::Delay::minTimeMs, dsp::Delay::maxTimeMs, 300.0f, 0.1f), 375.0f, milliseconds());
    addBool (*delay, ParamID::delaySync, "Delay Sync", true);
    addChoice (*delay, ParamID::delayDivision, "Delay Division", divisionChoices(), 7);
    addFloat (*delay, ParamID::delayFeedback, "Delay Feedback", range (0.0f, dsp::Delay::maxFeedback, 0.001f), 0.4f, percent());
    addFloat (*delay, ParamID::delayLowCut, "Delay Low Cut", skewed (20.0f, 2000.0f, 200.0f, 1.0f), 120.0f, hertz());
    addFloat (*delay, ParamID::delayHighCut, "Delay High Cut", skewed (1000.0f, 20000.0f, 6000.0f, 1.0f), 8000.0f, hertz());
    addBool (*delay, ParamID::delayPingPong, "Delay Ping-Pong", false);
    addFloat (*delay, ParamID::delayWidth, "Delay Width", range (0.0f, 1.0f, 0.001f), 1.0f, percent());
    addFloat (*delay, ParamID::delayMix, "Delay Mix", range (0.0f, 1.0f, 0.001f), 0.3f, percent());
    layout.add (std::move (delay));

    auto reverb = group ("reverb", "Reverb");
    addBool (*reverb, ParamID::reverbEnabled, "Reverb On", false);
    addFloat (*reverb, ParamID::reverbSize, "Reverb Size", range (0.0f, 1.0f, 0.001f), 0.6f, percent());
    addFloat (*reverb, ParamID::reverbDecay, "Reverb Decay", skewed (dsp::Reverb::minDecaySeconds, dsp::Reverb::maxDecaySeconds, 2.5f, 0.01f), 2.5f,
              Attributes {}.withLabel ("s").withStringFromValueFunction ([] (float v, int) { return String (v, v < 10.0f ? 2 : 1) + " s"; })
                  .withValueFromStringFunction ([] (const String& s) { return s.retainCharacters ("0123456789.").getFloatValue(); }));
    addFloat (*reverb, ParamID::reverbDamping, "Reverb Damping", range (0.0f, 1.0f, 0.001f), 0.4f, percent());
    addFloat (*reverb, ParamID::reverbPredelay, "Reverb Pre-Delay", skewed (0.0f, dsp::Reverb::maxPredelayMs, 40.0f, 0.1f), 20.0f, milliseconds());
    addFloat (*reverb, ParamID::reverbWidth, "Reverb Width", range (0.0f, 1.0f, 0.001f), 1.0f, percent());
    addFloat (*reverb, ParamID::reverbMix, "Reverb Mix", range (0.0f, 1.0f, 0.001f), 0.3f, percent());
    layout.add (std::move (reverb));

    return layout;
}
} // namespace sfx
