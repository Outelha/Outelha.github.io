#include "PluginProcessor.h"

#include "PluginEditor.h"

namespace sfx
{
namespace
{
constexpr auto stateTag = "ShobeatsFXState";
constexpr auto chainOrderProperty = "chainOrder";

float blockPeak (const juce::dsp::AudioBlock<float>& block, size_t channel) noexcept
{
    const auto* data = block.getChannelPointer (channel);
    float peak = 0.0f;
    for (size_t i = 0; i < block.getNumSamples(); ++i)
        peak = std::max (peak, std::abs (data[i]));
    return peak;
}

std::array<float*, 2> pointersOf (juce::dsp::AudioBlock<float>& block) noexcept
{
    std::array<float*, 2> p {};
    for (size_t ch = 0; ch < std::min<size_t> (2, block.getNumChannels()); ++ch)
        p[ch] = block.getChannelPointer (ch);
    return p;
}
} // namespace

ShobeatsAudioProcessor::ShobeatsAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      state (*this, nullptr, stateTag, createParameterLayout()),
      params (state)
{
}

bool ShobeatsAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void ShobeatsAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    preparedChannels = std::max (1, std::min (2, getTotalNumOutputChannels()));
    maxChunkSize = std::max (1, samplesPerBlock);

    oversampling = std::make_unique<juce::dsp::Oversampling<float>> (
        static_cast<size_t> (preparedChannels), static_cast<size_t> (oversamplingFactorLog2),
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true);
    oversampling->initProcessing (static_cast<size_t> (maxChunkSize));

    const auto latency = static_cast<int> (std::lround (oversampling->getLatencyInSamples()));
    setLatencySamples (latency);

    const auto osRate = sampleRate * static_cast<double> (oversampling->getOversamplingFactor());
    saturation.prepare (osRate, preparedChannels);
    resampler.prepare (sampleRate, preparedChannels);
    tape.prepare (sampleRate, preparedChannels);
    chorus.prepare (sampleRate, preparedChannels);
    tremolo.prepare (sampleRate, preparedChannels);
    reverse.prepare (sampleRate, preparedChannels);
    delay.prepare (sampleRate, preparedChannels);
    reverb.prepare (sampleRate, preparedChannels);

    for (auto* g : { &inputGain, &outputGain, &globalMix })
        g->reset (sampleRate, 0.02);
    inputGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (params.get (ParamID::inputGain)));
    outputGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (params.get (ParamID::outputGain)));
    globalMix.setCurrentAndTargetValue (params.get (ParamID::globalMix));

    dryBuffer.setSize (2, maxChunkSize, false, true, false);
    for (auto& d : dryDelay)
        d.prepare (static_cast<size_t> (latency) + 1);
    for (auto& d : bypassDelay)
        d.prepare (static_cast<size_t> (latency) + 1);

    updateDspParameters (fallbackBpm);
    reset();
}

void ShobeatsAudioProcessor::releaseResources()
{
    if (oversampling)
        oversampling->reset();
}

void ShobeatsAudioProcessor::reset()
{
    if (oversampling)
        oversampling->reset();
    saturation.reset();
    resampler.reset();
    tape.reset();
    chorus.reset();
    tremolo.reset();
    reverse.reset();
    delay.reset();
    reverb.reset();
    for (auto& d : dryDelay)
        d.reset();
    for (auto& d : bypassDelay)
        d.reset();
}

double ShobeatsAudioProcessor::getTailLengthSeconds() const
{
    // Longest reverb plus a long feedback delay tail.
    return static_cast<double> (dsp::Reverb::maxDecaySeconds + dsp::Reverb::maxPredelayMs * 0.001f) + 10.0;
}

double ShobeatsAudioProcessor::readHostBpm() const
{
    if (auto* head = getPlayHead())
        if (auto position = head->getPosition())
            if (auto bpm = position->getBpm())
                if (*bpm > 0.0)
                    return *bpm;
    return fallbackBpm;
}

void ShobeatsAudioProcessor::updateDspParameters (double bpm) noexcept
{
    const auto& p = params;

    const dsp::Movement movement { p.get (ParamID::drift), p.get (ParamID::unstable) };
    saturation.setMovement (movement);
    resampler.setMovement (movement);
    tape.setMovement (movement);
    chorus.setMovement (movement);
    tremolo.setMovement (movement);
    reverse.setMovement (movement);
    delay.setMovement (movement);
    reverb.setMovement (movement);

    dsp::Saturation::Parameters sp;
    sp.enabled = p.on (ParamID::satEnabled);
    sp.driveDb = p.get (ParamID::satDrive);
    sp.tone = p.get (ParamID::satTone);
    sp.mode = static_cast<dsp::Saturation::Mode> (std::clamp (p.choice (ParamID::satMode), 0, dsp::Saturation::numModes - 1));
    sp.outputDb = p.get (ParamID::satOutput);
    sp.mix = p.get (ParamID::satMix);
    saturation.setParameters (sp);

    dsp::Resampler::Parameters rp;
    rp.enabled = p.on (ParamID::rsEnabled);
    rp.rateHz = p.get (ParamID::rsRate);
    rp.bits = p.get (ParamID::rsBits);
    rp.filterHz = p.get (ParamID::rsFilter);
    rp.jitter = p.get (ParamID::rsJitter);
    rp.mix = p.get (ParamID::rsMix);
    resampler.setParameters (rp);

    dsp::Tape::Parameters tp;
    tp.enabled = p.on (ParamID::tapeEnabled);
    tp.driveDb = p.get (ParamID::tapeDrive);
    tp.wow = p.get (ParamID::tapeWow);
    tp.flutter = p.get (ParamID::tapeFlutter);
    tp.noise = p.get (ParamID::tapeNoise);
    tp.age = p.get (ParamID::tapeAge);
    tp.mix = p.get (ParamID::tapeMix);
    tape.setParameters (tp);

    dsp::Chorus::Parameters cp;
    cp.enabled = p.on (ParamID::chorusEnabled);
    cp.rateHz = p.get (ParamID::chorusRate);
    cp.depth = p.get (ParamID::chorusDepth);
    cp.delayMs = p.get (ParamID::chorusDelay);
    cp.feedback = p.get (ParamID::chorusFeedback);
    cp.width = p.get (ParamID::chorusWidth);
    cp.mix = p.get (ParamID::chorusMix);
    chorus.setParameters (cp);

    dsp::Tremolo::Parameters trp;
    trp.enabled = p.on (ParamID::tremEnabled);
    trp.rateHz = p.get (ParamID::tremRate);
    trp.sync = p.on (ParamID::tremSync);
    trp.division = p.choice (ParamID::tremDivision);
    trp.depth = p.get (ParamID::tremDepth);
    trp.shape = static_cast<dsp::Tremolo::Shape> (std::clamp (p.choice (ParamID::tremShape), 0, dsp::Tremolo::numShapes - 1));
    trp.stereo = p.get (ParamID::tremStereo);
    tremolo.setParameters (trp, bpm);

    dsp::Reverse::Parameters rvp;
    rvp.enabled = p.on (ParamID::revEnabled);
    rvp.timeMs = p.get (ParamID::revTime);
    rvp.sync = p.on (ParamID::revSync);
    rvp.division = p.choice (ParamID::revDivision);
    rvp.pitchSemitones = p.get (ParamID::revPitch);
    rvp.mix = p.get (ParamID::revMix);
    reverse.setParameters (rvp, bpm);

    dsp::Delay::Parameters dp;
    dp.enabled = p.on (ParamID::delayEnabled);
    dp.timeMs = p.get (ParamID::delayTime);
    dp.sync = p.on (ParamID::delaySync);
    dp.division = p.choice (ParamID::delayDivision);
    dp.feedback = p.get (ParamID::delayFeedback);
    dp.lowCutHz = p.get (ParamID::delayLowCut);
    dp.highCutHz = p.get (ParamID::delayHighCut);
    dp.pingPong = p.on (ParamID::delayPingPong);
    dp.width = p.get (ParamID::delayWidth);
    dp.mix = p.get (ParamID::delayMix);
    delay.setParameters (dp, bpm);

    dsp::Reverb::Parameters rbp;
    rbp.enabled = p.on (ParamID::reverbEnabled);
    rbp.size = p.get (ParamID::reverbSize);
    rbp.decaySeconds = p.get (ParamID::reverbDecay);
    rbp.damping = p.get (ParamID::reverbDamping);
    rbp.predelayMs = p.get (ParamID::reverbPredelay);
    rbp.width = p.get (ParamID::reverbWidth);
    rbp.mix = p.get (ParamID::reverbMix);
    reverb.setParameters (rbp);

    inputGain.setTargetValue (juce::Decibels::decibelsToGain (p.get (ParamID::inputGain)));
    outputGain.setTargetValue (juce::Decibels::decibelsToGain (p.get (ParamID::outputGain)));
    globalMix.setTargetValue (p.get (ParamID::globalMix));
}

void ShobeatsAudioProcessor::processEffect (EffectId id, juce::dsp::AudioBlock<float>& block) noexcept
{
    const auto numChannels = static_cast<int> (block.getNumChannels());
    const auto numSamples = static_cast<int> (block.getNumSamples());
    auto ptrs = pointersOf (block);

    switch (id)
    {
        case EffectId::saturation:
        {
            auto up = oversampling->processSamplesUp (block);
            auto upPtrs = pointersOf (up);
            saturation.process (upPtrs.data(), static_cast<int> (up.getNumChannels()), static_cast<int> (up.getNumSamples()));
            oversampling->processSamplesDown (block);
            break;
        }
        case EffectId::resampler: resampler.process (ptrs.data(), numChannels, numSamples); break;
        case EffectId::tape: tape.process (ptrs.data(), numChannels, numSamples); break;
        case EffectId::chorus: chorus.process (ptrs.data(), numChannels, numSamples); break;
        case EffectId::tremolo: tremolo.process (ptrs.data(), numChannels, numSamples); break;
        case EffectId::reverse: reverse.process (ptrs.data(), numChannels, numSamples); break;
        case EffectId::delay: delay.process (ptrs.data(), numChannels, numSamples); break;
        case EffectId::reverb: reverb.process (ptrs.data(), numChannels, numSamples); break;
    }
}

void ShobeatsAudioProcessor::processChunk (juce::dsp::AudioBlock<float> block) noexcept
{
    const auto numChannels = static_cast<int> (block.getNumChannels());
    const auto numSamples = static_cast<int> (block.getNumSamples());

    // Input gain + input meter.
    for (int i = 0; i < numSamples; ++i)
    {
        const auto g = inputGain.getNextValue();
        for (int ch = 0; ch < numChannels; ++ch)
            block.getChannelPointer (static_cast<size_t> (ch))[i] *= g;
    }
    for (size_t ch = 0; ch < block.getNumChannels(); ++ch)
        LevelMeters::update (meters.input[ch], blockPeak (block, ch));

    // Dry copy for the global mix, aligned with the (constant) oversampling latency.
    const auto latency = static_cast<size_t> (getLatencySamples());
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto& line = dryDelay[static_cast<size_t> (ch)];
        const auto* in = block.getChannelPointer (static_cast<size_t> (ch));
        auto* dry = dryBuffer.getWritePointer (ch);
        for (int i = 0; i < numSamples; ++i)
        {
            line.push (in[i]);
            dry[i] = line.readInteger (latency);
        }
    }

    // The effect chain, in the user's order. Saturation always runs through the
    // oversampler (even when off) so the reported latency never changes.
    for (auto id : chainOrder.get())
        processEffect (static_cast<EffectId> (id), block);

    // Global mix and output gain.
    for (int i = 0; i < numSamples; ++i)
    {
        const auto m = globalMix.getNextValue();
        const auto g = outputGain.getNextValue();
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* wet = block.getChannelPointer (static_cast<size_t> (ch));
            const auto dry = dryBuffer.getSample (ch, i);
            wet[i] = (dry + m * (wet[i] - dry)) * g;
        }
    }
    for (size_t ch = 0; ch < block.getNumChannels(); ++ch)
        LevelMeters::update (meters.output[ch], blockPeak (block, ch));
}

void ShobeatsAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (auto i = getTotalNumInputChannels(); i < getTotalNumOutputChannels(); ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    const auto bpm = readHostBpm();
    currentBpm.store (bpm, std::memory_order_relaxed);
    updateDspParameters (bpm);

    const auto channels = std::min (buffer.getNumChannels(), preparedChannels);
    juce::dsp::AudioBlock<float> full (buffer.getArrayOfWritePointers(), static_cast<size_t> (channels), static_cast<size_t> (buffer.getNumSamples()));

    // Hosts may exceed the announced block size; process in prepared-size chunks.
    for (size_t start = 0; start < full.getNumSamples(); start += static_cast<size_t> (maxChunkSize))
    {
        const auto len = std::min (static_cast<size_t> (maxChunkSize), full.getNumSamples() - start);
        processChunk (full.getSubBlock (start, len));
    }
}

void ShobeatsAudioProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
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

juce::AudioProcessorEditor* ShobeatsAudioProcessor::createEditor()
{
    return new ShobeatsAudioProcessorEditor (*this);
}

void ShobeatsAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto copy = state.copyState();
    copy.setProperty ("pluginVersion", JucePlugin_VersionString, nullptr);
    copy.setProperty (chainOrderProperty, chainOrder.toString(), nullptr);
    if (auto xml = copy.createXml())
        copyXmlToBinary (*xml, destData);
}

void ShobeatsAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (state.state.getType()))
        return;

    auto tree = juce::ValueTree::fromXml (*xml);
    if (! chainOrder.fromString (tree.getProperty (chainOrderProperty).toString()))
        chainOrder.set (ChainOrder::defaultOrder());
    state.replaceState (tree);
}
} // namespace sfx

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new sfx::ShobeatsAudioProcessor();
}
