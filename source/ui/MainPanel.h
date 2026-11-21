#pragma once

#include "ChainOrder.h"
#include "Controls.h"
#include "FrameClock.h"
#include "PedalLive.h"
#include "presets/PresetManager.h"

namespace sfx
{
class ShobeatsAudioProcessor;
}

namespace sfx::ui
{
// Hardware key with an optional icon and illumination.
//  - Style::key:   dark key; when lit, an orange rim and warm inner light (vibe keys).
//  - Style::large: solid illuminated orange key (GENERATE, view switch).
class KeyCap : public juce::Button
{
public:
    enum class Icon
    {
        none,
        cassette,
        cloud,
        moon,
        waveform,
        planet,
        flask,
        chevrons
    };

    KeyCap (const juce::String& label, juce::Colour ledColour, bool large = false);
    void paintButton (juce::Graphics&, bool over, bool down) override;
    void setLit (bool shouldBeLit)
    {
        if (lit != shouldBeLit)
        {
            lit = shouldBeLit;
            repaint();
        }
    }
    bool isLit() const noexcept { return lit; }
    void setIcon (Icon i) { icon = i; repaint(); }
    void setLabelFont (juce::Font f) { labelFont = f; hasLabelFont = true; repaint(); }

    static juce::Path iconPath (Icon, juce::Rectangle<float>);

private:
    juce::Colour accent;
    bool large, lit = false, hasLabelFont = false;
    Icon icon = Icon::none;
    juce::Font labelFont { juce::FontOptions {} };
};

// One effect in the main-view chain: a miniature of the real pedal. Click toggles it,
// double-click opens it in the Advanced view. Everything drawn on it (knob positions,
// window, LED) follows the real parameters.
class EffectTile final : public juce::Button, private FrameClock::Client
{
public:
    EffectTile (juce::AudioProcessorValueTreeState& state, EffectId id, FrameClock& clock, const ShobeatsAudioProcessor& processor);
    ~EffectTile() override;

    void paintButton (juce::Graphics&, bool over, bool down) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    EffectId getEffect() const noexcept { return effect; }
    void setSlot (int slotNumber)
    {
        slot = slotNumber;
        repaint();
    }

    std::function<void (EffectId)> onOpen;

private:
    bool frame (double now, double dt) override;

    EffectId effect;
    FrameClock& clock;
    PedalLive live;
    int slot = 0;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
};

// The central display: preset, vibe, tempo, chain status, global movement and a live
// waveform of the plugin's output (drawn from the processor's lock-free VisualFeed).
class CentralDisplay final : public juce::Component, private juce::Timer, private FrameClock::Client
{
public:
    CentralDisplay (ShobeatsAudioProcessor&, FrameClock&);
    ~CentralDisplay() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void update() { timerCallback(); }

private:
    void timerCallback() override;
    bool frame (double now, double dt) override;
    juce::Rectangle<float> screenArea() const;
    juce::Rectangle<float> waveArea() const;

    ShobeatsAudioProcessor& processor;
    FrameClock& clock;
    juce::String name, category, description, vibe;
    ChainOrder::Order order {};
    std::array<bool, numEffects> active {};
    float drift = 0.0f, unstable = 0.0f;
    double bpm = 0.0;
    bool hostTempo = false;

    static constexpr int waveSegments = 384;
    std::array<float, waveSegments> wave {};
    std::uint32_t lastWritten = 0;
};

// Static hardware of the main screen (plates, module housings, trays, grilles, labels),
// rendered once into a cached image and re-used while the controls on top animate.
class MainBackdrop final : public juce::Component
{
public:
    MainBackdrop();
    void paint (juce::Graphics&) override;
};

// Glass indicator tube next to Drift / Unstable; its glow tracks the parameter.
class IndicatorTube final : public juce::Component
{
public:
    IndicatorTube (juce::AudioProcessorValueTreeState&, const juce::String& paramId);
    void paint (juce::Graphics&) override;

private:
    float level = 0.0f;
    std::unique_ptr<juce::ParameterAttachment> attachment;
};

// Main view: big Drift/Unstable, display, vibe keys + GENERATE, effect chain, master section.
class MainPanel final : public juce::Component, private juce::Timer
{
public:
    MainPanel (ShobeatsAudioProcessor&, FrameClock&);

    std::function<void (EffectId)> onOpenEffect;

    void paint (juce::Graphics&) override;
    void resized() override;
    void setMeterLevels (float inL, float inR, float outL, float outR);
    void refreshDisplay()
    {
        display.update();
        timerCallback();
    }

    // Layout in view coordinates (the panel starts below the top bar).
    static constexpr int topOffset = 92;

private:
    void timerCallback() override;
    void layoutTiles();

    ShobeatsAudioProcessor& processor;
    MainBackdrop backdrop;
    Knob drift, unstable, input, mix, output;
    IndicatorTube driftTube, unstableTube;
    CentralDisplay display;
    juce::OwnedArray<KeyCap> vibeKeys;
    KeyCap generate { "Generate", juce::Colour (0xffff7a1f), true };
    std::array<std::unique_ptr<EffectTile>, numEffects> tiles;
    LevelMeter inputMeter { "IN" }, outputMeter { "OUT" };
    ChainOrder::Order shownOrder {};
};
} // namespace sfx::ui
