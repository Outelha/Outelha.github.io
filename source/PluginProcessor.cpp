#include "PluginProcessor.h"

#include "PluginEditor.h"

namespace trio
{
namespace
{
constexpr auto stateTag = "TrioFXState";

float bufferPeak (const juce::dsp::AudioBlock<float>& block, size_t channel) noexcept
{
    const auto* data = block.getChannelPointer (channel);
    float peak = 0.0f;
    for (size_t i = 0; i < block.getNumSamples(); ++i)
        peak = std::max (peak, std::abs (data[i]));
    return peak;
}
} // namespace

TrioAudioProcessor::TrioAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      state (*this, nullptr, stateTag, createParameterLayout()),
      params (state)
{
}

bool TrioAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void TrioAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    preparedChannels = std::max (1, std::min (2, getTotalNumOutputChannels()));
    maxChunkSize = std::max (1, samplesPerBlock);

    oversampling = std::make_unique<juce::dsp::Oversampling<float>> (
        static_cast<size_t> (preparedChannels),
        static_cast<size_t> (oversamplingFactorLog2),
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
        true,  // max quality
        true); // integer latency
    oversampling->initProcessing (static_cast<size_t> (maxChunkSize));

    const auto latency = static_cast<int> (std::lround (oversampling->getLatencyInSamples()));
    setLatencySamples (latency);

    const auto osRate = sampleRate * static_cast<double> (oversampling->getOversamplingFactor());
    saturation.prepare (osRate, preparedChannels);
    chorus.prepare (sampleRate, preparedChannels);
    reverb.prepare (sampleRate, preparedChannels);

    inputGain.reset (sampleRate, 0.02);
    outputGain.reset (sampleRate, 0.02);
    inputGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (params.inputGain.load()));
    outputGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (params.outputGain.load()));

    for (auto& d : bypassDelay)
        d.prepare (static_cast<size_t> (latency) + 1);

    updateDspParameters();
    saturation.reset();
    chorus.reset();
    reverb.reset();
}

void TrioAudioProcessor::releaseResources()
{
    if (oversampling)
        oversampling->reset();
}

void TrioAudioProcessor::reset()
{
    if (oversampling)
        oversampling->reset();
    saturation.reset();
    chorus.reset();
    reverb.reset();
    for (auto& d : bypassDelay)
        d.reset();
}

double TrioAudioProcessor::getTailLengthSeconds() const
{
    return static_cast<double> (dsp::Reverb::maxDecaySeconds + dsp::Reverb::maxPredelayMs * 0.001f);
}

void TrioAudioProcessor::updateDspParameters() noexcept
{
    dsp::Saturation::Parameters sp;
    sp.enabled = params.satEnabled.load() > 0.5f;
    sp.driveDb = params.satDrive.load();
    sp.tone = params.satTone.load();
    sp.mix = params.satMix.load();
    sp.mode = static_cast<dsp::Saturation::Mode> (std::clamp (juce::roundToInt (params.satMode.load()), 0, dsp::Saturation::numModes - 1));
    saturation.setParameters (sp);

    dsp::Chorus::Parameters cp;
    cp.enabled = params.chorusEnabled.load() > 0.5f;
    cp.rateHz = params.chorusRate.load();
    cp.depth = params.chorusDepth.load();
    cp.delayMs = params.chorusDelay.load();
    cp.mix = params.chorusMix.load();
    chorus.setParameters (cp);

    dsp::Reverb::Parameters rp;
    rp.enabled = params.reverbEnabled.load() > 0.5f;
    rp.size = params.reverbSize.load();
    rp.decaySeconds = params.reverbDecay.load();
    rp.damping = params.reverbDamping.load();
    rp.predelayMs = params.reverbPredelay.load();
    rp.width = params.reverbWidth.load();
    rp.mix = params.reverbMix.load();
    reverb.setParameters (rp);

    inputGain.setTargetValue (juce::Decibels::decibelsToGain (params.inputGain.load()));
    outputGain.setTargetValue (juce::Decibels::decibelsToGain (params.outputGain.load()));
}

void TrioAudioProcessor::processChunk (juce::dsp::AudioBlock<float> block) noexcept
{
    const auto numChannels = static_cast<int> (block.getNumChannels());
    const auto numSamples = static_cast<int> (block.getNumSamples());

    // Input gain.
    for (int i = 0; i < numSamples; ++i)
    {
        const auto g = inputGain.getNextValue();
        for (int ch = 0; ch < numChannels; ++ch)
            block.getChannelPointer (static_cast<size_t> (ch))[i] *= g;
    }
    for (size_t ch = 0; ch < block.getNumChannels(); ++ch)
        LevelMeters::update (meters.input[ch], bufferPeak (block, ch));

    // 1. Saturation (oversampled; always run through the filters so latency is constant).
    {
        auto up = oversampling->processSamplesUp (block);
        std::array<float*, 2> ptrs {};
        for (size_t ch = 0; ch < up.getNumChannels(); ++ch)
            ptrs[ch] = up.getChannelPointer (ch);
        saturation.process (ptrs.data(), static_cast<int> (up.getNumChannels()), static_cast<int> (up.getNumSamples()));
        oversampling->processSamplesDown (block);
    }

    std::array<float*, 2> ptrs {};
    for (int ch = 0; ch < numChannels; ++ch)
        ptrs[static_cast<size_t> (ch)] = block.getChannelPointer (static_cast<size_t> (ch));

    // 2. Chorus, 3. Reverb.
    chorus.process (ptrs.data(), numChannels, numSamples);
    reverb.process (ptrs.data(), numChannels, numSamples);

    // Output gain.
    for (int i = 0; i < numSamples; ++i)
    {
        const auto g = outputGain.getNextValue();
        for (int ch = 0; ch < numChannels; ++ch)
            ptrs[static_cast<size_t> (ch)][i] *= g;
    }
    for (size_t ch = 0; ch < block.getNumChannels(); ++ch)
        LevelMeters::update (meters.output[ch], bufferPeak (block, ch));
}

void TrioAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (auto i = getTotalNumInputChannels(); i < getTotalNumOutputChannels(); ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    updateDspParameters();

    const auto channels = std::min (buffer.getNumChannels(), preparedChannels);
    juce::dsp::AudioBlock<float> full (buffer.getArrayOfWritePointers(), static_cast<size_t> (channels), static_cast<size_t> (buffer.getNumSamples()));

    // Hosts may exceed the announced block size; process in prepared-size chunks.
    for (size_t start = 0; start < full.getNumSamples(); start += static_cast<size_t> (maxChunkSize))
    {
        const auto len = std::min (static_cast<size_t> (maxChunkSize), full.getNumSamples() - start);
        processChunk (full.getSubBlock (start, len));
    }
}

void TrioAudioProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    // Delay the dry signal by the reported latency so bypassing does not shift timing.
    const auto latency = static_cast<size_t> (getLatencySamples());
    const auto channels = std::min (buffer.getNumChannels(), 2);
    for (int ch = 0; ch < channels; ++ch)
    {
        auto& line = bypassDelay[static_cast<size_t> (ch)];
        auto* data = buffer.getWritePointer (ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            line.push (data[i]);
            data[i] = line.readInteger (latency);
        }
    }
}

juce::AudioProcessorEditor* TrioAudioProcessor::createEditor()
{
    return new TrioAudioProcessorEditor (*this);
}

void TrioAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto copy = state.copyState();
    copy.setProperty ("pluginVersion", JucePlugin_VersionString, nullptr);
    if (auto xml = copy.createXml())
        copyXmlToBinary (*xml, destData);
}

void TrioAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (state.state.getType()))
            state.replaceState (juce::ValueTree::fromXml (*xml));
}
} // namespace trio

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new trio::TrioAudioProcessor();
}
