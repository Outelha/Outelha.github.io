#pragma once

#include "Parameters.h"
#include "Fonts.h"

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

// Each pedal gets its own enclosure finish colour (the painted / anodised metal).
inline juce::Colour pedalColour (EffectId id)
{
    switch (id)
    {
        case EffectId::saturation: return juce::Colour (0xffc0531f); // copper
        case EffectId::resampler: return juce::Colour (0xff1c5d8c);  // deep blue
        case EffectId::tape: return juce::Colour (0xffb39d7c);       // champagne bronze
        case EffectId::chorus: return juce::Colour (0xff11807a);     // teal
        case EffectId::tremolo: return juce::Colour (0xff8a2b8e);    // magenta / purple
        case EffectId::reverse: return juce::Colour (0xff8c8a93);    // violet-tinted silver
        case EffectId::delay: return juce::Colour (0xffc99a1e);      // gold
        case EffectId::reverb: return juce::Colour (0xff22389c);     // cosmic blue
    }
    return juce::Colours::grey;
}

// The light colour of each effect: displays, selection glow, value arcs, panel trim.
inline juce::Colour accentColour (EffectId id)
{
    switch (id)
    {
        case EffectId::saturation: return juce::Colour (0xffff7a1f);
        case EffectId::resampler: return juce::Colour (0xff2fd6ff);
        case EffectId::tape: return juce::Colour (0xffffa53a);
        case EffectId::chorus: return juce::Colour (0xff35eadc);
        case EffectId::tremolo: return juce::Colour (0xffff4fd6);
        case EffectId::reverse: return juce::Colour (0xffb072ff);
        case EffectId::delay: return juce::Colour (0xffffbf2e);
        case EffectId::reverb: return juce::Colour (0xff4f8dff);
    }
    return juce::Colours::orange;
}

// SHOBEATS signal orange: global controls, master section, LEDs.
inline const juce::Colour brandOrange { 0xffff7a1f };
inline const juce::Colour ledAmber { 0xffffa236 };

juce::Font uiFont (float height, bool bold = false);

// Fine grain noise tile used to give enclosures and panels a physical texture.
juce::Image noiseTexture();
void fillWithTexture (juce::Graphics&, const juce::Path& area, float opacity);
} // namespace sfx::ui
