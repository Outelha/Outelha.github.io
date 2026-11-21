#include "Controls.h"

#include "Hardware.h"

namespace sfx::ui
{
//==============================================================================
Knob::Knob (APVTS& state, const juce::String& paramId, const juce::String& captionText, juce::Colour accent, Style s)
    : style (s)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
    slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
    slider.setMouseDragSensitivity (220);
    slider.setVelocityModeParameters (0.8, 1, 0.08, true, juce::ModifierKeys::shiftModifier);
    slider.setScrollWheelEnabled (true);

    if (style == Style::pedal)
    {
        slider.getProperties().set ("style", "pedal");
        slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        slider.setPopupDisplayEnabled (true, true, nullptr, 900);
        slider.setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff1b1a18).withAlpha (0.75f));
    }
    else if (style == Style::alu)
    {
        slider.getProperties().set ("style", "alu");
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 96, 22);
        slider.setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xfff4efe6));
        slider.setMouseDragSensitivity (300);
    }
    else
    {
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 84, 18);
    }
    addAndMakeVisible (slider);

    caption.setText (captionText.toUpperCase(), juce::dontSendNotification);
    caption.setFont (uiFont (style == Style::pedal ? 10.5f : style == Style::alu ? 15.0f : 11.0f, true));
    caption.setJustificationType (juce::Justification::centred);
    caption.setColour (juce::Label::textColourId, style == Style::pedal ? juce::Colour (0xff1b1a18)
                                                  : style == Style::alu  ? juce::Colour (0xffc9c3b8)
                                                                         : Palette::textDim);
    caption.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (caption);

    attachment = std::make_unique<APVTS::SliderAttachment> (state, paramId, slider);

    if (auto* param = state.getParameter (paramId))
    {
        const auto range = state.getParameterRange (paramId);
        slider.setDoubleClickReturnValue (true, range.convertFrom0to1 (param->getDefaultValue()));
        slider.setTooltip (param->getName (64) + "  -  double-click to reset, shift-drag for fine control");
    }
}

void Knob::setInkColour (juce::Colour c)
{
    caption.setColour (juce::Label::textColourId, c);
    slider.setColour (juce::Slider::rotarySliderOutlineColourId, c.withAlpha (0.75f));
}

void Knob::paint (juce::Graphics& g)
{
    if (! valueWindow || slider.getTextBoxPosition() == juce::Slider::NoTextBox)
        return;
    const auto s = slider.getBounds();
    const auto h = slider.getTextBoxHeight();
    const auto w = juce::jmin (slider.getTextBoxWidth(), s.getWidth() - 4);
    const auto box = juce::Rectangle<int> (w, h).withCentre ({ s.getCentreX(), s.getBottom() - h / 2 }).toFloat();
    hw::drawTray (g, box.reduced (0.0f, 1.0f), 5.0f);
    g.setColour (juce::Colours::white.withAlpha (0.07f));
    g.drawRoundedRectangle (box.reduced (1.5f, 2.5f), 4.0f, 0.8f);
}

void Knob::resized()
{
    auto area = getLocalBounds();
    if (style == Style::pedal)
        caption.setBounds (area.removeFromBottom (14));
    else
        caption.setBounds (area.removeFromTop (style == Style::alu ? 30 : 20));
    slider.setBounds (area);
}

//==============================================================================
SyncSwap::SyncSwap (APVTS& state, const juce::String& syncId, const juce::String& timeId, const juce::String& divisionId,
                    const juce::String& caption, juce::Colour accent, Knob::Style style)
    : time (state, timeId, caption, accent, style), division (state, divisionId, caption, accent, style)
{
    addChildComponent (time);
    addChildComponent (division);
    if (auto* sync = state.getParameter (syncId))
    {
        syncAttachment = std::make_unique<juce::ParameterAttachment> (*sync, [this] (float v) {
            time.setVisible (v < 0.5f);
            division.setVisible (v >= 0.5f);
        });
        syncAttachment->sendInitialUpdate();
    }
}

void SyncSwap::resized()
{
    time.setBounds (getLocalBounds());
    division.setBounds (getLocalBounds());
}

//==============================================================================
ToggleChip::ToggleChip (APVTS& state, const juce::String& paramId, const juce::String& label, juce::Colour accent)
    : juce::TextButton (label)
{
    setClickingTogglesState (true);
    setColour (juce::TextButton::buttonOnColourId, accent);
    attachment = std::make_unique<APVTS::ButtonAttachment> (state, paramId, *this);
}

//==============================================================================
SegmentedChoice::SegmentedChoice (APVTS& state, const juce::String& paramId, juce::Colour accent)
{
    auto* parameter = dynamic_cast<juce::AudioParameterChoice*> (state.getParameter (paramId));
    jassert (parameter != nullptr);

    const auto& choices = parameter->choices;
    for (int i = 0; i < choices.size(); ++i)
    {
        auto* b = buttons.add (new juce::TextButton (choices[i]));
        b->setColour (juce::TextButton::buttonOnColourId, accent);
        b->setConnectedEdges ((i > 0 ? juce::Button::ConnectedOnLeft : 0) | (i < choices.size() - 1 ? juce::Button::ConnectedOnRight : 0));
        b->onClick = [this, i] {
            if (attachment != nullptr)
                attachment->setValueAsCompleteGesture (static_cast<float> (i));
        };
        addAndMakeVisible (b);
    }

    attachment = std::make_unique<juce::ParameterAttachment> (*parameter, [this] (float v) { select (juce::roundToInt (v)); });
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
LevelMeter::LevelMeter (const juce::String& c) : caption (c)
{
    setInterceptsMouseClicks (false, false);
}

int LevelMeter::litSegments (float gain) noexcept
{
    constexpr float minDb = -48.0f, maxDb = 3.0f;
    const auto db = juce::Decibels::gainToDecibels (gain, minDb - 1.0f);
    return juce::jlimit (0, segments, static_cast<int> (std::ceil ((db - minDb) / (maxDb - minDb) * segments)));
}

void LevelMeter::setLevels (float left, float right)
{
    const std::array<float, 2> in { left, right };
    bool changed = false;
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
        const auto lit = litSegments (levels[i]), peak = litSegments (peaks[i]);
        changed = changed || lit != shownLit[i] || peak != shownPeak[i];
        shownLit[i] = lit;
        shownPeak[i] = peak;
    }
    if (changed)
        repaint(); // only when a segment actually changes
}

void LevelMeter::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    auto label = area.removeFromBottom (16.0f);
    hw::drawEngraved (g, caption, label, font (Type::labelBold, 12.0f, 0.08f), juce::Justification::centred, juce::Colour (0xffd8c8b0));
    area.removeFromBottom (4.0f);
    hw::drawTray (g, area, 4.0f);

    const auto slot = area.reduced (4.0f, 5.0f);
    const auto colW = (slot.getWidth() - 2.0f) * 0.5f;
    const auto segH = slot.getHeight() / static_cast<float> (segments);
    for (size_t ch = 0; ch < 2; ++ch)
        for (int k = 0; k < segments; ++k)
        {
            const auto cell = juce::Rectangle<float> (slot.getX() + static_cast<float> (ch) * (colW + 2.0f), slot.getBottom() - segH * static_cast<float> (k + 1),
                                                      colW, segH).reduced (0.0f, segH * 0.18f);
            const auto colour = k >= segments - 2 ? juce::Colour (0xffff4a2a) : k >= segments - 5 ? juce::Colour (0xffffc040) : brandOrange;
            const bool lit = k < shownLit[ch] || k == shownPeak[ch] - 1;
            if (lit)
            {
                g.setColour (colour.withAlpha (0.25f));
                g.fillRect (cell.expanded (1.5f, 1.0f));
            }
            g.setColour (lit ? colour : colour.withAlpha (0.10f));
            g.fillRect (cell);
        }
}
} // namespace sfx::ui
