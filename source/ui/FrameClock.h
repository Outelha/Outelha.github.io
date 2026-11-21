#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace sfx::ui
{
// One animation clock per editor, driven by the display's vertical blank
// (juce::VBlankAttachment) and capped at 60 frames per second.
//
// Components subscribe instead of running their own timers, so every animated element
// advances in the same frame and nothing animates while the editor is hidden: VBlank
// callbacks only arrive while the editor is on screen, and the attachment (and with it
// every callback) is destroyed with the editor, before its children.
//
// Subscribers are asked for their cost each frame: they return false from frame() when
// nothing moved, which lets the clock drop to a low idle rate.
class FrameClock
{
public:
    struct Client
    {
        virtual ~Client() = default;
        // Called on the message thread. `now` in seconds, `dt` since the previous call.
        // Return true while the client is animating.
        virtual bool frame (double now, double dt) = 0;
    };

    explicit FrameClock (juce::Component& host)
        : attachment (&host, [this] (double t) { onVBlank (t); })
    {
    }

    void add (Client& c) { clients.addIfNotAlreadyThere (&c); }
    void remove (Client& c) { clients.removeFirstMatchingValue (&c); }

    double now() const noexcept { return lastTime; }
    int framesRendered() const noexcept { return frames; }

    // Drive the clock manually (tests, or hosts without VBlank).
    void tick (double timeSeconds) { onVBlank (timeSeconds); }

private:
    void onVBlank (double t)
    {
        const auto minInterval = busy ? 1.0 / 60.0 : 1.0 / 15.0;
        if (lastTime > 0.0 && t - lastTime < minInterval * 0.92)
            return;
        const auto dt = lastTime > 0.0 ? juce::jlimit (0.0, 0.1, t - lastTime) : 1.0 / 60.0;
        lastTime = t;
        ++frames;
        bool any = false;
        for (int i = clients.size(); --i >= 0;)
            if (auto* c = clients[i])
                any = c->frame (t, dt) || any;
        busy = any;
    }

    juce::Array<Client*> clients;
    double lastTime = 0.0;
    int frames = 0;
    bool busy = true;
    juce::VBlankAttachment attachment;
};
// Visible in the editor's hierarchy. (isShowing() also needs a native window, which a
// headless snapshot render does not have; VBlank callbacks already stop when hidden.)
inline bool visibleInEditor (const juce::Component& c)
{
    // The top-level editor itself is made visible by the host window; only its children count.
    for (auto* p = &c; p != nullptr && p->getParentComponent() != nullptr; p = p->getParentComponent())
        if (! p->isVisible())
            return false;
    return true;
}
} // namespace sfx::ui
