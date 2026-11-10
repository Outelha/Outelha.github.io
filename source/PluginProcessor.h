#pragma once

#include "Parameters.h"
#include "dsp/Chorus.h"
#include "dsp/Reverb.h"
#include "dsp/Saturation.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include <array>

namespace trio
{
// Peak meters written by the audio thread, read (and cleared) by the UI.
struct LevelMeters
{
    std::array<std::atomic<float>, 2> input {}, output {};

    static void update (std::atomic<float>& slot, float peak) noexcept
    {
        auto prev = slot.load (std::memory_order_relaxed);
        while (peak > prev && ! slot.compare_exchange_weak (prev, peak, std::memory_order_relaxed)) {}
    }
    static float take (std::atomic<float>& slot) noexcept { return slot.exchange (0.0f, std::memory_order_relaxed); }
};

class TrioAudioProcessor final : public juce::AudioProcessor
{
public:
    // Saturation runs at 4x to keep aliasing out of the audible band.
    static constexpr int oversamplingFactorLog2 = 2;

    TrioAudioProcessor();
    ~TrioAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void reset() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;
    using AudioProcessor::processBlockBypassed;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override;

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getState() noexcept { return state; }
    LevelMeters& getMeters() noexcept { return meters; }

private:
    void updateDspParameters() noexcept;
    void processChunk (juce::dsp::AudioBlock<float> block) noexcept;

    juce::AudioProcessorValueTreeState state;
    ParameterRefs params;

    dsp::Saturation saturation;
    dsp::Chorus chorus;
    dsp::Reverb reverb;

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    int maxChunkSize = 512;
    int preparedChannels = 0;

    juce::SmoothedValue<float> inputGain, outputGain;

    // Latency-matching delay used while the host bypasses the plugin.
    std::array<dsp::DelayLine, 2> bypassDelay;

    LevelMeters meters;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrioAudioProcessor)
};
} // namespace trio
