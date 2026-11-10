#include "Controls.h"

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
        slider.setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xffffb347));
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

void Knob::resized()
{
    auto area = getLocalBounds();
    if (style == Style::pedal)
        caption.setBounds (area.removeFromBottom (14));
    else
        caption.setBounds (area.removeFromTop (style == Style::alu ? 22 : 16));
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
        auto fill = bar.withTop (bar.getBottom() - bar.getHeight() * toNorm (levels[i]));
        juce::ColourGradient grad (juce::Colour (0xff4ad99a), 0.0f, bar.getBottom(), juce::Colour (0xffff4d5e), 0.0f, bar.getY(), false);
        grad.addColour (0.70, juce::Colour (0xffd8e05a));
        grad.addColour (0.88, juce::Colour (0xffff9d3d));
        g.setGradientFill (grad);
        g.fillRoundedRectangle (fill, 2.0f);

        if (peaks[i] > 0.001f)
        {
            const auto peakY = bar.getBottom() - bar.getHeight() * toNorm (peaks[i]);
            g.setColour (peaks[i] > 1.0f ? juce::Colour (0xffff4d5e) : Palette::text.withAlpha (0.8f));
            g.fillRect (bar.getX(), peakY - 1.0f, bar.getWidth(), 1.5f);
        }
        g.setColour (Palette::textFaint.withAlpha (0.8f));
        g.fillRect (bar.getX(), zeroY, bar.getWidth(), 1.0f);
    }
}
} // namespace sfx::ui
