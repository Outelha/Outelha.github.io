#include "Pedalboard.h"

#include "Hardware.h"
#include "dsp/DspCommon.h"

namespace sfx::ui
{

//==============================================================================
Footswitch::Footswitch (APVTS& state, const juce::String& paramId) : juce::Button ("Footswitch")
{
    setClickingTogglesState (true);
    setTooltip ("Stomp to switch this effect on or off");
    attachment = std::make_unique<APVTS::ButtonAttachment> (state, paramId, *this);
}

void Footswitch::paintButton (juce::Graphics& g, bool over, bool down)
{
    hw::drawFootswitch (g, getLocalBounds().toFloat(), over, down);
}

//==============================================================================
Pedal::Pedal (APVTS& s, EffectId id, FrameClock& c, const VisualFeed& feed)
    : state (s), effect (id), clock (c), live (s, id, feed), footswitch (s, infoFor (id).enabledId)
{
    const auto k = pedal::faceKnobs (id);
    const auto accent = accentColour (id);
    auto makeKnob = [&] (const char* param) {
        auto knob = std::make_unique<Knob> (state, param, "", accent, Knob::Style::pedal);
        knob->hideCaption();
        if (id == EffectId::tape)
            knob->getSlider().getProperties().set ("style", "hidden"); // the reel is the knob
        return knob;
    };

    if (k.syncId != nullptr)
    {
        auto swap = std::make_unique<SyncSwap> (state, k.syncId, k.a, k.divisionId, "", accent, Knob::Style::pedal);
        swap->forEachKnob ([] (Knob& kn) { kn.hideCaption(); });
        knobA = std::move (swap);
    }
    else
    {
        knobA = makeKnob (k.a);
    }
    knobB = makeKnob (k.b);

    addAndMakeVisible (*knobA);
    addAndMakeVisible (*knobB);
    addAndMakeVisible (footswitch);

    if (auto* p = state.getParameter (infoFor (id).enabledId))
    {
        enabledAttachment = std::make_unique<juce::ParameterAttachment> (*p, [this] (float) { repaint(); });
        enabledAttachment->sendInitialUpdate();
    }

    setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    setTitle (infoFor (id).name);
    setSize (width, height);
    clock.add (*this);
}

Pedal::~Pedal()
{
    clock.remove (*this);
}

pedal::Geometry Pedal::geometry() const
{
    return pedal::layout (effect, getLocalBounds().toFloat().reduced (static_cast<float> (margin)), true);
}

bool Pedal::frame (double, double dt)
{
    if (! visibleInEditor (*this))
        return false;
    bool animating = false;
    if (live.advance (dt, animating))
        repaint();
    return animating;
}

void Pedal::setSelected (bool s)
{
    if (selected != s)
    {
        selected = s;
        repaint();
    }
}

void Pedal::paint (juce::Graphics& g)
{
    auto st = live.state();
    PedalLive::readParameters (state, effect, st);
    st.selected = selected;
    st.hover = isMouseOver (true);
    st.captions = true;
    st.paintKnobs = effect == EffectId::tape; // tape: reels and value arcs are painted, the sliders are invisible
    st.paintFootswitch = false;
    const auto geo = geometry();
    pedal::paint (g, effect, geo, st);

    // Delay: a second amber "tap" LED blinks at the delay time (120 BPM reference when synced).
    if (effect == EffectId::delay && st.enabled)
    {
        const auto sync = state.getRawParameterValue (ParamID::delaySync)->load() > 0.5f;
        const auto seconds = sync ? dsp::divisionSeconds (juce::roundToInt (state.getRawParameterValue (ParamID::delayDivision)->load()), 120.0)
                                  : state.getRawParameterValue (ParamID::delayTime)->load() * 0.001f;
        const bool blink = std::fmod (st.time, static_cast<double> (std::max (0.05f, static_cast<float> (seconds)))) < 0.06;
        hw::drawLed (g, geo.led.translated (geo.body.getWidth() * 0.28f, 0.0f), 2.6f, ledAmber, blink);
    }
}

void Pedal::paintOverChildren (juce::Graphics& g)
{
    if (dragging)
    {
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (static_cast<float> (margin)), 10.0f);
    }
}

void Pedal::resized()
{
    const auto geo = geometry();
    knobA->setBounds (geo.knobA.getSmallestIntegerContainer());
    knobB->setBounds (geo.knobB.getSmallestIntegerContainer());
    footswitch.setBounds (geo.footswitch.getSmallestIntegerContainer());
}

void Pedal::mouseDown (const juce::MouseEvent&)
{
    if (onSelect)
        onSelect (effect);
}

void Pedal::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging || e.getDistanceFromDragStart() < 6)
        return;
    if (auto* container = juce::DragAndDropContainer::findParentDragContainerFor (this))
    {
        dragging = true;
        repaint();
        container->startDragging (juce::String ("pedal:") + juce::String (static_cast<int> (effect)), this);
    }
}

void Pedal::mouseUp (const juce::MouseEvent&)
{
    endDrag();
}

void Pedal::endDrag()
{
    if (dragging)
    {
        dragging = false;
        repaint();
    }
}

//==============================================================================
Pedalboard::Pedalboard (APVTS& state, ChainOrder& o, FrameClock& clock, const VisualFeed& feed) : order (o)
{
    addAndMakeVisible (backdrop);
    for (int i = 0; i < numEffects; ++i)
    {
        auto& pedal = pedals[static_cast<size_t> (i)];
        pedal = std::make_unique<Pedal> (state, static_cast<EffectId> (i), clock, feed);
        pedal->onSelect = [this] (EffectId id) { select (id); };
        addAndMakeVisible (*pedal);
    }
    shownOrder = order.get();
    select (EffectId::saturation);
    startTimerHz (10);
}

void Pedalboard::select (EffectId id)
{
    selected = id;
    for (auto& p : pedals)
        p->setSelected (p->getEffect() == id);
    backdrop.repaint();
    repaint();
    if (onSelectionChanged)
        onSelectionChanged (id);
}

void Pedalboard::nudgeSelected (int direction)
{
    const auto current = order.get();
    for (int slot = 0; slot < numEffects; ++slot)
        if (current[static_cast<size_t> (slot)] == static_cast<int> (selected))
        {
            order.move (slot, std::clamp (slot + direction, 0, numEffects - 1));
            break;
        }
    timerCallback();
}

juce::Rectangle<float> Pedalboard::boardArea() const
{
    return getLocalBounds().toFloat().reduced (2.0f, 2.0f);
}

juce::Rectangle<int> Pedalboard::slotBounds (int slot) const
{
    const auto inner = boardArea().reduced (14.0f, 0.0f);
    const auto pitch = inner.getWidth() / static_cast<float> (numEffects);
    const auto cx = inner.getX() + pitch * (static_cast<float> (slot) + 0.5f);
    return juce::Rectangle<int> (Pedal::width, Pedal::height).withCentre ({ juce::roundToInt (cx), 0 }).withY (6);
}

int Pedalboard::slotForX (int x) const
{
    const auto inner = boardArea().reduced (14.0f, 0.0f);
    const auto pitch = inner.getWidth() / static_cast<float> (numEffects);
    return std::clamp (static_cast<int> ((static_cast<float> (x) - inner.getX()) / pitch), 0, numEffects - 1);
}

void Pedalboard::layoutPedals (bool animate)
{
    for (int slot = 0; slot < numEffects; ++slot)
    {
        auto& pedal = pedals[static_cast<size_t> (shownOrder[static_cast<size_t> (slot)])];
        const auto target = slotBounds (slot);
        if (animate)
            juce::Desktop::getInstance().getAnimator().animateComponent (pedal.get(), target, 1.0f, 160, false, 1.0, 0.0);
        else
            pedal->setBounds (target);
    }
    backdrop.repaint();
    repaint();
}

void Pedalboard::resized()
{
    backdrop.setBounds (getLocalBounds());
    layoutPedals (false);
}

void Pedalboard::timerCallback()
{
    const auto current = order.get();
    if (current != shownOrder)
    {
        shownOrder = current;
        layoutPedals (true);
    }
}

void Pedalboard::paintBoard (juce::Graphics& g)
{
    // Dark board in a polished frame; slot badges under each position.
    hw::drawBezelPanel (g, boardArea(), 16.0f, 5.0f, false);
    for (int slot = 0; slot < numEffects; ++slot)
    {
        const auto s = slotBounds (slot).toFloat();
        const auto id = static_cast<EffectId> (shownOrder[static_cast<size_t> (slot)]);
        const auto badge = juce::Rectangle<float> (40.0f, 22.0f).withCentre ({ s.getCentreX(), s.getBottom() + 12.0f });
        pedal::paintSlotBadge (g, badge, slot + 1, id == selected, accentColour (id));
    }
}

void Pedalboard::paintOverChildren (juce::Graphics& g)
{
    if (dropSlot < 0)
        return;
    const auto s = slotBounds (dropSlot).toFloat().reduced (static_cast<float> (Pedal::margin) - 4.0f);
    g.setColour (brandOrange.withAlpha (0.14f));
    g.fillRoundedRectangle (s, 12.0f);
    hw::drawRimGlow (g, s, 12.0f, brandOrange, 8.0f, 0.8f);
    g.setColour (brandOrange);
    g.drawRoundedRectangle (s, 12.0f, 2.0f);
}

void Pedalboard::dragOperationEnded (const SourceDetails&)
{
    // Covers drops outside the board too.
    for (auto& p : pedals)
        p->endDrag();
    dropSlot = -1;
    repaint();
}

bool Pedalboard::isInterestedInDragSource (const SourceDetails& details)
{
    return details.description.toString().startsWith ("pedal:");
}

void Pedalboard::itemDragMove (const SourceDetails& details)
{
    const auto slot = slotForX (details.localPosition.x);
    if (slot != dropSlot)
    {
        dropSlot = slot;
        repaint();
    }
}

void Pedalboard::itemDragExit (const SourceDetails&)
{
    dropSlot = -1;
    repaint();
}

void Pedalboard::itemDropped (const SourceDetails& details)
{
    const auto id = details.description.toString().fromFirstOccurrenceOf (":", false, false).getIntValue();
    const auto to = slotForX (details.localPosition.x);
    const auto current = order.get();
    for (int from = 0; from < numEffects; ++from)
        if (current[static_cast<size_t> (from)] == id)
            order.move (from, to);

    dropSlot = -1;
    select (static_cast<EffectId> (id));
    timerCallback();
}
} // namespace sfx::ui
