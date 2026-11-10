#include "Hardware.h"

namespace sfx::ui::hw
{
using namespace juce;

const Image& brushedTexture()
{
    static const Image tile = [] {
        constexpr int w = 512, h = 512, blend = 96;
        Image img (Image::ARGB, w, h, true);
        Random rng (0xb2b5);
        std::vector<float> row (static_cast<size_t> (w));
        for (int y = 0; y < h; ++y)
        {
            // Each row: a slowly varying brightness (the streak) plus fine grain.
            float walk = rng.nextFloat() * 2.0f - 1.0f;
            for (int x = 0; x < w; ++x)
            {
                walk = walk * 0.985f + (rng.nextFloat() * 2.0f - 1.0f) * 0.06f;
                row[static_cast<size_t> (x)] = walk + (rng.nextFloat() - 0.5f) * 0.25f;
            }
            // Crossfade the row's end into its start so the tile wraps without a seam.
            for (int x = 0; x < blend; ++x)
            {
                const auto t = static_cast<float> (x) / blend;
                auto& end = row[static_cast<size_t> (w - blend + x)];
                end = end * (1.0f - t) + row[static_cast<size_t> (x)] * t;
            }
            for (int x = 0; x < w; ++x)
            {
                const auto v = row[static_cast<size_t> (x)];
                img.setPixelAt (x, y, (v > 0.0f ? Colours::white : Colours::black).withAlpha (jlimit (0.0f, 1.0f, std::abs (v) * 0.3f)));
            }
        }
        return img;
    }();
    return tile;
}

void drawBrushedPlate (Graphics& g, Rectangle<float> r, Colour top, Colour bottom, float corner, float textureOpacity)
{
    Path plate;
    plate.addRoundedRectangle (r, corner);

    g.setColour (Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (r.translated (0.0f, 3.0f).expanded (1.0f), corner);

    g.setGradientFill (ColourGradient (top, r.getX(), r.getY(), bottom, r.getX(), r.getBottom(), false));
    g.fillPath (plate);
    {
        Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (plate);
        g.setTiledImageFill (brushedTexture(), 0, 0, textureOpacity);
        g.fillPath (plate);
        // Broad soft reflection band.
        g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.0f), r.getX(), r.getY(),
                                           Colours::white.withAlpha (0.0f), r.getRight(), r.getBottom(), false));
        ColourGradient sheen (Colours::white.withAlpha (0.0f), r.getX(), r.getY(), Colours::white.withAlpha (0.0f), r.getRight(), r.getY(), false);
        sheen.addColour (0.35, Colours::white.withAlpha (0.05f));
        sheen.addColour (0.5, Colours::white.withAlpha (0.09f));
        sheen.addColour (0.65, Colours::white.withAlpha (0.04f));
        g.setGradientFill (sheen);
        g.fillPath (plate);
    }
    // Bevel: light top edge fading out downwards, dark outline.
    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.18f), 0.0f, r.getY(), Colours::white.withAlpha (0.0f), 0.0f, r.getY() + jmin (60.0f, r.getHeight() * 0.5f), false));
    g.drawRoundedRectangle (r.reduced (1.0f), corner, 1.0f);
    g.setColour (Colours::black.withAlpha (0.6f));
    g.drawRoundedRectangle (r, corner, 1.0f);
}

void drawRecess (Graphics& g, Rectangle<float> r, float corner, Colour fill)
{
    Path well;
    well.addRoundedRectangle (r, corner);
    g.setColour (fill);
    g.fillPath (well);
    {
        Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (well);
        // Inner shadow from the top edge.
        g.setGradientFill (ColourGradient (Colours::black.withAlpha (0.55f), 0.0f, r.getY(), Colours::black.withAlpha (0.0f), 0.0f, r.getY() + 14.0f, false));
        g.fillRect (r.withHeight (14.0f));
        g.setTiledImageFill (noiseTexture(), 0, 0, 0.18f);
        g.fillPath (well);
    }
    g.setColour (Colours::black.withAlpha (0.7f));
    g.drawRoundedRectangle (r, corner, 1.2f);
    // Light catching the lower lip of the well.
    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.0f), 0.0f, r.getBottom() - jmin (40.0f, r.getHeight() * 0.5f),
                                       Colours::white.withAlpha (0.09f), 0.0f, r.getBottom(), false));
    g.drawRoundedRectangle (r.expanded (1.0f), corner + 1.0f, 1.0f);
}

void drawScrew (Graphics& g, Point<float> c, float r, float angle)
{
    g.setColour (Colours::black.withAlpha (0.5f));
    g.fillEllipse (Rectangle<float> (r * 2.0f + 2.0f, r * 2.0f + 2.0f).withCentre (c.translated (0.0f, 1.0f)));
    g.setGradientFill (ColourGradient (Colour (0xffe2e2e2), c.x - r, c.y - r, Colour (0xff5e6064), c.x + r, c.y + r, false));
    g.fillEllipse (Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c));
    g.setColour (Colour (0xff2c2c2c));
    const auto l = r * 0.62f;
    for (auto a : { angle, angle + MathConstants<float>::halfPi })
        g.drawLine (c.x - l * std::cos (a), c.y - l * std::sin (a), c.x + l * std::cos (a), c.y + l * std::sin (a), jmax (1.0f, r * 0.28f));
}

void drawEngraved (Graphics& g, const String& text, Rectangle<float> r, Font font, Justification j, Colour colour)
{
    g.setFont (font);
    g.setColour (Colours::black.withAlpha (0.8f));
    g.drawText (text, r.translated (0.0f, -1.0f), j);
    g.setColour (Colours::white.withAlpha (0.12f));
    g.drawText (text, r.translated (0.0f, 1.0f), j);
    g.setColour (colour);
    g.drawText (text, r, j);
}

void drawGlassDisplay (Graphics& g, Rectangle<float> r, float corner)
{
    drawRecess (g, r.expanded (6.0f), corner + 4.0f, Colour (0xff0a0a0b));
    Path glassPath;
    glassPath.addRoundedRectangle (r, corner);
    g.setGradientFill (ColourGradient (Colour (0xff16110c), r.getCentreX(), r.getY(), glass, r.getCentreX(), r.getBottom(), false));
    g.fillPath (glassPath);

    Graphics::ScopedSaveState save (g);
    g.reduceClipRegion (glassPath);
    // Scanlines.
    g.setColour (Colours::black.withAlpha (0.18f));
    for (auto y = r.getY(); y < r.getBottom(); y += 3.0f)
        g.fillRect (r.getX(), y, r.getWidth(), 1.0f);
    // Diagonal reflection.
    Path glare;
    glare.startNewSubPath (r.getX(), r.getY());
    glare.lineTo (r.getX() + r.getWidth() * 0.55f, r.getY());
    glare.lineTo (r.getX() + r.getWidth() * 0.35f, r.getBottom());
    glare.lineTo (r.getX(), r.getBottom());
    glare.closeSubPath();
    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.06f), r.getX(), r.getY(), Colours::white.withAlpha (0.0f), r.getX(), r.getBottom(), false));
    g.fillPath (glare);
}

void drawAluminiumKnob (Graphics& g, Rectangle<float> bounds, float angle, float startAngle, float endAngle, float proportion,
                        Colour accent, bool hover)
{
    const auto d = jmin (bounds.getWidth(), bounds.getHeight());
    const auto c = bounds.getCentre();
    const auto r = d * 0.5f;

    // Printed scale: ticks and numbers 0..10.
    const auto scaleR = r * 0.97f;
    for (int i = 0; i <= 10; ++i)
    {
        const auto a = startAngle + (endAngle - startAngle) * static_cast<float> (i) / 10.0f;
        const auto lit = static_cast<float> (i) / 10.0f <= proportion + 1.0e-4f;
        g.setColour (lit ? accent : engraving.withAlpha (0.45f));
        g.drawLine (c.x + scaleR * 0.84f * std::sin (a), c.y - scaleR * 0.84f * std::cos (a),
                    c.x + scaleR * 0.92f * std::sin (a), c.y - scaleR * 0.92f * std::cos (a), i % 5 == 0 ? 2.0f : 1.2f);
        if (i % 2 == 0 && r > 70.0f) // numbers only where they fit
        {
            g.setFont (uiFont (jmax (8.0f, r * 0.085f), true));
            const Point<float> tp { c.x + scaleR * std::sin (a), c.y - scaleR * std::cos (a) };
            g.drawText (String (i), Rectangle<float> (20.0f, 12.0f).withCentre (tp), Justification::centred);
        }
    }
    // Value arc glow in the accent colour.
    {
        Path arc;
        const auto ar = scaleR * 0.76f;
        arc.addCentredArc (c.x, c.y, ar, ar, 0.0f, startAngle, angle, true);
        g.setColour (accent.withAlpha (0.18f));
        g.strokePath (arc, PathStrokeType (r * 0.07f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (accent);
        g.strokePath (arc, PathStrokeType (r * 0.025f, PathStrokeType::curved, PathStrokeType::rounded));
    }

    // Knob body.
    const auto kr = r * 0.66f;
    const auto body = Rectangle<float> (kr * 2.0f, kr * 2.0f).withCentre (c);
    g.setColour (Colours::black.withAlpha (0.6f));
    g.fillEllipse (body.translated (0.0f, kr * 0.1f).expanded (kr * 0.06f));

    // Dark knurled skirt.
    g.setGradientFill (ColourGradient (Colour (0xff3a3c42), body.getX(), body.getY(), Colour (0xff0e0f11), body.getRight(), body.getBottom(), false));
    g.fillEllipse (body);
    g.setColour (Colours::white.withAlpha (0.06f));
    for (int i = 0; i < 60; ++i)
    {
        const auto a = angle + MathConstants<float>::twoPi * static_cast<float> (i) / 60.0f;
        g.drawLine (c.x + kr * 0.86f * std::sin (a), c.y - kr * 0.86f * std::cos (a), c.x + kr * 0.99f * std::sin (a), c.y - kr * 0.99f * std::cos (a), 1.0f);
    }

    // Spun aluminium cap: angle-dependent brightness wedges simulate the anisotropic reflection.
    const auto capR = kr * 0.8f;
    const auto cap = Rectangle<float> (capR * 2.0f, capR * 2.0f).withCentre (c);
    constexpr int wedges = 96;
    for (int i = 0; i < wedges; ++i)
    {
        const auto a0 = MathConstants<float>::twoPi * static_cast<float> (i) / wedges;
        const auto a1 = MathConstants<float>::twoPi * static_cast<float> (i + 1) / wedges;
        // Two highlights opposite each other (light from top-left), as on real spun metal.
        const auto shade = 0.5f + 0.5f * std::cos (2.0f * (a0 + 0.8f));
        const auto level = (hover ? 0.62f : 0.55f) + 0.33f * shade;
        Path wedge;
        wedge.addPieSegment (cap, a0, a1 + 0.01f, 0.0f);
        g.setColour (Colour::fromFloatRGBA (level, level, level * 1.01f, 1.0f));
        g.fillPath (wedge);
    }
    // Concentric machining rings.
    for (float rr = capR * 0.15f; rr < capR; rr += capR * 0.07f)
    {
        g.setColour (Colours::black.withAlpha (0.05f));
        g.drawEllipse (Rectangle<float> (rr * 2.0f, rr * 2.0f).withCentre (c), 0.8f);
    }
    g.setColour (Colours::black.withAlpha (0.35f));
    g.drawEllipse (cap, 1.2f);
    g.setColour (Colours::white.withAlpha (0.35f));
    g.drawEllipse (cap.reduced (1.2f), 0.8f);

    // Indicator: engraved line filled with the accent colour.
    const Point<float> p1 { c.x + capR * 0.35f * std::sin (angle), c.y - capR * 0.35f * std::cos (angle) };
    const Point<float> p2 { c.x + capR * 0.92f * std::sin (angle), c.y - capR * 0.92f * std::cos (angle) };
    g.setColour (Colours::black.withAlpha (0.55f));
    g.drawLine ({ p1, p2 }, jmax (3.0f, capR * 0.09f));
    g.setColour (accent);
    g.drawLine ({ p1, p2 }, jmax (1.8f, capR * 0.05f));
}

void drawLed (Graphics& g, Point<float> c, float r, Colour colour, bool on)
{
    if (on)
    {
        g.setGradientFill (ColourGradient (colour.withAlpha (0.55f), c.x, c.y, colour.withAlpha (0.0f), c.x + r * 3.5f, c.y, true));
        g.fillEllipse (Rectangle<float> (r * 7.0f, r * 7.0f).withCentre (c));
    }
    g.setColour (Colours::black.withAlpha (0.7f));
    g.fillEllipse (Rectangle<float> (r * 2.6f, r * 2.6f).withCentre (c));
    g.setColour (on ? colour.brighter (0.35f) : colour.darker (0.85f).withSaturation (0.4f));
    g.fillEllipse (Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c));
    if (on)
    {
        g.setColour (Colours::white.withAlpha (0.75f));
        g.fillEllipse (Rectangle<float> (r * 0.7f, r * 0.7f).withCentre (c.translated (-r * 0.3f, -r * 0.3f)));
    }
}
} // namespace sfx::ui::hw
