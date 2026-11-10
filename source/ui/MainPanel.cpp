#include "MainPanel.h"

#include "Hardware.h"
#include "PluginProcessor.h"

namespace sfx::ui
{
using namespace juce;

namespace
{
// Short labels for the compact chain readout.
const char* shortName (EffectId id)
{
    switch (id)
    {
        case EffectId::saturation: return "SAT";
        case EffectId::resampler: return "RESAMP";
        case EffectId::tape: return "TAPE";
        case EffectId::chorus: return "CHORUS";
        case EffectId::tremolo: return "TREM";
        case EffectId::reverse: return "REVERSE";
        case EffectId::delay: return "DELAY";
        case EffectId::reverb: return "REVERB";
    }
    return "";
}

// Local rectangles (MainPanel is 1240 x 700).
const Rectangle<int> driftArea { 46, 26, 250, 300 };
const Rectangle<int> unstableArea { 944, 26, 250, 300 };
const Rectangle<int> displayArea { 340, 34, 560, 236 };
const Rectangle<int> vibeArea { 340, 292, 560, 46 };
const Rectangle<int> generateArea { 470, 352, 300, 54 };
const Rectangle<int> rackArea { 40, 438, 820, 238 };
const Rectangle<int> globalArea { 880, 438, 320, 238 };
} // namespace

//==============================================================================
KeyCap::KeyCap (const String& label, Colour ledColour, bool isLarge) : Button (label), led (ledColour), large (isLarge)
{
    setMouseCursor (MouseCursor::PointingHandCursor);
}

void KeyCap::paintButton (Graphics& g, bool over, bool down)
{
    auto b = getLocalBounds().toFloat().reduced (2.0f);
    const auto corner = large ? 9.0f : 6.0f;
    const bool on = lit || getToggleState();

    // Shadow and body (pressed keys sit lower).
    g.setColour (Colours::black.withAlpha (0.6f));
    g.fillRoundedRectangle (b.translated (0.0f, down ? 1.0f : 3.0f), corner);
    if (down)
        b.translate (0.0f, 2.0f);

    auto top = large ? Colour (0xffe8763e) : Colour (0xff3b3d43);
    auto bottom = large ? Colour (0xffa9461f) : Colour (0xff222327);
    if (over)
    {
        top = top.brighter (0.08f);
        bottom = bottom.brighter (0.08f);
    }
    g.setGradientFill (ColourGradient (top, 0.0f, b.getY(), bottom, 0.0f, b.getBottom(), false));
    g.fillRoundedRectangle (b, corner);
    // Dished top surface highlight.
    g.setGradientFill (ColourGradient (Colours::white.withAlpha (large ? 0.22f : 0.1f), 0.0f, b.getY(), Colours::white.withAlpha (0.0f), 0.0f, b.getCentreY(), false));
    g.fillRoundedRectangle (b.reduced (2.0f), corner - 1.0f);
    g.setColour (Colours::black.withAlpha (0.7f));
    g.drawRoundedRectangle (b, corner, 1.0f);

    auto textArea = b;
    if (! large)
    {
        hw::drawLed (g, { b.getX() + 12.0f, b.getCentreY() }, 3.2f, led, on);
        textArea.removeFromLeft (20.0f);
    }
    auto font = uiFont (large ? 20.0f : 12.5f, true).withExtraKerningFactor (large ? 0.12f : 0.04f);
    // Shrink long labels to fit rather than truncating them.
    const auto textWidth = GlyphArrangement::getStringWidth (font, getButtonText().toUpperCase());
    if (textWidth > textArea.getWidth() - 6.0f)
        font = font.withHeight (font.getHeight() * jmax (0.6f, (textArea.getWidth() - 6.0f) / textWidth));
    if (large)
    {
        g.setFont (font);
        g.setColour (Colours::black.withAlpha (0.35f));
        g.drawText (getButtonText().toUpperCase(), textArea.translated (0.0f, 1.0f), Justification::centred);
        g.setColour (Colour (0xfffff4ea));
        g.drawText (getButtonText().toUpperCase(), textArea, Justification::centred);
    }
    else
    {
        hw::drawEngraved (g, getButtonText().toUpperCase(), textArea, font, Justification::centred, on ? Colour (0xfff4efe6) : hw::engraving.withAlpha (0.75f));
    }
}

//==============================================================================
EffectTile::EffectTile (AudioProcessorValueTreeState& state, EffectId id) : Button (infoFor (id).name), effect (id)
{
    setClickingTogglesState (true);
    setTooltip (String (infoFor (id).name) + ": click to switch on/off, double-click to open the pedal");
    attachment = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (state, infoFor (id).enabledId, *this);
    setMouseCursor (MouseCursor::PointingHandCursor);
}

void EffectTile::mouseDoubleClick (const MouseEvent&)
{
    // The first click of the double-click toggled the pedal; undo that before opening it.
    setToggleState (! getToggleState(), sendNotificationSync);
    if (onOpen)
        onOpen (effect);
}

void EffectTile::paintButton (Graphics& g, bool over, bool down)
{
    auto b = getLocalBounds().toFloat().reduced (3.0f);
    const bool on = getToggleState();
    const auto colour = pedalColour (effect);

    // Miniature enclosure in the pedal's colour.
    g.setColour (Colours::black.withAlpha (0.55f));
    g.fillRoundedRectangle (b.translated (0.0f, down ? 1.0f : 4.0f), 8.0f);
    if (down)
        b.translate (0.0f, 2.0f);
    Path body;
    body.addRoundedRectangle (b, 8.0f);
    const auto base = on ? colour : colour.withSaturation (colour.getSaturation() * 0.35f).darker (0.55f);
    g.setGradientFill (ColourGradient (base.brighter (over ? 0.3f : 0.18f), b.getX(), b.getY(), base.darker (0.4f), b.getRight(), b.getBottom(), false));
    g.fillPath (body);
    fillWithTexture (g, body, 0.3f);
    g.setColour (Colours::white.withAlpha (0.15f));
    g.drawRoundedRectangle (b.reduced (1.0f), 7.0f, 1.0f);
    g.setColour (Colours::black.withAlpha (0.6f));
    g.drawRoundedRectangle (b, 8.0f, 1.0f);

    // Slot number, LED, name plate, mini footswitch.
    g.setColour (Colours::black.withAlpha (0.55f));
    g.setFont (uiFont (11.0f, true));
    g.drawText (String (slot), b.reduced (8.0f, 6.0f).removeFromTop (14.0f), Justification::topLeft);
    hw::drawLed (g, { b.getRight() - 14.0f, b.getY() + 13.0f }, 3.5f, Palette::ledOn, on);

    auto plate = Rectangle<float> (b.getX() + 7.0f, b.getY() + b.getHeight() * 0.32f, b.getWidth() - 14.0f, 34.0f);
    g.setColour (Colour (0xff111113));
    g.fillRoundedRectangle (plate, 4.0f);
    g.setColour (on ? Colour (0xfff4f1ea) : hw::engraving.withAlpha (0.55f));
    g.setFont (uiFont (12.5f, true));
    g.drawFittedText (String (infoFor (effect).name).toUpperCase(), plate.toNearestInt().reduced (3, 0), Justification::centred, 1, 0.7f);

    const auto fs = Rectangle<float> (26.0f, 26.0f).withCentre ({ b.getCentreX(), b.getBottom() - 30.0f });
    g.setGradientFill (ColourGradient (Colour (0xffe9e9ea), fs.getX(), fs.getY(), Colour (0xff6c6f75), fs.getRight(), fs.getBottom(), true));
    g.fillEllipse (fs);
    g.setColour (Colours::black.withAlpha (0.4f));
    g.drawEllipse (fs, 1.0f);
}

//==============================================================================
CentralDisplay::CentralDisplay (ShobeatsAudioProcessor& p) : processor (p)
{
    setInterceptsMouseClicks (false, false);
    startTimerHz (10);
    timerCallback();
}

void CentralDisplay::timerCallback()
{
    const auto [n, c] = processor.getCurrentPresetLabel();
    String desc;
    auto& pm = processor.getPresetManager();
    if (pm.getCurrentIndex() >= 0)
        desc = pm.getPresets()[static_cast<size_t> (pm.getCurrentIndex())].description;
    else if (pm.getLastGenerated() && pm.getLastGenerated()->name == n)
        desc = pm.getLastGenerated()->description;

    std::array<bool, numEffects> on {};
    for (int i = 0; i < numEffects; ++i)
        on[static_cast<size_t> (i)] = processor.getState().getRawParameterValue (effectInfo[static_cast<size_t> (i)].enabledId)->load() > 0.5f;
    const auto o = processor.getChainOrder().get();
    const auto d = processor.getState().getRawParameterValue (ParamID::drift)->load();
    const auto u = processor.getState().getRawParameterValue (ParamID::unstable)->load();
    const auto t = processor.getCurrentBpm();

    const auto changed = [] (auto a, auto b) { return ! juce::exactlyEqual (a, b); };
    if (n != name || c != category || desc != description || on != active || o != order || changed (d, drift) || changed (u, unstable) || changed (t, bpm))
    {
        name = n;
        category = c;
        description = desc;
        active = on;
        order = o;
        drift = d;
        unstable = u;
        bpm = t;
        repaint();
    }
}

void CentralDisplay::paint (Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (8.0f);
    hw::drawGlassDisplay (g, r, 10.0f);

    auto area = r.reduced (22.0f, 16.0f);
    const auto glow = [&g] (const String& text, Rectangle<float> box, Font font, Justification j, Colour c) {
        g.setFont (font);
        g.setColour (c.withAlpha (0.25f));
        g.drawText (text, box.translated (0.0f, 0.5f).expanded (0.5f), j);
        g.setColour (c);
        g.drawText (text, box, j);
    };

    auto top = area.removeFromTop (18.0f);
    glow (category.isEmpty() ? String ("SHOBEATS FX") : category.toUpperCase(), top, uiFont (12.0f, true).withExtraKerningFactor (0.1f), Justification::centredLeft, hw::amber.withAlpha (0.75f));
    glow (String (bpm, 1) + " BPM", top, uiFont (12.0f, true), Justification::centredRight, hw::amber.withAlpha (0.75f));

    area.removeFromTop (6.0f);
    glow (name.isEmpty() ? String ("INIT") : name.toUpperCase(), area.removeFromTop (46.0f), uiFont (36.0f, true).withExtraKerningFactor (0.03f),
          Justification::centredLeft, hw::amber);

    g.setColour (hw::amber.withAlpha (0.7f));
    g.setFont (uiFont (13.5f));
    g.drawFittedText (description.isEmpty() ? String ("Pick a preset, or choose a vibe and press GENERATE.") : description,
                      area.removeFromTop (40.0f).toNearestInt(), Justification::topLeft, 2, 0.9f);

    // Chain readout: active pedals in processing order.
    area.removeFromTop (10.0f);
    auto chain = area.removeFromTop (34.0f);
    g.setColour (hw::amberDim.withAlpha (0.6f));
    g.drawHorizontalLine (static_cast<int> (chain.getY()) - 4, chain.getX(), chain.getRight());
    float x = chain.getX();
    bool any = false;
    for (auto id : order)
    {
        if (! active[static_cast<size_t> (id)])
            continue;
        const auto label = String (shortName (static_cast<EffectId> (id)));
        const auto w = GlyphArrangement::getStringWidth (uiFont (12.0f, true), label) + 22.0f;
        if (x + w > chain.getRight())
            break;
        if (any)
        {
            glow (">", Rectangle<float> (x, chain.getY(), 14.0f, chain.getHeight()), uiFont (13.0f, true), Justification::centred, hw::amber.withAlpha (0.6f));
            x += 16.0f;
        }
        auto pill = Rectangle<float> (x, chain.getY() + 6.0f, w, chain.getHeight() - 12.0f);
        g.setColour (accentColour (static_cast<EffectId> (id)).withAlpha (0.25f));
        g.fillRoundedRectangle (pill, 4.0f);
        g.setColour (accentColour (static_cast<EffectId> (id)));
        g.drawRoundedRectangle (pill, 4.0f, 1.0f);
        g.setFont (uiFont (12.0f, true));
        g.drawText (label, pill, Justification::centred);
        x += w + 2.0f;
        any = true;
    }
    if (! any)
        glow ("ALL PEDALS BYPASSED", chain, uiFont (12.0f, true), Justification::centredLeft, hw::amber.withAlpha (0.5f));

    // Movement status.
    auto status = area.removeFromBottom (18.0f);
    glow ("DRIFT " + String (roundToInt (drift * 100.0f)) + "%   /   UNSTABLE " + String (roundToInt (unstable * 100.0f)) + "%", status,
          uiFont (11.5f, true).withExtraKerningFactor (0.08f), Justification::centredLeft, hw::amber.withAlpha (0.6f));
}

//==============================================================================
MainPanel::MainPanel (ShobeatsAudioProcessor& p)
    : processor (p),
      drift (p.getState(), ParamID::drift, "Drift", hw::copper, Knob::Style::alu),
      unstable (p.getState(), ParamID::unstable, "Unstable", hw::copper, Knob::Style::alu),
      input (p.getState(), ParamID::inputGain, "Input", hw::amber, Knob::Style::alu),
      mix (p.getState(), ParamID::globalMix, "Mix", hw::copper, Knob::Style::alu),
      output (p.getState(), ParamID::outputGain, "Output", hw::amber, Knob::Style::alu),
      display (p)
{
    for (Component* c : { (Component*) &drift, (Component*) &unstable, (Component*) &input, (Component*) &mix, (Component*) &output,
                          (Component*) &display, (Component*) &generate, (Component*) &inputMeter, (Component*) &outputMeter })
        addAndMakeVisible (*c);

    drift.getSlider().setTooltip ("Drift: slow analog wander of pitch, time and rate across all active pedals");
    unstable.getSlider().setTooltip ("Unstable: fast irregular flutter, jitter and level flicker across all active pedals");

    auto& pm = p.getPresetManager();
    const auto names = Generator::vibeNames();
    for (int i = 0; i < names.size(); ++i)
    {
        auto* key = vibeKeys.add (new KeyCap (names[i], hw::amber));
        key->setTooltip ("Generator vibe: " + names[i]);
        key->onClick = [this, i] {
            processor.getPresetManager().setSelectedVibe (static_cast<Generator::Vibe> (i));
            timerCallback();
        };
        addAndMakeVisible (key);
    }
    generate.setTooltip ("Generate a new effect chain in the selected vibe. Like it? Press SAVE.");
    generate.onClick = [&pm] { pm.generate (pm.getSelectedVibe()); };

    for (int i = 0; i < numEffects; ++i)
    {
        auto& tile = tiles[static_cast<size_t> (i)];
        tile = std::make_unique<EffectTile> (p.getState(), static_cast<EffectId> (i));
        tile->onOpen = [this] (EffectId id) { if (onOpenEffect) onOpenEffect (id); };
        addAndMakeVisible (*tile);
    }
    shownOrder = p.getChainOrder().get();
    startTimerHz (10);
    timerCallback();
}

void MainPanel::timerCallback()
{
    const auto selected = static_cast<int> (processor.getPresetManager().getSelectedVibe());
    for (int i = 0; i < vibeKeys.size(); ++i)
        vibeKeys[i]->setLit (i == selected);

    const auto order = processor.getChainOrder().get();
    if (order != shownOrder)
    {
        shownOrder = order;
        layoutTiles();
    }
}

void MainPanel::setMeterLevels (float inL, float inR, float outL, float outR)
{
    inputMeter.setLevels (inL, inR);
    outputMeter.setLevels (outL, outR);
}

void MainPanel::paint (Graphics& g)
{
    // Recessed wells behind each control group.
    hw::drawRecess (g, driftArea.toFloat(), 18.0f, Colour (0xff17181b));
    hw::drawRecess (g, unstableArea.toFloat(), 18.0f, Colour (0xff17181b));
    hw::drawRecess (g, vibeArea.toFloat().expanded (8.0f), 10.0f);
    hw::drawRecess (g, rackArea.toFloat(), 14.0f);
    hw::drawRecess (g, globalArea.toFloat(), 14.0f);

    // Engraved section labels.
    const auto label = [&g] (const String& text, Rectangle<int> above) {
        hw::drawEngraved (g, text, above.toFloat().withHeight (16.0f).translated (6.0f, -20.0f), uiFont (11.0f, true).withExtraKerningFactor (0.18f),
                          Justification::centredLeft, hw::engraving.withAlpha (0.65f));
    };
    label ("FX CHAIN  -  CLICK TO SWITCH, DOUBLE-CLICK TO EDIT", rackArea);
    label ("MASTER", globalArea);
    label ("FX GENERATOR  -  CHOOSE A VIBE", vibeArea.expanded (8));

    // Screws on the wells around the big knobs.
    for (auto a : { driftArea, unstableArea })
    {
        const auto r = a.toFloat().reduced (12.0f);
        hw::drawScrew (g, r.getTopLeft(), 4.0f, 0.4f);
        hw::drawScrew (g, r.getTopRight(), 4.0f, 1.2f);
        hw::drawScrew (g, r.getBottomLeft(), 4.0f, 2.1f);
        hw::drawScrew (g, r.getBottomRight(), 4.0f, 0.9f);
    }
}

void MainPanel::layoutTiles()
{
    auto rack = rackArea.reduced (14, 16);
    const auto w = rack.getWidth() / numEffects;
    for (int slot = 0; slot < numEffects; ++slot)
    {
        auto& tile = tiles[static_cast<size_t> (shownOrder[static_cast<size_t> (slot)])];
        tile->setSlot (slot + 1);
        tile->setBounds (rack.getX() + slot * w, rack.getY(), w, rack.getHeight());
    }
}

void MainPanel::resized()
{
    drift.setBounds (driftArea.reduced (22, 16));
    unstable.setBounds (unstableArea.reduced (22, 16));
    display.setBounds (displayArea);

    auto vibes = vibeArea;
    const auto w = vibes.getWidth() / jmax (1, vibeKeys.size());
    for (int i = 0; i < vibeKeys.size(); ++i)
        vibeKeys[i]->setBounds (vibes.removeFromLeft (w).reduced (2, 0));
    generate.setBounds (generateArea);

    layoutTiles();

    auto global = globalArea.reduced (14, 18);
    inputMeter.setBounds (global.removeFromLeft (24));
    global.removeFromLeft (6);
    outputMeter.setBounds (global.removeFromRight (24));
    global.removeFromRight (6);
    const auto kw = global.getWidth() / 3;
    input.setBounds (global.removeFromLeft (kw).withSizeKeepingCentre (kw, 160));
    mix.setBounds (global.removeFromLeft (kw).withSizeKeepingCentre (kw, 160));
    output.setBounds (global.withSizeKeepingCentre (kw, 160));
}
} // namespace sfx::ui
