#include "Textures.h"

#include <atomic>

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
} // namespace

namespace
{
std::atomic<int> instances { 0 };
} // namespace

Textures::Textures() : noise (makeNoise()), brushed (makeBrushed())
{
    ++instances;
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
