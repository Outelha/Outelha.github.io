#include "PedalArt.h"

#include "Hardware.h"
#include "Textures.h"

namespace sfx::ui::pedal
{
using namespace juce;

namespace
{
constexpr float knobStart = MathConstants<float>::pi * 1.25f;
constexpr float knobEnd = MathConstants<float>::pi * 2.75f;

Point<float> polar (Point<float> c, float r, float a) { return { c.x + r * std::sin (a), c.y - r * std::cos (a) }; }

Rectangle<float> rel (Rectangle<float> b, float x0, float y0, float x1, float y1)
{
    return { b.getX() + b.getWidth() * x0, b.getY() + b.getHeight() * y0, b.getWidth() * (x1 - x0), b.getHeight() * (y1 - y0) };
}

Rectangle<float> square (Rectangle<float> b, float cx, float cy, float d)
{
    const auto size = b.getWidth() * d;
    return Rectangle<float> (size, size).withCentre ({ b.getX() + b.getWidth() * cx, b.getY() + b.getHeight() * cy });
}

struct Finish
{
    float hammered, sparkle, brushed;
};

Finish finishFor (EffectId id)
{
    switch (id)
    {
        case EffectId::saturation: return { 0.55f, 0.15f, 0.0f };
        case EffectId::resampler: return { 0.22f, 0.45f, 0.0f };
        case EffectId::tape: return { 0.22f, 0.12f, 0.35f };
        case EffectId::chorus: return { 0.28f, 0.40f, 0.0f };
        case EffectId::tremolo: return { 0.28f, 0.45f, 0.0f };
        case EffectId::reverse: return { 0.12f, 0.08f, 0.65f };
        case EffectId::delay: return { 0.32f, 0.35f, 0.0f };
        case EffectId::reverb: return { 0.10f, 0.30f, 0.0f };
    }
    return {};
}

void drawGlassWindow (Graphics& g, Rectangle<float> r, float corner, bool lit, Colour glow)
{
    // Chrome frame, dark glass, a little inner light when the effect is on.
    g.setColour (Colours::black.withAlpha (0.6f));
    g.fillRoundedRectangle (r.expanded (2.2f).translated (0.0f, 1.2f), corner + 2.0f);
    g.setGradientFill (ColourGradient (Colour (0xffb9bbc0), 0.0f, r.getY() - 2.0f, Colour (0xff1b1c1f), 0.0f, r.getBottom() + 2.0f, false));
    g.fillRoundedRectangle (r.expanded (2.0f), corner + 2.0f);
    g.setGradientFill (ColourGradient (Colour (0xff0c0b0b), 0.0f, r.getY(), Colour (0xff050505), 0.0f, r.getBottom(), false));
    g.fillRoundedRectangle (r, corner);
    if (lit)
        hw::drawLightSpill (g, r.reduced (r.getWidth() * 0.05f, 0.0f), glow, 0.35f);
}

void drawGlassGlare (Graphics& g, Rectangle<float> r, float corner)
{
    Graphics::ScopedSaveState save (g);
    Path clip;
    clip.addRoundedRectangle (r, corner);
    g.reduceClipRegion (clip);
    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.13f), 0.0f, r.getY(), Colours::white.withAlpha (0.0f), 0.0f, r.getY() + r.getHeight() * 0.45f, false));
    Path glare;
    glare.startNewSubPath (r.getX(), r.getY());
    glare.lineTo (r.getRight(), r.getY());
    glare.lineTo (r.getRight(), r.getY() + r.getHeight() * 0.18f);
    glare.quadraticTo (r.getCentreX(), r.getY() + r.getHeight() * 0.42f, r.getX(), r.getY() + r.getHeight() * 0.32f);
    glare.closeSubPath();
    g.fillPath (glare);
    g.setColour (Colours::black.withAlpha (0.8f));
    g.drawRoundedRectangle (r, corner, 1.0f);
}

void strokeGlow (Graphics& g, const Path& p, Colour c, float width, float intensity)
{
    for (auto [w, a] : { std::pair { 5.0f, 0.07f }, { 3.2f, 0.12f }, { 2.0f, 0.22f } })
    {
        g.setColour (c.withMultipliedAlpha (a * intensity));
        g.strokePath (p, PathStrokeType (width * w, PathStrokeType::curved, PathStrokeType::rounded));
    }
    g.setColour (c.withMultipliedAlpha (jmin (1.0f, 0.4f + 0.6f * intensity)));
    g.strokePath (p, PathStrokeType (width, PathStrokeType::curved, PathStrokeType::rounded));
    g.setColour (c.brighter (0.8f).withMultipliedAlpha (0.7f * intensity));
    g.strokePath (p, PathStrokeType (width * 0.35f, PathStrokeType::curved, PathStrokeType::rounded));
}

// Tremolo / chorus LFO shapes, phase in cycles.
float lfoShape (int shape, float phase)
{
    phase -= std::floor (phase);
    switch (shape)
    {
        case 1: return 1.0f - 4.0f * std::abs (phase - 0.5f);        // triangle (-1..1)
        case 2: return phase < 0.5f ? 1.0f : -1.0f;                   // square
        case 3: return 1.0f - 2.0f * phase;                           // saw (down)
        default: return std::sin (phase * MathConstants<float>::twoPi); // sine
    }
}

void drawStarfield (Graphics& g, Rectangle<float> area, int seed, int count, float brightness)
{
    Random rng (seed);
    for (int i = 0; i < count; ++i)
    {
        const Point<float> p { area.getX() + rng.nextFloat() * area.getWidth(), area.getY() + rng.nextFloat() * area.getHeight() };
        const auto b = rng.nextFloat();
        const auto size = 0.5f + b * b * 1.6f;
        g.setColour (Colours::white.withAlpha (brightness * (0.25f + 0.75f * b)));
        g.fillEllipse (Rectangle<float> (size, size).withCentre (p));
        if (b > 0.96f)
        {
            g.setColour (Colour (0xffcfe0ff).withAlpha (brightness * 0.7f));
            g.drawLine (p.x - size * 3.0f, p.y, p.x + size * 3.0f, p.y, 0.6f);
            g.drawLine (p.x, p.y - size * 3.0f, p.x, p.y + size * 3.0f, 0.6f);
        }
    }
}

//==============================================================================
void paintEnclosure (Graphics& g, EffectId id, const Geometry& geo, const State& s)
{
    const auto b = geo.body;
    const auto corner = b.getWidth() * 0.075f;
    const auto base = pedalColour (id);
    const auto accent = accentColour (id);

    // Light spilling on the board below an active pedal.
    if (s.enabled)
        hw::drawLightSeam (g, { b.getCentreX(), b.getBottom() + 3.0f }, b.getWidth() * 0.8f, ledAmber, 0.75f);

    // Contact shadow.
    for (int i = 0; i < 5; ++i)
    {
        g.setColour (Colours::black.withAlpha (0.13f));
        g.fillRoundedRectangle (b.translated (0.0f, 2.0f + static_cast<float> (i) * 1.6f).expanded (static_cast<float> (i) * 0.8f), corner + static_cast<float> (i));
    }

    if (s.selected)
        hw::drawRimGlow (g, b.expanded (2.0f), corner + 2.0f, accent, b.getWidth() * 0.09f, 1.0f);

    Path body;
    body.addRoundedRectangle (b, corner);

    // Painted / anodised metal.
    ColourGradient paint (base.brighter (0.35f), b.getX(), b.getY(), base.darker (0.55f), b.getRight(), b.getBottom(), false);
    paint.addColour (0.45, base);
    paint.addColour (0.8, base.darker (0.25f));
    g.setGradientFill (paint);
    g.fillPath (body);

    const auto finish = finishFor (id);
    {
        Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (body);
        const SharedResourcePointer<Textures> tex;
        const auto ox = static_cast<int> (b.getX() * 1.7f) % 256, oy = static_cast<int> (b.getY()) % 256;
        if (finish.brushed > 0.0f)
        {
            g.setTiledImageFill (tex->brushed, ox, oy, finish.brushed);
            g.fillPath (body);
        }
        if (finish.hammered > 0.0f)
        {
            g.setTiledImageFill (tex->hammered, ox, oy, finish.hammered);
            g.fillPath (body);
        }
        if (finish.sparkle > 0.0f)
        {
            g.setTiledImageFill (tex->sparkle, ox + 31, oy + 17, finish.sparkle);
            g.fillPath (body);
        }
        if (id == EffectId::reverb)
        {
            // Deep space: a darker centre and scattered stars over the paint.
            g.setGradientFill (ColourGradient (Colour (0x00000000), b.getCentreX(), b.getCentreY(), Colour (0x66000010), b.getRight(), b.getBottom(), true));
            g.fillPath (body);
            drawStarfield (g, b, 0x5eb, static_cast<int> (b.getWidth() * 0.5f), 0.8f);
        }
        // Edge vignette (the sides curve away from the light).
        g.setGradientFill (ColourGradient (Colours::transparentBlack, b.getCentreX(), b.getCentreY(), Colours::black.withAlpha (0.35f),
                                           b.getX(), b.getY(), true));
        g.fillPath (body);
        // Soft specular sheen from the upper left.
        ColourGradient sheen (Colours::white.withAlpha (0.20f), b.getX(), b.getY(), Colours::white.withAlpha (0.0f), b.getX() + b.getWidth() * 0.7f,
                              b.getY() + b.getHeight() * 0.45f, false);
        g.setGradientFill (sheen);
        g.fillPath (body);
    }

    // Bevel: bright top/left lip, dark bottom/right lip, black outline.
    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.55f), b.getX(), b.getY(), Colours::white.withAlpha (0.0f), b.getX(),
                                       b.getY() + b.getHeight() * 0.25f, false));
    g.drawRoundedRectangle (b.reduced (1.0f), corner - 1.0f, 1.2f);
    g.setGradientFill (ColourGradient (Colours::black.withAlpha (0.0f), b.getX(), b.getBottom() - b.getHeight() * 0.2f, Colours::black.withAlpha (0.6f),
                                       b.getX(), b.getBottom(), false));
    g.drawRoundedRectangle (b.reduced (1.6f), corner - 1.5f, 2.0f);
    g.setColour (s.selected ? accent.brighter (0.4f) : Colours::black.withAlpha (0.85f));
    g.drawRoundedRectangle (b, corner, s.selected ? 1.8f : 1.0f);

    // Corner screws.
    const auto sr = jmax (2.4f, b.getWidth() * 0.032f);
    const auto inset = b.getWidth() * 0.075f;
    for (auto p : { b.getTopLeft().translated (inset, inset), b.getTopRight().translated (-inset, inset),
                    b.getBottomLeft().translated (inset, -inset), b.getBottomRight().translated (-inset, -inset) })
        hw::drawScrew (g, p, sr, 0.5f + p.y * 0.07f);
}

//==============================================================================
void paintWindow (Graphics& g, EffectId id, const Geometry& geo, const State& s)
{
    const auto w = geo.window;
    if (w.isEmpty())
        return;
    const auto accent = accentColour (id);
    const auto on = s.enabled;
    const auto corner = jmax (3.0f, w.getHeight() * 0.12f);
    const auto inten = on ? 1.0f : 0.25f;

    if (id == EffectId::tape)
    {
        // VU meter: cream scale, red zone, needle driven by the signal level.
        g.setColour (Colours::black.withAlpha (0.6f));
        g.fillRoundedRectangle (w.expanded (2.5f).translated (0.0f, 1.2f), corner + 2.0f);
        g.setGradientFill (ColourGradient (Colour (0xff3a3128), 0.0f, w.getY() - 2.0f, Colour (0xff120f0c), 0.0f, w.getBottom() + 2.0f, false));
        g.fillRoundedRectangle (w.expanded (2.5f), corner + 2.0f);
        const auto face = w.reduced (1.5f);
        ColourGradient paper (on ? Colour (0xfffff0c8) : Colour (0xffc9bd9e), face.getCentreX(), face.getBottom(),
                              on ? Colour (0xffe0a24a) : Colour (0xff8a7556), face.getX(), face.getY(), true);
        g.setGradientFill (paper);
        g.fillRoundedRectangle (face, corner);
        const Point<float> pivot { face.getCentreX(), face.getBottom() + face.getHeight() * 0.35f };
        const auto radius = face.getHeight() * 1.05f;
        Path scale;
        scale.addCentredArc (pivot.x, pivot.y, radius * 0.88f, radius * 0.88f, 0.0f, -0.75f, 0.75f, true);
        g.setColour (Colour (0xff2a1c10));
        g.strokePath (scale, PathStrokeType (0.9f));
        for (int i = 0; i <= 12; ++i)
        {
            const auto a = -0.75f + 1.5f * static_cast<float> (i) / 12.0f;
            g.setColour (i >= 9 ? Colour (0xffc0281b) : Colour (0xff2a1c10));
            g.drawLine (Line<float> (polar (pivot, radius * 0.88f, a), polar (pivot, radius * (i % 3 == 0 ? 0.74f : 0.8f), a)), i % 3 == 0 ? 1.1f : 0.7f);
        }
        Path red;
        red.addCentredArc (pivot.x, pivot.y, radius * 0.9f, radius * 0.9f, 0.0f, 0.38f, 0.75f, true);
        g.setColour (Colour (0xffc0281b));
        g.strokePath (red, PathStrokeType (face.getHeight() * 0.06f));
        g.setColour (Colour (0xff2a1c10));
        g.setFont (font (Type::labelBold, face.getHeight() * 0.28f));
        g.drawText ("VU", face.withTrimmedTop (face.getHeight() * 0.45f), Justification::centredTop, false);
        const auto needle = -0.72f + 1.44f * jlimit (0.0f, 1.0f, s.level);
        Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (face.toNearestInt());
        g.setColour (Colours::black.withAlpha (0.25f));
        g.drawLine (Line<float> (pivot.translated (1.5f, 0.0f), polar (pivot.translated (1.5f, 0.0f), radius * 0.92f, needle)), 1.4f);
        g.setColour (Colour (0xff1a0d06));
        g.drawLine (Line<float> (pivot, polar (pivot, radius * 0.92f, needle)), 1.2f);
        drawGlassGlare (g, face, corner);
        return;
    }

    drawGlassWindow (g, w, corner, on, accent);
    {
        Graphics::ScopedSaveState save (g);
        Path clip;
        clip.addRoundedRectangle (w, corner);
        g.reduceClipRegion (clip);
        const auto inner = w.reduced (w.getWidth() * 0.08f, w.getHeight() * 0.18f);
        const auto stroke = jmax (1.2f, w.getHeight() * 0.045f);

        switch (id)
        {
            case EffectId::saturation:
            {
                // Two vacuum tubes; the filament glow follows the drive control.
                const auto glow = on ? 0.25f + 0.75f * s.extra[0] : 0.0f;
                const auto tw = w.getWidth() * 0.22f;
                for (auto cx : { 0.3f, 0.7f })
                {
                    const auto tube = Rectangle<float> (tw, w.getHeight() * 0.88f).withCentre ({ w.getX() + w.getWidth() * cx, w.getCentreY() + w.getHeight() * 0.06f });
                    hw::drawVacuumTube (g, tube, Colour (0xffff8a2a), glow);
                }
                break;
            }
            case EffectId::resampler:
            {
                // Quantised level bars: the number of steps follows the Bits control.
                const int bars = 9;
                const auto steps = jlimit (2, 10, 2 + static_cast<int> (s.extra[1] * 8.0f));
                const auto bw = inner.getWidth() / static_cast<float> (bars);
                static constexpr float shape[] = { 0.45f, 0.62f, 0.9f, 0.7f, 1.0f, 0.78f, 0.55f, 0.68f, 0.4f };
                const auto drive = on ? jmax (0.12f, s.level) : 0.12f;
                for (int i = 0; i < bars; ++i)
                {
                    const auto lit = static_cast<int> (std::ceil (shape[i] * drive * static_cast<float> (steps)));
                    const auto cellH = inner.getHeight() / static_cast<float> (steps);
                    for (int k = 0; k < steps; ++k)
                    {
                        const auto cell = Rectangle<float> (inner.getX() + bw * static_cast<float> (i) + bw * 0.12f, inner.getBottom() - cellH * static_cast<float> (k + 1) + cellH * 0.15f,
                                                            bw * 0.76f, cellH * 0.7f);
                        const bool isLit = k < lit;
                        g.setColour (isLit ? accent.withMultipliedAlpha (inten) : accent.withAlpha (0.07f));
                        g.fillRect (cell);
                        if (isLit && on)
                        {
                            g.setColour (accent.withAlpha (0.18f));
                            g.fillRect (cell.expanded (1.5f));
                        }
                    }
                }
                break;
            }
            case EffectId::chorus:
            case EffectId::tremolo:
            {
                // LFO shape scrolling at the Rate setting; amplitude follows Depth.
                const int shape = id == EffectId::tremolo ? static_cast<int> (s.extra[2] + 0.5f) : 0;
                const auto depth = 0.25f + 0.75f * s.extra[1];
                const auto cycles = 2.0f;
                const auto phase = static_cast<float> (s.time * (0.15 + 0.85 * s.extra[0]));
                auto makePath = [&] (float offset) {
                    Path p;
                    const int n = 64;
                    for (int i = 0; i <= n; ++i)
                    {
                        const auto t = static_cast<float> (i) / n;
                        const auto v = lfoShape (shape, t * cycles - (on ? phase : 0.0f) + offset);
                        const Point<float> pt { inner.getX() + inner.getWidth() * t, inner.getCentreY() - v * depth * inner.getHeight() * 0.5f };
                        i == 0 ? p.startNewSubPath (pt) : p.lineTo (pt);
                    }
                    return p;
                };
                if (id == EffectId::chorus)
                    strokeGlow (g, makePath (0.25f), accent.withMultipliedAlpha (0.5f), stroke * 0.8f, inten * 0.6f);
                strokeGlow (g, makePath (0.0f), accent, stroke, inten);
                break;
            }
            case EffectId::reverse:
            {
                // Rewind symbol.
                const auto h = w.getHeight() * 0.52f;
                const auto cx = w.getCentreX(), cy = w.getCentreY();
                Path arrows;
                for (auto off : { -h * 0.5f, h * 0.45f })
                {
                    arrows.startNewSubPath (cx + off - h * 0.45f, cy);
                    arrows.lineTo (cx + off + h * 0.45f, cy - h * 0.5f);
                    arrows.lineTo (cx + off + h * 0.45f, cy + h * 0.5f);
                    arrows.closeSubPath();
                }
                for (auto [grow, a] : { std::pair { 6.0f, 0.06f }, { 3.5f, 0.12f }, { 1.8f, 0.2f } })
                {
                    g.setColour (accent.withMultipliedAlpha (a * inten));
                    g.strokePath (arrows, PathStrokeType (grow, PathStrokeType::curved, PathStrokeType::rounded));
                }
                g.setColour (accent.withMultipliedAlpha (0.25f * inten));
                g.fillPath (arrows);
                g.setColour (accent.brighter (0.3f).withMultipliedAlpha (inten));
                g.strokePath (arrows, PathStrokeType (stroke, PathStrokeType::curved, PathStrokeType::rounded));
                break;
            }
            case EffectId::delay:
            {
                // Echo rings: one per audible repeat (Feedback), fading like the taps.
                const auto rings = 2 + static_cast<int> (s.extra[1] * 3.0f + 0.5f);
                const auto rr = inner.getHeight() * 0.62f;
                const auto spacing = rr * 0.85f;
                const auto x0 = w.getCentreX() - spacing * static_cast<float> (rings - 1) * 0.5f;
                for (int i = 0; i < rings; ++i)
                {
                    Path ring;
                    ring.addEllipse (Rectangle<float> (rr * 2.0f, rr * 2.0f).withCentre ({ x0 + spacing * static_cast<float> (i), w.getCentreY() }));
                    strokeGlow (g, ring, accent, stroke * 0.85f, inten * std::pow (0.78f, static_cast<float> (i)));
                }
                break;
            }
            case EffectId::tape:
            case EffectId::reverb:
                break;
        }
    }
    drawGlassGlare (g, w, corner);
}

void paintDome (Graphics& g, const Geometry& geo, const State& s)
{
    const auto d = geo.dome;
    if (d.isEmpty())
        return;
    const auto accent = accentColour (EffectId::reverb);
    Path dome;
    dome.startNewSubPath (d.getX(), d.getBottom());
    dome.cubicTo (d.getX(), d.getY() + d.getHeight() * 0.05f, d.getRight(), d.getY() + d.getHeight() * 0.05f, d.getRight(), d.getBottom());
    dome.closeSubPath();
    g.setColour (Colours::black.withAlpha (0.6f));
    g.fillPath (dome, AffineTransform::translation (0.0f, 1.5f));
    g.setGradientFill (ColourGradient (Colour (0xff1b1f4e), d.getCentreX(), d.getBottom(), Colour (0xff04051a), d.getCentreX(), d.getY(), false));
    g.fillPath (dome);
    {
        Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (dome);
        // Nebula: brightness follows Decay and Mix so the dome "fills" with a longer reverb.
        const auto amount = s.enabled ? 0.35f + 0.65f * s.extra[0] : 0.15f;
        ColourGradient neb (Colour (0xffb8c8ff).withAlpha (0.9f * amount), d.getCentreX(), d.getBottom() - d.getHeight() * 0.25f,
                            Colour (0x00000000), d.getCentreX() + d.getWidth() * 0.5f, d.getBottom(), true);
        neb.addColour (0.25, accent.withAlpha (0.65f * amount));
        neb.addColour (0.6, Colour (0xff7a3cff).withAlpha (0.3f * amount));
        g.setGradientFill (neb);
        g.fillRect (d);
        drawStarfield (g, d, 0x0d0e, static_cast<int> (d.getWidth() * 0.9f), 0.5f + 0.5f * amount);
        // Bright core star.
        const Point<float> c { d.getCentreX(), d.getY() + d.getHeight() * 0.55f };
        g.setColour (Colours::white.withAlpha (0.9f * amount));
        g.fillEllipse (Rectangle<float> (3.0f, 3.0f).withCentre (c));
        g.drawLine (c.x - d.getWidth() * 0.12f, c.y, c.x + d.getWidth() * 0.12f, c.y, 0.7f);
        g.drawLine (c.x, c.y - d.getHeight() * 0.25f, c.x, c.y + d.getHeight() * 0.25f, 0.7f);
        g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.22f), 0.0f, d.getY(), Colours::white.withAlpha (0.0f), 0.0f, d.getCentreY(), false));
        g.fillRect (d.withHeight (d.getHeight() * 0.5f));
    }
    g.setColour (Colour (0xffaab4d8).withAlpha (0.8f));
    g.strokePath (dome, PathStrokeType (1.2f));
}

void paintPlate (Graphics& g, EffectId id, const Geometry& geo, const State& s)
{
    const auto p = geo.plate;
    const auto corner = p.getHeight() * 0.22f;
    g.setColour (Colours::black.withAlpha (0.55f));
    g.fillRoundedRectangle (p.translated (0.0f, 1.5f).expanded (1.0f), corner + 1.0f);
    g.setGradientFill (ColourGradient (Colour (0xff8b8d92), 0.0f, p.getY(), Colour (0xff17181b), 0.0f, p.getBottom(), false));
    g.fillRoundedRectangle (p.expanded (1.2f), corner + 1.0f);
    g.setGradientFill (ColourGradient (Colour (0xff26272b), 0.0f, p.getY(), Colour (0xff0b0b0d), 0.0f, p.getBottom(), false));
    g.fillRoundedRectangle (p, corner);
    g.setColour (Colours::white.withAlpha (0.08f));
    g.drawLine (p.getX() + corner, p.getY() + 1.0f, p.getRight() - corner, p.getY() + 1.0f, 0.8f);
    const auto name = String (infoFor (id).name).toUpperCase();
    g.setFont (font (Type::labelBold, p.getHeight() * 0.86f, 0.0f));
    g.setColour (Colours::black);
    g.drawFittedText (name, p.reduced (p.getHeight() * 0.12f, 0.0f).translated (0.0f, 1.0f).toNearestInt(), Justification::centred, 1, 0.7f);
    g.setColour (s.selected ? Colours::white : Colour (0xffeeeae2));
    g.drawFittedText (name, p.reduced (p.getHeight() * 0.12f, 0.0f).toNearestInt(), Justification::centred, 1, 0.7f);
}

void paintCaption (Graphics& g, Rectangle<float> r, const String& text, Colour ink)
{
    if (r.isEmpty() || text.isEmpty())
        return;
    g.setFont (font (Type::labelBold, r.getHeight() * 0.95f, 0.04f));
    g.setColour (Colours::black.withAlpha (0.45f));
    g.drawText (text, r.translated (0.0f, 0.8f), Justification::centred, false);
    g.setColour (ink);
    g.drawText (text, r, Justification::centred, false);
}

std::pair<String, String> captionsFor (EffectId id)
{
    switch (id)
    {
        case EffectId::saturation: return { "DRIVE", "MIX" };
        case EffectId::resampler: return { "RATE", "BITS" };
        case EffectId::tape: return { "WOW", "AGE" };
        case EffectId::chorus: return { "RATE", "DEPTH" };
        case EffectId::tremolo: return { "RATE", "DEPTH" };
        case EffectId::reverse: return { "TIME", "MIX" };
        case EffectId::delay: return { "TIME", "REPEATS" };
        case EffectId::reverb: return { "DECAY", "MIX" };
    }
    return {};
}
} // namespace

//==============================================================================
FaceKnobs faceKnobs (EffectId id)
{
    switch (id)
    {
        case EffectId::saturation: return { ParamID::satDrive, ParamID::satMix };
        case EffectId::resampler: return { ParamID::rsRate, ParamID::rsBits };
        case EffectId::tape: return { ParamID::tapeWow, ParamID::tapeAge };
        case EffectId::chorus: return { ParamID::chorusRate, ParamID::chorusDepth };
        case EffectId::tremolo: return { ParamID::tremRate, ParamID::tremDepth, ParamID::tremSync, ParamID::tremDivision };
        case EffectId::reverse: return { ParamID::revTime, ParamID::revMix, ParamID::revSync, ParamID::revDivision };
        case EffectId::delay: return { ParamID::delayTime, ParamID::delayFeedback, ParamID::delaySync, ParamID::delayDivision };
        case EffectId::reverb: return { ParamID::reverbDecay, ParamID::reverbMix };
    }
    return {};
}

Geometry layout (EffectId id, Rectangle<float> b, bool captions)
{
    Geometry g;
    g.body = b;
    g.scale = b.getWidth() / 170.0f;

    const auto captionH = 0.042f;
    switch (id)
    {
        case EffectId::resampler:
            g.window = rel (b, 0.13f, 0.05f, 0.87f, 0.205f);
            g.knobA = square (b, 0.31f, 0.335f, 0.25f);
            g.knobB = square (b, 0.69f, 0.335f, 0.25f);
            break;
        case EffectId::reverb:
            g.dome = rel (b, 0.1f, 0.03f, 0.9f, 0.215f);
            g.knobA = square (b, 0.31f, 0.335f, 0.25f);
            g.knobB = square (b, 0.69f, 0.335f, 0.25f);
            break;
        case EffectId::tape:
            g.reelA = square (b, 0.285f, 0.125f, 0.38f);
            g.reelB = square (b, 0.715f, 0.125f, 0.38f);
            g.knobA = g.reelA;
            g.knobB = g.reelB;
            g.window = rel (b, 0.14f, 0.285f, 0.86f, 0.455f);
            break;
        case EffectId::saturation:
        case EffectId::chorus:
        case EffectId::tremolo:
        case EffectId::reverse:
        case EffectId::delay:
            g.knobA = square (b, 0.3f, 0.13f, 0.27f);
            g.knobB = square (b, 0.7f, 0.13f, 0.27f);
            g.window = rel (b, 0.12f, 0.29f, 0.88f, 0.455f);
            break;
    }

    if (captions)
    {
        // Captions sit below the knobs (or below the dome / bar display, above the knobs).
        const bool above = id == EffectId::resampler || id == EffectId::reverb;
        const auto y = above ? 0.215f : (id == EffectId::tape ? 0.243f : 0.225f);
        g.captionA = rel (b, 0.08f, y, 0.52f, y + captionH).withCentre ({ g.knobA.getCentreX(), b.getY() + b.getHeight() * (y + captionH * 0.5f) });
        g.captionB = rel (b, 0.48f, y, 0.92f, y + captionH).withCentre ({ g.knobB.getCentreX(), b.getY() + b.getHeight() * (y + captionH * 0.5f) });
    }

    g.plate = captions ? rel (b, 0.08f, 0.5f, 0.92f, 0.595f) : rel (b, 0.06f, 0.495f, 0.94f, 0.605f);
    g.led = { b.getCentreX(), b.getY() + b.getHeight() * 0.65f };
    g.footswitch = square (b, 0.5f, 0.785f, 0.32f);
    return g;
}

void paintReel (Graphics& g, Rectangle<float> r, float rotation, float tapeAmount)
{
    const auto c = r.getCentre();
    const auto rad = jmin (r.getWidth(), r.getHeight()) * 0.5f;

    g.setColour (Colours::black.withAlpha (0.55f));
    g.fillEllipse (r.translated (0.0f, rad * 0.08f).expanded (rad * 0.04f));

    // Flange: brushed chrome disc.
    g.setGradientFill (ColourGradient (Colour (0xffe9e8e4), r.getX(), r.getY(), Colour (0xff5d5b57), r.getRight(), r.getBottom(), false));
    g.fillEllipse (r);
    {
        Graphics::ScopedSaveState save (g);
        Path disc;
        disc.addEllipse (r);
        g.reduceClipRegion (disc);
        g.setOpacity (0.55f);
        g.drawImage (SharedResourcePointer<Textures>()->conic, r, RectanglePlacement::stretchToFit);
    }
    // Tape pack visible through the cut-outs.
    const auto pack = rad * (0.42f + 0.45f * jlimit (0.0f, 1.0f, tapeAmount));
    // Three window cut-outs that turn with the reel.
    for (int i = 0; i < 3; ++i)
    {
        const auto a0 = rotation + MathConstants<float>::twoPi * static_cast<float> (i) / 3.0f + 0.35f;
        Path hole;
        hole.addPieSegment (r.reduced (rad * 0.16f), a0, a0 + 1.35f, 0.42f);
        g.setColour (Colour (0xff17110b));
        g.fillPath (hole);
        Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (hole);
        g.setGradientFill (ColourGradient (Colour (0xff5a3a22), c.x, c.y, Colour (0xff22160d), c.x + pack, c.y, true));
        g.fillEllipse (Rectangle<float> (pack * 2.0f, pack * 2.0f).withCentre (c));
        g.setColour (Colours::black.withAlpha (0.5f));
        g.strokePath (hole, PathStrokeType (1.5f));
    }
    g.setColour (Colours::black.withAlpha (0.6f));
    g.drawEllipse (r, 0.9f);
    g.setColour (Colours::white.withAlpha (0.35f));
    g.drawEllipse (r.reduced (1.0f), 0.6f);

    // Hub with three drive notches.
    const auto hub = Rectangle<float> (rad * 0.5f, rad * 0.5f).withCentre (c);
    g.setGradientFill (ColourGradient (Colour (0xfff3f2ee), hub.getX(), hub.getY(), Colour (0xff4a4844), hub.getRight(), hub.getBottom(), false));
    g.fillEllipse (hub);
    g.setColour (Colour (0xff1b1b1b));
    g.fillEllipse (hub.reduced (rad * 0.15f));
    for (int i = 0; i < 3; ++i)
    {
        const auto a = rotation + MathConstants<float>::twoPi * static_cast<float> (i) / 3.0f;
        g.setColour (Colour (0xffd5d3ce));
        g.drawLine (Line<float> (polar (c, rad * 0.1f, a), polar (c, rad * 0.22f, a)), jmax (1.0f, rad * 0.06f));
    }
}

void paintSlotBadge (Graphics& g, Rectangle<float> r, int number, bool selected, Colour accent)
{
    if (selected)
        hw::drawRimGlow (g, r, 5.0f, accent, 6.0f, 0.9f);
    hw::drawTray (g, r, 5.0f);
    g.setColour (selected ? accent : Colour (0xff4a4038));
    g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, selected ? 1.6f : 1.0f);
    g.setFont (font (Type::labelBold, r.getHeight() * 0.78f));
    g.setColour (selected ? Colours::white : Colour (0xffe7c9a4));
    g.drawText (String (number), r, Justification::centred, false);
}

void paint (Graphics& g, EffectId id, const Geometry& geo, const State& s)
{
    paintEnclosure (g, id, geo, s);
    paintDome (g, geo, s);
    paintWindow (g, id, geo, s);

    const auto ink = pedalColour (id).getPerceivedBrightness() > 0.58f ? Colour (0xff1d1a16) : Colour (0xfff4efe6);
    if (s.captions)
    {
        const auto [a, b] = captionsFor (id);
        paintCaption (g, geo.captionA, a, ink);
        paintCaption (g, geo.captionB, b, ink);
    }

    if (id == EffectId::tape)
    {
        // The reels are the Wow and Age controls' housings; they turn while tape runs.
        paintReel (g, geo.reelA, s.reelPhase, 0.75f);
        paintReel (g, geo.reelB, s.reelPhase * 1.35f, 0.35f);
        if (s.paintKnobs)
            for (auto [r, v] : { std::pair { geo.reelA, s.knobA }, { geo.reelB, s.knobB } })
            {
                // Value tick around the reel.
                Path arc;
                const auto rr = r.getWidth() * 0.5f + 2.5f;
                arc.addCentredArc (r.getCentreX(), r.getCentreY(), rr, rr, 0.0f, knobStart, knobStart + (knobEnd - knobStart) * v, true);
                g.setColour (ledAmber.withAlpha (s.enabled ? 0.9f : 0.4f));
                g.strokePath (arc, PathStrokeType (1.6f, PathStrokeType::curved, PathStrokeType::rounded));
            }
    }
    else if (s.paintKnobs)
    {
        for (auto [r, v] : { std::pair { geo.knobA, s.knobA }, { geo.knobB, s.knobB } })
            hw::drawPedalKnob (g, r.reduced (r.getWidth() * 0.08f), knobStart + (knobEnd - knobStart) * v, false);
    }

    paintPlate (g, id, geo, s);
    hw::drawLed (g, geo.led, jmax (3.0f, geo.body.getWidth() * 0.034f), id == EffectId::reverse && s.selected ? accentColour (id) : ledAmber, s.enabled);
    if (s.paintFootswitch)
        hw::drawFootswitch (g, geo.footswitch, s.hover, false);
}
} // namespace sfx::ui::pedal
