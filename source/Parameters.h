#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <cstring>

namespace sfx
{
// The eight effect modules. The numeric value is the module's identity in the
// chain order and in saved state, so never reorder or renumber these.
enum class EffectId
{
    saturation = 0,
    resampler,
    tape,
    chorus,
    tremolo,
    reverse,
    delay,
    reverb
};
inline constexpr int numEffects = 8;

struct EffectInfo
{
    const char* key;         // short id used in parameter IDs and state
    const char* name;        // display name
    const char* description; // one line for the UI
    const char* enabledId;   // parameter id of the on/off switch
};

inline constexpr std::array<EffectInfo, numEffects> effectInfo { {
    { "sat", "Saturation", "Drive, tone and five distortion characters", "satEnabled" },
    { "rs", "Resampler", "Sample-rate and bit-depth reduction", "rsEnabled" },
    { "tape", "Tape", "Saturation, wow, flutter, hiss and wear", "tapeEnabled" },
    { "chorus", "Chorus", "Stereo modulated delay with feedback", "chorusEnabled" },
    { "trem", "Tremolo", "Amplitude modulation, auto-pan and sync", "tremEnabled" },
    { "rev", "Reverse", "Plays recent audio backwards in segments", "revEnabled" },
    { "delay", "Delay", "Filtered stereo echo, ping-pong and sync", "delayEnabled" },
    { "reverb", "Reverb", "8-line feedback delay network", "reverbEnabled" },
} };

inline const EffectInfo& infoFor (EffectId id) { return effectInfo[static_cast<std::size_t> (id)]; }

// Stable parameter IDs. Never rename: hosts store automation and sessions by ID.
namespace ParamID
{
inline constexpr auto inputGain = "inputGain";
inline constexpr auto outputGain = "outputGain";
inline constexpr auto globalMix = "globalMix";
inline constexpr auto drift = "drift";
inline constexpr auto unstable = "unstable";

inline constexpr auto satEnabled = "satEnabled";
inline constexpr auto satDrive = "satDrive";
inline constexpr auto satTone = "satTone";
inline constexpr auto satMode = "satMode";
inline constexpr auto satOutput = "satOutput";
inline constexpr auto satMix = "satMix";

inline constexpr auto rsEnabled = "rsEnabled";
inline constexpr auto rsRate = "rsRate";
inline constexpr auto rsBits = "rsBits";
inline constexpr auto rsFilter = "rsFilter";
inline constexpr auto rsJitter = "rsJitter";
inline constexpr auto rsMix = "rsMix";

inline constexpr auto tapeEnabled = "tapeEnabled";
inline constexpr auto tapeDrive = "tapeDrive";
inline constexpr auto tapeWow = "tapeWow";
inline constexpr auto tapeFlutter = "tapeFlutter";
inline constexpr auto tapeNoise = "tapeNoise";
inline constexpr auto tapeAge = "tapeAge";
inline constexpr auto tapeMix = "tapeMix";

inline constexpr auto chorusEnabled = "chorusEnabled";
inline constexpr auto chorusRate = "chorusRate";
inline constexpr auto chorusDepth = "chorusDepth";
inline constexpr auto chorusDelay = "chorusDelay";
inline constexpr auto chorusFeedback = "chorusFeedback";
inline constexpr auto chorusWidth = "chorusWidth";
inline constexpr auto chorusMix = "chorusMix";

inline constexpr auto tremEnabled = "tremEnabled";
inline constexpr auto tremRate = "tremRate";
inline constexpr auto tremSync = "tremSync";
inline constexpr auto tremDivision = "tremDivision";
inline constexpr auto tremDepth = "tremDepth";
inline constexpr auto tremShape = "tremShape";
inline constexpr auto tremStereo = "tremStereo";

inline constexpr auto revEnabled = "revEnabled";
inline constexpr auto revTime = "revTime";
inline constexpr auto revSync = "revSync";
inline constexpr auto revDivision = "revDivision";
inline constexpr auto revPitch = "revPitch";
inline constexpr auto revMix = "revMix";

inline constexpr auto delayEnabled = "delayEnabled";
inline constexpr auto delayTime = "delayTime";
inline constexpr auto delaySync = "delaySync";
inline constexpr auto delayDivision = "delayDivision";
inline constexpr auto delayFeedback = "delayFeedback";
inline constexpr auto delayLowCut = "delayLowCut";
inline constexpr auto delayHighCut = "delayHighCut";
inline constexpr auto delayPingPong = "delayPingPong";
inline constexpr auto delayWidth = "delayWidth";
inline constexpr auto delayMix = "delayMix";

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

// Every parameter id, in a fixed order (used for caching and tests).
inline constexpr std::size_t numParameters = 61;
extern const std::array<const char*, numParameters> allParameterIds;

// Lock-free read access to parameter values from the audio thread. Pointers to the
// parameters' atomic values are cached once at construction.
class ParameterReader
{
public:
    explicit ParameterReader (juce::AudioProcessorValueTreeState& state);

    float get (const char* id) const noexcept
    {
        // Fast path: identical pointer. Compilers are not required to merge identical
        // string literals across translation units (MSVC often does not), so fall back
        // to comparing the text. No allocation either way.
        for (std::size_t i = 0; i < allParameterIds.size(); ++i)
            if (allParameterIds[i] == id)
                return values[i]->load (std::memory_order_relaxed);
        for (std::size_t i = 0; i < allParameterIds.size(); ++i)
            if (std::strcmp (allParameterIds[i], id) == 0)
                return values[i]->load (std::memory_order_relaxed);
        jassertfalse; // unknown parameter id
        return 0.0f;
    }
    bool on (const char* id) const noexcept { return get (id) > 0.5f; }
    int choice (const char* id) const noexcept { return juce::roundToInt (get (id)); }

private:
    std::array<std::atomic<float>*, numParameters> values {};
};
} // namespace sfx
