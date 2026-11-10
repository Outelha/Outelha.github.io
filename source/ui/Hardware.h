#pragma once

#include "Theme.h"

// Procedural "hardware" drawing: brushed metal, recessed wells, screws, engraved
// text, glass displays and spun-aluminium knobs. Everything is vector or tiled
// texture, so it stays sharp at any display scaling.
namespace sfx::ui::hw
{
inline const juce::Colour chassisTop { 0xff2d2f34 };
inline const juce::Colour chassisBottom { 0xff1b1c20 };
inline const juce::Colour copper { 0xffe0703a };
inline const juce::Colour amber { 0xffffb347 };
inline const juce::Colour amberDim { 0xff7a5222 };
inline const juce::Colour glass { 0xff0d0b09 };
inline const juce::Colour engraving { 0xffc9c3b8 };

// Horizontal brushed-metal streak texture (tiled, 256 x 256).
const juce::Image& brushedTexture();

// A brushed metal plate with bevelled edges.
void drawBrushedPlate (juce::Graphics&, juce::Rectangle<float>, juce::Colour top, juce::Colour bottom, float corner, float textureOpacity = 0.5f);

// A recessed (sunken) well with inner shadow.
void drawRecess (juce::Graphics&, juce::Rectangle<float>, float corner, juce::Colour fill = juce::Colour (0xff121316));

// Phillips-head screw.
void drawScrew (juce::Graphics&, juce::Point<float> centre, float radius, float angle = 0.6f);

// Text that looks stamped into metal (dark shadow above, light edge below).
void drawEngraved (juce::Graphics&, const juce::String&, juce::Rectangle<float>, juce::Font, juce::Justification,
                   juce::Colour colour = engraving);

// Smoked glass display with subtle scanlines and reflection.
void drawGlassDisplay (juce::Graphics&, juce::Rectangle<float>, float corner);

// Spun aluminium knob with a numbered skirt. Used for the large main-view controls.
void drawAluminiumKnob (juce::Graphics&, juce::Rectangle<float>, float angle, float startAngle, float endAngle,
                        float proportion, juce::Colour accent, bool hover);

// Round LED with glow.
void drawLed (juce::Graphics&, juce::Point<float> centre, float radius, juce::Colour colour, bool on);
} // namespace sfx::ui::hw
