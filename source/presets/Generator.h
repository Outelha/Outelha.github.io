#pragma once

#include "Preset.h"

namespace sfx
{
// Rule-based FX idea generator. Each vibe is a curated recipe: which pedals are
// likely, one signature pedal that is always used, a musically safe range for
// every parameter, an order template, analog-movement ranges and a loudness trim.
// Results are deterministic for a given (vibe, seed).
class Generator
{
public:
    enum class Vibe
    {
        vintage = 0,
        dreamy,
        dark,
        lofi,
        space,
        experimental
    };
    static constexpr int numVibes = 6;

    static juce::StringArray vibeNames();
    static Preset generate (Vibe vibe, juce::uint32 seed);
};
} // namespace sfx
