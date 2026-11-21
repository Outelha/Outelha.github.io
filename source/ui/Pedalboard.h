#pragma once

#include "ChainOrder.h"
#include "Controls.h"
#include "FrameClock.h"
#include "PedalLive.h"

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

// One effect pedal: individually designed enclosure (PedalArt), two real knobs, name
// plate, status LED, real stomp switch. Click the enclosure to open its detail panel;
// drag it to move it in the chain.
class Pedal final : public juce::Component, private FrameClock::Client
{
public:
    static constexpr int margin = 10;                         // room for the selection glow
    static constexpr int bodyWidth = 134, bodyHeight = 252;
    static constexpr int width = bodyWidth + 2 * margin, height = bodyHeight + 2 * margin;

    Pedal (APVTS& state, EffectId id, FrameClock& clock, const VisualFeed& feed);
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
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }
    void endDrag();

private:
    bool frame (double now, double dt) override;
    pedal::Geometry geometry() const;

    APVTS& state;
    EffectId effect;
    FrameClock& clock;
    PedalLive live;
    bool selected = false, dragging = false;
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
    Pedalboard (APVTS& state, ChainOrder& order, FrameClock& clock, const VisualFeed& feed);

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
    juce::Rectangle<float> boardArea() const;
    int slotForX (int x) const;

    ChainOrder& order;
    ChainOrder::Order shownOrder {};
    std::array<std::unique_ptr<Pedal>, numEffects> pedals;
    EffectId selected = EffectId::saturation;
    int dropSlot = -1;
};
} // namespace sfx::ui
