#include "ShobeatsLookAndFeel.h"

namespace sfx::ui
{
juce::Font uiFont (float height, bool bold)
{
    auto options = juce::FontOptions (height);
    if (bold)
        options = options.withStyle ("Bold");
    return juce::Font (options);
}

const juce::Image& noiseTexture()
{
    static const juce::Image tile = [] {
        juce::Image img (juce::Image::ARGB, 128, 128, true);
        juce::Random rng (0x5b0b);
        for (int y = 0; y < img.getHeight(); ++y)
            for (int x = 0; x < img.getWidth(); ++x)
            {
                const auto v = rng.nextFloat();
                const auto c = v > 0.5f ? juce::Colours::white : juce::Colours::black;
                img.setPixelAt (x, y, c.withAlpha (std::abs (v - 0.5f) * 0.5f));
            }
        return img;
    }();
    return tile;
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
}

void ShobeatsLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                            float sliderPos, float startAngle, float endAngle, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat();
    const auto angle = startAngle + sliderPos * (endAngle - startAngle);
    if (slider.getProperties()["style"].toString() == "pedal")
        drawPedalKnob (g, bounds, angle, startAngle, endAngle, slider);
    else
        drawArcKnob (g, bounds, angle, startAngle, endAngle, slider);
}

void ShobeatsLookAndFeel::drawPedalKnob (juce::Graphics& g, juce::Rectangle<float> bounds, float angle,
                                         float startAngle, float endAngle, juce::Slider& slider)
{
    using namespace juce;
    const auto d = jmin (bounds.getWidth(), bounds.getHeight());
    const auto centre = bounds.getCentre();
    const auto r = d * 0.5f;
    const bool hover = slider.isMouseOverOrDragging();

    // Scale ticks printed on the enclosure.
    g.setColour (slider.findColour (Slider::rotarySliderOutlineColourId));
    constexpr int ticks = 11;
    for (int i = 0; i < ticks; ++i)
    {
        const auto a = startAngle + (endAngle - startAngle) * static_cast<float> (i) / (ticks - 1);
        const auto inner = r * 0.86f, outer = r * (i % 5 == 0 ? 1.0f : 0.95f);
        g.drawLine (centre.x + inner * std::sin (a), centre.y - inner * std::cos (a),
                    centre.x + outer * std::sin (a), centre.y - outer * std::cos (a), i % 5 == 0 ? 1.6f : 1.0f);
    }

    // Drop shadow.
    const auto knobR = r * 0.78f;
    auto body = Rectangle<float> (knobR * 2.0f, knobR * 2.0f).withCentre (centre);
    g.setColour (Colours::black.withAlpha (0.55f));
    g.fillEllipse (body.translated (0.0f, knobR * 0.12f).expanded (knobR * 0.04f));

    // Skirt (knurled outer ring) — the knurls rotate with the knob.
    ColourGradient skirt (Colour (0xff3a3a3d), centre.x - knobR, centre.y - knobR, Colour (0xff0b0b0c), centre.x + knobR, centre.y + knobR, false);
    g.setGradientFill (skirt);
    g.fillEllipse (body);
    g.setColour (Colours::white.withAlpha (0.07f));
    constexpr int knurls = 36;
    for (int i = 0; i < knurls; ++i)
    {
        const auto a = angle + MathConstants<float>::twoPi * static_cast<float> (i) / knurls;
        g.drawLine (centre.x + knobR * 0.80f * std::sin (a), centre.y - knobR * 0.80f * std::cos (a),
                    centre.x + knobR * 0.98f * std::sin (a), centre.y - knobR * 0.98f * std::cos (a), 1.0f);
    }

    // Cap with a soft top-left highlight.
    const auto capR = knobR * 0.74f;
    const auto cap = Rectangle<float> (capR * 2.0f, capR * 2.0f).withCentre (centre);
    ColourGradient capGrad (Colour (hover ? 0xff4b4b50 : 0xff3c3c40), cap.getX(), cap.getY(), Colour (0xff141415), cap.getRight(), cap.getBottom(), false);
    g.setGradientFill (capGrad);
    g.fillEllipse (cap);
    g.setColour (Colours::white.withAlpha (0.10f));
    g.drawEllipse (cap.reduced (0.6f), 1.0f);
    g.setColour (Colours::white.withAlpha (0.07f));
    g.fillEllipse (cap.withSizeKeepingCentre (capR * 1.1f, capR * 0.7f).translated (-capR * 0.2f, -capR * 0.35f));

    // Pointer line.
    const Point<float> p1 { centre.x + capR * 0.15f * std::sin (angle), centre.y - capR * 0.15f * std::cos (angle) };
    const Point<float> p2 { centre.x + knobR * 0.95f * std::sin (angle), centre.y - knobR * 0.95f * std::cos (angle) };
    g.setColour (Colour (0xfff4f1ea));
    g.drawLine ({ p1, p2 }, jmax (2.0f, knobR * 0.11f));
}

void ShobeatsLookAndFeel::drawArcKnob (juce::Graphics& g, juce::Rectangle<float> bounds, float angle,
                                       float startAngle, float endAngle, juce::Slider& slider)
{
    using namespace juce;
    const auto accent = slider.findColour (Slider::rotarySliderFillColourId);
    bounds = bounds.reduced (2.0f);
    const auto diameter = jmin (bounds.getWidth(), bounds.getHeight());
    const auto centre = bounds.getCentre();
    const auto radius = diameter * 0.5f;
    const auto thickness = jmax (2.5f, radius * 0.1f);
    const auto arcR = radius - thickness * 0.5f;

    Path track;
    track.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (Palette::track);
    g.strokePath (track, PathStrokeType (thickness, PathStrokeType::curved, PathStrokeType::rounded));

    const auto range = slider.getRange();
    const bool bipolar = range.getStart() < 0.0 && range.getEnd() > 0.0;
    const auto origin = bipolar ? startAngle + static_cast<float> (slider.valueToProportionOfLength (0.0)) * (endAngle - startAngle) : startAngle;
    if (std::abs (angle - origin) > 0.001f)
    {
        Path value;
        value.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, jmin (origin, angle), jmax (origin, angle), true);
        g.setColour (accent.withAlpha (0.22f));
        g.strokePath (value, PathStrokeType (thickness * 2.4f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (accent);
        g.strokePath (value, PathStrokeType (thickness, PathStrokeType::curved, PathStrokeType::rounded));
    }

    const auto bodyR = radius - thickness * 2.3f;
    const auto body = Rectangle<float> (bodyR * 2.0f, bodyR * 2.0f).withCentre (centre);
    g.setColour (Colours::black.withAlpha (0.5f));
    g.fillEllipse (body.translated (0.0f, bodyR * 0.08f).expanded (1.0f));
    ColourGradient grad (Colour (0xff3b3d44).brighter (slider.isMouseOverOrDragging() ? 0.12f : 0.0f), centre.x, body.getY(),
                         Colour (0xff1d1e23), centre.x, body.getBottom(), false);
    g.setGradientFill (grad);
    g.fillEllipse (body);
    g.setColour (Colours::white.withAlpha (0.07f));
    g.drawEllipse (body.reduced (0.5f), 1.0f);

    const Point<float> p1 { centre.x + bodyR * 0.3f * std::sin (angle), centre.y - bodyR * 0.3f * std::cos (angle) };
    const Point<float> p2 { centre.x + bodyR * 0.85f * std::sin (angle), centre.y - bodyR * 0.85f * std::cos (angle) };
    g.setColour (Palette::text);
    g.drawLine ({ p1, p2 }, jmax (2.0f, bodyR * 0.1f));
}

juce::Label* ShobeatsLookAndFeel::createSliderTextBox (juce::Slider& slider)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (slider);
    l->setFont (uiFont (12.5f));
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
    auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
    const auto accent = button.findColour (juce::TextButton::buttonOnColourId);
    const bool on = button.getToggleState();

    auto fill = on ? accent : Palette::display;
    if (! on && over)
        fill = fill.brighter (0.1f);
    if (down)
        fill = fill.darker (0.12f);

    const bool left = ! button.isConnectedOnLeft();
    const bool right = ! button.isConnectedOnRight();
    juce::Path p;
    p.addRoundedRectangle (bounds.getX(), bounds.getY(), bounds.getWidth(), bounds.getHeight(), 5.0f, 5.0f, left, right, left, right);
    g.setColour (fill);
    g.fillPath (p);
    g.setColour (on ? accent.brighter (0.25f) : Palette::panelEdge);
    g.strokePath (p, juce::PathStrokeType (1.0f));
}

void ShobeatsLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button, bool, bool)
{
    g.setFont (uiFont (11.5f, true));
    g.setColour (button.findColour (button.getToggleState() ? juce::TextButton::textColourOnId : juce::TextButton::textColourOffId)
                     .withMultipliedAlpha (button.isEnabled() ? 1.0f : 0.4f));
    g.drawText (button.getButtonText().toUpperCase(), button.getLocalBounds().reduced (4, 0), juce::Justification::centred);
}

void ShobeatsLookAndFeel::drawBubble (juce::Graphics& g, juce::BubbleComponent&, const juce::Point<float>&, const juce::Rectangle<float>& body)
{
    g.setColour (Palette::display.withAlpha (0.95f));
    g.fillRoundedRectangle (body, 5.0f);
    g.setColour (Palette::panelEdge);
    g.drawRoundedRectangle (body.reduced (0.5f), 5.0f, 1.0f);
}

juce::Font ShobeatsLookAndFeel::getSliderPopupFont (juce::Slider&)
{
    return uiFont (13.0f, true);
}

int ShobeatsLookAndFeel::getSliderPopupPlacement (juce::Slider&)
{
    return juce::BubbleComponent::above;
}
} // namespace sfx::ui
