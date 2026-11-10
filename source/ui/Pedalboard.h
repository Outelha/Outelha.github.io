#pragma once

#include "ChainOrder.h"
#include "Controls.h"

namespace sfx::ui
{
// Metal stomp switch bound to an effect's on/off parameter.
class Footswitch final : public juce::Button
{
public:
    Footswitch (APVTS& state, const juce::String& paramId);
    void paintButton (juce::Graphics&, bool over, bool down) override;

private:
    std::unique_ptr<APVTS::ButtonAttachment> attachment;
};

// One effect pedal: enclosure, two main knobs, name plate, status LED, stomp switch.
// Click the enclosure to open its detail panel; drag it to move it in the chain.
class Pedal final : public juce::Component, private juce::Timer
{
public:
    static constexpr int width = 138, height = 300;

    Pedal (APVTS& state, EffectId id);
    ~Pedal() override;

    EffectId getEffect() const noexcept { return effect; }
    void setSelected (bool shouldBeSelected);

    std::function<void (EffectId)> onSelect;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void endDrag();

private:
    void timerCallback() override;
    void drawSurface (juce::Graphics&, const juce::Path& body, juce::Rectangle<float> bounds) const;
    void drawNamePlate (juce::Graphics&, juce::Rectangle<float> plate) const;
    juce::String readout() const;
    float value (const char* id) const;

    APVTS& state;
    EffectId effect;
    juce::Colour colour, ink;
    juce::String lastReadout;
    double animationTime = 0.0;
    bool selected = false, enabled = false, dragging = false;
    std::unique_ptr<juce::Component> knobA, knobB;
    Footswitch footswitch;
    std::unique_ptr<juce::ParameterAttachment> enabledAttachment;
};

// The chain of eight pedals, laid out in processing order. Pedals can be
// dragged to a new position; the order is shared with the audio thread.
class Pedalboard final : public juce::Component,
                         public juce::DragAndDropContainer,
                         public juce::DragAndDropTarget,
                         private juce::Timer
{
public:
    Pedalboard (APVTS& state, ChainOrder& order);

    void select (EffectId id);
    EffectId getSelected() const noexcept { return selected; }
    std::function<void (EffectId)> onSelectionChanged;

    // Moves the selected pedal one slot left (-1) or right (+1).
    void nudgeSelected (int direction);

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;

    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDragMove (const SourceDetails&) override;
    void itemDragExit (const SourceDetails&) override;
    void itemDropped (const SourceDetails&) override;
    void dragOperationEnded (const SourceDetails&) override;

private:
    void timerCallback() override;
    void layoutPedals (bool animate);
    juce::Rectangle<int> slotBounds (int slot) const;
    int slotForX (int x) const;

    ChainOrder& order;
    ChainOrder::Order shownOrder {};
    std::array<std::unique_ptr<Pedal>, numEffects> pedals;
    EffectId selected = EffectId::saturation;
    int dropSlot = -1;
};
} // namespace sfx::ui
