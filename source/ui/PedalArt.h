#pragma once

#include "Theme.h"

#include <array>

// Painter for the eight individually designed SHOBEATS pedals. The same art is used by
// the miniature pedals on the main screen and the larger pedals of the Advanced
// pedalboard, so both always look the same and follow the same state.
//
// The painter is stateless: every visual input (on/off, selection, parameter positions,
// animation phase, signal level) is passed in, so what is drawn always reflects the real
// plugin state.
namespace sfx::ui::pedal
{
// Where each element of a pedal sits, for a given body rectangle.
struct Geometry
{
    juce::Rectangle<float> body;
    juce::Rectangle<float> knobA, knobB;       // the two face knobs (squares)
    juce::Rectangle<float> captionA, captionB; // captions under / above the knobs
    juce::Rectangle<float> window;             // display window (tubes, wave, VU, ...)
    juce::Rectangle<float> reelA, reelB;       // tape only: the two reels
    juce::Rectangle<float> dome;               // reverb only: the starry dome
    juce::Rectangle<float> plate;              // name plate
    juce::Point<float> led;
    juce::Rectangle<float> footswitch;
    float scale = 1.0f;                        // body width / reference width
};

Geometry layout (EffectId, juce::Rectangle<float> body, bool captions);

// Live inputs to the painter.
struct State
{
    bool enabled = false, selected = false, hover = false, captions = true;
    bool paintKnobs = true;       // false when real knob components sit on top
    bool paintFootswitch = true;  // false when a real footswitch component sits on top
    float knobA = 0.0f, knobB = 0.0f; // normalised knob positions (0..1)
    double time = 0.0;            // animation clock in seconds
    float level = 0.0f;           // smoothed signal level 0..1 (from the audio meters)
    float reelPhase = 0.0f;       // tape reels rotation (radians)
    // Effect-specific normalised parameters (see PedalArt.cpp for each effect's meaning).
    std::array<float, 4> extra {};
};

// The parameters behind the two face knobs. When `syncId` is set, knob A shows
// `divisionId` instead of `a` while the sync switch is on.
struct FaceKnobs
{
    const char* a;
    const char* b;
    const char* syncId = nullptr;
    const char* divisionId = nullptr;
};
FaceKnobs faceKnobs (EffectId);

// Full pedal: shadow, enclosure finish, screws, window art, captions, knobs, plate, LED, switch.
void paint (juce::Graphics&, EffectId, const Geometry&, const State&);

// Pieces, used by components that draw parts separately.
void paintReel (juce::Graphics&, juce::Rectangle<float>, float rotation, float tapeAmount);
void paintSlotBadge (juce::Graphics&, juce::Rectangle<float>, int number, bool selected, juce::Colour accent);
} // namespace sfx::ui::pedal
