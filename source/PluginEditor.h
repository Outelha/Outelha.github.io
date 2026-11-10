#pragma once

#include "PluginProcessor.h"
#include "ui/Controls.h"
#include "ui/DetailPanel.h"
#include "ui/MainPanel.h"
#include "ui/Pedalboard.h"
#include "ui/PresetBrowser.h"
#include "ui/ShobeatsLookAndFeel.h"

namespace sfx
{
// The whole UI is laid out at a fixed design size and scaled as one unit, so it
// stays consistent at every window size and Windows display scaling.
//
// Two views share the same window: MAIN (sound design at a glance) and ADVANCED
// (the full pedalboard). The top bar with the logo, preset navigation and the
// view switch is common to both.
class MainView final : public juce::Component
{
public:
    static constexpr int designWidth = 1240;
    static constexpr int designHeight = 800;

    explicit MainView (ShobeatsAudioProcessor&);

    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();

    void setAdvanced (bool shouldShowAdvanced);
    bool isAdvanced() const noexcept { return advanced; }

    ui::Pedalboard& getPedalboard() noexcept { return pedalboard; }
    ui::DetailPanel& getDetailPanel() noexcept { return detail; }
    ui::MainPanel& getMainPanel() noexcept { return mainPanel; }
    void showBrowser (bool show);
    void showSavePanel (bool show);

private:
    ShobeatsAudioProcessor& processor;
    ui::PresetBar presetBar;
    ui::KeyCap viewSwitch { "Advanced", juce::Colour (0xffffb347), true };
    ui::MainPanel mainPanel;
    ui::Pedalboard pedalboard;
    ui::DetailPanel detail;
    ui::PresetBrowser browser;
    ui::SavePresetPanel savePanel;
    bool advanced = false;
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
