#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace trio
{
// Stable parameter IDs. Never rename: hosts store automation and sessions by ID.
namespace ParamID
{
inline constexpr auto inputGain = "inputGain";
inline constexpr auto outputGain = "outputGain";

inline constexpr auto satEnabled = "satEnabled";
inline constexpr auto satDrive = "satDrive";
inline constexpr auto satTone = "satTone";
inline constexpr auto satMode = "satMode";
inline constexpr auto satMix = "satMix";

inline constexpr auto chorusEnabled = "chorusEnabled";
inline constexpr auto chorusRate = "chorusRate";
inline constexpr auto chorusDepth = "chorusDepth";
inline constexpr auto chorusDelay = "chorusDelay";
inline constexpr auto chorusMix = "chorusMix";

inline constexpr auto reverbEnabled = "reverbEnabled";
inline constexpr auto reverbSize = "reverbSize";
inline constexpr auto reverbDecay = "reverbDecay";
inline constexpr auto reverbDamping = "reverbDamping";
inline constexpr auto reverbPredelay = "reverbPredelay";
inline constexpr auto reverbWidth = "reverbWidth";
inline constexpr auto reverbMix = "reverbMix";
} // namespace ParamID

inline constexpr int parameterVersion = 1;

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

// Direct, lock-free access to every parameter's current value for the audio thread.
struct ParameterRefs
{
    explicit ParameterRefs (juce::AudioProcessorValueTreeState& state);

    std::atomic<float>& inputGain;
    std::atomic<float>& outputGain;

    std::atomic<float>& satEnabled;
    std::atomic<float>& satDrive;
    std::atomic<float>& satTone;
    std::atomic<float>& satMode;
    std::atomic<float>& satMix;

    std::atomic<float>& chorusEnabled;
    std::atomic<float>& chorusRate;
    std::atomic<float>& chorusDepth;
    std::atomic<float>& chorusDelay;
    std::atomic<float>& chorusMix;

    std::atomic<float>& reverbEnabled;
    std::atomic<float>& reverbSize;
    std::atomic<float>& reverbDecay;
    std::atomic<float>& reverbDamping;
    std::atomic<float>& reverbPredelay;
    std::atomic<float>& reverbWidth;
    std::atomic<float>& reverbMix;
};
} // namespace trio
