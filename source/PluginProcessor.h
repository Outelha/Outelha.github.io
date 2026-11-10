#pragma once

#include "ChainOrder.h"
#include "Parameters.h"
#include "presets/PresetManager.h"
#include "dsp/Chorus.h"
#include "dsp/Delay.h"
#include "dsp/Resampler.h"
#include "dsp/Reverb.h"
#include "dsp/Reverse.h"
#include "dsp/Saturation.h"
#include "dsp/Tape.h"
#include "dsp/Tremolo.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include <array>

namespace sfx
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

class ShobeatsAudioProcessor final : public juce::AudioProcessor
{
public:
    // Saturation runs at 4x to keep aliasing out of the audible band.
    static constexpr int oversamplingFactorLog2 = 2;
    static constexpr double defaultBpm = 120.0;

    ShobeatsAudioProcessor();
    ~ShobeatsAudioProcessor() override = default;

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
    ChainOrder& getChainOrder() noexcept { return chainOrder; }
    LevelMeters& getMeters() noexcept { return meters; }
    double getCurrentBpm() const noexcept { return currentBpm.load (std::memory_order_relaxed); }

    // For tests and hosts without a playhead.
    void setFallbackBpm (double bpm) noexcept { fallbackBpm = bpm; }

    PresetManager& getPresetManager() noexcept { return *presetManager; }

    // Requests a short fade-out -> (optional tail reset) -> fade-in on the audio thread.
    // Called before applying a preset so switching while audio plays is click- and spike-free.
    void requestTransition (bool resetTails) noexcept
    {
        if (resetTails)
            resetRequested.store (true);
        transitionRequested.store (true);
    }

    void setCurrentPresetLabel (const juce::String& name, const juce::String& category);
    std::pair<juce::String, juce::String> getCurrentPresetLabel() const;

    static constexpr double fadeOutSeconds = 0.008, fadeInSeconds = 0.04;

private:
    void updateDspParameters (double bpm) noexcept;
    void processChunk (juce::dsp::AudioBlock<float> block) noexcept;
    void processEffect (EffectId id, juce::dsp::AudioBlock<float>& block) noexcept;
    double readHostBpm() const;

    juce::AudioProcessorValueTreeState state;
    ParameterReader params;
    ChainOrder chainOrder;

    dsp::Saturation saturation;
    dsp::Resampler resampler;
    dsp::Tape tape;
    dsp::Chorus chorus;
    dsp::Tremolo tremolo;
    dsp::Reverse reverse;
    dsp::Delay delay;
    dsp::Reverb reverb;

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    int maxChunkSize = 512;
    int preparedChannels = 0;

    juce::SmoothedValue<float> inputGain, outputGain, globalMix;

    // Dry path for the global mix, delayed to match the oversampling latency.
    std::array<dsp::DelayLine, 2> dryDelay;
    juce::AudioBuffer<float> dryBuffer;

    // Latency-matching delay used while the host bypasses the plugin.
    std::array<dsp::DelayLine, 2> bypassDelay;

    LevelMeters meters;

    // Click-free transitions (preset loads, chain reordering).
    enum class Fade
    {
        none,
        out,
        in
    };
    void resetEffects() noexcept;
    Fade fade = Fade::none;
    int fadeRemaining = 0, fadeOutLength = 1, fadeInLength = 1;
    std::atomic<bool> transitionRequested { false }, resetRequested { false };
    bool resetAtSwitch = false;
    ChainOrder::Order activeOrder = ChainOrder::defaultOrder();

    std::unique_ptr<PresetManager> presetManager;
    juce::SpinLock labelLock;
    juce::String presetName, presetCategory;

    std::atomic<double> currentBpm { defaultBpm };
    double fallbackBpm = defaultBpm;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ShobeatsAudioProcessor)
};
} // namespace sfx
