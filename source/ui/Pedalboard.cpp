#include "Pedalboard.h"

namespace sfx::ui
{
namespace
{
struct PedalKnobs
{
    const char* a;
    const char* captionA;
    const char* b;
    const char* captionB;
    // When set, knob A is a time knob that swaps to a note division in sync mode.
    const char* syncId = nullptr;
    const char* divisionId = nullptr;
};

PedalKnobs knobsFor (EffectId id)
{
    switch (id)
    {
        case EffectId::saturation: return { ParamID::satDrive, "Drive", ParamID::satMix, "Mix" };
        case EffectId::resampler: return { ParamID::rsRate, "Rate", ParamID::rsBits, "Bits" };
        case EffectId::tape: return { ParamID::tapeWow, "Wow", ParamID::tapeAge, "Age" };
        case EffectId::chorus: return { ParamID::chorusRate, "Rate", ParamID::chorusDepth, "Depth" };
        case EffectId::tremolo: return { ParamID::tremRate, "Rate", ParamID::tremDepth, "Depth", ParamID::tremSync, ParamID::tremDivision };
        case EffectId::reverse: return { ParamID::revTime, "Time", ParamID::revMix, "Mix", ParamID::revSync, ParamID::revDivision };
        case EffectId::delay: return { ParamID::delayTime, "Time", ParamID::delayFeedback, "Repeats", ParamID::delaySync, ParamID::delayDivision };
        case EffectId::reverb: return { ParamID::reverbDecay, "Decay", ParamID::reverbMix, "Mix" };
    }
    return {};
}

void drawScrew (juce::Graphics& g, juce::Point<float> c, float r, float angle)
{
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffd8d8d8), c.x - r, c.y - r, juce::Colour (0xff6c6c6c), c.x + r, c.y + r, false));
    g.fillEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c));
    g.setColour (juce::Colour (0xff3a3a3a));
    g.drawLine (c.x - r * 0.7f * std::cos (angle), c.y - r * 0.7f * std::sin (angle), c.x + r * 0.7f * std::cos (angle), c.y + r * 0.7f * std::sin (angle), 1.2f);
}
} // namespace

//==============================================================================
Footswitch::Footswitch (APVTS& state, const juce::String& paramId) : juce::Button ("Footswitch")
{
    setClickingTogglesState (true);
    setTooltip ("Stomp to switch this effect on or off");
    attachment = std::make_unique<APVTS::ButtonAttachment> (state, paramId, *this);
}

void Footswitch::paintButton (juce::Graphics& g, bool over, bool down)
{
    auto b = getLocalBounds().toFloat();
    const auto d = std::min (b.getWidth(), b.getHeight());
    const auto c = b.getCentre();

    // Hex nut.
    juce::Path nut;
    const auto nr = d * 0.5f;
    for (int i = 0; i < 6; ++i)
    {
        const auto a = juce::MathConstants<float>::pi / 3.0f * static_cast<float> (i) + juce::MathConstants<float>::pi / 6.0f;
        const juce::Point<float> p { c.x + nr * std::cos (a), c.y + nr * std::sin (a) };
        i == 0 ? nut.startNewSubPath (p) : nut.lineTo (p);
    }
    nut.closeSubPath();
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillPath (nut, juce::AffineTransform::translation (0.0f, 2.5f));
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffcfd1d4), c.x - nr, c.y - nr, juce::Colour (0xff5d6066), c.x + nr, c.y + nr, false));
    g.fillPath (nut);

    // Chrome dome, pushed in when pressed.
    const auto dr = d * (down ? 0.33f : 0.35f);
    const auto dome = juce::Rectangle<float> (dr * 2.0f, dr * 2.0f).withCentre (c.translated (0.0f, down ? 1.0f : 0.0f));
    juce::ColourGradient chrome (juce::Colour (over ? 0xfffafafa : 0xffeeeeee), dome.getX(), dome.getY(), juce::Colour (0xff6f737a), dome.getRight(), dome.getBottom(), true);
    chrome.addColour (0.55, juce::Colour (0xffa9adb4));
    g.setGradientFill (chrome);
    g.fillEllipse (dome);
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.drawEllipse (dome, 1.0f);
}

//==============================================================================
Pedal::Pedal (APVTS& state, EffectId id) : effect (id), colour (pedalColour (id)), footswitch (state, infoFor (id).enabledId)
{
    ink = colour.getPerceivedBrightness() > 0.62f ? juce::Colour (0xff1b1a18) : juce::Colour (0xfff4f1ea);

    const auto k = knobsFor (id);
    auto makeKnob = [&] (const char* param, const char* caption) {
        auto knob = std::make_unique<Knob> (state, param, caption, colour, Knob::Style::pedal);
        knob->setInkColour (ink);
        return knob;
    };

    if (k.syncId != nullptr)
    {
        auto swap = std::make_unique<SyncSwap> (state, k.syncId, k.a, k.divisionId, k.captionA, colour, Knob::Style::pedal);
        swap->getTimeKnob().setInkColour (ink);
        swap->getDivisionKnob().setInkColour (ink);
        knobA = std::move (swap);
    }
    else
    {
        knobA = makeKnob (k.a, k.captionA);
    }
    knobB = makeKnob (k.b, k.captionB);

    addAndMakeVisible (*knobA);
    addAndMakeVisible (*knobB);
    addAndMakeVisible (footswitch);

    if (auto* p = state.getParameter (infoFor (id).enabledId))
    {
        enabledAttachment = std::make_unique<juce::ParameterAttachment> (*p, [this] (float v) {
            enabled = v >= 0.5f;
            repaint();
        });
        enabledAttachment->sendInitialUpdate();
    }

    setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    setTitle (infoFor (id).name);
    setSize (width, height);
}

Pedal::~Pedal() = default;

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
    using namespace juce;
    auto bounds = getLocalBounds().toFloat().reduced (3.0f);
    const auto corner = 10.0f;

    // Shadow under the enclosure.
    g.setColour (Colours::black.withAlpha (0.55f));
    g.fillRoundedRectangle (bounds.translated (0.0f, 5.0f), corner);

    // Enclosure: painted metal with a light top edge and darker bottom.
    Path body;
    body.addRoundedRectangle (bounds, corner);
    g.setGradientFill (ColourGradient (colour.brighter (0.18f), bounds.getX(), bounds.getY(), colour.darker (0.35f), bounds.getRight(), bounds.getBottom(), false));
    g.fillPath (body);
    fillWithTexture (g, body, 0.35f);
    g.setColour (Colours::white.withAlpha (0.18f));
    g.drawRoundedRectangle (bounds.reduced (1.0f), corner - 1.0f, 1.2f);
    g.setColour (Colours::black.withAlpha (0.45f));
    g.drawRoundedRectangle (bounds, corner, 1.0f);

    // Corner screws.
    const auto inset = 9.0f;
    drawScrew (g, { bounds.getX() + inset, bounds.getY() + inset }, 3.5f, 0.6f);
    drawScrew (g, { bounds.getRight() - inset, bounds.getY() + inset }, 3.5f, 1.9f);
    drawScrew (g, { bounds.getX() + inset, bounds.getBottom() - inset }, 3.5f, 2.6f);
    drawScrew (g, { bounds.getRight() - inset, bounds.getBottom() - inset }, 3.5f, 0.2f);

    // Name plate.
    const auto plate = Rectangle<float> (bounds.getX() + 12.0f, 118.0f, bounds.getWidth() - 24.0f, 50.0f);
    g.setColour (Colour (0xff121214));
    g.fillRoundedRectangle (plate, 5.0f);
    g.setColour (Colours::white.withAlpha (0.08f));
    g.drawRoundedRectangle (plate.reduced (0.5f), 5.0f, 1.0f);
    g.setColour (Colour (0xfff4f1ea));
    g.setFont (uiFont (15.0f, true));
    g.drawText (String (infoFor (effect).name).toUpperCase(), plate.withTrimmedBottom (16.0f).toNearestInt(), Justification::centredBottom);
    g.setColour (colour.brighter (0.4f));
    g.setFont (uiFont (8.5f, true));
    g.drawText ("S H O B E A T S", plate.withTrimmedTop (30.0f).toNearestInt(), Justification::centred);

    // Status LED with glow.
    const Point<float> led { bounds.getCentreX(), 190.0f };
    if (enabled)
    {
        g.setGradientFill (ColourGradient (Palette::ledOn.withAlpha (0.55f), led.x, led.y, Palette::ledOn.withAlpha (0.0f), led.x + 16.0f, led.y, true));
        g.fillEllipse (Rectangle<float> (32.0f, 32.0f).withCentre (led));
    }
    g.setColour (Colours::black.withAlpha (0.6f));
    g.fillEllipse (Rectangle<float> (11.0f, 11.0f).withCentre (led));
    g.setColour (enabled ? Palette::ledOn.brighter (0.3f) : Colour (0xff4a1512));
    g.fillEllipse (Rectangle<float> (8.0f, 8.0f).withCentre (led));
    if (enabled)
    {
        g.setColour (Colours::white.withAlpha (0.7f));
        g.fillEllipse (Rectangle<float> (3.0f, 3.0f).withCentre (led.translated (-1.0f, -1.0f)));
    }
}

void Pedal::paintOverChildren (juce::Graphics& g)
{
    if (selected)
    {
        g.setColour (Palette::text.withAlpha (0.85f));
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), 11.0f, 2.0f);
    }
    if (dragging)
    {
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (3.0f), 10.0f);
    }
}

void Pedal::resized()
{
    auto knobs = juce::Rectangle<int> (10, 22, width - 20, 86);
    knobA->setBounds (knobs.removeFromLeft (knobs.getWidth() / 2).reduced (2, 0));
    knobB->setBounds (knobs.reduced (2, 0));
    footswitch.setBounds (juce::Rectangle<int> (60, 60).withCentre ({ width / 2, 245 }));
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
Pedalboard::Pedalboard (APVTS& state, ChainOrder& o) : order (o)
{
    for (int i = 0; i < numEffects; ++i)
    {
        auto& pedal = pedals[static_cast<size_t> (i)];
        pedal = std::make_unique<Pedal> (state, static_cast<EffectId> (i));
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

juce::Rectangle<int> Pedalboard::slotBounds (int slot) const
{
    const auto gap = (getWidth() - numEffects * Pedal::width) / (numEffects + 1);
    return { gap + slot * (Pedal::width + gap), (getHeight() - Pedal::height) / 2 - 6, Pedal::width, Pedal::height };
}

int Pedalboard::slotForX (int x) const
{
    const auto pitch = static_cast<float> (getWidth()) / static_cast<float> (numEffects);
    return std::clamp (static_cast<int> (static_cast<float> (x) / pitch), 0, numEffects - 1);
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
    repaint();
}

void Pedalboard::resized()
{
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

void Pedalboard::paint (juce::Graphics& g)
{
    // Board surface with cable channel and slot numbers.
    auto b = getLocalBounds().toFloat();
    juce::Path surface;
    surface.addRoundedRectangle (b, 14.0f);
    g.setGradientFill (juce::ColourGradient (Palette::board.brighter (0.06f), 0.0f, 0.0f, Palette::board.darker (0.3f), 0.0f, b.getBottom(), false));
    g.fillPath (surface);
    fillWithTexture (g, surface, 0.25f);
    g.setColour (Palette::boardEdge);
    g.drawRoundedRectangle (b.reduced (0.5f), 14.0f, 1.0f);

    // Signal cable running through the chain.
    const auto cableY = static_cast<float> (slotBounds (0).getY() + 150);
    g.setColour (juce::Colour (0xff0b0b0d));
    g.fillRect (juce::Rectangle<float> (b.getX() + 8.0f, cableY - 3.0f, b.getWidth() - 16.0f, 6.0f));
    g.setColour (Palette::brand.withAlpha (0.35f));
    g.fillRect (juce::Rectangle<float> (b.getX() + 8.0f, cableY - 0.5f, b.getWidth() - 16.0f, 1.0f));

    g.setFont (uiFont (10.0f, true));
    for (int slot = 0; slot < numEffects; ++slot)
    {
        const auto s = slotBounds (slot);
        g.setColour (Palette::textFaint);
        g.drawText (juce::String (slot + 1), s.withY (s.getBottom() + 4).withHeight (12), juce::Justification::centred);
    }
}

void Pedalboard::paintOverChildren (juce::Graphics& g)
{
    if (dropSlot < 0)
        return;
    const auto s = slotBounds (dropSlot).toFloat().expanded (5.0f);
    g.setColour (Palette::brand.withAlpha (0.18f));
    g.fillRoundedRectangle (s, 12.0f);
    g.setColour (Palette::brand);
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
