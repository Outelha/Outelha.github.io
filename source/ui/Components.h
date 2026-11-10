#pragma once

#include "TrioLookAndFeel.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace trio
{
struct LevelMeters;
}

namespace trio::ui
{
using APVTS = juce::AudioProcessorValueTreeState;

// Rotary knob with a caption above and an editable value read-out below.
class Knob final : public juce::Component
{
public:
    Knob (APVTS& state, const juce::String& paramId, const juce::String& caption, juce::Colour accent);

    void resized() override;
    juce::Slider& getSlider() noexcept { return slider; }

private:
    juce::Slider slider;
    juce::Label caption;
    std::unique_ptr<APVTS::SliderAttachment> attachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Knob)
};

// Circular power toggle bound to a bool parameter.
class PowerButton final : public juce::Button
{
public:
    PowerButton (APVTS& state, const juce::String& paramId, juce::Colour accent);
    void paintButton (juce::Graphics&, bool isMouseOver, bool isButtonDown) override;

private:
    juce::Colour accent;
    std::unique_ptr<APVTS::ButtonAttachment> attachment;
};

// Row of mutually exclusive buttons bound to a choice parameter.
class SegmentedChoice final : public juce::Component
{
public:
    SegmentedChoice (APVTS& state, const juce::String& paramId, juce::Colour accent);
    void resized() override;

private:
    void select (int index);

    juce::OwnedArray<juce::TextButton> buttons;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    juce::AudioParameterChoice* parameter = nullptr;
};

// Card hosting one effect: title, power toggle, a visual display and knobs.
class ModulePanel : public juce::Component, private juce::Button::Listener
{
public:
    ModulePanel (APVTS& state, const juce::String& title, const juce::String& subtitle,
                 const juce::String& enabledParamId, juce::Colour accent);
    ~ModulePanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;

protected:
    // Subclasses place their controls in this area.
    virtual void layoutContent (juce::Rectangle<int> area) = 0;
    void addContent (juce::Component& c);

    APVTS& state;
    const juce::Colour accent;

private:
    void buttonClicked (juce::Button*) override {}
    void buttonStateChanged (juce::Button*) override;
    void refreshEnabledLook();

    juce::String title, subtitle;
    PowerButton power;
    juce::Array<juce::Component*> content;
    bool lastOn = true;
};

// Base for small animated/parametric displays drawn on a dark inset.
class DisplayBase : public juce::Component, private juce::Timer
{
public:
    explicit DisplayBase (juce::Colour accent);
    void paint (juce::Graphics&) final;

protected:
    virtual void drawContent (juce::Graphics&, juce::Rectangle<float> area) = 0;
    // Return true if the display needs repainting (polled at 30 Hz).
    virtual bool poll() = 0;
    void drawGrid (juce::Graphics&, juce::Rectangle<float> area, int columns, int rows) const;

    const juce::Colour accent;

private:
    void timerCallback() override;
};

class SaturationCurve final : public DisplayBase
{
public:
    explicit SaturationCurve (APVTS& state);

private:
    void drawContent (juce::Graphics&, juce::Rectangle<float>) override;
    bool poll() override;

    std::atomic<float>& drive;
    std::atomic<float>& mode;
    std::atomic<float>& mix;
    std::atomic<float>& enabled;
    float lastDrive = -1.0f, lastMode = -1.0f, lastMix = -1.0f, lastEnabled = -1.0f;
};

class ChorusDisplay final : public DisplayBase
{
public:
    explicit ChorusDisplay (APVTS& state);

private:
    void drawContent (juce::Graphics&, juce::Rectangle<float>) override;
    bool poll() override;

    std::atomic<float>& rate;
    std::atomic<float>& depth;
    std::atomic<float>& delay;
    std::atomic<float>& enabled;
    double phase = 0.0;
    double lastTime = 0.0;
};

class ReverbDisplay final : public DisplayBase
{
public:
    explicit ReverbDisplay (APVTS& state);

private:
    void drawContent (juce::Graphics&, juce::Rectangle<float>) override;
    bool poll() override;

    std::atomic<float>& size;
    std::atomic<float>& decay;
    std::atomic<float>& damping;
    std::atomic<float>& predelay;
    std::atomic<float>& enabled;
    std::array<float, 5> last { -1, -1, -1, -1, -1 };
};

// Stereo peak meter (dBFS).
class LevelMeter final : public juce::Component
{
public:
    explicit LevelMeter (const juce::String& caption);
    void setLevels (float left, float right);
    void paint (juce::Graphics&) override;

private:
    juce::String caption;
    std::array<float, 2> levels {}, peaks {};
    std::array<int, 2> peakHold {};
};
} // namespace trio::ui
