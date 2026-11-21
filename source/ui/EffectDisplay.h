#pragma once

#include "Controls.h"
#include "FrameClock.h"
#include "VisualFeed.h"

namespace sfx::ui
{
// Visualises the selected effect from its current parameter values: transfer
// curves, LFO shapes, echo taps, decay envelopes, etc. Animated where the effect
// itself moves over time.
class EffectDisplay final : public juce::Component, private FrameClock::Client
{
public:
    EffectDisplay (APVTS& state, FrameClock& clock, const VisualFeed& feed);
    ~EffectDisplay() override;

    void setEffect (EffectId id);
    void paint (juce::Graphics&) override;

    // Tempo used to draw synced effects.
    void setBpm (double newBpm) noexcept { bpm = newBpm; }

private:
    bool frame (double now, double dt) override;
    float value (const char* id) const;
    void drawCaptionPill (juce::Graphics&, juce::Rectangle<float>, const juce::String&) const;
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
    FrameClock& clock;
    const VisualFeed& feed;
    double time = 0.0;
    float reelPhase = 0.0f, reelSpeed = 0.0f;
    std::uint64_t lastSamples = 0;
    std::vector<float> lastValues;
};
} // namespace sfx::ui
