#include "PluginEditor.h"

#include "ui/Hardware.h"

namespace sfx
{
using namespace ui;

namespace
{
constexpr int topBarHeight = 96;
const juce::Rectangle<int> logoArea { 30, 20, 300, 60 };
const juce::Rectangle<int> presetArea { 372, 30, 520, 42 };
const juce::Rectangle<int> switchArea { 1022, 26, 186, 50 };
} // namespace

MainView::MainView (ShobeatsAudioProcessor& p)
    : processor (p),
      presetBar (p),
      mainPanel (p),
      pedalboard (p.getState(), p.getChainOrder()),
      detail (p.getState()),
      browser (p.getPresetManager()),
      savePanel (p.getPresetManager())
{
    addAndMakeVisible (presetBar);
    addAndMakeVisible (viewSwitch);
    addAndMakeVisible (mainPanel);
    addChildComponent (pedalboard);
    addChildComponent (detail);
    addChildComponent (browser);
    addChildComponent (savePanel);

    viewSwitch.setTooltip ("Switch between the main view and the full pedalboard");
    viewSwitch.onClick = [this] { setAdvanced (! advanced); };

    presetBar.onBrowse = [this] { showBrowser (true); };
    presetBar.onSave = [this] { showSavePanel (true); };
    browser.onClose = [this] { showBrowser (false); };
    savePanel.onClose = [this] { showSavePanel (false); };

    pedalboard.onSelectionChanged = [this] (EffectId id) { detail.showEffect (id); };
    detail.onMove = [this] (int direction) { pedalboard.nudgeSelected (direction); };
    detail.showEffect (pedalboard.getSelected());
    detail.setBpm (p.getCurrentBpm());

    mainPanel.onOpenEffect = [this] (EffectId id) {
        pedalboard.select (id);
        setAdvanced (true);
    };

    setSize (designWidth, designHeight);
}

void MainView::setAdvanced (bool shouldShowAdvanced)
{
    advanced = shouldShowAdvanced;
    mainPanel.setVisible (! advanced);
    pedalboard.setVisible (advanced);
    detail.setVisible (advanced);
    viewSwitch.setButtonText (advanced ? "Main" : "Advanced");
    viewSwitch.repaint();
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

void MainView::paint (juce::Graphics& g)
{
    // Chassis: brushed gunmetal with a darker inner deck.
    g.fillAll (juce::Colour (0xff0b0b0d));
    const auto chassis = getLocalBounds().toFloat().reduced (6.0f);
    hw::drawBrushedPlate (g, chassis, hw::chassisTop, hw::chassisBottom, 16.0f, 0.55f);
    for (auto c : { chassis.getTopLeft().translated (16.0f, 16.0f), chassis.getTopRight().translated (-16.0f, 16.0f),
                    chassis.getBottomLeft().translated (16.0f, -16.0f), chassis.getBottomRight().translated (-16.0f, -16.0f) })
        hw::drawScrew (g, c, 5.0f, c.x * 0.01f);

    // Top bar: engraved logo plate.
    const auto logo = logoArea.toFloat();
    hw::drawRecess (g, logo, 10.0f, juce::Colour (0xff141518));
    auto word = logo.reduced (16.0f, 8.0f);
    hw::drawEngraved (g, "SHOBEATS", word.removeFromLeft (196.0f), uiFont (31.0f, true).withExtraKerningFactor (0.12f),
                      juce::Justification::centredLeft, juce::Colour (0xffeee8dc));
    auto badge = word.removeFromLeft (52.0f).withSizeKeepingCentre (46.0f, 28.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffef7e44), 0.0f, badge.getY(), juce::Colour (0xffb24c22), 0.0f, badge.getBottom(), false));
    g.fillRoundedRectangle (badge, 5.0f);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawRoundedRectangle (badge, 5.0f, 1.0f);
    g.setColour (juce::Colour (0xfffff3e8));
    g.setFont (uiFont (17.0f, true));
    g.drawText ("FX", badge, juce::Justification::centred);

    // Faint model line under the top bar.
    hw::drawEngraved (g, juce::String (advanced ? "ADVANCED  /  PEDALBOARD" : "MAIN  /  SOUND DESIGN") + "     v" JucePlugin_VersionString,
                      juce::Rectangle<float> (presetArea.getX() + 4.0f, presetArea.getBottom() + 4.0f, 400.0f, 16.0f),
                      uiFont (10.5f, true).withExtraKerningFactor (0.15f), juce::Justification::centredLeft, hw::engraving.withAlpha (0.5f));
}

void MainView::resized()
{
    presetBar.setBounds (presetArea);
    viewSwitch.setBounds (switchArea);

    const auto content = getLocalBounds().withTrimmedTop (topBarHeight);
    mainPanel.setBounds (content);

    auto adv = content.reduced (22, 0).withTrimmedBottom (22);
    pedalboard.setBounds (adv.removeFromTop (352));
    adv.removeFromTop (14);
    detail.setBounds (adv);

    browser.setBounds (getLocalBounds());
    savePanel.setBounds (getLocalBounds());
}

void MainView::refresh()
{
    auto& m = processor.getMeters();
    const auto stereo = processor.getTotalNumOutputChannels() > 1;
    const auto inL = LevelMeters::take (m.input[0]);
    const auto inR = stereo ? LevelMeters::take (m.input[1]) : inL;
    const auto outL = LevelMeters::take (m.output[0]);
    const auto outR = stereo ? LevelMeters::take (m.output[1]) : outL;
    mainPanel.setMeterLevels (inL, inR, outL, outR);
    detail.setBpm (processor.getCurrentBpm());
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
    // Default to 85 % so the full UI fits on a 1080p screen inside a DAW.
    setSize (MainView::designWidth * 85 / 100, MainView::designHeight * 85 / 100);

    startTimerHz (30);
}

ShobeatsAudioProcessorEditor::~ShobeatsAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void ShobeatsAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0b0b0d));
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
