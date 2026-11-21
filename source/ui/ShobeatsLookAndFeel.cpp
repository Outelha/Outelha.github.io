#include "ShobeatsLookAndFeel.h"

#include "Hardware.h"
#include "Textures.h"

namespace sfx::ui
{
juce::Image noiseTexture()
{
    // Returns a counted handle; the pixels are owned by the open editors (see Textures.h).
    return juce::SharedResourcePointer<Textures>()->noise;
}

void fillWithTexture (juce::Graphics& g, const juce::Path& area, float opacity)
{
    juce::Graphics::ScopedSaveState save (g);
    g.reduceClipRegion (area);
    g.setTiledImageFill (noiseTexture(), 0, 0, opacity);
    g.fillPath (area);
}

ShobeatsLookAndFeel::ShobeatsLookAndFeel()
{
    using namespace Palette;
    setColour (juce::ResizableWindow::backgroundColourId, background);
    setColour (juce::Slider::textBoxTextColourId, text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId, track);
    setColour (juce::Slider::rotarySliderOutlineColourId, track);
    setColour (juce::Label::textColourId, textDim);
    setColour (juce::Label::textWhenEditingColourId, text);
    setColour (juce::Label::outlineWhenEditingColourId, track);
    setColour (juce::Label::backgroundWhenEditingColourId, display);
    setColour (juce::TextEditor::highlightColourId, track);
    setColour (juce::CaretComponent::caretColourId, text);
    setColour (juce::TextButton::buttonColourId, display);
    setColour (juce::TextButton::buttonOnColourId, brand);
    setColour (juce::TextButton::textColourOffId, textDim);
    setColour (juce::TextButton::textColourOnId, background);
    setColour (juce::TooltipWindow::backgroundColourId, panelEdge);
    setColour (juce::TooltipWindow::textColourId, text);
    setColour (juce::BubbleComponent::backgroundColourId, display);
    setColour (juce::BubbleComponent::outlineColourId, panelEdge);
    setColour (juce::ScrollBar::thumbColourId, track.brighter (0.3f));
}

void ShobeatsLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                            float sliderPos, float startAngle, float endAngle, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat();
    const auto angle = startAngle + sliderPos * (endAngle - startAngle);
    const auto style = slider.getProperties()["style"].toString();
    if (style == "pedal")
    {
        drawPedalKnob (g, bounds, angle, startAngle, endAngle, slider);
        return;
    }
    if (style == "hidden")
        return; // the owner paints the control itself (tape reels)

    hw::KnobLook look;
    look.accent = slider.findColour (juce::Slider::rotarySliderFillColourId);
    look.proportion = sliderPos;
    look.startAngle = startAngle;
    look.endAngle = endAngle;
    look.angle = angle;
    look.hover = slider.isMouseOver (true);
    look.dragging = slider.isMouseButtonDown();
    look.numbers = style == "alu";
    const auto range = slider.getRange();
    if (range.getStart() < 0.0 && range.getEnd() > 0.0)
        look.originProportion = static_cast<float> (slider.valueToProportionOfLength (0.0));
    hw::drawMachinedKnob (g, bounds.reduced (2.0f), look);
}

void ShobeatsLookAndFeel::drawPedalKnob (juce::Graphics& g, juce::Rectangle<float> bounds, float angle,
                                         float, float, juce::Slider& slider)
{
    const auto d = juce::jmin (bounds.getWidth(), bounds.getHeight());
    hw::drawPedalKnob (g, bounds.withSizeKeepingCentre (d, d).reduced (d * 0.08f), angle, slider.isMouseOverOrDragging());
}

void ShobeatsLookAndFeel::drawArcKnob (juce::Graphics& g, juce::Rectangle<float> bounds, float angle,
                                       float startAngle, float endAngle, juce::Slider& slider)
{
    hw::KnobLook look;
    look.accent = slider.findColour (juce::Slider::rotarySliderFillColourId);
    look.startAngle = startAngle;
    look.endAngle = endAngle;
    look.angle = angle;
    look.proportion = (angle - startAngle) / (endAngle - startAngle);
    hw::drawMachinedKnob (g, bounds, look);
}

juce::Label* ShobeatsLookAndFeel::createSliderTextBox (juce::Slider& slider)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (slider);
    l->setFont (font (Type::value, 16.0f, 0.02f));
    l->setJustificationType (juce::Justification::centred);
    l->setColour (juce::Label::textColourId, Palette::text);
    l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    return l;
}

juce::Slider::SliderLayout ShobeatsLookAndFeel::getSliderLayout (juce::Slider& slider)
{
    juce::Slider::SliderLayout layout;
    auto area = slider.getLocalBounds();
    if (slider.getTextBoxPosition() != juce::Slider::NoTextBox)
        layout.textBoxBounds = area.removeFromBottom (slider.getTextBoxHeight());
    layout.sliderBounds = area;
    return layout;
}

void ShobeatsLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&, bool over, bool down)
{
    hw::KeyLook key;
    key.accent = button.findColour (juce::TextButton::buttonOnColourId);
    key.lit = button.getToggleState();
    if (button.getProperties()["led"])
    {
        // Dark key with an LED and an accent rim when on.
        key.lit = false;
        key.over = over;
        key.down = down;
        key.corner = 8.0f;
        const auto r = button.getLocalBounds().toFloat().reduced (1.0f, 1.5f);
        if (button.getToggleState())
            hw::drawRimGlow (g, r, 8.0f, key.accent, 6.0f, 0.7f);
        hw::drawKey (g, r, key);
        g.setColour (button.getToggleState() ? key.accent.withAlpha (0.9f) : juce::Colour (0xff3a3632));
        g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.2f);
        hw::drawLed (g, { r.getX() + 22.0f, r.getCentreY() }, 4.2f, key.accent, button.getToggleState());
        return;
    }
    key.over = over;
    key.down = down;
    key.connectedLeft = button.isConnectedOnLeft();
    key.connectedRight = button.isConnectedOnRight();
    key.corner = juce::jmin (7.0f, static_cast<float> (button.getHeight()) * 0.22f);
    hw::drawKey (g, button.getLocalBounds().toFloat().reduced (1.0f, 1.5f), key);
}

void ShobeatsLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button, bool, bool)
{
    const auto lit = button.getToggleState();
    const auto size = juce::jlimit (12.0f, 18.0f, static_cast<float> (button.getHeight()) * 0.46f);
    auto area = button.getLocalBounds().toFloat().reduced (4.0f, 0.0f);
    if (button.isDown())
        area = area.translated (0.0f, 0.8f);
    const auto text = button.getButtonText().toUpperCase();
    if (button.getProperties()["led"])
    {
        hw::drawKeyText (g, text, area.withTrimmedLeft (26.0f), font (Type::labelBold, size, 0.06f), false, juce::Colours::white);
        return;
    }
    if (text == "<" || text == ">")
    {
        // Navigation arrows: orange chevrons.
        const auto c = area.getCentre();
        const auto h = juce::jmin (area.getHeight(), area.getWidth()) * 0.18f;
        juce::Path chevron;
        const auto dir = text == "<" ? -1.0f : 1.0f;
        chevron.startNewSubPath (c.x - dir * h * 0.5f, c.y - h);
        chevron.lineTo (c.x + dir * h * 0.5f, c.y);
        chevron.lineTo (c.x - dir * h * 0.5f, c.y + h);
        g.setColour (juce::Colour (0xffff8a1f).withMultipliedAlpha (button.isEnabled() ? 1.0f : 0.4f));
        g.strokePath (chevron, juce::PathStrokeType (2.4f, juce::PathStrokeType::mitered, juce::PathStrokeType::rounded));
        return;
    }
    hw::drawKeyText (g, text, area, font (Type::labelBold, size, 0.04f), lit,
                     button.findColour (juce::TextButton::buttonOnColourId), button.isEnabled());
}

void ShobeatsLookAndFeel::drawBubble (juce::Graphics& g, juce::BubbleComponent&, const juce::Point<float>&, const juce::Rectangle<float>& body)
{
    hw::drawTray (g, body, 5.0f);
    g.setColour (brandOrange.withAlpha (0.5f));
    g.drawRoundedRectangle (body.reduced (0.5f), 5.0f, 1.0f);
}

juce::Font ShobeatsLookAndFeel::getSliderPopupFont (juce::Slider&)
{
    return font (Type::value, 15.0f);
}

int ShobeatsLookAndFeel::getSliderPopupPlacement (juce::Slider&)
{
    return juce::BubbleComponent::above;
}
} // namespace sfx::ui
