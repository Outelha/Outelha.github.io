#pragma once

#include "PluginProcessor.h"
#include "ui/Controls.h"
#include "ui/DetailPanel.h"
#include "ui/Pedalboard.h"
#include "ui/PresetBrowser.h"
#include "ui/ShobeatsLookAndFeel.h"

namespace sfx
{
// The whole UI is laid out at a fixed design size and scaled as one unit, so
// it stays pixel-consistent at every window size.
class MainView final : public juce::Component
{
public:
    static constexpr int designWidth = 1240;
    static constexpr int designHeight = 800;

    explicit MainView (ShobeatsAudioProcessor&);

    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();

    ui::Pedalboard& getPedalboard() noexcept { return pedalboard; }
    ui::DetailPanel& getDetailPanel() noexcept { return detail; }
    void showBrowser (bool show);
    void showSavePanel (bool show);

private:
    ShobeatsAudioProcessor& processor;
    ui::Knob driftKnob, unstableKnob, inputKnob, mixKnob, outputKnob;
    ui::LevelMeter inputMeter { "IN" }, outputMeter { "OUT" };
    ui::Pedalboard pedalboard;
    ui::DetailPanel detail;
    ui::PresetBar presetBar;
    ui::GeneratorStrip generator;
    ui::PresetBrowser browser;
    ui::SavePresetPanel savePanel;
    double shownBpm = 0.0;
};

class ShobeatsAudioProcessorEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit ShobeatsAudioProcessorEditor (ShobeatsAudioProcessor&);
    ~ShobeatsAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    MainView& getView() noexcept { return view; }

private:
    void timerCallback() override;

    ui::ShobeatsLookAndFeel lookAndFeel;
    juce::TooltipWindow tooltips { this, 700 };
    MainView view;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ShobeatsAudioProcessorEditor)
};
} // namespace sfx
