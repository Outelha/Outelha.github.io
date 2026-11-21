#pragma once

#include <juce_graphics/juce_graphics.h>

#include <map>
#include <optional>

namespace sfx::ui
{
// High-resolution bitmap skin for the DELAY pedal (assets/images/delay, see
// docs/ui-redesign/DELAY_ASSETS.md). Every file is an @2x image of a fixed design-pixel
// size; the skin only replaces *painting*. Controls, attachments, hit areas, live
// curves, echo rings, value arcs and value text stay in code.
//
// Lifetime: same rule as Textures. The images are Direct2D images on Windows, so they are
// never held in static storage. Each editor's look-and-feel owns a
// juce::SharedResourcePointer<Skin>; the images are decoded on first use (message
// thread) and released when the last editor closes.
class Skin
{
public:
    // One asset: the @2x master and an exact 2:1 box-filtered @1x copy, so small
    // on-screen sizes are not drawn by heavily down-sampling the master (no shimmer).
    struct Layer
    {
        juce::Image full, half;
        bool isValid() const noexcept { return full.isValid(); }
    };

    Skin();
    ~Skin();

    // Number of Skin objects currently alive (0 or 1); used by lifecycle tests.
    static int liveInstances() noexcept;

    // The 20 files of the DELAY set, without the "@2x.png/.jpg" suffix.
    static juce::StringArray delayLayerNames();
    // The bundled file name of a layer ("panel_background" -> "panel_background@2x.jpg").
    static juce::String fileNameFor (const juce::String& layer);

    // True when every DELAY file is bundled. The skin is used all-or-nothing: with a
    // missing file the DELAY pedal keeps its procedural drawing.
    bool hasDelay();

    // Decodes on first use. Returns an invalid layer when the file is not bundled.
    const Layer& get (const juce::String& layer);

    // Draws `layer` stretched to `dest` (design pixels), choosing the master or the @1x
    // copy from the context's physical pixel scale.
    static void draw (juce::Graphics&, const Layer&, juce::Rectangle<float> dest, float opacity = 1.0f);
    // Same, filling the image's alpha with `tint` (white "light-only" layers).
    static void drawTinted (juce::Graphics&, const Layer&, juce::Rectangle<float> dest, juce::Colour tint);
    // Square layer centred on `centre`, `size` design pixels wide, rotated clockwise by
    // `angle` radians about its canvas centre (0 = as drawn, pointing to 12 o'clock).
    static void drawRotated (juce::Graphics&, const Layer&, juce::Point<float> centre, float size, float angle, float opacity = 1.0f);

private:
    std::map<juce::String, Layer> cache;
    std::optional<bool> delayComplete;
};
} // namespace sfx::ui
