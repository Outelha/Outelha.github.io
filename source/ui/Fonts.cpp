#include "Fonts.h"

#include "Theme.h"

#include <ShobeatsAssets.h>

namespace sfx::ui
{
namespace
{
juce::Typeface::Ptr load (const void* data, int size)
{
    return juce::Typeface::createSystemTypefaceFor (data, static_cast<size_t> (size));
}
} // namespace

Fonts::Fonts()
    : condensedSemiBold (load (ShobeatsAssets::BarlowCondensedSemiBold_ttf, ShobeatsAssets::BarlowCondensedSemiBold_ttfSize)),
      condensedBold (load (ShobeatsAssets::BarlowCondensedBold_ttf, ShobeatsAssets::BarlowCondensedBold_ttfSize)),
      condensedBoldItalic (load (ShobeatsAssets::BarlowCondensedBoldItalic_ttf, ShobeatsAssets::BarlowCondensedBoldItalic_ttfSize)),
      condensedSemiBoldItalic (load (ShobeatsAssets::BarlowCondensedSemiBoldItalic_ttf, ShobeatsAssets::BarlowCondensedSemiBoldItalic_ttfSize)),
      bodyMedium (load (ShobeatsAssets::BarlowMedium_ttf, ShobeatsAssets::BarlowMedium_ttfSize)),
      bodyMediumItalic (load (ShobeatsAssets::BarlowMediumItalic_ttf, ShobeatsAssets::BarlowMediumItalic_ttfSize)),
      bodySemiBold (load (ShobeatsAssets::BarlowSemiBold_ttf, ShobeatsAssets::BarlowSemiBold_ttfSize))
{
}

juce::Font font (Type type, float height, float kerning)
{
    const juce::SharedResourcePointer<Fonts> fonts;
    juce::Typeface::Ptr face;
    switch (type)
    {
        case Type::label: face = fonts->condensedSemiBold; break;
        case Type::labelBold: face = fonts->condensedBold; break;
        case Type::title: face = fonts->condensedBoldItalic; break;
        case Type::titleLight: face = fonts->condensedSemiBoldItalic; break;
        case Type::value: face = fonts->condensedSemiBold; break;
        case Type::body: face = fonts->bodyMedium; break;
        case Type::bodyItalic: face = fonts->bodyMediumItalic; break;
    }
    auto options = face != nullptr ? juce::FontOptions (face).withHeight (height) : juce::FontOptions (height);
    return juce::Font (options.withKerningFactor (kerning));
}

juce::Font uiFont (float height, bool bold)
{
    // Barlow Condensed is narrower than the old system font; scale up a little so existing
    // layouts keep their visual weight.
    return font (bold ? Type::labelBold : Type::label, height * 1.12f);
}
} // namespace sfx::ui
