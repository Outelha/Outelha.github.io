#include "EffectDisplay.h"

#include "Hardware.h"
#include "PedalArt.h"
#include "Skin.h"
#include "dsp/Saturation.h"
#include "dsp/Tremolo.h"

namespace sfx::ui
{
namespace
{
constexpr float twoPi = juce::MathConstants<float>::twoPi;
} // namespace

EffectDisplay::EffectDisplay (APVTS& s, FrameClock& c, const VisualFeed& f) : state (s), clock (c), feed (f)
{
    setInterceptsMouseClicks (false, false);
    setEffect (EffectId::saturation);
    clock.add (*this);
}

EffectDisplay::~EffectDisplay()
{
    clock.remove (*this);
}

void EffectDisplay::setEffect (EffectId id)
{
    effect = id;
    accent = accentColour (id);
    lastValues.clear();
    repaint();
}

float EffectDisplay::value (const char* id) const
{
    auto* v = state.getRawParameterValue (id);
    return v != nullptr ? v->load() : 0.0f;
}

bool EffectDisplay::isAnimated() const noexcept
{
    return effect == EffectId::tape || effect == EffectId::chorus || effect == EffectId::tremolo || effect == EffectId::reverse;
}

void EffectDisplay::drawCaptionPill (juce::Graphics& g, juce::Rectangle<float> area, const juce::String& text) const
{
    const auto f = font (Type::labelBold, 17.0f, 0.03f);
    const auto textW = juce::GlyphArrangement::getStringWidth (f, text);
    const auto w = textW + 58.0f;
    auto pill = juce::Rectangle<float> (w, 28.0f).withCentre ({ area.getCentreX() + area.getWidth() * 0.12f, area.getY() + 14.0f });
    if (pill.getRight() > area.getRight())
        pill.setX (area.getRight() - w);
    hw::drawTray (g, pill, 7.0f);
    g.setColour (accent.withAlpha (0.55f));
    g.drawRoundedRectangle (pill.reduced (0.5f), 7.0f, 1.0f);

    // Small accent sine glyph, then the read-out.
    auto glyphArea = pill.reduced (12.0f, 9.0f).removeFromLeft (18.0f);
    juce::Path glyph;
    for (int i = 0; i <= 16; ++i)
    {
        const auto u = static_cast<float> (i) / 16.0f;
        const juce::Point<float> pt { glyphArea.getX() + u * glyphArea.getWidth(), glyphArea.getCentreY() - std::sin (twoPi * u) * glyphArea.getHeight() * 0.5f };
        i == 0 ? glyph.startNewSubPath (pt) : glyph.lineTo (pt);
    }
    g.setColour (accent);
    g.strokePath (glyph, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setFont (f);
    g.setColour (juce::Colour (0xfff2ede4));
    g.drawText (text, pill.withTrimmedLeft (34.0f).withTrimmedRight (12.0f), juce::Justification::centred, false);
}

bool EffectDisplay::frame (double, double dt)
{
    if (! visibleInEditor (*this))
        return false;

    const bool on = value (infoFor (effect).enabledId) > 0.5f;
    const auto samples = feed.samplesProcessed();
    const bool running = samples != lastSamples;
    lastSamples = samples;

    // Modulation displays advance only while the effect is on; tape reels only while audio runs.
    if (on)
        time += dt;
    if (effect == EffectId::tape)
    {
        const auto target = on && running ? 2.4f : 0.0f;
        reelSpeed += (target - reelSpeed) * static_cast<float> (juce::jmin (1.0, dt * 3.0));
        if (reelSpeed < 0.01f && target <= 0.0f)
            reelSpeed = 0.0f;
        reelPhase += reelSpeed * static_cast<float> (dt);
    }

    if (isAnimated() && (on || reelSpeed > 0.0f))
    {
        repaint();
        return true;
    }

    // Static displays repaint only when one of the parameters changes (tempo included).
    std::vector<float> current;
    current.reserve (allParameterIds.size() + 1);
    for (auto* id : allParameterIds)
        current.push_back (value (id));
    current.push_back (static_cast<float> (bpm));
    if (current != lastValues)
    {
        lastValues = std::move (current);
        repaint();
    }
    return false;
}

void EffectDisplay::paint (juce::Graphics& g)
{
    const auto outer = getLocalBounds().toFloat().reduced (1.0f);
    const auto bounds = outer.reduced (4.0f);
    // DELAY skin: machined frame with empty glass (display_frame), live content in code,
    // reflections on top (display_glass). Both images cover the whole 350 x 344 display.
    const bool skinned = pedal::hasSkin (effect);
    const juce::SharedResourcePointer<Skin> skin;
    if (skinned)
    {
        Skin::draw (g, skin->get ("display_frame"), getLocalBounds().toFloat());
    }
    else
    {
        // Bezel: dark frame with a thin polished lip, then black glass.
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.fillRoundedRectangle (outer.translated (0.0f, 2.0f), 14.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff7d7f85), 0.0f, outer.getY(), juce::Colour (0xff141517), 0.0f, outer.getBottom(), false));
        g.fillRoundedRectangle (outer, 14.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0b0c0e), 0.0f, bounds.getY(), juce::Colour (0xff050506), 0.0f, bounds.getBottom(), false));
        g.fillRoundedRectangle (bounds, 11.0f);
    }
    {
        juce::Graphics::ScopedSaveState save (g);
        juce::Path clip;
        clip.addRoundedRectangle (bounds, 11.0f);
        g.reduceClipRegion (clip);

        // Faint accent light from the bottom of the screen.
        hw::drawLightSpill (g, juce::Rectangle<float> (bounds.getWidth(), bounds.getHeight() * 0.5f).withCentre ({ bounds.getCentreX(), bounds.getBottom() }),
                            accent, 0.18f);

        const bool on = value (infoFor (effect).enabledId) > 0.5f;
        if (! on)
            g.setOpacity (0.45f);

        const auto area = bounds.reduced (16.0f, 14.0f);
        switch (effect)
        {
            case EffectId::saturation: drawSaturation (g, area); break;
            case EffectId::resampler: drawResampler (g, area); break;
            case EffectId::tape: drawTape (g, area); break;
            case EffectId::chorus: drawChorus (g, area); break;
            case EffectId::tremolo: drawTremolo (g, area); break;
            case EffectId::reverse: drawReverse (g, area); break;
            case EffectId::delay: drawDelay (g, area); break;
            case EffectId::reverb: drawReverb (g, area); break;
        }
        // Glass reflection.
        g.setOpacity (1.0f);
        if (! skinned)
        {
            g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.06f), bounds.getX(), bounds.getY(), juce::Colours::white.withAlpha (0.0f),
                                                     bounds.getX() + bounds.getWidth() * 0.5f, bounds.getY() + bounds.getHeight() * 0.5f, false));
            g.fillRect (bounds);
        }
    }
    if (skinned)
    {
        Skin::draw (g, skin->get ("display_glass"), getLocalBounds().toFloat());
        return;
    }
    g.setColour (juce::Colours::black);
    g.drawRoundedRectangle (bounds, 11.0f, 1.2f);
}

void EffectDisplay::drawGrid (juce::Graphics& g, juce::Rectangle<float> area, int cols, int rows) const
{
    g.setColour (juce::Colour (0xff2a2d33).interpolatedWith (accent, 0.18f).withAlpha (0.6f));
    for (int c = 1; c < cols; ++c)
        g.drawVerticalLine (juce::roundToInt (area.getX() + area.getWidth() * static_cast<float> (c) / static_cast<float> (cols)), area.getY(), area.getBottom());
    for (int r = 1; r < rows; ++r)
        g.drawHorizontalLine (juce::roundToInt (area.getY() + area.getHeight() * static_cast<float> (r) / static_cast<float> (rows)), area.getX(), area.getRight());
}

void EffectDisplay::drawCaption (juce::Graphics& g, juce::Rectangle<float> area, const juce::String& text) const
{
    g.setFont (font (Type::labelBold, 17.0f, 0.04f));
    const auto line = area.removeFromTop (20.0f);
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.drawText (text, line.translated (0.0f, 1.0f), juce::Justification::topRight);
    g.setColour (juce::Colour (0xfff2ede4));
    g.drawText (text, line, juce::Justification::topRight);
}

namespace
{
// Neon trace: layered soft halo, saturated core and a hot centre line.
void strokeGlow (juce::Graphics& g, const juce::Path& p, juce::Colour c, float width = 2.4f)
{
    for (auto [w, a] : { std::pair { 7.0f, 0.06f }, { 4.0f, 0.12f }, { 2.2f, 0.26f } })
    {
        g.setColour (c.withMultipliedAlpha (a));
        g.strokePath (p, juce::PathStrokeType (width * w, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    g.setColour (c);
    g.strokePath (p, juce::PathStrokeType (width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (c.brighter (0.8f).withMultipliedAlpha (0.8f));
    g.strokePath (p, juce::PathStrokeType (width * 0.35f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}
} // namespace

void EffectDisplay::drawSaturation (juce::Graphics& g, juce::Rectangle<float> area) const
{
    using dsp::Saturation;
    drawGrid (g, area, 6, 6);
    const auto mode = static_cast<Saturation::Mode> (juce::jlimit (0, Saturation::numModes - 1, juce::roundToInt (value (ParamID::satMode))));
    const auto drive = std::pow (10.0f, value (ParamID::satDrive) / 20.0f);
    const auto comp = Saturation::compensationGain (mode, drive) * std::pow (10.0f, value (ParamID::satOutput) / 20.0f);
    const auto mix = value (ParamID::satMix);

    auto toScreen = [area] (float x, float y) {
        return juce::Point<float> { area.getX() + (x * 0.5f + 0.5f) * area.getWidth(), area.getY() + (0.5f - y * 0.45f) * area.getHeight() };
    };
    g.setColour (juce::Colour (0xffd8d2c8).withAlpha (0.55f));
    g.drawLine ({ toScreen (-1.0f, -1.0f), toScreen (1.0f, 1.0f) }, 0.9f);

    juce::Path curve;
    for (int i = 0; i <= 200; ++i)
    {
        const auto x = -1.0f + 2.0f * static_cast<float> (i) / 200.0f;
        const auto y = juce::jlimit (-1.1f, 1.1f, x + mix * (Saturation::shape (mode, x * drive) * comp - x));
        i == 0 ? curve.startNewSubPath (toScreen (x, y)) : curve.lineTo (toScreen (x, y));
    }
    strokeGlow (g, curve, accent);
    drawCaption (g, area, "TRANSFER    +" + juce::String (value (ParamID::satDrive), 1) + " dB");
}

namespace
{
juce::String divisionLabel (float v)
{
    return dsp::divisionNames[static_cast<size_t> (juce::jlimit (0, dsp::numDivisions - 1, juce::roundToInt (v)))];
}

const juce::Colour faintTrace { 0x55c8c4bc };
} // namespace

void EffectDisplay::drawResampler (juce::Graphics& g, juce::Rectangle<float> area) const
{
    drawGrid (g, area, 8, 6);
    const auto rate = value (ParamID::rsRate);
    const auto bits = value (ParamID::rsBits);
    const auto levels = std::pow (2.0f, bits - 1.0f);

    // Two cycles of a reference tone, sample-and-held at the chosen rate and quantised to the
    // chosen bit depth: exactly what the resampler does to the signal.
    constexpr float sr = 48000.0f, freq = 600.0f;
    const auto totalSamples = 2.0f * sr / freq;
    const auto hold = std::max (1.0f, sr / rate);
    const auto plot = area.withTrimmedTop (34.0f);

    juce::Path original, crushed;
    float held = 0.0f, next = 0.0f;
    for (int i = 0; i <= 480; ++i)
    {
        const auto t = totalSamples * static_cast<float> (i) / 480.0f;
        const auto x = std::sin (twoPi * freq * t / sr);
        const auto px = plot.getX() + plot.getWidth() * static_cast<float> (i) / 480.0f;
        const juce::Point<float> po { px, plot.getCentreY() - x * plot.getHeight() * 0.4f };
        if (t >= next)
        {
            const auto prev = held;
            held = std::round (x * levels) / levels;
            next += hold;
            if (i > 0)
                crushed.lineTo (px, plot.getCentreY() - prev * plot.getHeight() * 0.4f); // vertical step edge
        }
        const juce::Point<float> pc { px, plot.getCentreY() - held * plot.getHeight() * 0.4f };
        if (i == 0)
        {
            original.startNewSubPath (po);
            crushed.startNewSubPath (pc);
        }
        else
        {
            original.lineTo (po);
            crushed.lineTo (pc);
        }
    }
    g.setColour (faintTrace.withMultipliedAlpha (0.5f));
    g.strokePath (original, juce::PathStrokeType (1.0f));
    strokeGlow (g, crushed, accent, 2.0f);
    drawCaptionPill (g, area, juce::String (rate / 1000.0f, 1) + " kHz   /   " + juce::String (bits, 1) + " bit");
}

void EffectDisplay::drawTape (juce::Graphics& g, juce::Rectangle<float> area) const
{
    const auto wow = value (ParamID::tapeWow);
    const auto flutter = value (ParamID::tapeFlutter);
    const auto age = value (ParamID::tapeAge);
    const auto noise = value (ParamID::tapeNoise);
    const auto t = static_cast<float> (time);

    // Transport deck: dark plate holding two reels that turn only while audio is processed.
    auto deck = area.removeFromTop (area.getHeight() * 0.6f).expanded (6.0f, 4.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2a2623), 0.0f, deck.getY(), juce::Colour (0xff131110), 0.0f, deck.getBottom(), false));
    g.fillRoundedRectangle (deck, 8.0f);
    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.drawRoundedRectangle (deck.reduced (0.5f), 8.0f, 1.0f);
    for (auto pt : { deck.getTopLeft().translated (9.0f, 9.0f), deck.getTopRight().translated (-9.0f, 9.0f),
                     deck.getBottomLeft().translated (9.0f, -9.0f), deck.getBottomRight().translated (-9.0f, -9.0f) })
        hw::drawScrew (g, pt, 3.2f);

    const auto reelD = std::min (deck.getHeight() * 0.74f, deck.getWidth() * 0.44f);
    const auto cy = deck.getY() + deck.getHeight() * 0.44f;
    const auto reelA = juce::Rectangle<float> (reelD, reelD).withCentre ({ deck.getX() + deck.getWidth() * 0.28f, cy });
    const auto reelB = juce::Rectangle<float> (reelD, reelD).withCentre ({ deck.getX() + deck.getWidth() * 0.72f, cy });

    // Tape path: from the supply pack, round two guides and the head, onto the take-up pack.
    const auto headY = deck.getBottom() - 16.0f;
    const juce::Point<float> guideL { reelA.getCentreX() - reelD * 0.05f, headY - 2.0f }, guideR { reelB.getCentreX() + reelD * 0.05f, headY - 2.0f };
    juce::Path tapePath;
    tapePath.startNewSubPath (reelA.getX() + reelD * 0.12f, cy + reelD * 0.25f);
    tapePath.lineTo (guideL);
    tapePath.lineTo (guideR);
    tapePath.lineTo (reelB.getRight() - reelD * 0.12f, cy + reelD * 0.25f);
    g.setColour (juce::Colour (0xff4a2e18));
    g.strokePath (tapePath, juce::PathStrokeType (2.6f));

    pedal::paintReel (g, reelA, reelPhase, 0.75f - 0.4f * age);
    pedal::paintReel (g, reelB, reelPhase * 1.3f, 0.35f + 0.4f * age);
    // Champagne anodised flanges, as on the deck art.
    g.setColour (juce::Colour (0xffc08a4e).withAlpha (0.28f));
    for (auto r : { reelA, reelB })
        g.fillEllipse (r);

    for (auto pt : { guideL, guideR })
    {
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffd8d6d0), pt.x - 4.0f, pt.y - 4.0f, juce::Colour (0xff4a4844), pt.x + 4.0f, pt.y + 4.0f, false));
        g.fillEllipse (juce::Rectangle<float> (10.0f, 10.0f).withCentre (pt));
    }
    const auto head = juce::Rectangle<float> (reelD * 0.34f, 13.0f).withCentre ({ deck.getCentreX(), headY });
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffcfcdc7), 0.0f, head.getY(), juce::Colour (0xff57554f), 0.0f, head.getBottom(), false));
    g.fillRoundedRectangle (head, 3.0f);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawRoundedRectangle (head, 3.0f, 0.8f);

    g.setFont (font (Type::labelBold, 16.0f, 0.04f));
    g.setColour (accent);
    g.drawText ("AGE " + juce::String (juce::roundToInt (age * 100.0f)) + " %", deck.reduced (24.0f, 4.0f).removeFromTop (20.0f), juce::Justification::topRight);

    // Pitch wobble trace (wow + flutter) with hiss speckle; age dulls the colour.
    area.removeFromTop (12.0f);
    drawGrid (g, area, 8, 2);
    juce::Path trace;
    juce::Random rng (static_cast<juce::int64> (t * 30.0f));
    for (int i = 0; i <= 240; ++i)
    {
        const auto x = static_cast<float> (i) / 240.0f;
        const auto tt = t + x * 2.0f;
        auto y = 0.7f * wow * std::sin (twoPi * 0.55f * tt) + 0.25f * flutter * std::sin (twoPi * 6.5f * tt);
        y += noise * 0.1f * (rng.nextFloat() - 0.5f);
        const juce::Point<float> p { area.getX() + x * area.getWidth(), area.getCentreY() - y * area.getHeight() * 0.45f };
        i == 0 ? trace.startNewSubPath (p) : trace.lineTo (p);
    }
    strokeGlow (g, trace, accent.withMultipliedSaturation (1.0f - 0.4f * age), 1.8f);
}

void EffectDisplay::drawChorus (juce::Graphics& g, juce::Rectangle<float> area) const
{
    drawGrid (g, area, 8, 6);
    const auto rate = value (ParamID::chorusRate);
    const auto depth = value (ParamID::chorusDepth);
    const auto delayNorm = (value (ParamID::chorusDelay) - 5.0f) / 25.0f;
    const auto phase = static_cast<float> (std::fmod (time * rate, 1.0));
    const auto plot = area.withTrimmedTop (34.0f);
    // Base delay moves the voice band; depth sets how far the voices sweep around it.
    const auto midY = plot.getBottom() - plot.getHeight() * (0.3f + 0.4f * (1.0f - delayNorm));
    const auto amp = plot.getHeight() * (0.04f + 0.22f * depth);

    // Four voices, each a copy of the signal on its own LFO phase.
    const std::array<std::pair<float, float>, 4> voices { { { 0.0f, 1.0f }, { 0.5f, 0.85f }, { 0.25f, 0.5f }, { 0.75f, 0.4f } } };
    for (auto it = voices.rbegin(); it != voices.rend(); ++it)
    {
        juce::Path p;
        for (int i = 0; i <= 160; ++i)
        {
            const auto x = static_cast<float> (i) / 160.0f;
            const juce::Point<float> pt { plot.getX() + x * plot.getWidth(), midY - amp * std::sin (twoPi * (phase + it->first - x * 2.5f)) };
            i == 0 ? p.startNewSubPath (pt) : p.lineTo (pt);
        }
        strokeGlow (g, p, accent.withAlpha (it->second), 1.7f);
    }
    drawCaptionPill (g, area, juce::String (rate, 2) + " Hz   /   FB " + juce::String (juce::roundToInt (value (ParamID::chorusFeedback) * 100.0f)) + " %");
}

void EffectDisplay::drawTremolo (juce::Graphics& g, juce::Rectangle<float> area) const
{
    using dsp::Tremolo;
    drawGrid (g, area, 8, 6);
    const auto shape = static_cast<Tremolo::Shape> (juce::jlimit (0, Tremolo::numShapes - 1, juce::roundToInt (value (ParamID::tremShape))));
    const auto depth = value (ParamID::tremDepth);
    const auto stereo = value (ParamID::tremStereo);
    const bool sync = value (ParamID::tremSync) > 0.5f;
    const auto rate = sync ? 1.0f / dsp::divisionSeconds (juce::roundToInt (value (ParamID::tremDivision)), bpm) : value (ParamID::tremRate);
    const auto phase = static_cast<float> (std::fmod (time * rate, 1.0));
    const auto plot = area.withTrimmedTop (40.0f).withTrimmedBottom (6.0f);

    // Gain curve over two LFO cycles: left channel bright, right channel (offset by Stereo) dim.
    for (int ch = 1; ch >= 0; --ch)
    {
        if (ch == 1 && stereo < 0.01f)
            continue;
        juce::Path p;
        for (int i = 0; i <= 240; ++i)
        {
            const auto x = static_cast<float> (i) / 240.0f;
            const auto lfo = Tremolo::shapeValue (shape, x * 2.0f + (ch == 1 ? 0.5f * stereo : 0.0f));
            const auto gain = 1.0f - depth * (1.0f - lfo);
            const juce::Point<float> pt { plot.getX() + x * plot.getWidth(), plot.getBottom() - gain * plot.getHeight() };
            i == 0 ? p.startNewSubPath (pt) : p.lineTo (pt);
        }
        strokeGlow (g, p, ch == 0 ? accent : accent.withAlpha (0.35f), 2.0f);
    }
    // Playhead: where the LFO is right now.
    const auto x = plot.getX() + plot.getWidth() * phase * 0.5f;
    g.setColour (accent.withAlpha (0.55f));
    g.drawLine (x, plot.getY(), x, plot.getBottom(), 1.4f);
    const auto label = sync ? divisionLabel (value (ParamID::tremDivision)) : juce::String (rate, 2) + " Hz";
    drawCaptionPill (g, area, label + "   /   " + (stereo > 0.01f ? "L + R" : "MONO"));
}

void EffectDisplay::drawReverse (juce::Graphics& g, juce::Rectangle<float> area) const
{
    drawGrid (g, area, 6, 6);
    const auto pitch = value (ParamID::revPitch);
    const bool sync = value (ParamID::revSync) > 0.5f;
    const auto seconds = sync ? dsp::divisionSeconds (juce::roundToInt (value (ParamID::revDivision)), bpm) : value (ParamID::revTime) * 0.001f;
    const auto mix = value (ParamID::revMix);
    const auto plot = area.withTrimmedTop (34.0f).withTrimmedBottom (4.0f);

    // Each window of audio is played backwards: the faint trace is the forward segment
    // (attack then decay), the bright trace the reversed one (swell then cut).
    constexpr int segments = 3;
    const auto segW = plot.getWidth() / static_cast<float> (segments);
    const auto scroll = static_cast<float> (std::fmod (time / std::max (0.05f, seconds), 1.0));
    {
    juce::Graphics::ScopedSaveState save (g);
    g.reduceClipRegion (plot.toNearestInt());
    for (int s = -1; s < segments; ++s)
    {
        const auto x0 = plot.getX() + (static_cast<float> (s) + scroll) * segW;
        juce::Path forward, reversed;
        for (int i = 0; i <= 80; ++i)
        {
            const auto u = static_cast<float> (i) / 80.0f;
            const auto fwd = std::exp (-2.2f * u) * (1.0f - std::exp (-25.0f * u));
            const auto rev = std::exp (-2.2f * (1.0f - u)) * (1.0f - std::exp (-25.0f * (1.0f - u)));
            const auto osc = std::sin (twoPi * 1.5f * u);
            const juce::Point<float> pf { x0 + u * segW, plot.getCentreY() - plot.getHeight() * 0.45f * fwd * osc };
            const juce::Point<float> pr { x0 + u * segW, plot.getCentreY() - plot.getHeight() * 0.45f * rev * osc };
            i == 0 ? forward.startNewSubPath (pf) : forward.lineTo (pf);
            i == 0 ? reversed.startNewSubPath (pr) : reversed.lineTo (pr);
        }
        g.setColour (accent.withAlpha (0.3f));
        g.strokePath (forward, juce::PathStrokeType (1.0f));
        strokeGlow (g, reversed, accent.withAlpha (0.45f + 0.55f * mix), 2.0f);

        // Arrow pointing left: playback direction.
        const auto ay = plot.getY() + plot.getHeight() * 0.5f;
        const auto ax = x0 + segW * 0.5f;
        juce::Path arrow;
        arrow.addArrow ({ ax + segW * 0.22f, ay, ax - segW * 0.12f, ay }, 7.0f, 20.0f, 13.0f);
        g.setGradientFill (juce::ColourGradient (accent.withAlpha (0.0f), ax + segW * 0.18f, ay, accent.withAlpha (0.85f), ax - segW * 0.1f, ay, false));
        g.fillPath (arrow);
    }
    // Segment boundaries.
    g.setColour (accent.withAlpha (0.18f));
    for (int s = 0; s <= segments; ++s)
    {
        const auto x = plot.getX() + (static_cast<float> (s) + scroll) * segW;
        g.drawVerticalLine (juce::roundToInt (x), plot.getY(), plot.getBottom());
    }
    }
    const auto timeLabel = sync ? divisionLabel (value (ParamID::revDivision)) : juce::String (juce::roundToInt (seconds * 1000.0f)) + " ms";
    drawCaptionPill (g, area, timeLabel + "   /   " + (pitch > 0.0f ? "+" : "") + juce::String (pitch, 1) + " st");
}

void EffectDisplay::drawDelay (juce::Graphics& g, juce::Rectangle<float> area) const
{
    const bool sync = value (ParamID::delaySync) > 0.5f;
    const auto seconds = sync ? dsp::divisionSeconds (juce::roundToInt (value (ParamID::delayDivision)), bpm) : value (ParamID::delayTime) * 0.001f;
    const auto fb = value (ParamID::delayFeedback);
    const bool pingPong = value (ParamID::delayPingPong) > 0.5f;
    const auto mix = value (ParamID::delayMix);
    const auto window = std::max (seconds * 8.5f, 0.5f);

    auto plot = area.withTrimmedTop (34.0f);
    auto labels = plot.removeFromLeft (20.0f);
    drawGrid (g, plot, 8, 2);
    auto lane = [&] (int ch) {
        auto r = plot;
        return ch == 0 ? r.removeFromTop (plot.getHeight() * 0.5f).reduced (0.0f, 8.0f) : r.removeFromBottom (plot.getHeight() * 0.5f).reduced (0.0f, 8.0f);
    };

    g.setFont (font (Type::labelBold, 16.0f));
    for (int ch = 0; ch < 2; ++ch)
    {
        const auto l = lane (ch);
        g.setColour (juce::Colour (0xffb8b2a8));
        g.drawText (ch == 0 ? "L" : "R", labels.withY (l.getY()).withHeight (l.getHeight()), juce::Justification::centred);
        // Dry hit at t = 0.
        g.setColour (juce::Colour (0xffd8d2c8).withAlpha (0.7f));
        g.fillRect (juce::Rectangle<float> (2.0f, l.getHeight()).withPosition (l.getX() + 2.0f, l.getY()));
    }
    // Repeats every Time seconds, each one Feedback times quieter; ping-pong alternates lanes.
    float level = 0.35f + 0.6f * mix;
    for (int n = 1; n < 16; ++n)
    {
        const auto t = seconds * static_cast<float> (n);
        if (t > window || level < 0.01f)
            break;
        const auto x = plot.getX() + plot.getWidth() * (t / window);
        for (int ch = 0; ch < 2; ++ch)
        {
            if (pingPong && (n % 2 == 1) != (ch == 0))
                continue;
            const auto l = lane (ch);
            const auto h = l.getHeight() * juce::jlimit (0.04f, 1.0f, level);
            const auto bar = juce::Rectangle<float> (4.0f, h).withPosition (x - 2.0f, l.getBottom() - h);
            g.setColour (accent.withAlpha (0.12f));
            g.fillRoundedRectangle (bar.expanded (5.0f, 3.0f), 4.0f);
            g.setColour (accent.withAlpha (0.25f));
            g.fillRoundedRectangle (bar.expanded (2.0f, 1.0f), 3.0f);
            g.setGradientFill (juce::ColourGradient (accent.brighter (0.6f), 0.0f, bar.getY(), accent, 0.0f, bar.getBottom(), false));
            g.fillRoundedRectangle (bar, 1.5f);
        }
        level *= fb;
    }
    const auto label = sync ? divisionLabel (value (ParamID::delayDivision)) : juce::String (juce::roundToInt (seconds * 1000.0f)) + " ms";
    drawCaptionPill (g, area, label + "   /   FB " + juce::String (juce::roundToInt (fb * 100.0f)) + " %" + (pingPong ? "   /   PING-PONG" : ""));
}

void EffectDisplay::drawReverb (juce::Graphics& g, juce::Rectangle<float> area) const
{
    drawGrid (g, area, 8, 6);
    const auto size = value (ParamID::reverbSize);
    const auto decay = value (ParamID::reverbDecay);
    const auto damp = value (ParamID::reverbDamping);
    const auto pre = value (ParamID::reverbPredelay) * 0.001f;

    const auto windowSec = juce::jlimit (0.6f, 24.0f, pre + decay * 1.05f);
    auto xFor = [&] (float t) { return area.getX() + area.getWidth() * (t / windowSec); };
    const auto top = area.getY() + area.getHeight() * 0.12f, bottom = area.getBottom();

    // Decay envelope: full level after the pre-delay, reaching silence (-60 dB) at RT60.
    auto envelope = [&] (float rt60) {
        juce::Path p;
        p.startNewSubPath (xFor (0.0f), bottom);
        p.lineTo (xFor (pre), bottom);
        for (int i = 0; i <= 140; ++i)
        {
            const auto t = pre + (windowSec - pre) * static_cast<float> (i) / 140.0f;
            const auto fade = juce::jlimit (0.0f, 1.0f, 1.0f - (t - pre) / std::max (0.05f, rt60));
            p.lineTo (xFor (t), bottom - (bottom - top) * fade * fade);
        }
        p.lineTo (area.getRight(), bottom);
        p.closeSubPath();
        return p;
    };
    const auto full = envelope (decay);
    g.setGradientFill (juce::ColourGradient (accent.withAlpha (0.95f), xFor (pre), top, juce::Colour (0xff7a3cff).withAlpha (0.6f), area.getRight(), bottom, false));
    g.fillPath (full);
    {
        // Diffuse tail: sparse speckle inside the envelope, denser for bigger rooms.
        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (full);
        juce::Random rng (99);
        g.setColour (juce::Colours::white.withAlpha (0.35f));
        const auto dots = 120 + juce::roundToInt (size * 260.0f);
        for (int i = 0; i < dots; ++i)
            g.fillRect (area.getX() + rng.nextFloat() * area.getWidth(), top + rng.nextFloat() * (bottom - top), 1.2f, 1.2f);
    }
    // High frequencies die sooner with more damping.
    g.setColour (juce::Colours::white.withAlpha (0.1f));
    g.fillPath (envelope (std::max (0.05f, decay * (1.0f - 0.75f * damp))));
    strokeGlow (g, full, accent.brighter (0.3f), 1.4f);

    // Early reflections: spread grows with room size.
    juce::Random rng (1234);
    const auto count = 8 + juce::roundToInt (size * 14.0f);
    const auto spread = 0.02f + 0.08f * size;
    g.setColour (juce::Colour (0xffcfe0ff).withAlpha (0.8f));
    for (int i = 0; i < count; ++i)
    {
        const auto t = pre + spread * rng.nextFloat();
        const auto h = (bottom - top) * (0.3f + 0.6f * rng.nextFloat()) * (1.0f - (t - pre) / (spread * 1.6f));
        g.drawLine (xFor (t), bottom, xFor (t), bottom - h, 1.0f);
    }
    drawCaption (g, area, "RT60  " + juce::String (decay, decay < 10.0f ? 1 : 0) + " s");
}
} // namespace sfx::ui
