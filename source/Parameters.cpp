#include "Parameters.h"

#include "dsp/Chorus.h"
#include "dsp/Reverb.h"
#include "dsp/Saturation.h"

namespace trio
{
namespace
{
using Attributes = juce::AudioParameterFloatAttributes;

juce::ParameterID pid (const char* id) { return { id, parameterVersion }; }

juce::NormalisableRange<float> skewedRange (float min, float max, float centre, float interval = 0.0f)
{
    juce::NormalisableRange<float> r { min, max, interval };
    r.setSkewForCentre (centre);
    return r;
}

Attributes decibels (int decimals = 1)
{
    return Attributes {}
        .withLabel ("dB")
        .withStringFromValueFunction ([decimals] (float v, int) {
            return (v > 0.0f ? "+" : "") + juce::String (v, decimals) + " dB";
        })
        .withValueFromStringFunction ([] (const juce::String& s) { return s.retainCharacters ("-+0123456789.").getFloatValue(); });
}

Attributes percent()
{
    return Attributes {}
        .withLabel ("%")
        .withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; })
        .withValueFromStringFunction ([] (const juce::String& s) { return s.retainCharacters ("-0123456789.").getFloatValue() / 100.0f; });
}

Attributes bipolarPercent()
{
    return Attributes {}
        .withLabel ("%")
        .withStringFromValueFunction ([] (float v, int) {
            const auto p = juce::roundToInt (v * 100.0f);
            return (p > 0 ? "+" : "") + juce::String (p) + " %";
        })
        .withValueFromStringFunction ([] (const juce::String& s) { return s.retainCharacters ("-+0123456789.").getFloatValue() / 100.0f; });
}

Attributes withUnit (const juce::String& unit, int decimals)
{
    return Attributes {}
        .withLabel (unit)
        .withStringFromValueFunction ([unit, decimals] (float v, int) { return juce::String (v, decimals) + " " + unit; })
        .withValueFromStringFunction ([] (const juce::String& s) { return s.retainCharacters ("-0123456789.").getFloatValue(); });
}

Attributes seconds()
{
    return Attributes {}
        .withLabel ("s")
        .withStringFromValueFunction ([] (float v, int) { return juce::String (v, v < 10.0f ? 2 : 1) + " s"; })
        .withValueFromStringFunction ([] (const juce::String& s) { return s.retainCharacters ("0123456789.").getFloatValue(); });
}

std::atomic<float>& raw (juce::AudioProcessorValueTreeState& s, const char* id)
{
    auto* p = s.getRawParameterValue (id);
    jassert (p != nullptr);
    return *p;
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    // Global
    layout.add (std::make_unique<AudioParameterFloat> (pid (ParamID::inputGain), "Input", NormalisableRange<float> { -24.0f, 24.0f, 0.01f }, 0.0f, decibels()));
    layout.add (std::make_unique<AudioParameterFloat> (pid (ParamID::outputGain), "Output", NormalisableRange<float> { -24.0f, 24.0f, 0.01f }, 0.0f, decibels()));

    // Saturation
    auto sat = std::make_unique<AudioProcessorParameterGroup> ("saturation", "Saturation", " | ");
    sat->addChild (std::make_unique<AudioParameterBool> (pid (ParamID::satEnabled), "Saturation On", true));
    sat->addChild (std::make_unique<AudioParameterFloat> (pid (ParamID::satDrive), "Drive",
                                                          NormalisableRange<float> { dsp::Saturation::minDriveDb, dsp::Saturation::maxDriveDb, 0.01f },
                                                          12.0f, decibels()));
    sat->addChild (std::make_unique<AudioParameterFloat> (pid (ParamID::satTone), "Tone", NormalisableRange<float> { -1.0f, 1.0f, 0.001f }, 0.0f, bipolarPercent()));
    sat->addChild (std::make_unique<AudioParameterChoice> (pid (ParamID::satMode), "Character", StringArray { "Soft", "Warm", "Hard" }, 0));
    sat->addChild (std::make_unique<AudioParameterFloat> (pid (ParamID::satMix), "Saturation Mix", NormalisableRange<float> { 0.0f, 1.0f, 0.001f }, 1.0f, percent()));
    layout.add (std::move (sat));

    // Chorus
    auto chorus = std::make_unique<AudioProcessorParameterGroup> ("chorus", "Chorus", " | ");
    chorus->addChild (std::make_unique<AudioParameterBool> (pid (ParamID::chorusEnabled), "Chorus On", true));
    chorus->addChild (std::make_unique<AudioParameterFloat> (pid (ParamID::chorusRate), "Rate",
                                                             skewedRange (dsp::Chorus::minRateHz, dsp::Chorus::maxRateHz, 0.8f, 0.001f),
                                                             0.8f, withUnit ("Hz", 2)));
    chorus->addChild (std::make_unique<AudioParameterFloat> (pid (ParamID::chorusDepth), "Depth", NormalisableRange<float> { 0.0f, 1.0f, 0.001f }, 0.5f, percent()));
    chorus->addChild (std::make_unique<AudioParameterFloat> (pid (ParamID::chorusDelay), "Delay",
                                                             NormalisableRange<float> { dsp::Chorus::minDelayMs, dsp::Chorus::maxDelayMs, 0.01f },
                                                             12.0f, withUnit ("ms", 1)));
    chorus->addChild (std::make_unique<AudioParameterFloat> (pid (ParamID::chorusMix), "Chorus Mix", NormalisableRange<float> { 0.0f, 1.0f, 0.001f }, 0.5f, percent()));
    layout.add (std::move (chorus));

    // Reverb
    auto reverb = std::make_unique<AudioProcessorParameterGroup> ("reverb", "Reverb", " | ");
    reverb->addChild (std::make_unique<AudioParameterBool> (pid (ParamID::reverbEnabled), "Reverb On", true));
    reverb->addChild (std::make_unique<AudioParameterFloat> (pid (ParamID::reverbSize), "Size", NormalisableRange<float> { 0.0f, 1.0f, 0.001f }, 0.6f, percent()));
    reverb->addChild (std::make_unique<AudioParameterFloat> (pid (ParamID::reverbDecay), "Decay",
                                                             skewedRange (dsp::Reverb::minDecaySeconds, dsp::Reverb::maxDecaySeconds, 2.5f, 0.01f),
                                                             2.5f, seconds()));
    reverb->addChild (std::make_unique<AudioParameterFloat> (pid (ParamID::reverbDamping), "Damping", NormalisableRange<float> { 0.0f, 1.0f, 0.001f }, 0.4f, percent()));
    reverb->addChild (std::make_unique<AudioParameterFloat> (pid (ParamID::reverbPredelay), "Pre-Delay",
                                                             skewedRange (0.0f, dsp::Reverb::maxPredelayMs, 40.0f, 0.1f),
                                                             20.0f, withUnit ("ms", 0)));
    reverb->addChild (std::make_unique<AudioParameterFloat> (pid (ParamID::reverbWidth), "Width", NormalisableRange<float> { 0.0f, 1.0f, 0.001f }, 1.0f, percent()));
    reverb->addChild (std::make_unique<AudioParameterFloat> (pid (ParamID::reverbMix), "Reverb Mix", NormalisableRange<float> { 0.0f, 1.0f, 0.001f }, 0.3f, percent()));
    layout.add (std::move (reverb));

    return layout;
}

ParameterRefs::ParameterRefs (juce::AudioProcessorValueTreeState& s)
    : inputGain (raw (s, ParamID::inputGain)),
      outputGain (raw (s, ParamID::outputGain)),
      satEnabled (raw (s, ParamID::satEnabled)),
      satDrive (raw (s, ParamID::satDrive)),
      satTone (raw (s, ParamID::satTone)),
      satMode (raw (s, ParamID::satMode)),
      satMix (raw (s, ParamID::satMix)),
      chorusEnabled (raw (s, ParamID::chorusEnabled)),
      chorusRate (raw (s, ParamID::chorusRate)),
      chorusDepth (raw (s, ParamID::chorusDepth)),
      chorusDelay (raw (s, ParamID::chorusDelay)),
      chorusMix (raw (s, ParamID::chorusMix)),
      reverbEnabled (raw (s, ParamID::reverbEnabled)),
      reverbSize (raw (s, ParamID::reverbSize)),
      reverbDecay (raw (s, ParamID::reverbDecay)),
      reverbDamping (raw (s, ParamID::reverbDamping)),
      reverbPredelay (raw (s, ParamID::reverbPredelay)),
      reverbWidth (raw (s, ParamID::reverbWidth)),
      reverbMix (raw (s, ParamID::reverbMix))
{
}
} // namespace trio
