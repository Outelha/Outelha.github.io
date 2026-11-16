#pragma once

#include <juce_graphics/juce_graphics.h>

namespace sfx::ui
{
// Texture images used while painting.
//
// Lifetime rule: these images must never live in static storage. On Windows, JUCE 8
// creates them as Direct2D images that hold COM/DirectX resources and a reference to
// JUCE's DirectX singleton. A function-level static would only be destroyed when the
// plugin DLL is unloaded - after the last plugin instance has shut JUCE down - and
// releasing DirectX objects at that point is unsafe.
//
// Instead, every editor's look-and-feel holds a juce::SharedResourcePointer<Textures>:
// the images are created on the message thread with the first editor and released when
// the last editor closes, while JUCE is fully alive.
struct Textures
{
    Textures();
    ~Textures();

    // Number of Textures objects currently alive (0 or 1); used by lifecycle tests.
    static int liveInstances() noexcept;

    juce::Image noise;   // fine grain, 128 x 128
    juce::Image brushed; // horizontal brushed-metal streaks, 512 x 512, seamless
};
} // namespace sfx::ui
