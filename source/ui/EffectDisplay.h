#pragma once

#include "Controls.h"

namespace sfx::ui
{
// Visualises the selected effect from its current parameter values: transfer
// curves, LFO shapes, echo taps, decay envelopes, etc. Animated where the effect
// itself moves over time.
class EffectDisplay final : public juce::Component, private juce::Timer
{
public:
    explicit EffectDisplay (APVTS& state);

    void setEffect (EffectId id);
    void paint (juce::Graphics&) override;

    // Tempo used to draw synced effects.
    void setBpm (double newBpm) noexcept { bpm = newBpm; }

private:
    void timerCallback() override;
    float value (const char* id) const;
    bool isAnimated() const noexcept;

    void drawGrid (juce::Graphics&, juce::Rectangle<float>, int cols, int rows) const;
    void drawCaption (juce::Graphics&, juce::Rectangle<float>, const juce::String&) const;

    void drawSaturation (juce::Graphics&, juce::Rectangle<float>) const;
    void drawResampler (juce::Graphics&, juce::Rectangle<float>) const;
    void drawTape (juce::Graphics&, juce::Rectangle<float>) const;
    void drawChorus (juce::Graphics&, juce::Rectangle<float>) const;
    void drawTremolo (juce::Graphics&, juce::Rectangle<float>) const;
    void drawReverse (juce::Graphics&, juce::Rectangle<float>) const;
    void drawDelay (juce::Graphics&, juce::Rectangle<float>) const;
    void drawReverb (juce::Graphics&, juce::Rectangle<float>) const;

    APVTS& state;
    EffectId effect = EffectId::saturation;
    juce::Colour accent;
    double bpm = 120.0;
    double time = 0.0, lastTick = 0.0;
    std::vector<float> lastValues;
};
} // namespace sfx::ui
