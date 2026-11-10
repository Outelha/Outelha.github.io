#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace trio::ui
{
namespace Palette
{
inline const juce::Colour background { 0xff121419 };
inline const juce::Colour backgroundTop { 0xff1a1d24 };
inline const juce::Colour panel { 0xff1c1f26 };
inline const juce::Colour panelEdge { 0xff2a2e38 };
inline const juce::Colour display { 0xff14161b };
inline const juce::Colour track { 0xff323743 };
inline const juce::Colour text { 0xffe8eaef };
inline const juce::Colour textDim { 0xff8a90a0 };
inline const juce::Colour textFaint { 0xff5a6070 };

inline const juce::Colour saturation { 0xffff8a3d };
inline const juce::Colour chorus { 0xff3dd6c6 };
inline const juce::Colour reverb { 0xff9a86ff };
inline const juce::Colour neutral { 0xffc9ccd6 };
} // namespace Palette

juce::Font uiFont (float height, bool bold = false);

class TrioLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    TrioLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float startAngle, float endAngle, juce::Slider&) override;

    juce::Label* createSliderTextBox (juce::Slider&) override;
    juce::Slider::SliderLayout getSliderLayout (juce::Slider&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool isMouseOverButton, bool isButtonDown) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool isMouseOverButton, bool isButtonDown) override;
};
} // namespace trio::ui
