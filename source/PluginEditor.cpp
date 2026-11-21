#include "PluginEditor.h"

#include "ui/Hardware.h"

namespace sfx
{
using namespace ui;

namespace
{
constexpr int topBarHeight = ui::MainPanel::topOffset;
const juce::Rectangle<int> headerPlate { 10, 6, 1240, 84 };
const juce::Rectangle<int> logoArea { 40, 16, 330, 58 };
const juce::Rectangle<int> presetArea { 400, 18, 560, 48 };
const juce::Rectangle<int> switchArea { 984, 12, 242, 58 };

// Chrome-edged pill behind the SHOBEATS FX logotype.
void drawLogo (juce::Graphics& g, juce::Rectangle<float> r)
{
    using namespace juce;
    const auto corner = r.getHeight() * 0.5f;
    g.setColour (Colours::black.withAlpha (0.6f));
    g.fillRoundedRectangle (r.translated (0.0f, 2.5f), corner);
    g.setGradientFill (ColourGradient (Colour (0xffb8babf), 0.0f, r.getY(), Colour (0xff1d1e21), 0.0f, r.getBottom(), false));
    g.fillRoundedRectangle (r, corner);
    const auto inner = r.reduced (3.0f);
    g.setGradientFill (ColourGradient (Colour (0xff222327), 0.0f, inner.getY(), Colour (0xff09090a), 0.0f, inner.getBottom(), false));
    g.fillRoundedRectangle (inner, corner - 3.0f);
    g.setColour (Colours::white.withAlpha (0.1f));
    g.drawRoundedRectangle (inner.reduced (0.5f), corner - 3.0f, 0.8f);

    // SHOBEATS: brushed-silver italic letters with a dark engraving shadow.
    auto text = inner.reduced (22.0f, 0.0f);
    const auto wordFont = ui::font (ui::Type::title, r.getHeight() * 0.98f, 0.01f);
    const auto word = text.removeFromLeft (GlyphArrangement::getStringWidth (wordFont, "SHOBEATS") + 4.0f);
    GlyphArrangement glyphs;
    glyphs.addFittedText (wordFont, "SHOBEATS", word.getX(), word.getY(), word.getWidth(), word.getHeight(), Justification::centredLeft, 1);
    Path letters;
    glyphs.createPath (letters);
    g.setColour (Colours::black.withAlpha (0.8f));
    g.fillPath (letters, AffineTransform::translation (0.0f, 2.0f));
    ColourGradient silver (Colour (0xfffdfdfd), 0.0f, word.getY() + word.getHeight() * 0.25f, Colour (0xff7b7d83), 0.0f, word.getBottom() - word.getHeight() * 0.2f, false);
    silver.addColour (0.48, Colour (0xffd8d9dd));
    silver.addColour (0.52, Colour (0xff9a9ca2));
    g.setGradientFill (silver);
    g.fillPath (letters);

    // FX badge: italic orange outline with light.
    text.removeFromLeft (14.0f);
    const auto badge = text.removeFromLeft (64.0f).withSizeKeepingCentre (64.0f, r.getHeight() * 0.62f);
    Path shape;
    const auto skew = badge.getHeight() * 0.22f;
    shape.startNewSubPath (badge.getX() + skew, badge.getY());
    shape.lineTo (badge.getRight(), badge.getY());
    shape.lineTo (badge.getRight() - skew, badge.getBottom());
    shape.lineTo (badge.getX(), badge.getBottom());
    shape.closeSubPath();
    ui::hw::drawLightSpill (g, badge.expanded (14.0f, 6.0f), ui::brandOrange, 0.6f);
    g.setColour (Colour (0xff1a0d05));
    g.fillPath (shape);
    for (auto [w, a] : { std::pair { 6.0f, 0.12f }, { 3.5f, 0.25f } })
    {
        g.setColour (ui::brandOrange.withAlpha (a));
        g.strokePath (shape, PathStrokeType (w, PathStrokeType::mitered));
    }
    g.setColour (ui::brandOrange.brighter (0.2f));
    g.strokePath (shape, PathStrokeType (2.0f, PathStrokeType::mitered));
    g.setFont (ui::font (ui::Type::title, badge.getHeight() * 0.86f, 0.04f));
    g.setColour (ui::brandOrange.brighter (0.4f));
    g.drawText ("FX", badge.translated (1.0f, 0.0f), Justification::centred, false);
}
} // namespace

MainView::MainView (ShobeatsAudioProcessor& p)
    : processor (p),
      presetBar (p),
      mainPanel (p, frameClock),
      pedalboard (p.getState(), p.getChainOrder(), frameClock, p.getVisualFeed()),
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

    viewSwitch.setLabelFont (font (Type::labelBold, 31.0f, 0.22f));
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
    repaint();
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
    // Chassis: near-black with a faint brushed grain.
    g.fillAll (juce::Colour (0xff0b0b0c));
    g.setTiledImageFill (hw::brushedTexture(), 0, 0, 0.18f);
    g.fillRect (getLocalBounds());

    // Header plate, logo and model line.
    hw::drawBezelPanel (g, headerPlate.toFloat(), 20.0f, 6.0f, true);
    drawLogo (g, logoArea.toFloat());
    hw::drawLightSeam (g, { presetArea.toFloat().getCentreX() - 20.0f, static_cast<float> (presetArea.getBottom()) - 4.0f }, 300.0f, brandOrange, 0.7f);
    hw::drawEngraved (g, juce::String (advanced ? "ADVANCED   /   SOUND DESIGN" : "MAIN   /   SOUND DESIGN") + "        v" JucePlugin_VersionString,
                      juce::Rectangle<float> (presetArea.getX() + 52.0f, presetArea.getBottom() + 2.0f, 460.0f, 16.0f),
                      font (Type::label, 13.5f, 0.2f), juce::Justification::centredLeft, juce::Colour (0xffc8c0b2));
}

void MainView::resized()
{
    presetBar.setBounds (presetArea);
    viewSwitch.setBounds (switchArea);

    const auto content = getLocalBounds().withTrimmedTop (topBarHeight);
    mainPanel.setBounds (content);

    pedalboard.setBounds (14, 96, 1232, 318);
    detail.setBounds (10, 422, 1240, 408);

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
