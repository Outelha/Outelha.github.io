#pragma once

#include "Theme.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace sfx::ui
{
using APVTS = juce::AudioProcessorValueTreeState;

// Rotary control with a caption. Two looks:
//  - Style::panel: arc knob with an editable value read-out below (detail panel, header)
//  - Style::pedal: hardware knob with caption below and a value bubble while dragging
class Knob final : public juce::Component
{
public:
    enum class Style
    {
        panel,
        pedal
    };

    Knob (APVTS& state, const juce::String& paramId, const juce::String& caption, juce::Colour accent, Style style = Style::panel);

    void resized() override;
    juce::Slider& getSlider() noexcept { return slider; }
    void setInkColour (juce::Colour c);

private:
    Style style;
    juce::Slider slider;
    juce::Label caption;
    std::unique_ptr<APVTS::SliderAttachment> attachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Knob)
};

// Shows the "time" knob, or the "division" knob when the sync switch is on.
class SyncSwap final : public juce::Component
{
public:
    SyncSwap (APVTS& state, const juce::String& syncId, const juce::String& timeId, const juce::String& divisionId,
              const juce::String& caption, juce::Colour accent, Knob::Style style);
    void resized() override;
    Knob& getTimeKnob() noexcept { return time; }
    Knob& getDivisionKnob() noexcept { return division; }

private:
    Knob time, division;
    std::unique_ptr<juce::ParameterAttachment> syncAttachment;
};

// Pill-shaped on/off switch bound to a bool parameter.
class ToggleChip final : public juce::TextButton
{
public:
    ToggleChip (APVTS& state, const juce::String& paramId, const juce::String& label, juce::Colour accent);

private:
    std::unique_ptr<APVTS::ButtonAttachment> attachment;
};

// Row of mutually exclusive buttons bound to a choice parameter.
class SegmentedChoice final : public juce::Component
{
public:
    SegmentedChoice (APVTS& state, const juce::String& paramId, juce::Colour accent);
    void resized() override;

private:
    void select (int index);
    juce::OwnedArray<juce::TextButton> buttons;
    std::unique_ptr<juce::ParameterAttachment> attachment;
};

// Stereo peak meter (dBFS).
class LevelMeter final : public juce::Component
{
public:
    explicit LevelMeter (const juce::String& caption);
    void setLevels (float left, float right);
    void paint (juce::Graphics&) override;

private:
    juce::String caption;
    std::array<float, 2> levels {}, peaks {};
    std::array<int, 2> peakHold {};
};
} // namespace sfx::ui
