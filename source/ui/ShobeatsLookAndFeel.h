#pragma once

#include "Theme.h"

namespace sfx::ui
{
// Two knob styles: "pedal" (black knurled hardware knob on a pedal face) and the
// default arc knob used in the detail panel and header. Pick the pedal style with
// slider.getProperties().set ("style", "pedal").
class ShobeatsLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    ShobeatsLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float startAngle, float endAngle, juce::Slider&) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
    juce::Slider::SliderLayout getSliderLayout (juce::Slider&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;

    void drawBubble (juce::Graphics&, juce::BubbleComponent&, const juce::Point<float>& tip, const juce::Rectangle<float>& body) override;
    juce::Font getSliderPopupFont (juce::Slider&) override;
    int getSliderPopupPlacement (juce::Slider&) override;

private:
    void drawPedalKnob (juce::Graphics&, juce::Rectangle<float>, float angle, float startAngle, float endAngle, juce::Slider&);
    void drawArcKnob (juce::Graphics&, juce::Rectangle<float>, float angle, float startAngle, float endAngle, juce::Slider&);
};
} // namespace sfx::ui
