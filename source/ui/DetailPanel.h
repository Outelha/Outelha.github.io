#pragma once

#include "EffectDisplay.h"

namespace sfx::ui
{
// Advanced panel for the selected pedal: every parameter of the effect, its
// on/off switch, chain position controls and a live display.
class DetailPanel final : public juce::Component
{
public:
    explicit DetailPanel (APVTS& state);

    void showEffect (EffectId id);
    EffectId getEffect() const noexcept { return effect; }
    void setBpm (double bpm) { display.setBpm (bpm); }

    // Called with -1 / +1 when the user moves the effect in the chain.
    std::function<void (int)> onMove;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    APVTS& state;
    EffectId effect = EffectId::saturation;
    EffectDisplay display;
    juce::TextButton moveLeft { "<  Earlier" }, moveRight { "Later  >" };
    std::unique_ptr<ToggleChip> power;
    juce::OwnedArray<juce::Component> topRow; // segmented choices, toggle chips
    juce::OwnedArray<juce::Component> knobs;
};
} // namespace sfx::ui
