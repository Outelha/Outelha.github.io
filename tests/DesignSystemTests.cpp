// Renders the SHOBEATS hardware design system (knobs, keys, pedals, panels) into an image.
// Always runs as a smoke test (every painter at two scales, no crash, typefaces loaded);
// with SHOBEATS_SNAPSHOT=<prefix> it also writes <prefix>design.png for visual review.

#include "ui/Hardware.h"
#include "ui/PedalArt.h"
#include "ui/Textures.h"

#include <catch2/catch_test_macros.hpp>
#include <juce_gui_basics/juce_gui_basics.h>

namespace
{
using namespace sfx;
using namespace sfx::ui;

void paintSheet (juce::Graphics& g, juce::Rectangle<float> area, double time)
{
    g.fillAll (juce::Colour (0xff0b0b0d));
    hw::drawBezelPanel (g, area.reduced (10.0f), 14.0f, 8.0f, true);

    // Knobs: machined in four accents, big numbered knob, pedal knobs.
    float x = 40.0f;
    for (auto id : { EffectId::saturation, EffectId::resampler, EffectId::tremolo, EffectId::reverb })
    {
        hw::KnobLook k;
        k.accent = accentColour (id);
        k.startAngle = juce::MathConstants<float>::pi * 1.25f;
        k.endAngle = juce::MathConstants<float>::pi * 2.75f;
        k.proportion = 0.2f + 0.2f * static_cast<float> (static_cast<int> (id) % 4);
        k.angle = k.startAngle + (k.endAngle - k.startAngle) * k.proportion;
        hw::drawMachinedKnob (g, { x, 40.0f, 110.0f, 110.0f }, k);
        hw::drawValueWindow (g, { x + 15.0f, 160.0f, 80.0f, 26.0f }, juce::String (k.proportion * 100.0f, 0) + " %", juce::Colours::white, 15.0f);
        x += 130.0f;
    }
    hw::KnobLook big;
    big.startAngle = juce::MathConstants<float>::pi * 1.25f;
    big.endAngle = juce::MathConstants<float>::pi * 2.75f;
    big.proportion = 0.1f;
    big.angle = big.startAngle + (big.endAngle - big.startAngle) * big.proportion;
    big.numbers = true;
    hw::drawMachinedKnob (g, { x, 25.0f, 190.0f, 190.0f }, big);
    hw::drawGlassTube (g, { x + 210.0f, 40.0f, 26.0f, 150.0f }, brandOrange, 0.7f);
    hw::drawVentSlots (g, { x + 250.0f, 120.0f, 22.0f, 70.0f }, 7);

    // Keys.
    hw::KeyLook key;
    hw::drawKey (g, { 40.0f, 220.0f, 120.0f, 44.0f }, key);
    hw::drawKeyText (g, "SAVE", { 40.0f, 220.0f, 120.0f, 44.0f }, font (Type::labelBold, 18.0f), false, brandOrange);
    key.lit = true;
    hw::drawKey (g, { 180.0f, 220.0f, 240.0f, 44.0f }, key);
    hw::drawKeyText (g, "GENERATE", { 180.0f, 220.0f, 240.0f, 44.0f }, font (Type::labelBold, 22.0f, 0.2f), true, brandOrange);
    hw::drawSpeakerGrille (g, { 440.0f, 222.0f, 130.0f, 40.0f });
    hw::drawLightSeam (g, { 760.0f, 242.0f }, 260.0f, brandOrange);

    // All eight pedals.
    for (int i = 0; i < numEffects; ++i)
    {
        const auto id = static_cast<EffectId> (i);
        const auto body = juce::Rectangle<float> (40.0f + static_cast<float> (i) * 150.0f, 300.0f, 136.0f, 262.0f);
        const auto geo = pedal::layout (id, body, true);
        pedal::State s;
        s.enabled = i != 5;
        s.selected = i == 2;
        s.knobA = 0.3f + 0.05f * static_cast<float> (i);
        s.knobB = 0.7f;
        s.time = time;
        s.level = 0.6f;
        s.reelPhase = static_cast<float> (time);
        s.extra = { 0.6f, 0.6f, static_cast<float> (i % 4), 0.5f };
        pedal::paint (g, id, geo, s);
        pedal::paintSlotBadge (g, { body.getCentreX() - 18.0f, body.getBottom() + 12.0f, 36.0f, 22.0f }, i + 1, s.selected, accentColour (id));
    }
}
} // namespace

TEST_CASE ("Design system: every painter renders at 1x and 2x", "[gui][design]")
{
    juce::ScopedJuceInitialiser_GUI juce;
    {
        const juce::SharedResourcePointer<Textures> textures;
        const juce::SharedResourcePointer<Fonts> fonts;
        REQUIRE (fonts->condensedBold != nullptr);
        REQUIRE (fonts->bodyMedium != nullptr);
        CHECK (fonts->condensedBold->getName().containsIgnoreCase ("Barlow"));
        CHECK (textures->conic.isValid());
        CHECK (textures->hammered.isValid());

        for (auto scale : { 1.0f, 2.0f })
        {
            juce::Image image (juce::Image::ARGB, static_cast<int> (1260 * scale), static_cast<int> (620 * scale), true);
            {
                juce::Graphics g (image);
                g.addTransform (juce::AffineTransform::scale (scale));
                paintSheet (g, { 0.0f, 0.0f, 1260.0f, 620.0f }, 1.3);
            }
            // Something was drawn in the middle of the sheet (not left transparent).
            CHECK (image.getPixelAt (image.getWidth() / 2, image.getHeight() / 2).getAlpha() == 255);

            if (const auto* prefix = std::getenv ("SHOBEATS_SNAPSHOT"); prefix != nullptr && scale > 1.5f)
            {
                juce::File file (juce::String::fromUTF8 (prefix) + "design.png");
                file.deleteFile();
                juce::FileOutputStream out (file);
                REQUIRE (out.openedOk());
                CHECK (juce::PNGImageFormat().writeImageToStream (image, out));
            }
        }
    }
    CHECK (Textures::liveInstances() == 0);
}
