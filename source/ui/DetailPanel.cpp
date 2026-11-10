#include "DetailPanel.h"

#include "Hardware.h"

namespace sfx::ui
{
DetailPanel::DetailPanel (APVTS& s) : state (s), display (s)
{
    addAndMakeVisible (display);
    for (auto* b : { &moveLeft, &moveRight })
    {
        addAndMakeVisible (*b);
        b->setTooltip ("Move this effect in the processing chain");
    }
    moveLeft.onClick = [this] { if (onMove) onMove (-1); };
    moveRight.onClick = [this] { if (onMove) onMove (1); };
    showEffect (EffectId::saturation);
}

void DetailPanel::showEffect (EffectId id)
{
    effect = id;
    const auto accent = accentColour (id);
    display.setEffect (id);

    topRow.clear();
    knobs.clear();
    power = std::make_unique<ToggleChip> (state, infoFor (id).enabledId, "On", accent);
    power->setTooltip ("Switch this effect on or off (same as the pedal's footswitch)");
    addAndMakeVisible (*power);

    auto knob = [&] (const char* param, const char* caption) { knobs.add (new Knob (state, param, caption, accent)); };
    auto swap = [&] (const char* sync, const char* time, const char* division, const char* caption) {
        knobs.add (new SyncSwap (state, sync, time, division, caption, accent, Knob::Style::panel));
    };
    auto chip = [&] (const char* param, const char* text) { topRow.add (new ToggleChip (state, param, text, accent)); };
    auto segments = [&] (const char* param) { topRow.add (new SegmentedChoice (state, param, accent)); };

    switch (id)
    {
        case EffectId::saturation:
            segments (ParamID::satMode);
            knob (ParamID::satDrive, "Drive");
            knob (ParamID::satTone, "Tone");
            knob (ParamID::satOutput, "Output");
            knob (ParamID::satMix, "Mix");
            break;
        case EffectId::resampler:
            knob (ParamID::rsRate, "Rate");
            knob (ParamID::rsBits, "Bits");
            knob (ParamID::rsFilter, "Filter");
            knob (ParamID::rsJitter, "Jitter");
            knob (ParamID::rsMix, "Mix");
            break;
        case EffectId::tape:
            knob (ParamID::tapeDrive, "Saturation");
            knob (ParamID::tapeWow, "Wow");
            knob (ParamID::tapeFlutter, "Flutter");
            knob (ParamID::tapeNoise, "Noise");
            knob (ParamID::tapeAge, "Age");
            knob (ParamID::tapeMix, "Mix");
            break;
        case EffectId::chorus:
            knob (ParamID::chorusRate, "Rate");
            knob (ParamID::chorusDepth, "Depth");
            knob (ParamID::chorusDelay, "Delay");
            knob (ParamID::chorusFeedback, "Feedback");
            knob (ParamID::chorusWidth, "Width");
            knob (ParamID::chorusMix, "Mix");
            break;
        case EffectId::tremolo:
            segments (ParamID::tremShape);
            chip (ParamID::tremSync, "BPM Sync");
            swap (ParamID::tremSync, ParamID::tremRate, ParamID::tremDivision, "Rate");
            knob (ParamID::tremDepth, "Depth");
            knob (ParamID::tremStereo, "Stereo");
            break;
        case EffectId::reverse:
            chip (ParamID::revSync, "BPM Sync");
            swap (ParamID::revSync, ParamID::revTime, ParamID::revDivision, "Length");
            knob (ParamID::revPitch, "Pitch");
            knob (ParamID::revMix, "Mix");
            break;
        case EffectId::delay:
            chip (ParamID::delaySync, "BPM Sync");
            chip (ParamID::delayPingPong, "Ping-Pong");
            swap (ParamID::delaySync, ParamID::delayTime, ParamID::delayDivision, "Time");
            knob (ParamID::delayFeedback, "Feedback");
            knob (ParamID::delayLowCut, "Low Cut");
            knob (ParamID::delayHighCut, "High Cut");
            knob (ParamID::delayWidth, "Width");
            knob (ParamID::delayMix, "Mix");
            break;
        case EffectId::reverb:
            knob (ParamID::reverbSize, "Size");
            knob (ParamID::reverbDecay, "Decay");
            knob (ParamID::reverbDamping, "Damping");
            knob (ParamID::reverbPredelay, "Pre-Delay");
            knob (ParamID::reverbWidth, "Width");
            knob (ParamID::reverbMix, "Mix");
            break;
    }

    for (auto* c : topRow)
        addAndMakeVisible (c);
    for (auto* c : knobs)
        addAndMakeVisible (c);

    // New components need the editor's look-and-feel (text boxes are built from it).
    sendLookAndFeelChange();
    resized();
    repaint();
}

void DetailPanel::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat().reduced (1.0f);
    const auto accent = accentColour (effect);
    hw::drawRecess (g, b, 14.0f, juce::Colour (0xff16171a));

    // Accent strip + title.
    g.setColour (accent);
    g.fillRoundedRectangle (b.getX() + 22.0f, b.getY(), 120.0f, 3.0f, 1.5f);

    auto info = getLocalBounds().reduced (24, 20).removeFromLeft (230);
    hw::drawEngraved (g, "PEDAL SETTINGS", info.removeFromTop (16).toFloat(), uiFont (11.0f, true).withExtraKerningFactor (0.18f),
                      juce::Justification::centredLeft, hw::engraving.withAlpha (0.6f));
    g.setColour (Palette::text);
    g.setFont (uiFont (26.0f, true));
    g.drawText (juce::String (infoFor (effect).name).toUpperCase(), info.removeFromTop (34), juce::Justification::centredLeft);
    g.setColour (Palette::textDim);
    g.setFont (uiFont (13.0f));
    g.drawFittedText (infoFor (effect).description, info.removeFromTop (40), juce::Justification::topLeft, 2);
}

void DetailPanel::resized()
{
    auto area = getLocalBounds().reduced (24, 20);

    auto info = area.removeFromLeft (230);
    info.removeFromTop (16 + 34 + 44);
    if (power != nullptr)
        power->setBounds (info.removeFromTop (30).withWidth (90));
    info.removeFromTop (12);
    auto moves = info.removeFromTop (30);
    moveLeft.setBounds (moves.removeFromLeft (108));
    moves.removeFromLeft (8);
    moveRight.setBounds (moves.removeFromLeft (108));

    area.removeFromLeft (16);
    display.setBounds (area.removeFromLeft (330));
    area.removeFromLeft (24);

    // Top row: choices and switches; below: knobs in one or two rows.
    if (! topRow.isEmpty())
    {
        auto row = area.removeFromTop (30);
        for (auto* c : topRow)
        {
            const auto w = dynamic_cast<SegmentedChoice*> (c) != nullptr ? juce::jmin (row.getWidth(), 300) : 110;
            c->setBounds (row.removeFromLeft (w));
            row.removeFromLeft (10);
        }
        area.removeFromTop (14);
    }

    const auto n = knobs.size();
    if (n == 0)
        return;
    const auto perRow = n > 6 ? (n + 1) / 2 : n;
    const auto rows = (n + perRow - 1) / perRow;
    const auto cellW = juce::jmin (100, area.getWidth() / perRow);
    const auto cellH = juce::jmin (140, area.getHeight() / rows);
    for (int r = 0; r < rows; ++r)
    {
        auto row = area.removeFromTop (cellH);
        for (int c = 0; c < perRow; ++c)
        {
            const auto index = r * perRow + c;
            if (index >= n)
                break;
            knobs[index]->setBounds (row.removeFromLeft (cellW).reduced (3, 2));
        }
    }
}
} // namespace sfx::ui
