#pragma once

#include "PluginProcessor.h"
#include "ui/Components.h"
#include "ui/TrioLookAndFeel.h"

namespace trio
{
// The whole UI is laid out at a fixed design size and scaled as one unit, so
// it stays pixel-consistent at every window size.
class MainView final : public juce::Component
{
public:
    static constexpr int designWidth = 1000;
    static constexpr int designHeight = 600;

    explicit MainView (TrioAudioProcessor&);
    ~MainView() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void updateMeters();

private:
    class SaturationPanel;
    class ChorusPanel;
    class ReverbPanel;

    TrioAudioProcessor& processor;
    ui::Knob inputKnob, outputKnob;
    ui::LevelMeter inputMeter { "IN" }, outputMeter { "OUT" };
    std::unique_ptr<juce::Component> saturationPanel, chorusPanel, reverbPanel;
};

class TrioAudioProcessorEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit TrioAudioProcessorEditor (TrioAudioProcessor&);
    ~TrioAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    ui::TrioLookAndFeel lookAndFeel;
    juce::TooltipWindow tooltips { this, 700 };
    MainView view;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrioAudioProcessorEditor)
};
} // namespace trio
