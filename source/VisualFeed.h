#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>

namespace sfx
{
// Audio -> editor data for audio-reactive visuals. Real-time safe by construction:
//  - one writer (the audio thread), any number of readers (editors on the message thread);
//  - fixed-size storage, no allocation, no locks, relaxed atomics only;
//  - a reader can never block or slow the audio thread. A reader racing with the writer may
//    see one segment from the newer pass of the ring, which is harmless for drawing.
//
// The output signal is reduced to a history of peak values, one per short segment
// (~2.9 ms), which is what the main display's waveform and the pedal meters draw.
class VisualFeed
{
public:
    static constexpr std::uint32_t historySize = 1024; // power of two (~3 s of history)

    // Called from prepareToPlay (never concurrently with push()).
    void prepare (double sampleRate) noexcept
    {
        segmentLength = std::max (16, static_cast<int> (sampleRate / 350.0));
        segmentSeconds.store (static_cast<float> (segmentLength / sampleRate), std::memory_order_relaxed);
        counter = 0;
        accumulator = 0.0f;
    }

    // Audio thread: feed processed output samples.
    void push (const float* const* channels, int numChannels, int numSamples) noexcept
    {
        for (int i = 0; i < numSamples; ++i)
        {
            float peak = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
                peak = std::max (peak, std::abs (channels[ch][i]));
            accumulator = std::max (accumulator, peak);
            if (++counter >= segmentLength)
            {
                const auto index = written.load (std::memory_order_relaxed);
                peaks[index & (historySize - 1)].store (std::isfinite (accumulator) ? accumulator : 0.0f, std::memory_order_relaxed);
                written.store (index + 1, std::memory_order_release);
                counter = 0;
                accumulator = 0.0f;
            }
        }
        processed.fetch_add (static_cast<std::uint64_t> (numSamples), std::memory_order_relaxed);
    }

    // Any thread: copies the newest `count` segment peaks, oldest first. Returns the total
    // number of segments written so far (use it to tell whether anything new arrived).
    std::uint32_t read (float* destination, std::uint32_t count) const noexcept
    {
        count = std::min (count, historySize);
        const auto end = written.load (std::memory_order_acquire);
        for (std::uint32_t i = 0; i < count; ++i)
        {
            const auto age = count - i; // 1 = newest
            destination[i] = age <= end ? peaks[(end - age) & (historySize - 1)].load (std::memory_order_relaxed) : 0.0f;
        }
        return end;
    }

    // Samples the plugin has processed (not counting host bypass). Stops growing when the
    // host stops calling process(), which is how animations know audio is running.
    std::uint64_t samplesProcessed() const noexcept { return processed.load (std::memory_order_relaxed); }
    float getSegmentSeconds() const noexcept { return segmentSeconds.load (std::memory_order_relaxed); }

private:
    std::array<std::atomic<float>, historySize> peaks {};
    std::atomic<std::uint32_t> written { 0 };
    std::atomic<std::uint64_t> processed { 0 };
    std::atomic<float> segmentSeconds { 128.0f / 44100.0f };
    int segmentLength = 128, counter = 0;
    float accumulator = 0.0f;
};
} // namespace sfx
