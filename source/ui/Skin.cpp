#include "Skin.h"

#include <ShobeatsAssets.h>

#include <atomic>

namespace sfx::ui
{
namespace
{
std::atomic<int> instances { 0 };

// Looks a bundled file up by its original file name (BinaryData mangles '@' and '.').
const char* findResource (const juce::String& fileName, int& size)
{
    size = 0;
    for (int i = 0; i < ShobeatsAssets::namedResourceListSize; ++i)
        if (fileName == ShobeatsAssets::originalFilenames[i])
            return ShobeatsAssets::getNamedResource (ShobeatsAssets::namedResourceList[i], size);
    return nullptr;
}

// Exact 2:1 reduction: every output pixel is the mean of a 2 x 2 block of premultiplied
// ARGB pixels. Done once per asset, at load.
juce::Image halfSize (const juce::Image& source)
{
    const auto src = source.convertedToFormat (juce::Image::ARGB);
    const auto sw = src.getWidth(), sh = src.getHeight();
    const auto w = juce::jmax (1, sw / 2), h = juce::jmax (1, sh / 2);
    juce::Image out (juce::Image::ARGB, w, h, false);
    const juce::Image::BitmapData in (src, juce::Image::BitmapData::readOnly);
    juce::Image::BitmapData dst (out, juce::Image::BitmapData::writeOnly);
    for (int y = 0; y < h; ++y)
    {
        const auto* row0 = in.getLinePointer (juce::jmin (2 * y, sh - 1));
        const auto* row1 = in.getLinePointer (juce::jmin (2 * y + 1, sh - 1));
        auto* o = dst.getLinePointer (y);
        for (int x = 0; x < w; ++x)
        {
            const auto x0 = juce::jmin (2 * x, sw - 1) * in.pixelStride;
            const auto x1 = juce::jmin (2 * x + 1, sw - 1) * in.pixelStride;
            for (int c = 0; c < 4; ++c)
            {
                const int sum = row0[x0 + c] + row0[x1 + c] + row1[x0 + c] + row1[x1 + c];
                o[x * dst.pixelStride + c] = static_cast<juce::uint8> ((sum + 2) >> 2);
            }
        }
    }
    return out;
}

const juce::Image& pick (const juce::Graphics& g, const Skin::Layer& layer, float destWidth)
{
    const auto physical = destWidth * g.getInternalContext().getPhysicalPixelScaleFactor();
    return layer.half.isValid() && physical <= static_cast<float> (layer.half.getWidth()) * 1.05f ? layer.half : layer.full;
}

juce::AffineTransform fit (const juce::Image& image, juce::Rectangle<float> dest)
{
    return juce::AffineTransform::scale (dest.getWidth() / static_cast<float> (image.getWidth()), dest.getHeight() / static_cast<float> (image.getHeight()))
        .translated (dest.getX(), dest.getY());
}
} // namespace

Skin::Skin()
{
    ++instances;
}

Skin::~Skin()
{
    --instances;
}

int Skin::liveInstances() noexcept
{
    return instances.load();
}

juce::StringArray Skin::delayLayerNames()
{
    return {
        "delay_adv_enclosure", "delay_adv_window_glass", "delay_adv_selected_glow", "delay_main_enclosure", "delay_main_window_glass",
        "pedal_knob_shadow", "pedal_knob_body", "pedal_knob_gloss", "footswitch_up", "footswitch_down",
        "led_amber_off", "led_amber_on", "panel_background", "display_frame", "display_glass",
        "panel_knob_shadow", "panel_knob_skirt", "panel_knob_cap", "panel_knob_pointer", "art_delay"
    };
}

juce::String Skin::fileNameFor (const juce::String& layer)
{
    return layer + (layer == "panel_background" ? "@2x.jpg" : "@2x.png");
}

bool Skin::hasDelay()
{
    if (! delayComplete.has_value())
    {
        bool all = true;
        for (const auto& name : delayLayerNames())
        {
            int size = 0;
            all = all && findResource (fileNameFor (name), size) != nullptr && size > 0;
        }
        delayComplete = all;
    }
    return *delayComplete;
}

const Skin::Layer& Skin::get (const juce::String& layer)
{
    if (auto it = cache.find (layer); it != cache.end())
        return it->second;

    Layer result;
    int size = 0;
    if (const auto* data = findResource (fileNameFor (layer), size); data != nullptr && size > 0)
        result.full = juce::ImageFileFormat::loadFrom (data, static_cast<size_t> (size));
    if (result.full.isValid())
        result.half = halfSize (result.full);
    return cache.emplace (layer, std::move (result)).first->second;
}

void Skin::draw (juce::Graphics& g, const Layer& layer, juce::Rectangle<float> dest, float opacity)
{
    if (! layer.isValid() || dest.isEmpty() || opacity <= 0.0f)
        return;
    const auto& image = pick (g, layer, dest.getWidth());
    juce::Graphics::ScopedSaveState save (g);
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.setOpacity (juce::jmin (1.0f, opacity));
    g.drawImageTransformed (image, fit (image, dest));
}

void Skin::drawTinted (juce::Graphics& g, const Layer& layer, juce::Rectangle<float> dest, juce::Colour tint)
{
    if (! layer.isValid() || dest.isEmpty() || tint.getAlpha() == 0)
        return;
    const auto& image = pick (g, layer, dest.getWidth());
    juce::Graphics::ScopedSaveState save (g);
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.setColour (tint);
    g.drawImageTransformed (image, fit (image, dest), true);
}

void Skin::drawRotated (juce::Graphics& g, const Layer& layer, juce::Point<float> centre, float size, float angle, float opacity)
{
    if (! layer.isValid() || size <= 0.0f || opacity <= 0.0f)
        return;
    const auto& image = pick (g, layer, size);
    const auto w = static_cast<float> (image.getWidth()), h = static_cast<float> (image.getHeight());
    juce::Graphics::ScopedSaveState save (g);
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.setOpacity (juce::jmin (1.0f, opacity));
    g.drawImageTransformed (image, juce::AffineTransform::translation (-w * 0.5f, -h * 0.5f)
                                       .scaled (size / w, size / h)
                                       .rotated (angle)
                                       .translated (centre.x, centre.y));
}
} // namespace sfx::ui
