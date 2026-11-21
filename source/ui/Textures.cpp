#include "Textures.h"

#include <ShobeatsAssets.h>

#include <atomic>
#include <cmath>

namespace sfx::ui
{
namespace
{
juce::Image makeNoise()
{
    juce::Image img (juce::Image::ARGB, 128, 128, true);
    juce::Random rng (0x5b0b);
    for (int y = 0; y < img.getHeight(); ++y)
        for (int x = 0; x < img.getWidth(); ++x)
        {
            const auto v = rng.nextFloat();
            const auto c = v > 0.5f ? juce::Colours::white : juce::Colours::black;
            img.setPixelAt (x, y, c.withAlpha (std::abs (v - 0.5f) * 0.5f));
        }
    return img;
}

juce::Image makeBrushed()
{
    constexpr int w = 512, h = 512, blend = 96;
    juce::Image img (juce::Image::ARGB, w, h, true);
    juce::Random rng (0xb2b5);
    std::vector<float> row (static_cast<size_t> (w));
    for (int y = 0; y < h; ++y)
    {
        // Each row: a slowly varying brightness (the streak) plus fine grain.
        float walk = rng.nextFloat() * 2.0f - 1.0f;
        for (int x = 0; x < w; ++x)
        {
            walk = walk * 0.985f + (rng.nextFloat() * 2.0f - 1.0f) * 0.06f;
            row[static_cast<size_t> (x)] = walk + (rng.nextFloat() - 0.5f) * 0.25f;
        }
        // Crossfade the row's end into its start so the tile wraps without a seam.
        for (int x = 0; x < blend; ++x)
        {
            const auto t = static_cast<float> (x) / blend;
            auto& end = row[static_cast<size_t> (w - blend + x)];
            end = end * (1.0f - t) + row[static_cast<size_t> (x)] * t;
        }
        for (int x = 0; x < w; ++x)
        {
            const auto v = row[static_cast<size_t> (x)];
            img.setPixelAt (x, y, (v > 0.0f ? juce::Colours::white : juce::Colours::black).withAlpha (juce::jlimit (0.0f, 1.0f, std::abs (v) * 0.3f)));
        }
    }
    return img;
}
juce::Image makeConic()
{
    constexpr int size = 512;
    constexpr int bins = 2048;
    juce::Image img (juce::Image::ARGB, size, size, true);
    juce::Random rng (0xc0c1);

    // Fine angular streaks: smoothed random walk around the circle (wraps seamlessly).
    std::vector<float> streak (bins);
    float walk = 0.0f;
    for (auto& v : streak)
    {
        walk = walk * 0.97f + (rng.nextFloat() - 0.5f) * 0.035f;
        v = walk + (rng.nextFloat() - 0.5f) * 0.03f;
    }
    for (int i = 0; i < 64; ++i)
    {
        const auto t = static_cast<float> (i) / 64.0f;
        auto& end = streak[static_cast<size_t> (bins - 64 + i)];
        end = end * (1.0f - t) + streak[static_cast<size_t> (i)] * t;
    }

    const auto c = (size - 1) * 0.5f;
    const auto radius = size * 0.5f - 1.0f;
    juce::Image::BitmapData data (img, juce::Image::BitmapData::writeOnly);
    for (int y = 0; y < size; ++y)
        for (int x = 0; x < size; ++x)
        {
            const auto dx = static_cast<float> (x) - c, dy = static_cast<float> (y) - c;
            const auto r = std::sqrt (dx * dx + dy * dy);
            if (r > radius + 1.0f)
                continue;
            const auto theta = std::atan2 (dy, dx);
            const auto bin = static_cast<size_t> ((theta + juce::MathConstants<float>::pi) / juce::MathConstants<float>::twoPi * (bins - 1));
            // Light from the upper left: two broad highlights opposite each other, as on spun metal.
            auto b = 0.52f + 0.27f * std::cos (2.0f * (theta + 2.25f)) + 0.05f * std::cos (4.0f * theta + 0.7f);
            b += streak[bin] * (0.4f + 0.6f * r / radius);
            b += 0.012f * std::sin (r * 0.9f); // machining rings
            b = juce::jlimit (0.0f, 1.0f, b);
            const auto alpha = juce::jlimit (0.0f, 1.0f, radius + 1.0f - r);
            data.setPixelColour (x, y, juce::Colour::fromFloatRGBA (b * 0.97f, b * 0.98f, b, alpha));
        }
    return img;
}

// Height field made of soft random dents, shaded by its slope (light from the upper left).
juce::Image makeHammered()
{
    constexpr int size = 256;
    std::vector<float> h (size * size, 0.0f);
    juce::Random rng (0x4a33);
    for (int i = 0; i < 1400; ++i)
    {
        const auto cx = rng.nextFloat() * size, cy = rng.nextFloat() * size;
        const auto rad = 1.5f + rng.nextFloat() * 4.0f;
        const auto depth = 0.3f + rng.nextFloat() * 0.4f;
        const auto reach = static_cast<int> (rad * 2.5f);
        for (int oy = -reach; oy <= reach; ++oy)
            for (int ox = -reach; ox <= reach; ++ox)
            {
                const auto px = (static_cast<int> (cx) + ox + size) % size, py = (static_cast<int> (cy) + oy + size) % size;
                const auto d2 = static_cast<float> (ox * ox + oy * oy) / (rad * rad);
                h[static_cast<size_t> (py * size + px)] -= depth * std::exp (-d2);
            }
    }
    juce::Image img (juce::Image::ARGB, size, size, true);
    juce::Image::BitmapData data (img, juce::Image::BitmapData::writeOnly);
    auto at = [&] (int x, int y) { return h[static_cast<size_t> (((y + size) % size) * size + (x + size) % size)]; };
    for (int y = 0; y < size; ++y)
        for (int x = 0; x < size; ++x)
        {
            const auto slope = (at (x + 1, y + 1) - at (x - 1, y - 1)) * 1.4f + (rng.nextFloat() - 0.5f) * 0.08f;
            const auto a = juce::jlimit (0.0f, 0.6f, std::abs (slope) * 0.45f);
            data.setPixelColour (x, y, (slope > 0.0f ? juce::Colours::white : juce::Colours::black).withAlpha (a));
        }
    return img;
}

juce::Image makeSparkle()
{
    constexpr int size = 256;
    juce::Image img (juce::Image::ARGB, size, size, true);
    juce::Random rng (0x5a4c);
    for (int i = 0; i < 900; ++i)
    {
        const auto x = rng.nextInt (size), y = rng.nextInt (size);
        const auto bright = rng.nextFloat();
        img.setPixelAt (x, y, juce::Colours::white.withAlpha (0.15f + 0.6f * bright * bright));
        if (bright > 0.93f)
            for (auto [ox, oy] : { std::pair { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } })
                img.setPixelAt ((x + ox + size) % size, (y + oy + size) % size, juce::Colours::white.withAlpha (0.25f));
    }
    return img;
}
} // namespace

namespace
{
std::atomic<int> instances { 0 };
} // namespace

Textures::Textures()
    : noise (makeNoise()), brushed (makeBrushed()), conic (makeConic()), hammered (makeHammered()), sparkle (makeSparkle())
{
    ++instances;
}

juce::Image Textures::artwork (const juce::String& name)
{
    if (auto it = artworkCache.find (name); it != artworkCache.end())
        return it->second;

    juce::Image image;
    const auto resource = name.replaceCharacter ('-', '_').replaceCharacter ('.', '_');
    int size = 0;
    if (const auto* data = ShobeatsAssets::getNamedResource (resource.toRawUTF8(), size); data != nullptr && size > 0)
        image = juce::ImageFileFormat::loadFrom (data, static_cast<size_t> (size));
    artworkCache[name] = image;
    return image;
}

Textures::~Textures()
{
    --instances;
}

int Textures::liveInstances() noexcept
{
    return instances.load();
}
} // namespace sfx::ui
