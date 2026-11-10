#include "TrioLookAndFeel.h"

namespace trio::ui
{
juce::Font uiFont (float height, bool bold)
{
    auto options = juce::FontOptions (height);
    if (bold)
        options = options.withStyle ("Bold");
    return juce::Font (options);
}

TrioLookAndFeel::TrioLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, Palette::background);
    setColour (juce::Slider::textBoxTextColourId, Palette::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId, Palette::track);
    setColour (juce::Slider::rotarySliderOutlineColourId, Palette::track);
    setColour (juce::Label::textColourId, Palette::textDim);
    setColour (juce::Label::textWhenEditingColourId, Palette::text);
    setColour (juce::Label::outlineWhenEditingColourId, Palette::track);
    setColour (juce::Label::backgroundWhenEditingColourId, Palette::display);
    setColour (juce::TextEditor::highlightColourId, Palette::track);
    setColour (juce::CaretComponent::caretColourId, Palette::text);
    setColour (juce::TextButton::buttonColourId, Palette::display);
    setColour (juce::TextButton::textColourOffId, Palette::textDim);
    setColour (juce::TextButton::textColourOnId, Palette::background);
    setColour (juce::TooltipWindow::backgroundColourId, Palette::panelEdge);
    setColour (juce::TooltipWindow::textColourId, Palette::text);
}

void TrioLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                        float sliderPos, float startAngle, float endAngle, juce::Slider& slider)
{
    using namespace juce;

    const auto accent = slider.findColour (Slider::rotarySliderFillColourId);
    const auto bounds = Rectangle<int> (x, y, width, height).toFloat().reduced (2.0f);
    const auto diameter = jmin (bounds.getWidth(), bounds.getHeight());
    const auto centre = bounds.getCentre();
    const auto radius = diameter * 0.5f;
    const auto arcThickness = jmax (2.5f, radius * 0.11f);
    const auto arcRadius = radius - arcThickness * 0.5f;
    const auto angle = startAngle + sliderPos * (endAngle - startAngle);
    const bool hover = slider.isMouseOverOrDragging() && slider.isEnabled();

    // Track
    Path track;
    track.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, endAngle, true);
    g.setColour (Palette::track);
    g.strokePath (track, PathStrokeType (arcThickness, PathStrokeType::curved, PathStrokeType::rounded));

    // Value arc (from the centre for bipolar parameters).
    const auto range = slider.getRange();
    const bool bipolar = range.getStart() < 0.0 && range.getEnd() > 0.0;
    const auto originAngle = bipolar ? startAngle + static_cast<float> (slider.valueToProportionOfLength (0.0)) * (endAngle - startAngle)
                                     : startAngle;
    if (std::abs (angle - originAngle) > 0.001f)
    {
        Path value;
        value.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                             jmin (originAngle, angle), jmax (originAngle, angle), true);
        g.setColour (accent.withAlpha (0.25f));
        g.strokePath (value, PathStrokeType (arcThickness * 2.2f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (accent);
        g.strokePath (value, PathStrokeType (arcThickness, PathStrokeType::curved, PathStrokeType::rounded));
    }

    // Knob body
    const auto bodyRadius = radius - arcThickness * 2.4f;
    const auto body = Rectangle<float> (bodyRadius * 2.0f, bodyRadius * 2.0f).withCentre (centre);

    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillEllipse (body.translated (0.0f, bodyRadius * 0.08f).expanded (1.5f));

    ColourGradient grad (Colour (0xff3a3f4b).brighter (hover ? 0.12f : 0.0f), centre.x, body.getY(),
                         Colour (0xff20232b), centre.x, body.getBottom(), false);
    g.setGradientFill (grad);
    g.fillEllipse (body);
    g.setColour (juce::Colours::white.withAlpha (0.07f));
    g.drawEllipse (body.reduced (0.5f), 1.0f);

    // Pointer
    const auto inner = bodyRadius * 0.35f;
    const auto outer = bodyRadius * 0.85f;
    const Point<float> p1 { centre.x + inner * std::sin (angle), centre.y - inner * std::cos (angle) };
    const Point<float> p2 { centre.x + outer * std::sin (angle), centre.y - outer * std::cos (angle) };
    g.setColour (slider.isEnabled() ? Palette::text : Palette::textFaint);
    g.drawLine ({ p1, p2 }, jmax (2.0f, bodyRadius * 0.1f));
}

juce::Label* TrioLookAndFeel::createSliderTextBox (juce::Slider& slider)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (slider);
    l->setFont (uiFont (12.5f));
    l->setJustificationType (juce::Justification::centred);
    l->setColour (juce::Label::textColourId, Palette::text);
    l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    return l;
}

juce::Slider::SliderLayout TrioLookAndFeel::getSliderLayout (juce::Slider& slider)
{
    juce::Slider::SliderLayout layout;
    auto area = slider.getLocalBounds();
    const auto textHeight = slider.getTextBoxHeight();
    layout.textBoxBounds = area.removeFromBottom (textHeight);
    layout.sliderBounds = area;
    return layout;
}

void TrioLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&,
                                            bool isMouseOverButton, bool isButtonDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
    const auto accent = button.findColour (juce::TextButton::buttonOnColourId);
    const bool on = button.getToggleState();

    auto fill = on ? accent : Palette::display;
    if (! on && isMouseOverButton)
        fill = fill.brighter (0.08f);
    if (isButtonDown)
        fill = fill.darker (0.1f);

    const bool left = ! button.isConnectedOnLeft();
    const bool right = ! button.isConnectedOnRight();
    const auto corner = 5.0f;

    juce::Path p;
    p.addRoundedRectangle (bounds.getX(), bounds.getY(), bounds.getWidth(), bounds.getHeight(), corner, corner,
                           left, right, left, right);
    g.setColour (fill);
    g.fillPath (p);
    g.setColour (on ? accent.brighter (0.2f) : Palette::panelEdge);
    g.strokePath (p, juce::PathStrokeType (1.0f));
}

void TrioLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button, bool, bool)
{
    g.setFont (uiFont (12.0f, true));
    g.setColour (button.findColour (button.getToggleState() ? juce::TextButton::textColourOnId
                                                            : juce::TextButton::textColourOffId));
    g.drawText (button.getButtonText().toUpperCase(), button.getLocalBounds(), juce::Justification::centred);
}
} // namespace trio::ui
