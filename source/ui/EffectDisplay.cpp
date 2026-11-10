#include "EffectDisplay.h"

#include "dsp/Saturation.h"
#include "dsp/Tremolo.h"

namespace sfx::ui
{
namespace
{
constexpr float twoPi = juce::MathConstants<float>::twoPi;
} // namespace

EffectDisplay::EffectDisplay (APVTS& s) : state (s)
{
    setInterceptsMouseClicks (false, false);
    setEffect (EffectId::saturation);
    lastTick = juce::Time::getMillisecondCounterHiRes();
    startTimerHz (30);
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

void EffectDisplay::timerCallback()
{
    const auto now = juce::Time::getMillisecondCounterHiRes();
    time += (now - lastTick) * 0.001;
    lastTick = now;

    if (isAnimated())
    {
        repaint();
        return;
    }

    // Static displays repaint only when one of the effect's parameters changes.
    std::vector<float> current;
    current.reserve (allParameterIds.size());
    for (auto* id : allParameterIds)
        current.push_back (value (id));
    if (current != lastValues)
    {
        lastValues = std::move (current);
        repaint();
    }
}

void EffectDisplay::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (Palette::display);
    g.fillRoundedRectangle (bounds, 8.0f);
    {
        juce::Graphics::ScopedSaveState save (g);
        juce::Path clip;
        clip.addRoundedRectangle (bounds, 8.0f);
        g.reduceClipRegion (clip);

        const bool on = value (infoFor (effect).enabledId) > 0.5f;
        if (! on)
            g.setOpacity (0.45f);

        const auto area = bounds.reduced (12.0f, 12.0f);
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
    }
    g.setColour (Palette::panelEdge);
    g.drawRoundedRectangle (bounds, 8.0f, 1.0f);
}

void EffectDisplay::drawGrid (juce::Graphics& g, juce::Rectangle<float> area, int cols, int rows) const
{
    g.setColour (Palette::track.withAlpha (0.35f));
    for (int c = 1; c < cols; ++c)
        g.drawVerticalLine (juce::roundToInt (area.getX() + area.getWidth() * static_cast<float> (c) / static_cast<float> (cols)), area.getY(), area.getBottom());
    for (int r = 1; r < rows; ++r)
        g.drawHorizontalLine (juce::roundToInt (area.getY() + area.getHeight() * static_cast<float> (r) / static_cast<float> (rows)), area.getX(), area.getRight());
}

void EffectDisplay::drawCaption (juce::Graphics& g, juce::Rectangle<float> area, const juce::String& text) const
{
    g.setFont (uiFont (11.0f, true));
    g.setColour (Palette::textDim);
    g.drawText (text, area.removeFromTop (14.0f), juce::Justification::topRight);
}

namespace
{
void strokeGlow (juce::Graphics& g, const juce::Path& p, juce::Colour c, float width = 2.0f)
{
    g.setColour (c.withAlpha (0.22f));
    g.strokePath (p, juce::PathStrokeType (width * 3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (c);
    g.strokePath (p, juce::PathStrokeType (width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}
} // namespace

void EffectDisplay::drawSaturation (juce::Graphics& g, juce::Rectangle<float> area) const
{
    using dsp::Saturation;
    drawGrid (g, area, 4, 4);
    const auto mode = static_cast<Saturation::Mode> (juce::jlimit (0, Saturation::numModes - 1, juce::roundToInt (value (ParamID::satMode))));
    const auto drive = std::pow (10.0f, value (ParamID::satDrive) / 20.0f);
    const auto comp = Saturation::compensationGain (mode, drive) * std::pow (10.0f, value (ParamID::satOutput) / 20.0f);
    const auto mix = value (ParamID::satMix);

    auto toScreen = [area] (float x, float y) {
        return juce::Point<float> { area.getX() + (x * 0.5f + 0.5f) * area.getWidth(), area.getY() + (0.5f - y * 0.45f) * area.getHeight() };
    };
    g.setColour (Palette::textFaint.withAlpha (0.6f));
    g.drawLine ({ toScreen (-1.0f, -1.0f), toScreen (1.0f, 1.0f) }, 1.0f);

    juce::Path curve;
    for (int i = 0; i <= 200; ++i)
    {
        const auto x = -1.0f + 2.0f * static_cast<float> (i) / 200.0f;
        const auto y = juce::jlimit (-1.1f, 1.1f, x + mix * (Saturation::shape (mode, x * drive) * comp - x));
        i == 0 ? curve.startNewSubPath (toScreen (x, y)) : curve.lineTo (toScreen (x, y));
    }
    strokeGlow (g, curve, accent);
    drawCaption (g, area, "TRANSFER  +" + juce::String (value (ParamID::satDrive), 1) + " dB");
}

void EffectDisplay::drawResampler (juce::Graphics& g, juce::Rectangle<float> area) const
{
    drawGrid (g, area, 8, 4);
    const auto rate = value (ParamID::rsRate);
    const auto bits = value (ParamID::rsBits);
    const auto levels = std::pow (2.0f, bits - 1.0f);

    // Two cycles of a 600 Hz reference tone at 48 kHz, held at the chosen rate.
    constexpr float sr = 48000.0f, freq = 600.0f;
    const auto totalSamples = 2.0f * sr / freq;
    const auto hold = std::max (1.0f, sr / rate);

    juce::Path original, crushed;
    float held = 0.0f, next = 0.0f;
    for (int i = 0; i <= 400; ++i)
    {
        const auto t = totalSamples * static_cast<float> (i) / 400.0f;
        const auto x = std::sin (twoPi * freq * t / sr);
        if (t >= next)
        {
            held = std::round (x * levels) / levels;
            next += hold;
        }
        const auto px = area.getX() + area.getWidth() * static_cast<float> (i) / 400.0f;
        const juce::Point<float> po { px, area.getCentreY() - x * area.getHeight() * 0.42f };
        const juce::Point<float> pc { px, area.getCentreY() - held * area.getHeight() * 0.42f };
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
    g.setColour (Palette::textFaint);
    g.strokePath (original, juce::PathStrokeType (1.0f));
    strokeGlow (g, crushed, accent, 1.8f);
    drawCaption (g, area, juce::String (rate / 1000.0f, 1) + " kHz  /  " + juce::String (bits, 1) + " BIT");
}

void EffectDisplay::drawTape (juce::Graphics& g, juce::Rectangle<float> area) const
{
    const auto wow = value (ParamID::tapeWow);
    const auto flutter = value (ParamID::tapeFlutter);
    const auto age = value (ParamID::tapeAge);
    const auto noise = value (ParamID::tapeNoise);
    const auto t = static_cast<float> (time);

    // Two spinning reels.
    auto reels = area.removeFromTop (area.getHeight() * 0.55f);
    const auto r = std::min (reels.getHeight() * 0.45f, reels.getWidth() * 0.2f);
    for (int i = 0; i < 2; ++i)
    {
        const juce::Point<float> c { reels.getX() + reels.getWidth() * (i == 0 ? 0.28f : 0.72f), reels.getCentreY() };
        g.setColour (accent.withAlpha (0.18f));
        g.fillEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c));
        g.setColour (accent);
        g.drawEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c), 1.5f);
        g.drawEllipse (juce::Rectangle<float> (r * 0.5f, r * 0.5f).withCentre (c), 1.5f);
        const auto spin = t * 2.4f * (1.0f + 0.04f * wow * std::sin (t * 3.4f));
        for (int s = 0; s < 3; ++s)
        {
            const auto a = spin + twoPi * static_cast<float> (s) / 3.0f;
            g.drawLine (c.x + r * 0.25f * std::cos (a), c.y + r * 0.25f * std::sin (a), c.x + r * 0.85f * std::cos (a), c.y + r * 0.85f * std::sin (a), 2.0f);
        }
    }

    // Pitch wobble trace (wow + flutter) with hiss speckle and age fading the highs.
    area.removeFromTop (8.0f);
    drawGrid (g, area, 6, 2);
    juce::Path trace;
    juce::Random rng (static_cast<juce::int64> (t * 30.0f));
    for (int i = 0; i <= 200; ++i)
    {
        const auto x = static_cast<float> (i) / 200.0f;
        const auto tt = t + x * 2.0f;
        auto y = 0.7f * wow * std::sin (twoPi * 0.55f * tt) + 0.25f * flutter * std::sin (twoPi * 6.5f * tt);
        y += noise * 0.08f * rng.nextFloat();
        const juce::Point<float> p { area.getX() + x * area.getWidth(), area.getCentreY() - y * area.getHeight() * 0.45f };
        i == 0 ? trace.startNewSubPath (p) : trace.lineTo (p);
    }
    strokeGlow (g, trace, accent.withMultipliedSaturation (1.0f - 0.5f * age), 1.6f);
    drawCaption (g, reels, "AGE " + juce::String (juce::roundToInt (age * 100.0f)) + " %");
}

void EffectDisplay::drawChorus (juce::Graphics& g, juce::Rectangle<float> area) const
{
    drawGrid (g, area, 6, 4);
    const auto rate = value (ParamID::chorusRate);
    const auto depth = value (ParamID::chorusDepth);
    const auto delayNorm = (value (ParamID::chorusDelay) - 5.0f) / 25.0f;
    const auto phase = static_cast<float> (std::fmod (time * rate, 1.0));
    const auto midY = area.getBottom() - area.getHeight() * (0.25f + 0.5f * delayNorm);
    const auto amp = area.getHeight() * 0.22f * depth;

    const std::array<std::pair<float, float>, 4> voices { { { 0.0f, 1.0f }, { 0.5f, 0.75f }, { 0.25f, 0.45f }, { 0.75f, 0.3f } } };
    for (auto it = voices.rbegin(); it != voices.rend(); ++it)
    {
        juce::Path p;
        for (int i = 0; i <= 120; ++i)
        {
            const auto x = static_cast<float> (i) / 120.0f;
            const juce::Point<float> pt { area.getX() + x * area.getWidth(), midY - amp * std::sin (twoPi * (phase + it->first - x * 2.0f)) };
            i == 0 ? p.startNewSubPath (pt) : p.lineTo (pt);
        }
        strokeGlow (g, p, accent.withAlpha (it->second), 1.6f);
    }
    drawCaption (g, area, juce::String (rate, 2) + " Hz  /  FB " + juce::String (juce::roundToInt (value (ParamID::chorusFeedback) * 100.0f)) + " %");
}

void EffectDisplay::drawTremolo (juce::Graphics& g, juce::Rectangle<float> area) const
{
    using dsp::Tremolo;
    drawGrid (g, area, 8, 4);
    const auto shape = static_cast<Tremolo::Shape> (juce::jlimit (0, Tremolo::numShapes - 1, juce::roundToInt (value (ParamID::tremShape))));
    const auto depth = value (ParamID::tremDepth);
    const auto stereo = value (ParamID::tremStereo);
    const bool sync = value (ParamID::tremSync) > 0.5f;
    const auto rate = sync ? 1.0f / dsp::divisionSeconds (juce::roundToInt (value (ParamID::tremDivision)), bpm) : value (ParamID::tremRate);
    const auto phase = static_cast<float> (std::fmod (time * rate, 1.0));

    for (int ch = 1; ch >= 0; --ch)
    {
        juce::Path p;
        for (int i = 0; i <= 240; ++i)
        {
            const auto x = static_cast<float> (i) / 240.0f;
            const auto lfo = Tremolo::shapeValue (shape, x * 2.0f + (ch == 1 ? 0.5f * stereo : 0.0f));
            const auto gain = 1.0f - depth * (1.0f - lfo);
            const juce::Point<float> pt { area.getX() + x * area.getWidth(), area.getBottom() - gain * area.getHeight() * 0.9f };
            i == 0 ? p.startNewSubPath (pt) : p.lineTo (pt);
        }
        strokeGlow (g, p, ch == 0 ? accent : accent.withAlpha (0.4f), 1.8f);
    }
    const auto x = area.getX() + area.getWidth() * phase * 0.5f;
    g.setColour (Palette::text.withAlpha (0.7f));
    g.drawVerticalLine (juce::roundToInt (x), area.getY(), area.getBottom());
    const auto label = sync ? juce::String (dsp::divisionNames[static_cast<size_t> (juce::jlimit (0, dsp::numDivisions - 1, juce::roundToInt (value (ParamID::tremDivision))))])
                            : juce::String (rate, 2) + " Hz";
    drawCaption (g, area, label + "  /  L + R");
}

void EffectDisplay::drawReverse (juce::Graphics& g, juce::Rectangle<float> area) const
{
    drawGrid (g, area, 4, 4);
    const auto pitch = value (ParamID::revPitch);
    const bool sync = value (ParamID::revSync) > 0.5f;
    const auto seconds = sync ? dsp::divisionSeconds (juce::roundToInt (value (ParamID::revDivision)), bpm) : value (ParamID::revTime) * 0.001f;
    const auto mix = value (ParamID::revMix);

    // Each reversed segment drawn as a falling ramp (audio played backwards) inside its window.
    constexpr int segments = 3;
    const auto segW = area.getWidth() / static_cast<float> (segments);
    const auto scroll = static_cast<float> (std::fmod (time / std::max (0.05f, seconds), 1.0));
    for (int s = -1; s < segments; ++s)
    {
        const auto x0 = area.getX() + (static_cast<float> (s) + scroll) * segW;
        juce::Path ramp, window;
        for (int i = 0; i <= 60; ++i)
        {
            const auto u = static_cast<float> (i) / 60.0f;
            const auto w = std::sin (juce::MathConstants<float>::pi * u);
            const auto envelope = w * w;
            const auto y = area.getBottom() - area.getHeight() * 0.85f * envelope * (1.0f - u);
            const juce::Point<float> pr { x0 + u * segW, y };
            const juce::Point<float> pw { x0 + u * segW, area.getBottom() - area.getHeight() * 0.85f * envelope };
            i == 0 ? ramp.startNewSubPath (pr) : ramp.lineTo (pr);
            i == 0 ? window.startNewSubPath (pw) : window.lineTo (pw);
        }
        g.setColour (Palette::textFaint);
        g.strokePath (window, juce::PathStrokeType (1.0f));
        strokeGlow (g, ramp, accent.withAlpha (0.4f + 0.6f * mix), 1.8f);
    }
    drawCaption (g, area, juce::String (juce::roundToInt (seconds * 1000.0f)) + " ms  /  " + (pitch > 0.0f ? "+" : "") + juce::String (pitch, 1) + " st");
}

void EffectDisplay::drawDelay (juce::Graphics& g, juce::Rectangle<float> area) const
{
    drawGrid (g, area, 8, 2);
    const bool sync = value (ParamID::delaySync) > 0.5f;
    const auto seconds = sync ? dsp::divisionSeconds (juce::roundToInt (value (ParamID::delayDivision)), bpm) : value (ParamID::delayTime) * 0.001f;
    const auto fb = value (ParamID::delayFeedback);
    const bool pingPong = value (ParamID::delayPingPong) > 0.5f;
    const auto mix = value (ParamID::delayMix);
    const auto window = std::max (seconds * 8.5f, 0.5f);

    auto lane = [&] (int ch) {
        auto r = area;
        return ch == 0 ? r.removeFromTop (area.getHeight() * 0.5f).reduced (0.0f, 4.0f) : r.removeFromBottom (area.getHeight() * 0.5f).reduced (0.0f, 4.0f);
    };

    // Dry hit at t = 0, then repeats; in ping-pong mode repeats alternate lanes.
    for (int ch = 0; ch < 2; ++ch)
    {
        const auto l = lane (ch);
        g.setColour (Palette::textDim);
        g.fillRect (juce::Rectangle<float> (3.0f, l.getHeight()).withPosition (l.getX(), l.getY()));
    }
    float level = 0.35f + 0.6f * mix; // first repeat height follows the mix
    for (int n = 1; n < 16; ++n)
    {
        const auto t = seconds * static_cast<float> (n);
        if (t > window || level < 0.01f)
            break;
        const auto x = area.getX() + area.getWidth() * (t / window);
        for (int ch = 0; ch < 2; ++ch)
        {
            if (pingPong && (n % 2 == 1) != (ch == 0))
                continue;
            const auto l = lane (ch);
            const auto h = l.getHeight() * juce::jlimit (0.05f, 1.0f, level);
            g.setColour (accent.withAlpha (0.25f));
            g.fillRect (juce::Rectangle<float> (7.0f, h).withPosition (x - 3.5f, l.getBottom() - h));
            g.setColour (accent);
            g.fillRect (juce::Rectangle<float> (3.0f, h).withPosition (x - 1.5f, l.getBottom() - h));
        }
        level *= fb;
    }
    const auto label = sync ? juce::String (dsp::divisionNames[static_cast<size_t> (juce::jlimit (0, dsp::numDivisions - 1, juce::roundToInt (value (ParamID::delayDivision))))])
                            : juce::String (juce::roundToInt (seconds * 1000.0f)) + " ms";
    drawCaption (g, area, label + (pingPong ? "  /  PING-PONG" : ""));
}

void EffectDisplay::drawReverb (juce::Graphics& g, juce::Rectangle<float> area) const
{
    drawGrid (g, area, 6, 4);
    const auto size = value (ParamID::reverbSize);
    const auto decay = value (ParamID::reverbDecay);
    const auto damp = value (ParamID::reverbDamping);
    const auto pre = value (ParamID::reverbPredelay) * 0.001f;

    const auto windowSec = juce::jlimit (0.6f, 24.0f, pre + decay * 1.15f);
    auto xFor = [&] (float t) { return area.getX() + area.getWidth() * (t / windowSec); };
    const auto top = area.getY() + area.getHeight() * 0.12f, bottom = area.getBottom();

    auto envelope = [&] (float rt60) {
        juce::Path p;
        p.startNewSubPath (xFor (0.0f), bottom);
        p.lineTo (xFor (pre), bottom);
        for (int i = 0; i <= 100; ++i)
        {
            const auto t = pre + (windowSec - pre) * static_cast<float> (i) / 100.0f;
            const auto norm = juce::jlimit (0.0f, 1.0f, 1.0f - (t - pre) / rt60);
            p.lineTo (xFor (t), bottom - (bottom - top) * norm);
        }
        p.lineTo (area.getRight(), bottom);
        p.closeSubPath();
        return p;
    };
    const auto full = envelope (decay);
    g.setGradientFill (juce::ColourGradient (accent.withAlpha (0.45f), 0.0f, top, accent.withAlpha (0.04f), 0.0f, bottom, false));
    g.fillPath (full);
    g.setColour (accent);
    g.strokePath (full, juce::PathStrokeType (1.6f));
    g.setColour (Palette::text.withAlpha (0.16f));
    g.fillPath (envelope (std::max (0.05f, decay * (1.0f - 0.75f * damp))));

    juce::Random rng (1234);
    const auto count = 6 + juce::roundToInt (size * 10.0f);
    const auto spread = 0.02f + 0.08f * size;
    g.setColour (accent.brighter (0.4f));
    for (int i = 0; i < count; ++i)
    {
        const auto t = pre + spread * rng.nextFloat();
        const auto h = (bottom - top) * (0.35f + 0.55f * rng.nextFloat()) * (1.0f - (t - pre) / (spread * 1.6f));
        g.drawLine (xFor (t), bottom, xFor (t), bottom - h, 1.2f);
    }
    drawCaption (g, area, "RT60 " + juce::String (decay, decay < 10.0f ? 1 : 0) + " s");
}
} // namespace sfx::ui
