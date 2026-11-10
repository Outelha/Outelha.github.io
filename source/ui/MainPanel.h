#pragma once

#include "ChainOrder.h"
#include "Controls.h"
#include "presets/PresetManager.h"

namespace sfx
{
class ShobeatsAudioProcessor;
}

namespace sfx::ui
{
// Hardware key: rounded cap with bevel, engraved label and a status LED.
class KeyCap : public juce::Button
{
public:
    KeyCap (const juce::String& label, juce::Colour ledColour, bool large = false);
    void paintButton (juce::Graphics&, bool over, bool down) override;
    void setLit (bool shouldBeLit) { lit = shouldBeLit; repaint(); }

private:
    juce::Colour led;
    bool large, lit = false;
};

// One effect in the main-view rack: click toggles it, double-click opens it in the Advanced view.
class EffectTile final : public juce::Button
{
public:
    EffectTile (juce::AudioProcessorValueTreeState& state, EffectId id);
    void paintButton (juce::Graphics&, bool over, bool down) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    EffectId getEffect() const noexcept { return effect; }
    void setSlot (int slotNumber) { slot = slotNumber; repaint(); }

    std::function<void (EffectId)> onOpen;

private:
    EffectId effect;
    int slot = 0;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
};

// The central amber display: current preset / idea, its chain and the global state.
class CentralDisplay final : public juce::Component, private juce::Timer
{
public:
    explicit CentralDisplay (ShobeatsAudioProcessor&);
    void paint (juce::Graphics&) override;
    void update() { timerCallback(); }

private:
    void timerCallback() override;
    ShobeatsAudioProcessor& processor;
    juce::String name, category, description;
    ChainOrder::Order order {};
    std::array<bool, numEffects> active {};
    float drift = 0.0f, unstable = 0.0f;
    double bpm = 0.0;
};

// Main view: big Drift/Unstable, display, vibe keys + GENERATE, effect rack, global knobs.
class MainPanel final : public juce::Component, private juce::Timer
{
public:
    explicit MainPanel (ShobeatsAudioProcessor&);

    std::function<void (EffectId)> onOpenEffect;

    void paint (juce::Graphics&) override;
    void resized() override;
    void setMeterLevels (float inL, float inR, float outL, float outR);
    void refreshDisplay()
    {
        display.update();
        timerCallback();
    }

private:
    void timerCallback() override;
    void layoutTiles();

    ShobeatsAudioProcessor& processor;
    Knob drift, unstable, input, mix, output;
    CentralDisplay display;
    juce::OwnedArray<KeyCap> vibeKeys;
    KeyCap generate { "Generate", hw_copper(), true };
    std::array<std::unique_ptr<EffectTile>, numEffects> tiles;
    LevelMeter inputMeter { "IN" }, outputMeter { "OUT" };
    ChainOrder::Order shownOrder {};

    static juce::Colour hw_copper() { return juce::Colour (0xffe0703a); }
};
} // namespace sfx::ui
