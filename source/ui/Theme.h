#pragma once

#include "Parameters.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace sfx::ui
{
namespace Palette
{
inline const juce::Colour background { 0xff0e0f12 };
inline const juce::Colour board { 0xff17181c };
inline const juce::Colour boardEdge { 0xff26282e };
inline const juce::Colour panel { 0xff1b1d22 };
inline const juce::Colour panelEdge { 0xff2c2f37 };
inline const juce::Colour display { 0xff101114 };
inline const juce::Colour track { 0xff343842 };
inline const juce::Colour text { 0xfff0ece4 };
inline const juce::Colour textDim { 0xff9a968e };
inline const juce::Colour textFaint { 0xff5c5a56 };
inline const juce::Colour brand { 0xffff6a2b };   // SHOBEATS signal orange
inline const juce::Colour ledOn { 0xffff3b30 };
} // namespace Palette

// Each pedal gets its own enclosure colour.
inline juce::Colour pedalColour (EffectId id)
{
    switch (id)
    {
        case EffectId::saturation: return juce::Colour (0xffd9512c);
        case EffectId::resampler: return juce::Colour (0xff2b8fd0);
        case EffectId::tape: return juce::Colour (0xffc7a46a);
        case EffectId::chorus: return juce::Colour (0xff27b394);
        case EffectId::tremolo: return juce::Colour (0xffcf4a86);
        case EffectId::reverse: return juce::Colour (0xff8a6be0);
        case EffectId::delay: return juce::Colour (0xffd9ad2f);
        case EffectId::reverb: return juce::Colour (0xff5866c4);
    }
    return juce::Colours::grey;
}

// Readable accent for drawing on dark backgrounds.
inline juce::Colour accentColour (EffectId id) { return pedalColour (id).brighter (0.25f); }

juce::Font uiFont (float height, bool bold = false);

// Fine grain noise tile used to give enclosures and panels a physical texture.
const juce::Image& noiseTexture();
void fillWithTexture (juce::Graphics&, const juce::Path& area, float opacity);
} // namespace sfx::ui
