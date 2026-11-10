#pragma once

#include "Parameters.h"

#include <juce_core/juce_core.h>

#include <array>
#include <atomic>
#include <cstdint>

namespace sfx
{
// Order of the eight effects, shared between UI (writer) and audio thread
// (reader) as a single lock-free packed word: 4 bits per slot.
class ChainOrder
{
public:
    using Order = std::array<int, numEffects>;

    static Order defaultOrder() noexcept
    {
        Order o {};
        for (int i = 0; i < numEffects; ++i)
            o[static_cast<std::size_t> (i)] = i;
        return o;
    }

    static bool isValid (const Order& o) noexcept
    {
        std::array<bool, numEffects> seen {};
        for (auto id : o)
        {
            if (id < 0 || id >= numEffects || seen[static_cast<std::size_t> (id)])
                return false;
            seen[static_cast<std::size_t> (id)] = true;
        }
        return true;
    }

    ChainOrder() noexcept { set (defaultOrder()); }

    Order get() const noexcept { return unpack (packed.load (std::memory_order_acquire)); }

    bool set (const Order& o) noexcept
    {
        if (! isValid (o))
            return false;
        packed.store (pack (o), std::memory_order_release);
        return true;
    }

    // Moves the effect at slot 'from' so it ends up at slot 'to'.
    void move (int from, int to) noexcept
    {
        auto o = get();
        from = std::clamp (from, 0, numEffects - 1);
        to = std::clamp (to, 0, numEffects - 1);
        const auto id = o[static_cast<std::size_t> (from)];
        if (from < to)
            for (int i = from; i < to; ++i)
                o[static_cast<std::size_t> (i)] = o[static_cast<std::size_t> (i + 1)];
        else
            for (int i = from; i > to; --i)
                o[static_cast<std::size_t> (i)] = o[static_cast<std::size_t> (i - 1)];
        o[static_cast<std::size_t> (to)] = id;
        set (o);
    }

    juce::String toString() const
    {
        juce::StringArray parts;
        for (auto id : get())
            parts.add (effectInfo[static_cast<std::size_t> (id)].key);
        return parts.joinIntoString (",");
    }

    bool fromString (const juce::String& text)
    {
        auto parts = juce::StringArray::fromTokens (text, ",", {});
        if (parts.size() != numEffects)
            return false;
        Order o {};
        for (int i = 0; i < numEffects; ++i)
        {
            int found = -1;
            for (int e = 0; e < numEffects; ++e)
                if (parts[i].trim() == effectInfo[static_cast<std::size_t> (e)].key)
                    found = e;
            o[static_cast<std::size_t> (i)] = found;
        }
        return set (o);
    }

private:
    static std::uint32_t pack (const Order& o) noexcept
    {
        std::uint32_t v = 0;
        for (int i = 0; i < numEffects; ++i)
            v |= static_cast<std::uint32_t> (o[static_cast<std::size_t> (i)]) << (4 * i);
        return v;
    }
    static Order unpack (std::uint32_t v) noexcept
    {
        Order o {};
        for (int i = 0; i < numEffects; ++i)
            o[static_cast<std::size_t> (i)] = static_cast<int> ((v >> (4 * i)) & 0xfu);
        return o;
    }

    std::atomic<std::uint32_t> packed { 0 };
};
} // namespace sfx
