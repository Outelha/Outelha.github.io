#include "PluginEditor.h"

namespace sfx
{
using namespace ui;

namespace
{
constexpr int headerHeight = 92;
constexpr int margin = 22;
} // namespace

MainView::MainView (ShobeatsAudioProcessor& p)
    : processor (p),
      driftKnob (p.getState(), ParamID::drift, "Drift", Palette::brand),
      unstableKnob (p.getState(), ParamID::unstable, "Unstable", Palette::brand),
      inputKnob (p.getState(), ParamID::inputGain, "Input", Palette::text),
      mixKnob (p.getState(), ParamID::globalMix, "Mix", Palette::brand),
      outputKnob (p.getState(), ParamID::outputGain, "Output", Palette::text),
      pedalboard (p.getState(), p.getChainOrder()),
      detail (p.getState()),
      presetBar (p),
      browser (p.getPresetManager()),
      savePanel (p.getPresetManager())
{
    for (juce::Component* c : { (juce::Component*) &driftKnob, (juce::Component*) &unstableKnob, (juce::Component*) &inputKnob, (juce::Component*) &mixKnob, (juce::Component*) &outputKnob,
                                (juce::Component*) &inputMeter, (juce::Component*) &outputMeter,
                                (juce::Component*) &pedalboard, (juce::Component*) &detail, (juce::Component*) &presetBar })
        addAndMakeVisible (*c);
    addChildComponent (browser);
    addChildComponent (savePanel);
    presetBar.onBrowse = [this] { showBrowser (true); };
    presetBar.onSave = [this] { showSavePanel (true); };
    browser.onClose = [this] { showBrowser (false); };
    savePanel.onClose = [this] { showSavePanel (false); };

    pedalboard.onSelectionChanged = [this] (EffectId id) { detail.showEffect (id); };
    detail.onMove = [this] (int direction) { pedalboard.nudgeSelected (direction); };
    detail.showEffect (pedalboard.getSelected());
    shownBpm = p.getCurrentBpm();
    detail.setBpm (shownBpm);

    setSize (designWidth, designHeight);
}

void MainView::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    g.setGradientFill (juce::ColourGradient (Palette::background.brighter (0.06f), 0.0f, 0.0f, Palette::background, 0.0f, b.getHeight(), false));
    g.fillAll();

    // Wordmark: SHOBEATS + FX badge.
    auto header = getLocalBounds().reduced (margin + 4, 0).removeFromTop (headerHeight);
    auto brand = header.removeFromLeft (320).withSizeKeepingCentre (320, 50);
    g.setColour (Palette::text);
    g.setFont (uiFont (34.0f, true).withExtraKerningFactor (0.06f));
    const juce::String word ("SHOBEATS");
    const auto wordWidth = juce::roundToInt (juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), word)) + 6;
    g.drawText (word, brand.removeFromLeft (wordWidth).withTrimmedBottom (14), juce::Justification::centredLeft);
    auto badge = brand.removeFromLeft (46).withTrimmedBottom (14).withSizeKeepingCentre (42, 26).toFloat();
    g.setColour (Palette::brand);
    g.fillRoundedRectangle (badge, 5.0f);
    g.setColour (Palette::background);
    g.setFont (uiFont (17.0f, true));
    g.drawText ("FX", badge, juce::Justification::centred);

    auto sub = getLocalBounds().reduced (margin + 4, 0).removeFromTop (headerHeight).withSizeKeepingCentre (getWidth() - 2 * (margin + 4), 50);
    g.setColour (Palette::textFaint);
    g.setFont (uiFont (11.5f, true));
    g.drawText ("8-PEDAL FX RACK  /  v" JucePlugin_VersionString, sub.withTrimmedTop (34), juce::Justification::centredLeft);

    // Host tempo readout (used by every synced effect).
    auto tempo = juce::Rectangle<int> (346, 64, 90, 20).toFloat();
    g.setColour (Palette::textDim);
    g.setFont (uiFont (11.0f, true));
    g.drawText ("HOST " + juce::String (shownBpm, 1) + " BPM", tempo, juce::Justification::centredLeft);
}

void MainView::showBrowser (bool show)
{
    if (show)
    {
        browser.refresh();
        browser.toFront (true);
    }
    browser.setVisible (show);
}

void MainView::showSavePanel (bool show)
{
    savePanel.setVisible (show);
    if (show)
    {
        savePanel.toFront (true);
        savePanel.open (processor.getCurrentPresetLabel().first);
    }
}

void MainView::resized()
{
    auto area = getLocalBounds().reduced (margin, 0);
    auto header = area.removeFromTop (headerHeight);

    auto io = header.removeFromRight (330).reduced (0, 8);
    auto movement = header.removeFromRight (190).reduced (0, 8);
    driftKnob.setBounds (movement.removeFromLeft (90));
    unstableKnob.setBounds (movement.removeFromLeft (90));
    inputMeter.setBounds (io.removeFromLeft (20));
    io.removeFromLeft (8);
    inputKnob.setBounds (io.removeFromLeft (84));
    io.removeFromLeft (4);
    mixKnob.setBounds (io.removeFromLeft (84));
    io.removeFromLeft (4);
    outputKnob.setBounds (io.removeFromLeft (84));
    io.removeFromLeft (8);
    outputMeter.setBounds (io.removeFromLeft (20));

    presetBar.setBounds (juce::Rectangle<int> (346, 26, 330, 34));
    browser.setBounds (getLocalBounds());
    savePanel.setBounds (getLocalBounds());

    area.removeFromBottom (margin);
    pedalboard.setBounds (area.removeFromTop (352));
    area.removeFromTop (16);
    detail.setBounds (area);
}

void MainView::refresh()
{
    auto& m = processor.getMeters();
    const auto stereo = processor.getTotalNumOutputChannels() > 1;
    const auto inL = LevelMeters::take (m.input[0]);
    const auto inR = stereo ? LevelMeters::take (m.input[1]) : inL;
    const auto outL = LevelMeters::take (m.output[0]);
    const auto outR = stereo ? LevelMeters::take (m.output[1]) : outL;
    inputMeter.setLevels (inL, inR);
    outputMeter.setLevels (outL, outR);

    const auto bpm = processor.getCurrentBpm();
    if (std::abs (bpm - shownBpm) > 0.01)
    {
        shownBpm = bpm;
        detail.setBpm (bpm);
        repaint (346, 64, 90, 20);
    }
}

//==============================================================================
ShobeatsAudioProcessorEditor::ShobeatsAudioProcessorEditor (ShobeatsAudioProcessor& p)
    : AudioProcessorEditor (p), view (p)
{
    setLookAndFeel (&lookAndFeel);
    addAndMakeVisible (view);
    // The view was built before the look-and-feel was attached; let sliders rebuild their text boxes.
    view.sendLookAndFeelChange();

    constexpr double aspect = static_cast<double> (MainView::designWidth) / MainView::designHeight;
    setResizable (true, true);
    setResizeLimits (MainView::designWidth * 3 / 5, MainView::designHeight * 3 / 5, MainView::designWidth * 2, MainView::designHeight * 2);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio (aspect);
    // Default to 85 % so the full board fits on a 1080p screen inside a DAW.
    setSize (MainView::designWidth * 85 / 100, MainView::designHeight * 85 / 100);

    startTimerHz (30);
}

ShobeatsAudioProcessorEditor::~ShobeatsAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void ShobeatsAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (Palette::background);
}

void ShobeatsAudioProcessorEditor::resized()
{
    const auto scale = static_cast<float> (getWidth()) / static_cast<float> (MainView::designWidth);
    view.setTransform (juce::AffineTransform::scale (scale));
    view.setBounds (0, 0, MainView::designWidth, MainView::designHeight);
}

void ShobeatsAudioProcessorEditor::timerCallback()
{
    view.refresh();
}
} // namespace sfx
