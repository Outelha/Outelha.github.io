#include "DetailPanel.h"

#include "Hardware.h"
#include "PedalArt.h"
#include "Skin.h"
#include "Textures.h"

namespace sfx::ui
{
using namespace juce;

namespace
{
// Panel-local layout (the panel is 1240 x 408).
const Rectangle<int> infoArea { 52, 30, 270, 90 };
const Rectangle<int> powerArea { 50, 124, 100, 38 };
const Rectangle<int> movesArea { 50, 176, 266, 38 };
const Rectangle<int> artArea { 20, 226, 300, 162 };
const Rectangle<int> displayArea { 330, 32, 350, 344 };
const Rectangle<int> controlsArea { 704, 34, 500, 340 };

float raw (const APVTS& s, const char* id)
{
    if (auto* v = s.getRawParameterValue (id))
        return v->load();
    return 0.0f;
}

float norm (const APVTS& s, const char* id)
{
    if (auto* p = s.getParameter (id))
        return p->getValue();
    return 0.0f;
}

void applyKnobLook (Component& c)
{
    auto style = [] (Knob& k) {
        k.setValueWindow (true);
        k.setCaptionFont (font (Type::labelBold, 18.0f, 0.04f));
        k.getSlider().setTextBoxStyle (Slider::TextBoxBelow, false, 92, 28);
    };
    if (auto* k = dynamic_cast<Knob*> (&c))
        style (*k);
    else if (auto* s = dynamic_cast<SyncSwap*> (&c))
        s->forEachKnob (style);
}
} // namespace

//==============================================================================
EffectArt::EffectArt (APVTS& s) : state (s)
{
    setInterceptsMouseClicks (false, false);
}

void EffectArt::setEffect (EffectId id)
{
    effect = id;
    refresh();
    repaint();
}

void EffectArt::refresh()
{
    const auto wasOn = on;
    const auto wasIntensity = intensity;
    on = raw (state, infoFor (effect).enabledId) > 0.5f;
    switch (effect)
    {
        case EffectId::saturation: intensity = norm (state, ParamID::satDrive); break;
        case EffectId::resampler: intensity = 1.0f - norm (state, ParamID::rsBits); break;
        case EffectId::tape: intensity = norm (state, ParamID::tapeDrive); break;
        case EffectId::chorus: intensity = norm (state, ParamID::chorusDepth); break;
        case EffectId::tremolo: intensity = norm (state, ParamID::tremDepth); break;
        case EffectId::reverse: intensity = norm (state, ParamID::revMix); break;
        case EffectId::delay: intensity = norm (state, ParamID::delayFeedback); break;
        case EffectId::reverb: intensity = norm (state, ParamID::reverbDecay); break;
    }
    if (on != wasOn || std::abs (intensity - wasIntensity) > 0.01f)
        repaint();
}

void EffectArt::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const auto accent = accentColour (effect);
    const auto glow = on ? 0.35f + 0.65f * intensity : 0.12f;

    if (pedal::hasSkin (effect))
    {
        // DELAY skin: 300 x 162 illustration with feathered edges (art_delay@2x.png).
        Skin::draw (g, SharedResourcePointer<Skin>()->get ("art_delay"), b, on ? 1.0f : 0.55f);
        return;
    }

    static const char* names[] = { "art_saturation.png", "art_resampler.png", "art_tape.png", "art_chorus.png",
                                   "art_tremolo.png", "art_reverse.png", "art_delay.png", "art_reverb.png" };
    const auto image = SharedResourcePointer<Textures>()->artwork (names[static_cast<int> (effect)]);

    // Fade the picture into the panel at its top and right edges.
    Graphics::ScopedSaveState save (g);
    Path clip;
    clip.addRoundedRectangle (b, 12.0f);
    g.reduceClipRegion (clip);

    if (image.isValid())
    {
        g.setOpacity (on ? 1.0f : 0.55f);
        g.drawImage (image, b, RectanglePlacement::centred | RectanglePlacement::fillDestination);
        return;
    }

    // Procedural fallback, one per effect.
    switch (effect)
    {
        case EffectId::saturation:
        {
            // Tube amplifier corner: perforated chassis plate and two output tubes.
            auto plate = b.withTrimmedTop (b.getHeight() * 0.42f);
            g.setGradientFill (ColourGradient (Colour (0xff2b2b2e), plate.getX(), plate.getY(), Colour (0xff0d0d0e), plate.getRight(), plate.getBottom(), false));
            g.fillRect (plate);
            g.setColour (Colours::white.withAlpha (0.12f));
            g.drawLine (plate.getX(), plate.getY(), plate.getRight(), plate.getY(), 1.0f);
            for (int row = 0; row < 4; ++row)
                for (int col = 0; col < 6; ++col)
                {
                    const auto c = Point<float> (b.getRight() - 24.0f - static_cast<float> (col) * 15.0f - static_cast<float> (row % 2) * 7.0f,
                                                 plate.getY() + 18.0f + static_cast<float> (row) * 13.0f);
                    g.setColour (Colours::black);
                    g.fillEllipse (Rectangle<float> (8.0f, 8.0f).withCentre (c));
                    g.setColour (accent.withAlpha (0.12f * glow));
                    g.drawEllipse (Rectangle<float> (8.0f, 8.0f).withCentre (c), 0.8f);
                }
            for (auto [x, w] : { std::pair { 0.13f, 0.2f }, { 0.4f, 0.22f } })
            {
                const auto tube = Rectangle<float> (b.getWidth() * w, b.getHeight() * 0.82f).withPosition (b.getX() + b.getWidth() * x, b.getY() + b.getHeight() * 0.1f);
                hw::drawVacuumTube (g, tube, Colour (0xffff8a2a), glow);
            }
            break;
        }
        case EffectId::resampler:
        {
            // A converter chip on a circuit board with cyan traces.
            g.setColour (Colour (0xff06121a));
            g.fillRect (b);
            Random rng (0x5eed);
            for (int i = 0; i < 26; ++i)
            {
                const auto y = b.getY() + rng.nextFloat() * b.getHeight();
                const auto x0 = b.getX() + rng.nextFloat() * b.getWidth() * 0.5f;
                Path trace;
                trace.startNewSubPath (x0, y);
                trace.lineTo (x0 + 30.0f + rng.nextFloat() * 60.0f, y);
                trace.lineTo (x0 + 50.0f + rng.nextFloat() * 70.0f, y + (rng.nextFloat() - 0.5f) * 40.0f);
                g.setColour (accent.withAlpha (0.08f + 0.25f * glow * rng.nextFloat()));
                g.strokePath (trace, PathStrokeType (1.0f));
            }
            const auto chip = Rectangle<float> (b.getWidth() * 0.34f, b.getWidth() * 0.34f).withCentre (b.getCentre().translated (0.0f, b.getHeight() * 0.08f));
            hw::drawLightSpill (g, chip.expanded (40.0f), accent, 0.5f * glow);
            for (int i = 0; i < 8; ++i)
                for (auto side : { -1.0f, 1.0f })
                {
                    const auto y = chip.getY() + chip.getHeight() * (0.1f + 0.8f * static_cast<float> (i) / 7.0f);
                    g.setColour (Colour (0xffb9bcc2));
                    g.fillRect (Rectangle<float> (10.0f, 3.0f).withCentre ({ side < 0 ? chip.getX() - 5.0f : chip.getRight() + 5.0f, y }));
                }
            g.setGradientFill (ColourGradient (Colour (0xff2c2e33), chip.getX(), chip.getY(), Colour (0xff0b0b0c), chip.getRight(), chip.getBottom(), false));
            g.fillRoundedRectangle (chip, 4.0f);
            g.setColour (Colours::white.withAlpha (0.15f));
            g.drawRoundedRectangle (chip, 4.0f, 0.8f);
            g.setColour (accent.withAlpha (0.4f + 0.6f * glow));
            g.setFont (font (Type::labelBold, chip.getHeight() * 0.16f, 0.1f));
            g.drawText ("SHB-DAC", chip, Justification::centred, false);
            break;
        }
        case EffectId::tape:
        {
            const auto reel = Rectangle<float> (b.getHeight() * 1.1f, b.getHeight() * 1.1f).withCentre ({ b.getX() + b.getWidth() * 0.32f, b.getBottom() - b.getHeight() * 0.2f });
            pedal::paintReel (g, reel, 0.4f, 0.8f);
            for (auto x : { 0.72f, 0.86f })
            {
                const auto post = Rectangle<float> (16.0f, 34.0f).withCentre ({ b.getX() + b.getWidth() * x, b.getBottom() - 30.0f });
                g.setGradientFill (ColourGradient (Colour (0xffe8e4dc), post.getX(), 0.0f, Colour (0xff4a4640), post.getRight(), 0.0f, false));
                g.fillRoundedRectangle (post, 3.0f);
            }
            hw::drawLightSpill (g, b.withTrimmedTop (b.getHeight() * 0.5f), accent, 0.3f * glow);
            break;
        }
        case EffectId::chorus:
        case EffectId::tremolo:
        case EffectId::reverb:
        case EffectId::reverse:
        case EffectId::delay:
        {
            // Flowing light ribbons (chorus / tremolo), ripples (reverb), echo puck (delay),
            // rewind button (reverse). Brightness follows the effect's depth / amount.
            if (effect == EffectId::reverb)
            {
                const auto c = Point<float> (b.getCentreX(), b.getBottom() - b.getHeight() * 0.3f);
                for (int i = 6; i >= 1; --i)
                {
                    const auto rx = static_cast<float> (i) * b.getWidth() * 0.075f;
                    g.setColour (accent.withAlpha ((0.12f + 0.35f * glow) * (1.0f - static_cast<float> (i) / 7.0f)));
                    g.drawEllipse (Rectangle<float> (rx * 2.0f, rx * 0.5f).withCentre (c), 1.4f);
                }
                hw::drawLightSpill (g, Rectangle<float> (b.getWidth() * 0.7f, 40.0f).withCentre (c), accent, 0.7f * glow);
            }
            else if (effect == EffectId::delay || effect == EffectId::reverse)
            {
                const auto puck = Rectangle<float> (b.getWidth() * 0.78f, b.getHeight() * 0.48f).withCentre ({ b.getCentreX(), b.getBottom() - b.getHeight() * 0.28f });
                g.setGradientFill (ColourGradient (Colour (0xffd8d2c8), puck.getX(), puck.getY(), Colour (0xff2a2826), puck.getRight(), puck.getBottom(), false));
                g.fillEllipse (puck);
                g.setGradientFill (ColourGradient (Colour (0xff3a3836), puck.getX(), puck.getY(), Colour (0xff111111), puck.getRight(), puck.getBottom(), false));
                g.fillEllipse (puck.reduced (puck.getWidth() * 0.06f, puck.getHeight() * 0.1f));
                const auto top = puck.reduced (puck.getWidth() * 0.18f, puck.getHeight() * 0.28f).translated (0.0f, -puck.getHeight() * 0.08f);
                hw::drawLightSpill (g, top.expanded (20.0f), accent, 0.6f * glow);
                Path sym;
                if (effect == EffectId::delay)
                {
                    for (int i = 0; i < 4; ++i)
                        sym.addEllipse (Rectangle<float> (top.getHeight() * 1.1f, top.getHeight() * 1.1f)
                                            .withCentre ({ top.getX() + top.getWidth() * (0.2f + 0.2f * static_cast<float> (i)), top.getCentreY() - top.getHeight() * 0.3f }));
                }
                else
                {
                    const auto h = top.getHeight() * 0.9f;
                    for (auto off : { -h * 0.45f, h * 0.45f })
                        sym.addTriangle (top.getCentreX() + off - h * 0.45f, top.getCentreY() - h * 0.3f, top.getCentreX() + off + h * 0.45f,
                                         top.getCentreY() - h * 0.8f, top.getCentreX() + off + h * 0.45f, top.getCentreY() + h * 0.2f);
                }
                g.setColour (accent.withAlpha (0.25f * glow));
                g.strokePath (sym, PathStrokeType (6.0f));
                g.setColour (accent.withAlpha (0.4f + 0.6f * glow));
                g.strokePath (sym, PathStrokeType (2.0f));
            }
            else
            {
                for (int i = 0; i < 14; ++i)
                {
                    Path ribbon;
                    const auto phase = static_cast<float> (i) * 0.18f;
                    for (int k = 0; k <= 40; ++k)
                    {
                        const auto t = static_cast<float> (k) / 40.0f;
                        const auto y = b.getY() + b.getHeight() * (0.62f + 0.18f * std::sin (t * 5.0f + phase) - 0.25f * t + 0.01f * static_cast<float> (i));
                        k == 0 ? ribbon.startNewSubPath (b.getX() + b.getWidth() * t, y) : ribbon.lineTo (b.getX() + b.getWidth() * t, y);
                    }
                    g.setColour (accent.withAlpha ((0.05f + 0.25f * glow) * (1.0f - static_cast<float> (i) / 16.0f)));
                    g.strokePath (ribbon, PathStrokeType (1.2f));
                }
            }
            break;
        }
    }
    // Soft fade into the panel.
    g.setGradientFill (ColourGradient (Colour (0xff141518), b.getX(), b.getY(), Colour (0x00141518), b.getX(), b.getY() + b.getHeight() * 0.35f, false));
    g.fillRect (b.withHeight (b.getHeight() * 0.35f));
}

//==============================================================================
DetailPanel::DetailPanel (APVTS& s, FrameClock& clock, const VisualFeed& feed)
    : state (s), display (s, clock, feed), art (s)
{
    addAndMakeVisible (backdrop);
    addAndMakeVisible (art);
    addAndMakeVisible (display);
    for (auto* b : { &moveLeft, &moveRight })
    {
        addAndMakeVisible (*b);
        b->setTooltip ("Move this effect in the processing chain");
    }
    moveLeft.onClick = [this] { if (onMove) onMove (-1); };
    moveRight.onClick = [this] { if (onMove) onMove (1); };
    showEffect (EffectId::saturation);
    startTimerHz (10);
}

void DetailPanel::showEffect (EffectId id)
{
    effect = id;
    const auto accent = accentColour (id);
    display.setEffect (id);
    art.setEffect (id);

    topRow.clear();
    knobs.clear();
    power = std::make_unique<ToggleChip> (state, infoFor (id).enabledId, "On", accent, true);
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
    {
        applyKnobLook (*c);
        if (id == EffectId::delay)
        {
            // Bitmap knob layers (ShobeatsLookAndFeel::drawSkinnedPanelKnob); the slider,
            // its attachment, value arc and value text are unchanged.
            auto skinKnob = [] (Knob& k) { k.getSlider().getProperties().set ("skin", "delay"); };
            if (auto* k = dynamic_cast<Knob*> (c))
                skinKnob (*k);
            else if (auto* sw = dynamic_cast<SyncSwap*> (c))
                sw->forEachKnob (skinKnob);
        }
        addAndMakeVisible (c);
    }

    // New components need the editor's look-and-feel (text boxes are built from it).
    sendLookAndFeelChange();
    resized();
    repaint();
}

void DetailPanel::paintPanel (Graphics& g)
{
    const auto b = getLocalBounds().toFloat().reduced (2.0f);
    const auto accent = accentColour (effect);
    const bool skinned = pedal::hasSkin (effect);
    if (skinned)
        Skin::draw (g, SharedResourcePointer<Skin>()->get ("panel_background"), getLocalBounds().toFloat()); // 1240 x 408, divider included
    else
        hw::drawBezelPanel (g, b, 22.0f, 9.0f, true);

    // Accent light seams along the frame: the panel takes the colour of the selected pedal.
    for (auto x : { 0.36f, 0.62f, 0.86f })
        hw::drawLightSeam (g, { b.getX() + b.getWidth() * x, b.getY() + 5.0f }, b.getWidth() * 0.16f, accent, 0.9f);
    for (auto x : { 0.36f, 0.62f })
        hw::drawLightSeam (g, { b.getX() + b.getWidth() * x, b.getBottom() - 5.0f }, b.getWidth() * 0.16f, accent, 0.8f);

    auto info = infoArea.toFloat();
    hw::drawEngraved (g, "PEDAL SETTINGS", info.removeFromTop (18.0f), font (Type::label, 15.0f, 0.18f), Justification::centredLeft,
                      Colour (0xffbdb3a4));
    const auto title = info.removeFromTop (44.0f);
    const auto name = String (infoFor (effect).name).toUpperCase();
    g.setFont (font (Type::title, 44.0f, 0.02f));
    g.setColour (Colours::black.withAlpha (0.6f));
    g.drawText (name, title.translated (0.0f, 2.0f), Justification::centredLeft, false);
    ColourGradient ink (accent.brighter (0.5f), 0.0f, title.getY() + 6.0f, accent.darker (0.1f), 0.0f, title.getBottom() - 4.0f, false);
    g.setGradientFill (ink);
    g.drawText (name, title, Justification::centredLeft, false);
    g.setColour (Colour (0xffe2dcd2));
    g.setFont (font (Type::bodyItalic, 16.0f));
    g.drawFittedText (infoFor (effect).description, info.toNearestInt(), Justification::topLeft, 2, 0.9f);

    // Thin rule above the controls (as on the references); part of the skin's background.
    if (! skinned)
    {
        const auto rule = controlsArea.toFloat();
        g.setColour (Colours::white.withAlpha (0.07f));
        g.fillRect (rule.getX(), rule.getY() - 6.0f, rule.getWidth(), 1.0f);
    }
}

void DetailPanel::resized()
{
    backdrop.setBounds (getLocalBounds());
    art.setBounds (artArea);
    display.setBounds (displayArea);
    if (power != nullptr)
        power->setBounds (powerArea);
    auto moves = movesArea;
    moveLeft.setBounds (moves.removeFromLeft (126));
    moveRight.setBounds (moves.removeFromRight (126));

    auto area = controlsArea;

    // Top row: mode segments and switches.
    if (! topRow.isEmpty())
    {
        auto row = area.removeFromTop (44);
        int fixed = 0, segmentsCount = 0;
        for (auto* c : topRow)
            dynamic_cast<SegmentedChoice*> (c) != nullptr ? ++segmentsCount : fixed += 150;
        const auto segW = segmentsCount > 0 ? (row.getWidth() - fixed - 10 * (topRow.size() - 1)) / segmentsCount : 0;
        for (auto* c : topRow)
        {
            const auto w = dynamic_cast<SegmentedChoice*> (c) != nullptr ? segW : 150;
            c->setBounds (row.removeFromLeft (w));
            row.removeFromLeft (10);
        }
        area.removeFromTop (16);
    }
    else
    {
        area.removeFromTop (30);
    }

    const auto n = knobs.size();
    if (n == 0)
        return;
    const auto perRow = (! topRow.isEmpty() && n > 4) ? (n + 1) / 2 : n;
    const auto rows = (n + perRow - 1) / perRow;
    const auto cellW = jmin (150, area.getWidth() / perRow);
    const auto cellH = jmin (rows > 1 ? 140 : 190, area.getHeight() / rows);
    for (int r = 0; r < rows; ++r)
    {
        auto row = area.removeFromTop (cellH);
        const auto count = jmin (perRow, n - r * perRow);
        row = row.withSizeKeepingCentre (cellW * count, cellH);
        for (int c = 0; c < count; ++c)
            knobs[r * perRow + c]->setBounds (row.removeFromLeft (cellW).reduced (4, 2));
    }
}
} // namespace sfx::ui
