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

// Horizontal brushed-metal streak texture (tiled, 512 x 512).
juce::Image brushedTexture();

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

//==============================================================================
// SHOBEATS hardware design system (redesign). All functions draw in the caller's
// coordinate space and scale cleanly with the editor transform.

// Soft light spill: an elliptical radial glow (cheap; no blur pass).
void drawLightSpill (juce::Graphics&, juce::Rectangle<float> area, juce::Colour colour, float intensity = 1.0f);

// Thin illuminated seam (the orange light lines along panel edges in the references).
void drawLightSeam (juce::Graphics&, juce::Point<float> centre, float width, juce::Colour colour, float intensity = 1.0f);

// Outer glow around a rounded rectangle (selection, lit keys).
void drawRimGlow (juce::Graphics&, juce::Rectangle<float>, float corner, juce::Colour colour, float size, float intensity = 1.0f);

// Heavy machined panel: polished chrome bezel, dark brushed interior, corner screws.
void drawBezelPanel (juce::Graphics&, juce::Rectangle<float>, float corner, float bezel = 7.0f, bool screws = true);

// Dark inset tray (behind segmented switches, displays, the FX chain).
void drawTray (juce::Graphics&, juce::Rectangle<float>, float corner);

// Hardware key (push button). `lit` fills it with an illuminated accent; `toggled`
// keys glow at their rim. `connectedLeft/Right` square the corners for segmented rows.
struct KeyLook
{
    juce::Colour accent { 0xffff7a1f };
    bool lit = false;      // solid illuminated key (GENERATE, ADVANCED, selected segment)
    bool over = false, down = false;
    bool connectedLeft = false, connectedRight = false;
    float corner = 7.0f;
};
void drawKey (juce::Graphics&, juce::Rectangle<float>, const KeyLook&);

// Engraved key label with a soft light shadow (dark keys) or a dark emboss (lit keys).
void drawKeyText (juce::Graphics&, const juce::String&, juce::Rectangle<float>, juce::Font, bool lit, juce::Colour accent,
                  bool enabled = true);

// Perforated speaker grille in a chamfered housing.
void drawSpeakerGrille (juce::Graphics&, juce::Rectangle<float>);

// Diagonal vent slots.
void drawVentSlots (juce::Graphics&, juce::Rectangle<float>, int count);

// Vertical glass tube with a filament glow; `level` 0..1 drives the glow.
void drawGlassTube (juce::Graphics&, juce::Rectangle<float>, juce::Colour glow, float level);

// Vacuum tube (rounded glass envelope, glowing plate structure); `level` 0..1.
void drawVacuumTube (juce::Graphics&, juce::Rectangle<float>, juce::Colour glow, float level);

// Recessed value window ("0 %" read-outs under knobs).
void drawValueWindow (juce::Graphics&, juce::Rectangle<float>, const juce::String&, juce::Colour textColour, float fontSize = 14.0f);

// Precision machined knob used everywhere outside the pedals.
// `ringStyle`: accent value arc around a dark groove, with outer ticks.
struct KnobLook
{
    juce::Colour accent { 0xffff7a1f };
    float proportion = 0.0f;           // 0..1 value position
    float originProportion = 0.0f;     // where the value arc starts (bipolar knobs: 0.5)
    float startAngle = 0.0f, endAngle = 0.0f, angle = 0.0f;
    bool hover = false, dragging = false;
    bool numbers = false;              // print 0..10 around the scale (large knobs)
    bool ticks = true;
};
void drawMachinedKnob (juce::Graphics&, juce::Rectangle<float>, const KnobLook&);

// Small glossy black pedal knob with a white pointer.
void drawPedalKnob (juce::Graphics&, juce::Rectangle<float>, float angle, bool hover, juce::Colour skirtTint = juce::Colours::black);

// Stomp switch (chrome dome in a knurled nut), seen from above.
void drawFootswitch (juce::Graphics&, juce::Rectangle<float>, bool over, bool down);
} // namespace sfx::ui::hw
