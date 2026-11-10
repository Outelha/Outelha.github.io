#include "Components.h"

#include "dsp/Saturation.h"

namespace trio::ui
{
namespace
{
std::atomic<float>& rawParam (APVTS& s, const char* id)
{
    auto* p = s.getRawParameterValue (id);
    jassert (p != nullptr);
    return *p;
}
} // namespace

//==============================================================================
Knob::Knob (APVTS& state, const juce::String& paramId, const juce::String& captionText, juce::Colour accent)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 18);
    slider.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
    slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
    slider.setMouseDragSensitivity (220);
    slider.setVelocityModeParameters (0.8, 1, 0.08, true, juce::ModifierKeys::shiftModifier);
    slider.setScrollWheelEnabled (true);
    addAndMakeVisible (slider);

    caption.setText (captionText.toUpperCase(), juce::dontSendNotification);
    caption.setFont (uiFont (11.0f, true));
    caption.setJustificationType (juce::Justification::centred);
    caption.setColour (juce::Label::textColourId, Palette::textDim);
    caption.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (caption);

    attachment = std::make_unique<APVTS::SliderAttachment> (state, paramId, slider);

    if (auto* param = state.getParameter (paramId))
    {
        const auto range = state.getParameterRange (paramId);
        slider.setDoubleClickReturnValue (true, range.convertFrom0to1 (param->getDefaultValue()));
        slider.setTooltip (param->getName (64) + "  (double-click to reset, shift-drag for fine control)");
    }
}

void Knob::resized()
{
    auto area = getLocalBounds();
    caption.setBounds (area.removeFromTop (16));
    slider.setBounds (area);
}

//==============================================================================
PowerButton::PowerButton (APVTS& state, const juce::String& paramId, juce::Colour accentColour)
    : juce::Button ("Power"), accent (accentColour)
{
    setClickingTogglesState (true);
    setTooltip ("Enable / bypass this module");
    attachment = std::make_unique<APVTS::ButtonAttachment> (state, paramId, *this);
}

void PowerButton::paintButton (juce::Graphics& g, bool isMouseOver, bool isButtonDown)
{
    auto b = getLocalBounds().toFloat().reduced (1.0f);
    const auto d = juce::jmin (b.getWidth(), b.getHeight());
    b = b.withSizeKeepingCentre (d, d);
    const bool on = getToggleState();

    g.setColour (on ? accent.withAlpha (0.18f) : Palette::display);
    g.fillEllipse (b);
    g.setColour (on ? accent : (isMouseOver ? Palette::textDim : Palette::panelEdge.brighter (0.3f)));
    g.drawEllipse (b.reduced (0.5f), 1.0f);

    const auto c = b.getCentre();
    const auto r = d * 0.24f;
    juce::Path icon;
    icon.addCentredArc (c.x, c.y, r, r, 0.0f, juce::degreesToRadians (40.0f), juce::degreesToRadians (320.0f), true);
    icon.startNewSubPath (c.x, c.y - r * 1.25f);
    icon.lineTo (c.x, c.y - r * 0.2f);

    auto iconColour = on ? accent : Palette::textFaint;
    if (isButtonDown)
        iconColour = iconColour.darker (0.2f);
    g.setColour (iconColour);
    g.strokePath (icon, juce::PathStrokeType (juce::jmax (1.5f, d * 0.08f), juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

//==============================================================================
SegmentedChoice::SegmentedChoice (APVTS& state, const juce::String& paramId, juce::Colour accent)
{
    parameter = dynamic_cast<juce::AudioParameterChoice*> (state.getParameter (paramId));
    jassert (parameter != nullptr);

    const auto& choices = parameter->choices;
    for (int i = 0; i < choices.size(); ++i)
    {
        auto* b = buttons.add (new juce::TextButton (choices[i]));
        b->setClickingTogglesState (false);
        b->setColour (juce::TextButton::buttonOnColourId, accent);
        b->setConnectedEdges ((i > 0 ? juce::Button::ConnectedOnLeft : 0)
                              | (i < choices.size() - 1 ? juce::Button::ConnectedOnRight : 0));
        b->onClick = [this, i] {
            if (attachment != nullptr)
                attachment->setValueAsCompleteGesture (static_cast<float> (i));
        };
        addAndMakeVisible (b);
    }

    attachment = std::make_unique<juce::ParameterAttachment> (*parameter, [this] (float v) { select (juce::roundToInt (v)); }, nullptr);
    attachment->sendInitialUpdate();
}

void SegmentedChoice::select (int index)
{
    for (int i = 0; i < buttons.size(); ++i)
        buttons[i]->setToggleState (i == index, juce::dontSendNotification);
}

void SegmentedChoice::resized()
{
    auto area = getLocalBounds();
    const auto w = area.getWidth() / juce::jmax (1, buttons.size());
    for (int i = 0; i < buttons.size(); ++i)
        buttons[i]->setBounds (i == buttons.size() - 1 ? area : area.removeFromLeft (w));
}

//==============================================================================
ModulePanel::ModulePanel (APVTS& s, const juce::String& titleText, const juce::String& subtitleText,
                          const juce::String& enabledParamId, juce::Colour accentColour)
    : state (s), accent (accentColour), title (titleText), subtitle (subtitleText), power (s, enabledParamId, accentColour)
{
    addAndMakeVisible (power);
    power.addListener (this);
    lastOn = power.getToggleState();
}

ModulePanel::~ModulePanel()
{
    power.removeListener (this);
}

void ModulePanel::addContent (juce::Component& c)
{
    content.add (&c);
    addAndMakeVisible (c);
    refreshEnabledLook();
}

void ModulePanel::buttonStateChanged (juce::Button*)
{
    if (power.getToggleState() != lastOn)
    {
        lastOn = power.getToggleState();
        refreshEnabledLook();
        repaint();
    }
}

void ModulePanel::refreshEnabledLook()
{
    const auto alpha = power.getToggleState() ? 1.0f : 0.38f;
    for (auto* c : content)
        c->setAlpha (alpha);
}

void ModulePanel::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (0.5f);
    const bool on = power.getToggleState();

    juce::ColourGradient bg (Palette::panel.brighter (0.04f), 0.0f, bounds.getY(), Palette::panel, 0.0f, bounds.getBottom(), false);
    g.setGradientFill (bg);
    g.fillRoundedRectangle (bounds, 12.0f);
    g.setColour (Palette::panelEdge);
    g.drawRoundedRectangle (bounds, 12.0f, 1.0f);

    // Accent strip along the top edge.
    juce::Path strip;
    strip.addRoundedRectangle (bounds.getX() + 18.0f, bounds.getY(), bounds.getWidth() - 36.0f, 2.5f, 1.25f);
    g.setColour (on ? accent : Palette::track);
    g.fillPath (strip);

    auto header = getLocalBounds().reduced (18, 0).withTrimmedTop (16).removeFromTop (34);
    header.removeFromRight (40);
    g.setColour (on ? Palette::text : Palette::textDim);
    g.setFont (uiFont (17.0f, true));
    g.drawText (title.toUpperCase(), header.removeFromTop (20), juce::Justification::centredLeft);
    g.setColour (Palette::textFaint);
    g.setFont (uiFont (11.5f));
    g.drawText (subtitle, header, juce::Justification::centredLeft);
}

void ModulePanel::resized()
{
    auto area = getLocalBounds().reduced (18, 16);
    auto header = area.removeFromTop (34);
    power.setBounds (header.removeFromRight (30).withSizeKeepingCentre (28, 28));
    area.removeFromTop (12);
    layoutContent (area);
}

//==============================================================================
DisplayBase::DisplayBase (juce::Colour accentColour) : accent (accentColour)
{
    setInterceptsMouseClicks (false, false);
    startTimerHz (30);
}

void DisplayBase::timerCallback()
{
    if (poll())
        repaint();
}

void DisplayBase::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (Palette::display);
    g.fillRoundedRectangle (bounds, 8.0f);

    {
        juce::Graphics::ScopedSaveState save (g);
        juce::Path clip;
        clip.addRoundedRectangle (bounds, 8.0f);
        g.reduceClipRegion (clip);
        drawContent (g, bounds.reduced (10.0f, 10.0f));
    }

    g.setColour (Palette::panelEdge);
    g.drawRoundedRectangle (bounds, 8.0f, 1.0f);
}

void DisplayBase::drawGrid (juce::Graphics& g, juce::Rectangle<float> area, int columns, int rows) const
{
    g.setColour (Palette::track.withAlpha (0.35f));
    for (int c = 1; c < columns; ++c)
    {
        const auto x = area.getX() + area.getWidth() * static_cast<float> (c) / static_cast<float> (columns);
        g.drawVerticalLine (juce::roundToInt (x), area.getY(), area.getBottom());
    }
    for (int r = 1; r < rows; ++r)
    {
        const auto y = area.getY() + area.getHeight() * static_cast<float> (r) / static_cast<float> (rows);
        g.drawHorizontalLine (juce::roundToInt (y), area.getX(), area.getRight());
    }
}

//==============================================================================
SaturationCurve::SaturationCurve (APVTS& s)
    : DisplayBase (Palette::saturation),
      drive (rawParam (s, "satDrive")),
      mode (rawParam (s, "satMode")),
      mix (rawParam (s, "satMix")),
      enabled (rawParam (s, "satEnabled"))
{
    poll();
}

bool SaturationCurve::poll()
{
    const std::array<float, 4> now { drive.load(), mode.load(), mix.load(), enabled.load() };
    const std::array<float, 4> previous { lastDrive, lastMode, lastMix, lastEnabled };
    if (now == previous)
        return false;
    lastDrive = now[0];
    lastMode = now[1];
    lastMix = now[2];
    lastEnabled = now[3];
    return true;
}

void SaturationCurve::drawContent (juce::Graphics& g, juce::Rectangle<float> area)
{
    using dsp::Saturation;

    // Square plot, centred.
    const auto side = juce::jmin (area.getWidth(), area.getHeight());
    auto plot = area.withSizeKeepingCentre (side * 1.6f > area.getWidth() ? area.getWidth() : side * 1.6f, side);
    drawGrid (g, plot, 4, 4);

    const auto modeValue = static_cast<Saturation::Mode> (juce::jlimit (0, Saturation::numModes - 1, juce::roundToInt (lastMode)));
    const auto driveGain = std::pow (10.0f, lastDrive / 20.0f);
    const auto comp = Saturation::compensationGain (modeValue, driveGain);
    const auto wet = lastEnabled > 0.5f ? lastMix : 0.0f;
    constexpr float range = 1.0f; // +-0 dBFS

    auto toScreen = [plot] (float x, float y) {
        return juce::Point<float> { plot.getX() + (x * 0.5f + 0.5f) * plot.getWidth(),
                                    plot.getY() + (0.5f - y * 0.5f) * plot.getHeight() };
    };

    // Unity reference.
    g.setColour (Palette::textFaint.withAlpha (0.6f));
    g.drawLine ({ toScreen (-range, -range), toScreen (range, range) }, 1.0f);

    juce::Path curve;
    constexpr int steps = 160;
    for (int i = 0; i <= steps; ++i)
    {
        const auto x = -range + 2.0f * range * static_cast<float> (i) / steps;
        // DC-free view of the shaper (the bias offset is removed by the DC blocker in the audio path).
        const auto shaped = Saturation::shape (modeValue, x * driveGain) * comp;
        const auto y = juce::jlimit (-1.05f, 1.05f, x + wet * (shaped - x));
        const auto p = toScreen (x, y);
        if (i == 0)
            curve.startNewSubPath (p);
        else
            curve.lineTo (p);
    }

    g.setColour (accent.withAlpha (0.22f));
    g.strokePath (curve, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (accent);
    g.strokePath (curve, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setFont (uiFont (11.0f, true));
    g.setColour (Palette::textDim);
    g.drawText ("+" + juce::String (lastDrive, 1) + " dB", area.removeFromTop (14.0f), juce::Justification::topRight);
}

//==============================================================================
ChorusDisplay::ChorusDisplay (APVTS& s)
    : DisplayBase (Palette::chorus),
      rate (rawParam (s, "chorusRate")),
      depth (rawParam (s, "chorusDepth")),
      delay (rawParam (s, "chorusDelay")),
      enabled (rawParam (s, "chorusEnabled"))
{
    lastTime = juce::Time::getMillisecondCounterHiRes();
}

bool ChorusDisplay::poll()
{
    const auto now = juce::Time::getMillisecondCounterHiRes();
    const auto dt = (now - lastTime) * 0.001;
    lastTime = now;
    if (enabled.load() > 0.5f)
        phase = std::fmod (phase + dt * static_cast<double> (rate.load()), 1.0);
    return true;
}

void ChorusDisplay::drawContent (juce::Graphics& g, juce::Rectangle<float> area)
{
    drawGrid (g, area, 6, 4);

    const auto d = depth.load();
    const auto centreNorm = (delay.load() - 5.0f) / 25.0f; // 0..1 across the delay range
    const auto midY = area.getBottom() - area.getHeight() * (0.25f + 0.5f * centreNorm);
    const auto amp = area.getHeight() * 0.22f * d;

    // Two cycles across the width; each voice drawn as a travelling sine.
    const std::array<std::pair<float, float>, 4> voices { { { 0.0f, 1.0f }, { 0.5f, 0.75f }, { 0.25f, 0.45f }, { 0.75f, 0.3f } } };
    for (auto it = voices.rbegin(); it != voices.rend(); ++it)
    {
        const auto [offset, alpha] = *it;
        juce::Path p;
        constexpr int steps = 120;
        for (int i = 0; i <= steps; ++i)
        {
            const auto t = static_cast<float> (i) / steps;
            const auto ph = static_cast<float> (phase) + offset - t * 2.0f;
            const auto y = midY - amp * std::sin (juce::MathConstants<float>::twoPi * ph);
            const juce::Point<float> pt { area.getX() + t * area.getWidth(), y };
            if (i == 0)
                p.startNewSubPath (pt);
            else
                p.lineTo (pt);
        }
        g.setColour (accent.withAlpha (alpha * 0.25f));
        g.strokePath (p, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (accent.withAlpha (alpha));
        g.strokePath (p, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // "Now" marker at the left edge.
    const auto nowY = midY - amp * std::sin (juce::MathConstants<float>::twoPi * static_cast<float> (phase));
    g.setColour (Palette::text);
    g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre ({ area.getX() + 3.5f, nowY }));

    g.setFont (uiFont (11.0f, true));
    g.setColour (Palette::textDim);
    g.drawText ("L / R  4 VOICES", area.removeFromTop (14.0f), juce::Justification::topRight);
}

//==============================================================================
ReverbDisplay::ReverbDisplay (APVTS& s)
    : DisplayBase (Palette::reverb),
      size (rawParam (s, "reverbSize")),
      decay (rawParam (s, "reverbDecay")),
      damping (rawParam (s, "reverbDamping")),
      predelay (rawParam (s, "reverbPredelay")),
      enabled (rawParam (s, "reverbEnabled"))
{
    poll();
}

bool ReverbDisplay::poll()
{
    const std::array<float, 5> now { size.load(), decay.load(), damping.load(), predelay.load(), enabled.load() };
    if (now == last)
        return false;
    last = now;
    return true;
}

void ReverbDisplay::drawContent (juce::Graphics& g, juce::Rectangle<float> area)
{
    drawGrid (g, area, 6, 4);

    const auto sz = last[0], dec = last[1], damp = last[2], pre = last[3];

    // Time axis: auto-scale to the decay so the shape always reads well.
    const auto windowSec = juce::jlimit (0.6f, 24.0f, pre * 0.001f + dec * 1.15f);
    auto xForTime = [&] (float t) { return area.getX() + area.getWidth() * (t / windowSec); };
    const auto preSec = pre * 0.001f;
    const auto top = area.getY() + area.getHeight() * 0.12f;
    const auto bottom = area.getBottom();

    auto envelope = [&] (float rt60) {
        juce::Path p;
        p.startNewSubPath (xForTime (0.0f), bottom);
        p.lineTo (xForTime (preSec), bottom);
        constexpr int steps = 100;
        for (int i = 0; i <= steps; ++i)
        {
            const auto t = preSec + (windowSec - preSec) * static_cast<float> (i) / steps;
            const auto db = -60.0f * (t - preSec) / rt60; // linear in dB
            const auto norm = juce::jlimit (0.0f, 1.0f, 1.0f + db / 60.0f);
            p.lineTo (xForTime (t), bottom - (bottom - top) * norm);
        }
        p.lineTo (area.getRight(), bottom);
        p.closeSubPath();
        return p;
    };

    // Full-band and high-frequency decays (damping shortens the highs).
    const auto highRt = dec * (1.0f - 0.75f * damp);
    auto full = envelope (dec);
    auto highs = envelope (juce::jmax (0.05f, highRt));

    g.setGradientFill (juce::ColourGradient (accent.withAlpha (0.45f), 0.0f, top, accent.withAlpha (0.04f), 0.0f, bottom, false));
    g.fillPath (full);
    g.setColour (accent);
    g.strokePath (full, juce::PathStrokeType (1.6f));
    g.setColour (Palette::text.withAlpha (0.18f));
    g.fillPath (highs);

    // Early reflections: density follows size.
    juce::Random rng (1234);
    const auto count = 6 + juce::roundToInt (sz * 10.0f);
    const auto spread = 0.02f + 0.08f * sz;
    g.setColour (accent.brighter (0.4f));
    for (int i = 0; i < count; ++i)
    {
        const auto t = preSec + spread * rng.nextFloat();
        const auto h = (bottom - top) * (0.35f + 0.55f * rng.nextFloat()) * (1.0f - (t - preSec) / (spread * 1.6f));
        g.drawLine (xForTime (t), bottom, xForTime (t), bottom - h, 1.2f);
    }

    g.setFont (uiFont (11.0f, true));
    g.setColour (Palette::textDim);
    g.drawText ("RT60 " + juce::String (dec, dec < 10.0f ? 1 : 0) + " s", area.removeFromTop (14.0f), juce::Justification::topRight);
}

//==============================================================================
LevelMeter::LevelMeter (const juce::String& c) : caption (c)
{
    setInterceptsMouseClicks (false, false);
}

void LevelMeter::setLevels (float left, float right)
{
    const std::array<float, 2> in { left, right };
    for (size_t i = 0; i < 2; ++i)
    {
        levels[i] = juce::jmax (in[i], levels[i] * 0.82f);
        if (in[i] >= peaks[i])
        {
            peaks[i] = in[i];
            peakHold[i] = 30;
        }
        else if (--peakHold[i] <= 0)
        {
            peaks[i] *= 0.92f;
        }
    }
    repaint();
}

void LevelMeter::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    g.setFont (uiFont (10.0f, true));
    g.setColour (Palette::textFaint);
    g.drawText (caption, area.removeFromBottom (14.0f), juce::Justification::centred);
    area.removeFromBottom (3.0f);

    constexpr float minDb = -60.0f, maxDb = 6.0f;
    auto toNorm = [] (float gain) {
        const auto db = juce::Decibels::gainToDecibels (gain, minDb);
        return juce::jlimit (0.0f, 1.0f, (db - minDb) / (maxDb - minDb));
    };

    const auto barW = (area.getWidth() - 3.0f) * 0.5f;
    for (size_t i = 0; i < 2; ++i)
    {
        auto bar = juce::Rectangle<float> (area.getX() + static_cast<float> (i) * (barW + 3.0f), area.getY(), barW, area.getHeight());
        g.setColour (Palette::display);
        g.fillRoundedRectangle (bar, 2.0f);

        const auto zeroY = bar.getBottom() - bar.getHeight() * ((0.0f - minDb) / (maxDb - minDb));
        const auto h = bar.getHeight() * toNorm (levels[i]);
        auto fill = bar.withTop (bar.getBottom() - h);

        juce::ColourGradient grad (juce::Colour (0xff4ad99a), 0.0f, bar.getBottom(), juce::Colour (0xffff4d5e), 0.0f, bar.getY(), false);
        grad.addColour (0.70, juce::Colour (0xffd8e05a));
        grad.addColour (0.88, juce::Colour (0xffff9d3d));
        g.setGradientFill (grad);
        g.fillRoundedRectangle (fill, 2.0f);

        const auto peakY = bar.getBottom() - bar.getHeight() * toNorm (peaks[i]);
        if (peaks[i] > 0.001f)
        {
            g.setColour (peaks[i] > 1.0f ? juce::Colour (0xffff4d5e) : Palette::text.withAlpha (0.8f));
            g.fillRect (bar.getX(), peakY - 1.0f, bar.getWidth(), 1.5f);
        }

        g.setColour (Palette::textFaint.withAlpha (0.8f));
        g.fillRect (bar.getX(), zeroY, bar.getWidth(), 1.0f);
    }
}
} // namespace trio::ui
