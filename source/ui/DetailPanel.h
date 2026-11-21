#pragma once

#include "EffectDisplay.h"
#include "FrameClock.h"

namespace sfx::ui
{
// Decorative illustration in the panel's lower-left corner. Uses bundled artwork
// (assets/images/art_<effect>.png) when present, otherwise a procedural drawing. Where it
// glows, the glow follows the effect's state (on/off, drive, decay ...), never a free-running
// animation.
class EffectArt final : public juce::Component
{
public:
    explicit EffectArt (APVTS& state);
    void setEffect (EffectId id);
    void paint (juce::Graphics&) override;
    void refresh(); // re-reads parameters; repaints if the picture changes

private:
    APVTS& state;
    EffectId effect = EffectId::saturation;
    float intensity = 0.0f;
    bool on = false;
};

// Advanced panel for the selected pedal: every parameter of the effect, its
// on/off switch, chain position controls and a live display. One shared layout,
// rebuilt for the selected effect.
class DetailPanel final : public juce::Component, private juce::Timer
{
public:
    DetailPanel (APVTS& state, FrameClock& clock, const VisualFeed& feed);

    void showEffect (EffectId id);
    EffectId getEffect() const noexcept { return effect; }
    void setBpm (double bpm) { display.setBpm (bpm); }
    EffectDisplay& getDisplay() noexcept { return display; }

    // Called with -1 / +1 when the user moves the effect in the chain.
    std::function<void (int)> onMove;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override { art.refresh(); }

    APVTS& state;
    EffectId effect = EffectId::saturation;
    EffectDisplay display;
    EffectArt art;
    juce::TextButton moveLeft { "<  Earlier" }, moveRight { "Later  >" };
    std::unique_ptr<ToggleChip> power;
    juce::OwnedArray<juce::Component> topRow; // segmented choices, toggle chips
    juce::OwnedArray<juce::Component> knobs;  // Knob or SyncSwap, in display order
};
} // namespace sfx::ui
