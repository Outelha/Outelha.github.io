#pragma once

#include <juce_graphics/juce_graphics.h>

namespace sfx::ui
{
// Embedded SHOBEATS typefaces (Barlow / Barlow Condensed, SIL Open Font License).
//
// Same lifetime rule as Textures: never in static storage. Every editor's look-and-feel
// holds a juce::SharedResourcePointer<Fonts>, so the typefaces are created with the first
// editor and released with the last one while JUCE is fully alive.
struct Fonts
{
    Fonts();

    juce::Typeface::Ptr condensedSemiBold, condensedBold, condensedBoldItalic, condensedSemiBoldItalic;
    juce::Typeface::Ptr bodyMedium, bodyMediumItalic, bodySemiBold;
};

// Typographic roles used across the UI.
enum class Type
{
    label,       // engraved captions, button text: condensed semibold
    labelBold,   // plates and keys: condensed bold
    title,       // effect / preset titles: condensed bold italic
    titleLight,  // display sub-lines: condensed semibold italic
    value,       // numeric read-outs: condensed semibold
    body,        // descriptions: Barlow medium
    bodyItalic,  // italic descriptions
};

juce::Font font (Type, float height, float kerning = 0.0f);
} // namespace sfx::ui
