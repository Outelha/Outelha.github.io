#include "Hardware.h"

#include "Fonts.h"
#include "Textures.h"

namespace sfx::ui::hw
{
using namespace juce;

Image brushedTexture()
{
    // Returns a counted handle; the pixels are owned by the open editors (see Textures.h).
    return SharedResourcePointer<Textures>()->brushed;
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

//==============================================================================
namespace
{
Point<float> polar (Point<float> c, float r, float a) { return { c.x + r * std::sin (a), c.y - r * std::cos (a) }; }
} // namespace

void drawLightSpill (Graphics& g, Rectangle<float> area, Colour colour, float intensity)
{
    if (intensity <= 0.001f)
        return;
    ColourGradient grad (colour.withMultipliedAlpha (0.55f * intensity), area.getCentreX(), area.getCentreY(),
                         colour.withAlpha (0.0f), area.getCentreX() + area.getWidth() * 0.5f, area.getCentreY(), true);
    grad.addColour (0.35, colour.withMultipliedAlpha (0.22f * intensity));
    // Radial gradients are circular: squash the ellipse with a transform.
    const auto sy = area.getHeight() / jmax (1.0f, area.getWidth());
    Graphics::ScopedSaveState save (g);
    g.addTransform (AffineTransform::scale (1.0f, sy, area.getCentreX(), area.getCentreY()));
    g.setGradientFill (grad);
    g.fillEllipse (area.withSizeKeepingCentre (area.getWidth(), area.getWidth()));
}

void drawLightSeam (Graphics& g, Point<float> c, float width, Colour colour, float intensity)
{
    drawLightSpill (g, Rectangle<float> (width * 1.1f, 26.0f).withCentre (c), colour, 0.8f * intensity);
    ColourGradient core (colour.withAlpha (0.0f), c.x - width * 0.5f, c.y, colour.withAlpha (0.0f), c.x + width * 0.5f, c.y, false);
    core.addColour (0.5, colour.brighter (0.6f).withMultipliedAlpha (intensity));
    core.addColour (0.3, colour.withMultipliedAlpha (0.6f * intensity));
    core.addColour (0.7, colour.withMultipliedAlpha (0.6f * intensity));
    g.setGradientFill (core);
    g.fillRect (Rectangle<float> (width, 1.6f).withCentre (c));
}

void drawRimGlow (Graphics& g, Rectangle<float> r, float corner, Colour colour, float size, float intensity)
{
    if (intensity <= 0.001f)
        return;
    constexpr int steps = 7;
    for (int i = steps; i >= 1; --i)
    {
        const auto t = static_cast<float> (i) / steps;
        const auto grow = size * t;
        g.setColour (colour.withMultipliedAlpha (intensity * 0.10f * (1.0f - t * 0.75f)));
        g.drawRoundedRectangle (r.expanded (grow), corner + grow, size / steps * 1.6f);
    }
}

void drawTray (Graphics& g, Rectangle<float> r, float corner)
{
    // Lip catching the light below, dark well, inner shadow from the top.
    g.setColour (Colours::white.withAlpha (0.07f));
    g.drawRoundedRectangle (r.expanded (1.0f).translated (0.0f, 1.0f), corner + 1.0f, 1.0f);
    Path well;
    well.addRoundedRectangle (r, corner);
    g.setGradientFill (ColourGradient (Colour (0xff0b0b0d), 0.0f, r.getY(), Colour (0xff141518), 0.0f, r.getBottom(), false));
    g.fillPath (well);
    Graphics::ScopedSaveState save (g);
    g.reduceClipRegion (well);
    g.setGradientFill (ColourGradient (Colours::black.withAlpha (0.6f), 0.0f, r.getY(), Colours::transparentBlack, 0.0f, r.getY() + 10.0f, false));
    g.fillRect (r.withHeight (10.0f));
    g.setColour (Colours::black.withAlpha (0.8f));
    g.drawRoundedRectangle (r.reduced (0.5f), corner, 1.0f);
}

void drawBezelPanel (Graphics& g, Rectangle<float> r, float corner, float bezel, bool screws)
{
    // Cast shadow.
    for (int i = 0; i < 4; ++i)
    {
        g.setColour (Colours::black.withAlpha (0.16f));
        g.fillRoundedRectangle (r.translated (0.0f, 2.0f + static_cast<float> (i) * 1.5f).expanded (static_cast<float> (i)), corner + static_cast<float> (i));
    }

    // Polished bezel: bright upper edge, dark lower edge, a sharp specular line.
    Path outer;
    outer.addRoundedRectangle (r, corner);
    // Smoked-bronze anodising: warm greys rather than neutral steel.
    ColourGradient rim (Colour (0xff9c9186), r.getX(), r.getY(), Colour (0xff211c19), r.getX(), r.getBottom(), false);
    rim.addColour (0.08, Colour (0xff6b6159));
    rim.addColour (0.5, Colour (0xff3a3430));
    rim.addColour (0.92, Colour (0xff574b42));
    g.setGradientFill (rim);
    g.fillPath (outer);
    {
        Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (outer);
        g.setTiledImageFill (brushedTexture(), 0, 0, 0.35f);
        g.fillPath (outer);
        // Diagonal sheen across the bezel.
        ColourGradient sheen (Colours::white.withAlpha (0.0f), r.getX(), r.getY(), Colours::white.withAlpha (0.0f), r.getRight(), r.getBottom(), false);
        sheen.addColour (0.25, Colours::white.withAlpha (0.10f));
        sheen.addColour (0.32, Colours::white.withAlpha (0.0f));
        sheen.addColour (0.7, Colours::white.withAlpha (0.06f));
        g.setGradientFill (sheen);
        g.fillPath (outer);
        // Orange light from the panel below / the lit controls, reflected in the lower bezel.
        ColourGradient warm (Colour (0x00ff7a1f), 0.0f, r.getBottom() - bezel * 2.2f, Colour (0x44ff7a1f), 0.0f, r.getBottom(), false);
        g.setGradientFill (warm);
        g.fillRect (r.withTop (r.getBottom() - bezel * 2.2f));
        ColourGradient warmTop (Colour (0x22ff8a30), 0.0f, r.getY(), Colour (0x00ff8a30), 0.0f, r.getY() + bezel * 1.6f, false);
        g.setGradientFill (warmTop);
        g.fillRect (r.withHeight (bezel * 1.6f));
    }
    g.setColour (Colours::white.withAlpha (0.28f));
    g.drawRoundedRectangle (r.reduced (0.8f), corner, 0.8f);
    g.setColour (Colours::black.withAlpha (0.8f));
    g.drawRoundedRectangle (r, corner, 1.0f);

    // Interior.
    const auto inner = r.reduced (bezel);
    const auto innerCorner = jmax (2.0f, corner - bezel * 0.6f);
    Path in;
    in.addRoundedRectangle (inner, innerCorner);
    g.setColour (Colours::black.withAlpha (0.7f));
    g.drawRoundedRectangle (inner.expanded (1.0f), innerCorner + 1.0f, 2.0f);
    g.setGradientFill (ColourGradient (Colour (0xff1e1b19), inner.getX(), inner.getY(), Colour (0xff0f0e0d), inner.getX(), inner.getBottom(), false));
    g.fillPath (in);
    {
        Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (in);
        g.setTiledImageFill (brushedTexture(), 0, 0, 0.22f);
        g.fillPath (in);
        g.setGradientFill (ColourGradient (Colours::black.withAlpha (0.55f), 0.0f, inner.getY(), Colours::transparentBlack, 0.0f, inner.getY() + 18.0f, false));
        g.fillRect (inner.withHeight (18.0f));
        // Large soft reflection from the upper left.
        ColourGradient glare (Colours::white.withAlpha (0.045f), inner.getX(), inner.getY(), Colours::white.withAlpha (0.0f),
                              inner.getX() + inner.getWidth() * 0.6f, inner.getY() + inner.getHeight() * 0.9f, false);
        g.setGradientFill (glare);
        g.fillPath (in);
    }

    if (screws)
    {
        const auto inset = bezel * 0.5f + 5.0f;
        for (auto p : { r.getTopLeft().translated (inset, inset), r.getTopRight().translated (-inset, inset),
                        r.getBottomLeft().translated (inset, -inset), r.getBottomRight().translated (-inset, -inset) })
            drawScrew (g, p, jmax (3.2f, bezel * 0.55f), 0.4f + p.x * 0.013f);
    }
}

void drawKey (Graphics& g, Rectangle<float> r, const KeyLook& k)
{
    const bool cl = ! k.connectedLeft, cr = ! k.connectedRight;
    auto shape = [&] (Rectangle<float> b, float c) {
        Path p;
        p.addRoundedRectangle (b.getX(), b.getY(), b.getWidth(), b.getHeight(), c, c, cl, cr, cl, cr);
        return p;
    };
    const auto press = k.down ? 1.0f : 0.0f;

    // Drop shadow under the key.
    g.setColour (Colours::black.withAlpha (k.down ? 0.35f : 0.6f));
    g.fillPath (shape (r.translated (0.0f, 2.0f - press), k.corner));

    if (k.lit)
        drawRimGlow (g, r, k.corner, k.accent, 9.0f, k.down ? 0.6f : 1.0f);

    // Chrome bezel.
    g.setGradientFill (ColourGradient (k.lit ? k.accent.brighter (0.6f) : Colour (0xff6a6c72), 0.0f, r.getY(),
                                       k.lit ? k.accent.darker (0.9f) : Colour (0xff0c0c0e), 0.0f, r.getBottom(), false));
    g.fillPath (shape (r, k.corner));

    // Face.
    const auto face = r.reduced (1.6f).translated (0.0f, press * 0.8f);
    const auto facePath = shape (face, k.corner - 1.2f);
    if (k.lit)
    {
        ColourGradient fill (k.accent.brighter (0.45f), 0.0f, face.getY(), k.accent.darker (0.45f), 0.0f, face.getBottom(), false);
        fill.addColour (0.45, k.accent.brighter (0.08f));
        fill.addColour (0.55, k.accent);
        g.setGradientFill (fill);
        g.fillPath (facePath);
        Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (facePath);
        g.setTiledImageFill (brushedTexture(), 0, 0, 0.16f);
        g.fillPath (facePath);
        // Hot core: light spilling from inside the key.
        drawLightSpill (g, face.withSizeKeepingCentre (face.getWidth() * 0.9f, face.getHeight() * 1.2f), Colour (0xffffe3b0), k.over ? 0.55f : 0.4f);
        // Glassy upper half.
        g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.28f), 0.0f, face.getY(), Colours::white.withAlpha (0.0f), 0.0f, face.getCentreY(), false));
        g.fillRect (face.withHeight (face.getHeight() * 0.5f));
    }
    else
    {
        const auto base = k.over ? Colour (0xff2a2b30) : Colour (0xff1f2024);
        g.setGradientFill (ColourGradient (base.brighter (0.12f), 0.0f, face.getY(), base.darker (0.45f), 0.0f, face.getBottom(), false));
        g.fillPath (facePath);
        Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (facePath);
        g.setTiledImageFill (brushedTexture(), 0, 0, 0.25f);
        g.fillPath (facePath);
        g.setGradientFill (ColourGradient (Colours::white.withAlpha (k.down ? 0.02f : 0.09f), 0.0f, face.getY(), Colours::white.withAlpha (0.0f),
                                           0.0f, face.getY() + face.getHeight() * 0.45f, false));
        g.fillRect (face);
    }
    // Top specular edge.
    g.setColour (Colours::white.withAlpha (k.lit ? 0.45f : 0.14f));
    g.drawLine (face.getX() + k.corner, face.getY() + 0.7f, face.getRight() - k.corner, face.getY() + 0.7f, 0.8f);
    g.setColour (Colours::black.withAlpha (0.75f));
    g.strokePath (shape (r, k.corner), PathStrokeType (0.9f));
}

void drawKeyText (Graphics& g, const String& text, Rectangle<float> r, Font f, bool lit, Colour accent, bool enabled)
{
    g.setFont (f);
    if (lit)
    {
        g.setColour (Colour (0xff5a1e00).withAlpha (0.7f));
        g.drawText (text, r.translated (0.0f, 1.2f), Justification::centred, false);
        g.setColour (Colour (0xfffff1dc).withMultipliedAlpha (enabled ? 1.0f : 0.5f));
        g.drawText (text, r, Justification::centred, false);
        juce::ignoreUnused (accent);
        return;
    }
    g.setColour (Colours::black.withAlpha (0.8f));
    g.drawText (text, r.translated (0.0f, 1.0f), Justification::centred, false);
    g.setColour (Colour (0xffe9e5dd).withMultipliedAlpha (enabled ? 1.0f : 0.45f));
    g.drawText (text, r, Justification::centred, false);
}

void drawSpeakerGrille (Graphics& g, Rectangle<float> r)
{
    // Chamfered (hexagonal) housing.
    const auto ch = r.getHeight() * 0.42f;
    Path housing;
    housing.startNewSubPath (r.getX() + ch, r.getY());
    housing.lineTo (r.getRight() - ch, r.getY());
    housing.lineTo (r.getRight(), r.getCentreY());
    housing.lineTo (r.getRight() - ch, r.getBottom());
    housing.lineTo (r.getX() + ch, r.getBottom());
    housing.lineTo (r.getX(), r.getCentreY());
    housing.closeSubPath();
    g.setColour (Colours::black.withAlpha (0.55f));
    g.fillPath (housing, AffineTransform::translation (0.0f, 2.5f));
    g.setGradientFill (ColourGradient (Colour (0xff8a8c92), 0.0f, r.getY(), Colour (0xff222327), 0.0f, r.getBottom(), false));
    g.fillPath (housing);
    const auto inner = r.reduced (ch * 0.35f, r.getHeight() * 0.16f);
    const auto ich = inner.getHeight() * 0.42f;
    Path well;
    well.startNewSubPath (inner.getX() + ich, inner.getY());
    well.lineTo (inner.getRight() - ich, inner.getY());
    well.lineTo (inner.getRight(), inner.getCentreY());
    well.lineTo (inner.getRight() - ich, inner.getBottom());
    well.lineTo (inner.getX() + ich, inner.getBottom());
    well.lineTo (inner.getX(), inner.getCentreY());
    well.closeSubPath();
    g.setColour (Colour (0xff141416));
    g.fillPath (well);
    Graphics::ScopedSaveState save (g);
    g.reduceClipRegion (well);
    const auto pitch = jmax (4.0f, inner.getHeight() / 5.0f);
    const auto hole = pitch * 0.62f;
    int row = 0;
    for (auto y = inner.getY() + pitch * 0.5f; y < inner.getBottom(); y += pitch * 0.87f, ++row)
        for (auto x = inner.getX() + (row % 2 == 0 ? pitch * 0.5f : pitch); x < inner.getRight(); x += pitch)
        {
            const auto h = Rectangle<float> (hole, hole).withCentre ({ x, y });
            g.setColour (Colour (0xff45464b));
            g.fillEllipse (h.translated (0.0f, 0.6f));
            g.setColour (Colour (0xff050505));
            g.fillEllipse (h);
        }
    g.setGradientFill (ColourGradient (Colours::black.withAlpha (0.5f), 0.0f, inner.getY(), Colours::transparentBlack, 0.0f, inner.getCentreY(), false));
    g.fillPath (well);
}

void drawVentSlots (Graphics& g, Rectangle<float> r, int count)
{
    const auto step = r.getHeight() / static_cast<float> (count);
    for (int i = 0; i < count; ++i)
    {
        const auto y = r.getY() + step * static_cast<float> (i);
        Path slot;
        slot.startNewSubPath (r.getX(), y + step * 0.75f);
        slot.lineTo (r.getRight() - r.getWidth() * 0.35f, y);
        slot.lineTo (r.getRight(), y);
        slot.lineTo (r.getX() + r.getWidth() * 0.35f, y + step * 0.75f);
        slot.closeSubPath();
        g.setColour (Colours::white.withAlpha (0.10f));
        g.fillPath (slot, AffineTransform::translation (0.0f, 1.0f));
        g.setColour (Colour (0xff060607));
        g.fillPath (slot);
    }
}

void drawGlassTube (Graphics& g, Rectangle<float> r, Colour glow, float level)
{
    level = jlimit (0.0f, 1.0f, level);
    const auto capH = r.getWidth() * 0.55f;
    const auto tubeGlass = r.reduced (r.getWidth() * 0.12f, capH * 0.7f);
    // Light thrown on the surroundings.
    drawLightSpill (g, tubeGlass.expanded (tubeGlass.getWidth() * 1.4f, tubeGlass.getHeight() * 0.25f), glow, 0.25f + 0.75f * level);
    // Glass body with the glowing gas column: brightest in the middle, darker at the walls.
    g.setColour (Colour (0xff120c08));
    g.fillRoundedRectangle (tubeGlass, tubeGlass.getWidth() * 0.45f);
    {
        ColourGradient column (glow.darker (0.6f).withAlpha (0.5f * level), tubeGlass.getX(), 0.0f, glow.darker (0.6f).withAlpha (0.5f * level), tubeGlass.getRight(), 0.0f, false);
        column.addColour (0.5, glow.brighter (0.3f).withAlpha (0.35f + 0.65f * level));
        column.addColour (0.3, glow.withAlpha (0.6f * level + 0.1f));
        column.addColour (0.7, glow.withAlpha (0.6f * level + 0.1f));
        g.setGradientFill (column);
        g.fillRoundedRectangle (tubeGlass.reduced (1.0f), tubeGlass.getWidth() * 0.45f);
        // Fade toward both ends.
        g.setGradientFill (ColourGradient (Colour (0xff120c08).withAlpha (0.85f), 0.0f, tubeGlass.getY(), Colour (0x00120c08), 0.0f, tubeGlass.getY() + tubeGlass.getHeight() * 0.22f, false));
        g.fillRoundedRectangle (tubeGlass.withHeight (tubeGlass.getHeight() * 0.22f), tubeGlass.getWidth() * 0.45f);
        g.setGradientFill (ColourGradient (Colour (0x00120c08), 0.0f, tubeGlass.getBottom() - tubeGlass.getHeight() * 0.22f, Colour (0xff120c08).withAlpha (0.85f), 0.0f, tubeGlass.getBottom(), false));
        g.fillRoundedRectangle (tubeGlass.withTrimmedTop (tubeGlass.getHeight() * 0.78f), tubeGlass.getWidth() * 0.45f);
    }
    g.setColour (Colour (0xffffe0b0).withAlpha (0.35f + 0.65f * level));
    g.fillRoundedRectangle (tubeGlass.withSizeKeepingCentre (jmax (1.2f, tubeGlass.getWidth() * 0.1f), tubeGlass.getHeight() * 0.62f), 1.0f);
    // Glass reflections.
    g.setColour (Colours::white.withAlpha (0.22f));
    g.fillRoundedRectangle (tubeGlass.withWidth (tubeGlass.getWidth() * 0.14f).translated (tubeGlass.getWidth() * 0.14f, 0.0f).reduced (0.0f, tubeGlass.getHeight() * 0.06f), 1.5f);
    g.setColour (Colours::white.withAlpha (0.18f));
    g.drawRoundedRectangle (tubeGlass, tubeGlass.getWidth() * 0.45f, 0.8f);
    // Chrome end caps.
    for (auto cap : { r.withHeight (capH), r.withTrimmedTop (r.getHeight() - capH) })
    {
        g.setGradientFill (ColourGradient (Colour (0xffd8d9dc), cap.getX(), 0.0f, Colour (0xff3b3c40), cap.getRight(), 0.0f, false));
        g.fillRoundedRectangle (cap, 2.5f);
        g.setColour (Colours::black.withAlpha (0.4f));
        for (int i = 1; i < 3; ++i)
            g.fillRect (cap.getX(), cap.getY() + cap.getHeight() * static_cast<float> (i) / 3.0f, cap.getWidth(), 0.8f);
    }
}

void drawVacuumTube (Graphics& g, Rectangle<float> r, Colour glow, float level)
{
    level = jlimit (0.0f, 1.0f, level);
    const auto baseH = r.getHeight() * 0.14f;
    const auto envelope = r.withTrimmedBottom (baseH);
    const auto rad = envelope.getWidth() * 0.5f;
    Path glassPath;
    glassPath.addRoundedRectangle (envelope.getX(), envelope.getY(), envelope.getWidth(), envelope.getHeight(), rad, rad, true, true, false, false);

    drawLightSpill (g, envelope.expanded (envelope.getWidth() * 0.9f, envelope.getHeight() * 0.15f), glow, 0.2f + 0.8f * level);
    g.setColour (Colour (0xff160d07).withAlpha (0.85f));
    g.fillPath (glassPath);
    {
        Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (glassPath);
        // Internal glow and plate structure.
        ColourGradient core (glow.brighter (0.6f).withAlpha (0.25f + 0.75f * level), envelope.getCentreX(), envelope.getCentreY() + envelope.getHeight() * 0.1f,
                             glow.withAlpha (0.0f), envelope.getRight(), envelope.getCentreY(), true);
        g.setGradientFill (core);
        g.fillRect (envelope);
        g.setColour (glow.withAlpha (0.12f + 0.45f * level));
        g.fillRect (envelope);
        ColourGradient heart (Colour (0xffffd9a0).withAlpha (0.2f + 0.7f * level), envelope.getCentreX(), envelope.getCentreY() + envelope.getHeight() * 0.12f,
                              glow.withAlpha (0.0f), envelope.getCentreX() + envelope.getWidth() * 0.45f, envelope.getCentreY(), true);
        g.setGradientFill (heart);
        g.fillRect (envelope);
        const auto plate = envelope.reduced (envelope.getWidth() * 0.26f, envelope.getHeight() * 0.22f).translated (0.0f, envelope.getHeight() * 0.08f);
        g.setColour (Colour (0xff3a2414).withAlpha (0.9f));
        g.drawRect (plate, 1.0f);
        for (auto fx : { 0.35f, 0.65f })
        {
            const auto x = plate.getX() + plate.getWidth() * fx;
            g.setColour (glow.brighter (0.9f).withAlpha (0.3f + 0.7f * level));
            g.drawLine (x, plate.getY() + 2.0f, x, plate.getBottom() - 2.0f, jmax (1.0f, plate.getWidth() * 0.12f));
        }
        g.setColour (Colour (0xff6b5a4a).withAlpha (0.7f));
        g.drawLine (envelope.getX() + 2.0f, plate.getY() - 3.0f, envelope.getRight() - 2.0f, plate.getY() - 3.0f, 0.8f);
        // Glass highlights.
        g.setColour (Colours::white.withAlpha (0.28f));
        g.fillRoundedRectangle (Rectangle<float> (envelope.getWidth() * 0.12f, envelope.getHeight() * 0.6f).withPosition (envelope.getX() + envelope.getWidth() * 0.16f, envelope.getY() + envelope.getHeight() * 0.15f), 1.5f);
    }
    g.setColour (Colours::white.withAlpha (0.25f));
    g.strokePath (glassPath, PathStrokeType (0.8f));
    // Bakelite base with pins.
    const auto base = r.withTop (envelope.getBottom()).reduced (envelope.getWidth() * 0.04f, 0.0f);
    g.setGradientFill (ColourGradient (Colour (0xff3a3632), base.getX(), 0.0f, Colour (0xff0e0d0c), base.getRight(), 0.0f, false));
    g.fillRoundedRectangle (base, 1.5f);
}

void drawValueWindow (Graphics& g, Rectangle<float> r, const String& text, Colour textColour, float fontSize)
{
    drawTray (g, r, 5.0f);
    g.setColour (Colours::white.withAlpha (0.06f));
    g.drawRoundedRectangle (r.reduced (1.5f), 4.0f, 0.8f);
    g.setFont (font (Type::value, fontSize, 0.04f));
    g.setColour (textColour);
    g.drawText (text, r, Justification::centred, false);
}

void drawMachinedKnob (Graphics& g, Rectangle<float> bounds, const KnobLook& k)
{
    const auto d = jmin (bounds.getWidth(), bounds.getHeight());
    const auto c = bounds.getCentre();
    auto r = d * 0.5f;
    if (k.numbers)
        r *= 0.84f; // room for the printed numerals

    const auto groove = r * 0.86f;
    const auto grooveW = r * 0.10f;

    // Scale ticks outside the groove: the lit part follows the value.
    if (k.ticks)
    {
        const int count = k.numbers ? 21 : 11;
        for (int i = 0; i < count; ++i)
        {
            const auto t = static_cast<float> (i) / static_cast<float> (count - 1);
            const auto a = k.startAngle + (k.endAngle - k.startAngle) * t;
            const auto lo = jmin (k.originProportion, k.proportion), hi = jmax (k.originProportion, k.proportion);
            const bool lit = t >= lo - 1.0e-4f && t <= hi + 1.0e-4f;
            const bool major = k.numbers ? (i % 2 == 0) : (i % 5 == 0);
            const auto r0 = r * (major ? 0.98f : 1.0f), r1 = r * (major ? 1.10f : 1.06f);
            g.setColour (lit ? k.accent.withMultipliedAlpha (0.95f) : Colour (0xff8b877f).withAlpha (major ? 0.8f : 0.45f));
            g.drawLine (Line<float> (polar (c, r0, a), polar (c, r1, a)), major ? jmax (1.2f, r * 0.028f) : jmax (0.9f, r * 0.018f));
            if (k.numbers && i % 4 == 0)
            {
                g.setFont (font (Type::labelBold, jmax (11.0f, r * 0.24f)));
                g.setColour (Colour (0xffd9d4ca));
                const auto tp = polar (c, r * 1.25f, a);
                g.drawText (String (i / 2), Rectangle<float> (r * 0.4f, r * 0.24f).withCentre (tp), Justification::centred, false);
            }
        }
    }

    // Groove ring.
    {
        Path ring;
        ring.addCentredArc (c.x, c.y, groove, groove, 0.0f, 0.0f, MathConstants<float>::twoPi, true);
        g.setColour (Colour (0xff050506));
        g.strokePath (ring, PathStrokeType (grooveW * 1.5f));
        g.setColour (Colours::white.withAlpha (0.06f));
        g.drawEllipse (Rectangle<float> (groove * 2.0f + grooveW * 1.5f, groove * 2.0f + grooveW * 1.5f).withCentre (c.translated (0.0f, 0.8f)), 0.8f);
    }

    // Illuminated value arc with layered glow.
    const auto a0 = k.startAngle + (k.endAngle - k.startAngle) * k.originProportion;
    if (std::abs (k.angle - a0) > 0.002f)
    {
        Path arc;
        arc.addCentredArc (c.x, c.y, groove, groove, 0.0f, jmin (a0, k.angle), jmax (a0, k.angle), true);
        const auto glow = k.dragging ? 1.25f : (k.hover ? 1.1f : 1.0f);
        for (auto [w, a] : { std::pair { 4.2f, 0.06f }, { 2.8f, 0.10f }, { 1.8f, 0.18f } })
        {
            g.setColour (k.accent.withMultipliedAlpha (jmin (1.0f, a * glow)));
            g.strokePath (arc, PathStrokeType (grooveW * w, PathStrokeType::curved, PathStrokeType::rounded));
        }
        g.setColour (k.accent);
        g.strokePath (arc, PathStrokeType (grooveW * 0.75f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (k.accent.brighter (0.7f).withAlpha (0.8f));
        g.strokePath (arc, PathStrokeType (grooveW * 0.25f, PathStrokeType::curved, PathStrokeType::rounded));
    }

    // Knob body: soft cast shadow, then the knurled black skirt.
    const auto kr = r * 0.74f;
    if (k.numbers)
    {
        // Large knobs sit in an illuminated ring whose brightness follows the value.
        const auto ringR = kr * 1.06f;
        const auto glowAmount = 0.3f + 0.7f * k.proportion;
        for (auto [w, a] : { std::pair { 9.0f, 0.06f }, { 5.0f, 0.12f }, { 2.5f, 0.3f } })
        {
            g.setColour (k.accent.withMultipliedAlpha (a * glowAmount * 1.4f));
            g.drawEllipse (Rectangle<float> (ringR * 2.0f, ringR * 2.0f).withCentre (c), w);
        }
        g.setColour (k.accent.withMultipliedAlpha (0.35f + 0.65f * glowAmount));
        g.drawEllipse (Rectangle<float> (ringR * 2.0f, ringR * 2.0f).withCentre (c), 1.6f);
    }
    const auto body = Rectangle<float> (kr * 2.0f, kr * 2.0f).withCentre (c);
    {
        ColourGradient shadow (Colours::black.withAlpha (0.75f), c.x, c.y + kr * 0.18f, Colours::transparentBlack, c.x + kr * 1.25f, c.y + kr * 0.18f, true);
        g.setGradientFill (shadow);
        g.fillEllipse (body.expanded (kr * 0.25f).translated (0.0f, kr * 0.18f));
    }
    g.setGradientFill (ColourGradient (Colour (0xff3a3b40), body.getX(), body.getY(), Colour (0xff060607), body.getRight(), body.getBottom(), false));
    g.fillEllipse (body);
    {
        // Knurling: fine ridges that turn with the knob, shaded by their angle to the light.
        constexpr int ridges = 72;
        for (int i = 0; i < ridges; ++i)
        {
            const auto a = k.angle + MathConstants<float>::twoPi * static_cast<float> (i) / ridges;
            const auto lightness = 0.5f + 0.5f * std::cos (a + 0.8f);
            g.setColour (Colours::white.withAlpha (0.03f + 0.10f * lightness));
            g.drawLine (Line<float> (polar (c, kr * 0.84f, a), polar (c, kr * 0.985f, a)), jmax (0.7f, kr * 0.018f));
        }
    }
    g.setColour (Colours::black.withAlpha (0.9f));
    g.drawEllipse (body, 1.0f);

    // Chamfer between skirt and cap.
    const auto capR = kr * 0.80f;
    const auto cap = Rectangle<float> (capR * 2.0f, capR * 2.0f).withCentre (c);
    g.setGradientFill (ColourGradient (Colour (0xffcfd0d3), cap.getX(), cap.getY(), Colour (0xff1c1d20), cap.getRight(), cap.getBottom(), false));
    g.fillEllipse (cap.expanded (kr * 0.045f));

    // Spun aluminium cap (cached conic texture; the reflection stays fixed to the light).
    {
        Graphics::ScopedSaveState save (g);
        Path capPath;
        capPath.addEllipse (cap);
        g.reduceClipRegion (capPath);
        g.drawImage (SharedResourcePointer<Textures>()->conic, cap, RectanglePlacement::stretchToFit);
        g.setGradientFill (ColourGradient (Colours::transparentBlack, c.x, c.y, Colours::black.withAlpha (0.35f), c.x + capR, c.y, true));
        g.fillEllipse (cap);
        g.setGradientFill (ColourGradient (Colours::white.withAlpha (k.hover ? 0.16f : 0.10f), cap.getX(), cap.getY(), Colours::transparentBlack,
                                           c.x, c.y, false));
        g.fillEllipse (cap);
    }
    g.setColour (Colours::white.withAlpha (0.5f));
    g.drawEllipse (cap.reduced (0.6f), 0.7f);

    // Pointer: engraved groove filled with white.
    const auto p1 = polar (c, capR * 0.28f, k.angle), p2 = polar (c, capR * 0.97f, k.angle);
    g.setColour (Colours::black.withAlpha (0.6f));
    g.drawLine (Line<float> (p1, p2).withShortenedStart (-0.5f), jmax (2.6f, capR * 0.12f));
    g.setColour (Colour (0xfffbf8f2));
    g.drawLine (Line<float> (p1, p2), jmax (1.6f, capR * 0.065f));
}

void drawPedalKnob (Graphics& g, Rectangle<float> bounds, float angle, bool hover, Colour skirtTint)
{
    const auto d = jmin (bounds.getWidth(), bounds.getHeight());
    const auto c = bounds.getCentre();
    const auto r = d * 0.5f;
    const auto body = Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c);

    ColourGradient shadow (Colours::black.withAlpha (0.7f), c.x, c.y + r * 0.25f, Colours::transparentBlack, c.x + r * 1.3f, c.y + r * 0.25f, true);
    g.setGradientFill (shadow);
    g.fillEllipse (body.expanded (r * 0.3f).translated (0.0f, r * 0.22f));

    // Skirt.
    g.setGradientFill (ColourGradient (Colour (0xff2c2c30).interpolatedWith (skirtTint, 0.15f), body.getX(), body.getY(),
                                       Colour (0xff030303), body.getRight(), body.getBottom(), false));
    g.fillEllipse (body);
    constexpr int ridges = 40;
    for (int i = 0; i < ridges; ++i)
    {
        const auto a = angle + MathConstants<float>::twoPi * static_cast<float> (i) / ridges;
        g.setColour (Colours::white.withAlpha (0.03f + 0.08f * (0.5f + 0.5f * std::cos (a + 0.8f))));
        g.drawLine (Line<float> (polar (c, r * 0.80f, a), polar (c, r * 0.98f, a)), jmax (0.6f, r * 0.04f));
    }
    // Glossy domed top.
    const auto top = body.reduced (r * 0.2f);
    g.setGradientFill (ColourGradient (Colour (hover ? 0xff4a4a50 : 0xff3a3a3f), top.getX(), top.getY(), Colour (0xff070708), top.getRight(), top.getBottom(), false));
    g.fillEllipse (top);
    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.38f), top.getX() + top.getWidth() * 0.3f, top.getY(),
                                       Colours::white.withAlpha (0.0f), top.getX() + top.getWidth() * 0.3f, top.getCentreY(), false));
    g.fillEllipse (top.withSizeKeepingCentre (top.getWidth() * 0.7f, top.getHeight() * 0.5f).translated (-top.getWidth() * 0.08f, -top.getHeight() * 0.2f));
    g.setColour (Colours::black);
    g.drawEllipse (body, 0.8f);

    g.setColour (Colour (0xfff7f4ee));
    g.drawLine (Line<float> (polar (c, r * 0.25f, angle), polar (c, r * 0.92f, angle)), jmax (1.4f, r * 0.13f));
}

void drawFootswitch (Graphics& g, Rectangle<float> bounds, bool over, bool down)
{
    const auto d = jmin (bounds.getWidth(), bounds.getHeight());
    const auto c = bounds.getCentre();
    const auto r = d * 0.5f;

    ColourGradient shadow (Colours::black.withAlpha (0.75f), c.x, c.y + r * 0.2f, Colours::transparentBlack, c.x + r * 1.2f, c.y + r * 0.2f, true);
    g.setGradientFill (shadow);
    g.fillEllipse (Rectangle<float> (r * 2.4f, r * 2.4f).withCentre (c.translated (0.0f, r * 0.2f)));

    // Knurled nut.
    Path nut;
    constexpr int teeth = 28;
    for (int i = 0; i <= teeth * 2; ++i)
    {
        const auto a = MathConstants<float>::twoPi * static_cast<float> (i) / (teeth * 2);
        const auto p = polar (c, i % 2 == 0 ? r : r * 0.93f, a);
        i == 0 ? nut.startNewSubPath (p) : nut.lineTo (p);
    }
    nut.closeSubPath();
    g.setGradientFill (ColourGradient (Colour (0xffe4e5e8), c.x - r, c.y - r, Colour (0xff3a3b3f), c.x + r, c.y + r, false));
    g.fillPath (nut);
    g.setColour (Colours::black.withAlpha (0.5f));
    g.strokePath (nut, PathStrokeType (0.6f));
    const auto collar = Rectangle<float> (r * 1.56f, r * 1.56f).withCentre (c);
    g.setGradientFill (ColourGradient (Colour (0xff2a2b2e), collar.getX(), collar.getY(), Colour (0xffa9abb0), collar.getRight(), collar.getBottom(), false));
    g.fillEllipse (collar);

    // Chrome dome.
    const auto dr = r * (down ? 0.66f : 0.70f);
    const auto dome = Rectangle<float> (dr * 2.0f, dr * 2.0f).withCentre (c.translated (0.0f, down ? 0.8f : 0.0f));
    ColourGradient chrome (Colour (over ? 0xffffffff : 0xfff2f2f2), dome.getX() + dr * 0.6f, dome.getY() + dr * 0.5f,
                           Colour (0xff4b4e55), dome.getRight(), dome.getBottom(), true);
    chrome.addColour (0.35, Colour (0xffb7bac0));
    chrome.addColour (0.7, Colour (0xff6c6f76));
    g.setGradientFill (chrome);
    g.fillEllipse (dome);
    g.setColour (Colours::white.withAlpha (0.55f));
    g.fillEllipse (Rectangle<float> (dr * 0.55f, dr * 0.35f).withCentre (dome.getCentre().translated (-dr * 0.25f, -dr * 0.4f)));
    g.setColour (Colours::black.withAlpha (0.45f));
    g.drawEllipse (dome, 0.8f);
}
} // namespace sfx::ui::hw
