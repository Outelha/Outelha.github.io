#include "PluginEditor.h"

namespace trio
{
using namespace ui;

namespace
{
constexpr int headerHeight = 104;

// Lays out knobs in rows of equal-width cells, centred horizontally.
void layoutKnobRows (juce::Rectangle<int> area, std::initializer_list<std::initializer_list<juce::Component*>> rows)
{
    const auto rowHeight = area.getHeight() / static_cast<int> (rows.size());
    constexpr int cellWidth = 86;
    for (const auto& row : rows)
    {
        auto r = area.removeFromTop (rowHeight);
        const auto total = cellWidth * static_cast<int> (row.size());
        r = r.withSizeKeepingCentre (juce::jmin (r.getWidth(), total), r.getHeight());
        const auto w = r.getWidth() / static_cast<int> (row.size());
        for (auto* c : row)
            c->setBounds (r.removeFromLeft (w).reduced (2, 4));
    }
}
} // namespace

//==============================================================================
class MainView::SaturationPanel final : public ModulePanel
{
public:
    explicit SaturationPanel (APVTS& s)
        : ModulePanel (s, "Saturation", "Oversampled waveshaper", ParamID::satEnabled, Palette::saturation),
          curve (s), character (s, ParamID::satMode, Palette::saturation),
          drive (s, ParamID::satDrive, "Drive", Palette::saturation),
          tone (s, ParamID::satTone, "Tone", Palette::saturation),
          mix (s, ParamID::satMix, "Mix", Palette::saturation)
    {
        for (juce::Component* c : { (juce::Component*) &curve, (juce::Component*) &character, (juce::Component*) &drive,
                                    (juce::Component*) &tone, (juce::Component*) &mix })
            addContent (*c);
    }

private:
    void layoutContent (juce::Rectangle<int> area) override
    {
        // The single knob row lines up with the bottom knob row of the other cards.
        auto knobs = area.removeFromBottom (116);
        area.removeFromBottom (14);
        character.setBounds (area.removeFromBottom (30));
        area.removeFromBottom (14);
        curve.setBounds (area);
        layoutKnobRows (knobs, { { &drive, &tone, &mix } });
    }

    SaturationCurve curve;
    SegmentedChoice character;
    Knob drive, tone, mix;
};

class MainView::ChorusPanel final : public ModulePanel
{
public:
    explicit ChorusPanel (APVTS& s)
        : ModulePanel (s, "Chorus", "Stereo modulated delay", ParamID::chorusEnabled, Palette::chorus),
          display (s),
          rate (s, ParamID::chorusRate, "Rate", Palette::chorus),
          depth (s, ParamID::chorusDepth, "Depth", Palette::chorus),
          delay (s, ParamID::chorusDelay, "Delay", Palette::chorus),
          mix (s, ParamID::chorusMix, "Mix", Palette::chorus)
    {
        for (juce::Component* c : { (juce::Component*) &display, (juce::Component*) &rate, (juce::Component*) &depth,
                                    (juce::Component*) &delay, (juce::Component*) &mix })
            addContent (*c);
    }

private:
    void layoutContent (juce::Rectangle<int> area) override
    {
        auto knobs = area.removeFromBottom (232);
        area.removeFromBottom (12);
        display.setBounds (area);
        layoutKnobRows (knobs, { { &rate, &depth }, { &delay, &mix } });
    }

    ChorusDisplay display;
    Knob rate, depth, delay, mix;
};

class MainView::ReverbPanel final : public ModulePanel
{
public:
    explicit ReverbPanel (APVTS& s)
        : ModulePanel (s, "Reverb", "8-line feedback delay network", ParamID::reverbEnabled, Palette::reverb),
          display (s),
          size (s, ParamID::reverbSize, "Size", Palette::reverb),
          decay (s, ParamID::reverbDecay, "Decay", Palette::reverb),
          damping (s, ParamID::reverbDamping, "Damping", Palette::reverb),
          predelay (s, ParamID::reverbPredelay, "Pre-Delay", Palette::reverb),
          width (s, ParamID::reverbWidth, "Width", Palette::reverb),
          mix (s, ParamID::reverbMix, "Mix", Palette::reverb)
    {
        for (juce::Component* c : { (juce::Component*) &display, (juce::Component*) &size, (juce::Component*) &decay,
                                    (juce::Component*) &damping, (juce::Component*) &predelay, (juce::Component*) &width,
                                    (juce::Component*) &mix })
            addContent (*c);
    }

private:
    void layoutContent (juce::Rectangle<int> area) override
    {
        auto knobs = area.removeFromBottom (232);
        area.removeFromBottom (12);
        display.setBounds (area);
        layoutKnobRows (knobs, { { &size, &decay, &damping }, { &predelay, &width, &mix } });
    }

    ReverbDisplay display;
    Knob size, decay, damping, predelay, width, mix;
};

//==============================================================================
MainView::MainView (TrioAudioProcessor& p)
    : processor (p),
      inputKnob (p.getState(), ParamID::inputGain, "Input", Palette::neutral),
      outputKnob (p.getState(), ParamID::outputGain, "Output", Palette::neutral)
{
    auto& s = p.getState();
    saturationPanel = std::make_unique<SaturationPanel> (s);
    chorusPanel = std::make_unique<ChorusPanel> (s);
    reverbPanel = std::make_unique<ReverbPanel> (s);

    for (auto* c : { saturationPanel.get(), chorusPanel.get(), reverbPanel.get() })
        addAndMakeVisible (*c);

    for (juce::Component* c : { (juce::Component*) &inputKnob, (juce::Component*) &outputKnob,
                                (juce::Component*) &inputMeter, (juce::Component*) &outputMeter })
        addAndMakeVisible (*c);

    setSize (designWidth, designHeight);
}

MainView::~MainView() = default;

void MainView::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    g.setGradientFill (juce::ColourGradient (Palette::backgroundTop, 0.0f, 0.0f, Palette::background, 0.0f, b.getHeight(), false));
    g.fillAll();

    // Brand
    auto header = getLocalBounds().reduced (28, 0).removeFromTop (headerHeight);
    auto brand = header.removeFromLeft (420).withSizeKeepingCentre (420, 48);

    // Logo mark: three overlapping dots in the module colours.
    auto mark = brand.removeFromLeft (52).toFloat().withSizeKeepingCentre (46, 30);
    const std::array<juce::Colour, 3> marks { Palette::saturation, Palette::chorus, Palette::reverb };
    for (size_t i = 0; i < marks.size(); ++i)
    {
        g.setColour (marks[i].withAlpha (0.9f));
        g.fillEllipse (mark.getX() + static_cast<float> (i) * 12.0f, mark.getCentreY() - 11.0f, 22.0f, 22.0f);
    }

    brand.removeFromLeft (8);
    g.setColour (Palette::text);
    g.setFont (uiFont (26.0f, true));
    g.drawText ("TRIO FX", brand.removeFromTop (28), juce::Justification::bottomLeft);
    g.setColour (Palette::textFaint);
    g.setFont (uiFont (12.0f));
    g.drawText ("SATURATION  /  CHORUS  /  REVERB      v" JucePlugin_VersionString, brand, juce::Justification::centredLeft);

    // Signal flow arrows between cards.
    const auto drawArrow = [&g] (juce::Rectangle<int> from, juce::Rectangle<int> to) {
        const auto y = static_cast<float> (from.getY() + 32);
        const auto x1 = static_cast<float> (from.getRight()) + 2.0f;
        const auto x2 = static_cast<float> (to.getX()) - 2.0f;
        juce::Path arrow;
        arrow.addTriangle (x1 + 2.0f, y - 4.0f, x1 + 2.0f, y + 4.0f, x2 - 1.0f, y);
        g.setColour (Palette::textFaint);
        g.fillPath (arrow);
    };
    drawArrow (saturationPanel->getBounds(), chorusPanel->getBounds());
    drawArrow (chorusPanel->getBounds(), reverbPanel->getBounds());
}

void MainView::resized()
{
    auto area = getLocalBounds().reduced (28, 0);
    auto header = area.removeFromTop (headerHeight);

    // Header right side: IN meter | Input | Output | OUT meter
    auto io = header.removeFromRight (240).reduced (0, 10);
    inputMeter.setBounds (io.removeFromLeft (22));
    io.removeFromLeft (10);
    inputKnob.setBounds (io.removeFromLeft (84));
    io.removeFromLeft (8);
    outputKnob.setBounds (io.removeFromLeft (84));
    io.removeFromLeft (10);
    outputMeter.setBounds (io.removeFromLeft (22));

    area.removeFromBottom (24);
    constexpr int gap = 16;
    const auto cardWidth = (area.getWidth() - 2 * gap) / 3;
    saturationPanel->setBounds (area.removeFromLeft (cardWidth));
    area.removeFromLeft (gap);
    chorusPanel->setBounds (area.removeFromLeft (cardWidth));
    area.removeFromLeft (gap);
    reverbPanel->setBounds (area);
}

void MainView::updateMeters()
{
    auto& m = processor.getMeters();
    const auto stereo = processor.getTotalNumOutputChannels() > 1;
    const auto inL = LevelMeters::take (m.input[0]);
    const auto inR = stereo ? LevelMeters::take (m.input[1]) : inL;
    const auto outL = LevelMeters::take (m.output[0]);
    const auto outR = stereo ? LevelMeters::take (m.output[1]) : outL;
    inputMeter.setLevels (inL, inR);
    outputMeter.setLevels (outL, outR);
}

//==============================================================================
TrioAudioProcessorEditor::TrioAudioProcessorEditor (TrioAudioProcessor& p)
    : AudioProcessorEditor (p), view (p)
{
    setLookAndFeel (&lookAndFeel);
    addAndMakeVisible (view);
    // The view was built before the look-and-feel was attached; let sliders rebuild their text boxes.
    view.sendLookAndFeelChange();

    constexpr double aspect = static_cast<double> (MainView::designWidth) / MainView::designHeight;
    setResizable (true, true);
    setResizeLimits (MainView::designWidth * 3 / 4, MainView::designHeight * 3 / 4,
                     MainView::designWidth * 2, MainView::designHeight * 2);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio (aspect);
    setSize (MainView::designWidth, MainView::designHeight);

    startTimerHz (30);
}

TrioAudioProcessorEditor::~TrioAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void TrioAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (Palette::background);
}

void TrioAudioProcessorEditor::resized()
{
    const auto scale = static_cast<float> (getWidth()) / static_cast<float> (MainView::designWidth);
    view.setTransform (juce::AffineTransform::scale (scale));
    view.setBounds (0, 0, MainView::designWidth, MainView::designHeight);
}

void TrioAudioProcessorEditor::timerCallback()
{
    view.updateMeters();
}
} // namespace trio
