#include "PluginEditor.h"
#include "PluginProcessor.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdlib>
#include <random>
#include <thread>

using sfx::ShobeatsAudioProcessor;
namespace ParamID = sfx::ParamID;

namespace
{
constexpr double sampleRate = 48000.0;
constexpr int blockSize = 512;

struct JuceFixture
{
    juce::ScopedJuceInitialiser_GUI juce;
};

void setParam (ShobeatsAudioProcessor& p, const char* id, float value)
{
    auto* param = p.getState().getParameter (id);
    REQUIRE (param != nullptr);
    param->setValueNotifyingHost (param->convertTo0to1 (value));
}

void prepare (ShobeatsAudioProcessor& p, double sr = sampleRate, int block = blockSize, int channels = 2)
{
    const auto set = channels == 1 ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo();
    juce::AudioProcessor::BusesLayout layout;
    layout.inputBuses.add (set);
    layout.outputBuses.add (set);
    REQUIRE (p.setBusesLayout (layout));
    p.setRateAndBufferSizeDetails (sr, block);
    p.prepareToPlay (sr, block);
}

void fillSine (juce::AudioBuffer<float>& b, float freq, float amp, int offset)
{
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
        for (int i = 0; i < b.getNumSamples(); ++i)
            b.setSample (ch, i, amp * std::sin (juce::MathConstants<float>::twoPi * freq * static_cast<float> (offset + i) / static_cast<float> (sampleRate)));
}

bool isFinite (const juce::AudioBuffer<float>& b)
{
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
        for (int i = 0; i < b.getNumSamples(); ++i)
            if (! std::isfinite (b.getSample (ch, i)))
                return false;
    return true;
}

void disableAllModules (ShobeatsAudioProcessor& p)
{
    for (const auto& info : sfx::effectInfo)
        setParam (p, info.enabledId, 0.0f);
}

void enableAllModules (ShobeatsAudioProcessor& p)
{
    for (const auto& info : sfx::effectInfo)
        setParam (p, info.enabledId, 1.0f);
}

float processSine (ShobeatsAudioProcessor& p, int blocks, std::vector<float>* capture = nullptr)
{
    juce::MidiBuffer midi;
    juce::AudioBuffer<float> buffer (2, blockSize);
    float lastRms = 0.0f;
    for (int block = 0; block < blocks; ++block)
    {
        fillSine (buffer, 500.0f, 0.5f, block * blockSize);
        p.processBlock (buffer, midi);
        lastRms = buffer.getRMSLevel (0, 0, blockSize);
        if (capture != nullptr)
            capture->insert (capture->end(), buffer.getReadPointer (0), buffer.getReadPointer (0) + blockSize);
    }
    return lastRms;
}
} // namespace

TEST_CASE_METHOD (JuceFixture, "Plugin: exposes every expected parameter", "[plugin]")
{
    ShobeatsAudioProcessor p;
    for (auto* id : sfx::allParameterIds)
    {
        INFO (id);
        CHECK (p.getState().getParameter (id) != nullptr);
    }
    CHECK (static_cast<size_t> (p.getParameters().size()) == sfx::numParameters);

    // Every effect has an on/off switch, and a fresh instance starts with all of them off.
    for (const auto& info : sfx::effectInfo)
    {
        INFO (info.name);
        auto* param = p.getState().getParameter (info.enabledId);
        REQUIRE (param != nullptr);
        CHECK (param->getValue() < 0.5f);
    }
}

TEST_CASE_METHOD (JuceFixture, "Plugin: supports mono and stereo, rejects other layouts", "[plugin]")
{
    ShobeatsAudioProcessor p;
    auto layoutOf = [] (juce::AudioChannelSet in, juce::AudioChannelSet out) {
        juce::AudioProcessor::BusesLayout l;
        l.inputBuses.add (in);
        l.outputBuses.add (out);
        return l;
    };
    CHECK (p.checkBusesLayoutSupported (layoutOf (juce::AudioChannelSet::stereo(), juce::AudioChannelSet::stereo())));
    CHECK (p.checkBusesLayoutSupported (layoutOf (juce::AudioChannelSet::mono(), juce::AudioChannelSet::mono())));
    CHECK_FALSE (p.checkBusesLayoutSupported (layoutOf (juce::AudioChannelSet::mono(), juce::AudioChannelSet::stereo())));
    CHECK_FALSE (p.checkBusesLayoutSupported (layoutOf (juce::AudioChannelSet::create5point1(), juce::AudioChannelSet::create5point1())));
}

TEST_CASE_METHOD (JuceFixture, "Plugin: state save/restore round-trips every parameter", "[plugin]")
{
    ShobeatsAudioProcessor a;
    setParam (a, ParamID::satDrive, 27.5f);
    setParam (a, ParamID::satMode, 2.0f);
    setParam (a, ParamID::chorusRate, 3.3f);
    setParam (a, ParamID::reverbDecay, 7.0f);
    setParam (a, ParamID::reverbEnabled, 0.0f);
    setParam (a, ParamID::outputGain, -6.0f);
    setParam (a, ParamID::delaySync, 0.0f);
    setParam (a, ParamID::tremShape, 3.0f);
    a.getChainOrder().move (7, 0); // reverb first
    a.getChainOrder().move (3, 5);

    juce::MemoryBlock blob;
    a.getStateInformation (blob);
    REQUIRE (blob.getSize() > 0);

    ShobeatsAudioProcessor b;
    b.setStateInformation (blob.getData(), static_cast<int> (blob.getSize()));

    for (auto* param : a.getParameters())
    {
        auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (param);
        REQUIRE (withId != nullptr);
        auto* other = b.getState().getParameter (withId->paramID);
        REQUIRE (other != nullptr);
        INFO (withId->paramID);
        CHECK (std::abs (other->getValue() - param->getValue()) < 1.0e-6f);
    }
    CHECK (b.getChainOrder().get() == a.getChainOrder().get());
    CHECK (b.getChainOrder().get()[0] == static_cast<int> (sfx::EffectId::reverb));
}

TEST_CASE_METHOD (JuceFixture, "Plugin: invalid chain order in a saved state falls back to default", "[plugin]")
{
    ShobeatsAudioProcessor a;
    a.getChainOrder().move (0, 7);
    juce::MemoryBlock blob;
    a.getStateInformation (blob);

    auto xml = juce::AudioProcessor::getXmlFromBinary (blob.getData(), static_cast<int> (blob.getSize()));
    REQUIRE (xml != nullptr);
    xml->setAttribute ("chainOrder", "sat,sat,tape,chorus,trem,rev,delay,reverb");
    juce::MemoryBlock corrupted;
    juce::AudioProcessor::copyXmlToBinary (*xml, corrupted);

    ShobeatsAudioProcessor b;
    b.getChainOrder().move (2, 4);
    b.setStateInformation (corrupted.getData(), static_cast<int> (corrupted.getSize()));
    CHECK (b.getChainOrder().get() == sfx::ChainOrder::defaultOrder());
}

TEST_CASE ("ChainOrder: move keeps a valid permutation", "[plugin]")
{
    sfx::ChainOrder order;
    juce::Random r (5);
    for (int i = 0; i < 500; ++i)
    {
        order.move (r.nextInt (sfx::numEffects), r.nextInt (sfx::numEffects));
        REQUIRE (sfx::ChainOrder::isValid (order.get()));
    }
    sfx::ChainOrder copy;
    REQUIRE (copy.fromString (order.toString()));
    CHECK (copy.get() == order.get());
    CHECK_FALSE (copy.fromString ("sat,rs"));
}

TEST_CASE_METHOD (JuceFixture, "Plugin: garbage state data is ignored safely", "[plugin]")
{
    ShobeatsAudioProcessor p;
    const char junk[] = "definitely not a plugin state";
    p.setStateInformation (junk, static_cast<int> (sizeof (junk)));
    CHECK (p.getState().getParameter (ParamID::satDrive)->getValue()
           == p.getState().getParameter (ParamID::satDrive)->getDefaultValue());
}

TEST_CASE_METHOD (JuceFixture, "Plugin: reports oversampling latency", "[plugin]")
{
    ShobeatsAudioProcessor p;
    prepare (p);
    CHECK (p.getLatencySamples() > 0);
    CHECK (p.getLatencySamples() < 64);
    CHECK (p.getTailLengthSeconds() > 1.0);
}

TEST_CASE_METHOD (JuceFixture, "Plugin: with every module off the signal passes at unity gain", "[plugin]")
{
    ShobeatsAudioProcessor p;
    disableAllModules (p);
    prepare (p);

    juce::MidiBuffer midi;
    juce::AudioBuffer<float> buffer (2, blockSize);
    double inEnergy = 0.0, outEnergy = 0.0;
    for (int block = 0; block < 40; ++block)
    {
        fillSine (buffer, 1000.0f, 0.5f, block * blockSize);
        const auto inRms = buffer.getRMSLevel (0, 0, blockSize);
        p.processBlock (buffer, midi);
        if (block >= 4)
        {
            inEnergy += inRms * inRms;
            outEnergy += buffer.getRMSLevel (0, 0, blockSize) * buffer.getRMSLevel (0, 0, blockSize);
        }
    }
    const auto diffDb = 10.0 * std::log10 (outEnergy / inEnergy);
    INFO ("gain change " << diffDb << " dB");
    CHECK (std::abs (diffDb) < 0.1);
}

TEST_CASE_METHOD (JuceFixture, "Plugin: host bypass delays the dry signal by exactly the latency", "[plugin]")
{
    ShobeatsAudioProcessor p;
    prepare (p);
    const auto latency = p.getLatencySamples();

    juce::MidiBuffer midi;
    juce::AudioBuffer<float> buffer (2, blockSize);
    buffer.clear();
    buffer.setSample (0, 10, 1.0f);
    buffer.setSample (1, 10, 1.0f);
    p.processBlockBypassed (buffer, midi);

    CHECK (buffer.getSample (0, 10 + latency) == 1.0f);
    CHECK (buffer.getSample (1, 10 + latency) == 1.0f);
    CHECK (buffer.getMagnitude (0, 0, 10 + latency) == 0.0f);
}

TEST_CASE_METHOD (JuceFixture, "Plugin: input/output gain apply", "[plugin]")
{
    ShobeatsAudioProcessor p;
    disableAllModules (p);
    setParam (p, ParamID::outputGain, -12.0f);
    prepare (p);

    juce::MidiBuffer midi;
    juce::AudioBuffer<float> buffer (2, blockSize);
    float lastRms = 0.0f;
    for (int block = 0; block < 20; ++block)
    {
        fillSine (buffer, 500.0f, 0.5f, block * blockSize);
        p.processBlock (buffer, midi);
        lastRms = buffer.getRMSLevel (0, 0, blockSize);
    }
    const auto expected = 0.5f / std::sqrt (2.0f) * juce::Decibels::decibelsToGain (-12.0f);
    CHECK (std::abs (juce::Decibels::gainToDecibels (lastRms / expected)) < 0.2f);
}

TEST_CASE_METHOD (JuceFixture, "Plugin: global mix at 0 returns the latency-aligned dry signal", "[plugin]")
{
    // Every effect on with heavy settings, but global mix at zero: output == input delayed by latency.
    ShobeatsAudioProcessor p;
    enableAllModules (p);
    setParam (p, ParamID::satDrive, 36.0f);
    setParam (p, ParamID::delayFeedback, 0.9f);
    setParam (p, ParamID::globalMix, 0.0f);
    prepare (p);
    const auto latency = p.getLatencySamples();

    juce::MidiBuffer midi;
    juce::AudioBuffer<float> buffer (2, blockSize);
    std::vector<float> in, out;
    for (int block = 0; block < 20; ++block)
    {
        fillSine (buffer, 300.0f, 0.5f, block * blockSize);
        in.insert (in.end(), buffer.getReadPointer (0), buffer.getReadPointer (0) + blockSize);
        p.processBlock (buffer, midi);
        out.insert (out.end(), buffer.getReadPointer (0), buffer.getReadPointer (0) + blockSize);
    }
    float maxError = 0.0f;
    for (size_t i = static_cast<size_t> (blockSize); i < out.size(); ++i)
        maxError = std::max (maxError, std::abs (out[i] - in[i - static_cast<size_t> (latency)]));
    CHECK (maxError < 1.0e-5f);
}

TEST_CASE_METHOD (JuceFixture, "Plugin: each effect changes the sound when switched on", "[plugin]")
{
    // Guards against controls that look active but do nothing.
    for (const auto& info : sfx::effectInfo)
    {
        ShobeatsAudioProcessor dry, wet;
        disableAllModules (dry);
        disableAllModules (wet);
        setParam (wet, info.enabledId, 1.0f);
        // Make the subtler effects obvious.
        setParam (wet, ParamID::rsRate, 2000.0f);
        setParam (wet, ParamID::tremDepth, 1.0f);
        setParam (wet, ParamID::chorusMix, 1.0f);
        setParam (wet, ParamID::revMix, 1.0f);
        setParam (wet, ParamID::delayMix, 1.0f);
        setParam (wet, ParamID::reverbMix, 1.0f);
        setParam (wet, ParamID::tapeWow, 1.0f);
        prepare (dry);
        prepare (wet);

        std::vector<float> a, b;
        processSine (dry, 40, &a);
        processSine (wet, 40, &b);
        double diff = 0.0;
        for (size_t i = a.size() / 2; i < a.size(); ++i)
            diff += std::abs (a[i] - b[i]);
        INFO (info.name);
        CHECK (diff / static_cast<double> (a.size() / 2) > 0.01);
    }
}

TEST_CASE_METHOD (JuceFixture, "Plugin: chain order changes the result", "[plugin]")
{
    // Saturation before vs after a deep tremolo produces different audio.
    auto render = [] (bool satFirst) {
        ShobeatsAudioProcessor p;
        setParam (p, ParamID::satEnabled, 1.0f);
        setParam (p, ParamID::satDrive, 30.0f);
        setParam (p, ParamID::tremEnabled, 1.0f);
        setParam (p, ParamID::tremDepth, 1.0f);
        if (! satFirst)
            p.getChainOrder().move (0, 7);
        prepare (p);
        std::vector<float> out;
        processSine (p, 30, &out);
        return out;
    };
    const auto a = render (true), b = render (false);
    double diff = 0.0;
    for (size_t i = 0; i < a.size(); ++i)
        diff += std::abs (a[i] - b[i]);
    CHECK (diff / static_cast<double> (a.size()) > 0.01);
}

TEST_CASE_METHOD (JuceFixture, "Plugin: survives randomised automation, odd block sizes and sample rates", "[plugin]")
{
    for (double sr : { 44100.0, 96000.0 })
        for (int channels : { 1, 2 })
        {
            ShobeatsAudioProcessor p;
            prepare (p, sr, 256, channels);

            std::mt19937 rng (static_cast<unsigned> (sr) + static_cast<unsigned> (channels));
            std::uniform_real_distribution<float> unit (0.0f, 1.0f);
            std::uniform_int_distribution<int> blockDist (1, 1024); // includes blocks larger than prepared
            juce::MidiBuffer midi;

            for (int iteration = 0; iteration < 150; ++iteration)
            {
                for (auto* param : p.getParameters())
                    if (unit (rng) < 0.3f)
                        param->setValueNotifyingHost (unit (rng));
                if (unit (rng) < 0.2f)
                    p.getChainOrder().move (static_cast<int> (unit (rng) * 8.0f), static_cast<int> (unit (rng) * 8.0f));

                juce::AudioBuffer<float> buffer (channels, blockDist (rng));
                for (int ch = 0; ch < channels; ++ch)
                    for (int i = 0; i < buffer.getNumSamples(); ++i)
                        buffer.setSample (ch, i, (unit (rng) * 2.0f - 1.0f) * 1.5f);

                p.processBlock (buffer, midi);
                INFO ("sr " << sr << " ch " << channels << " iteration " << iteration);
                REQUIRE (isFinite (buffer));
                // Worst case: +24 dB in, +24 dB out and a 20 s reverb build-up. The point is
                // to catch instability (runaway feedback), not to police loudness.
                REQUIRE (buffer.getMagnitude (0, buffer.getNumSamples()) < 5000.0f);
            }

            p.releaseResources();
        }
}

TEST_CASE_METHOD (JuceFixture, "Plugin: silence in gives silence out after reset", "[plugin]")
{
    ShobeatsAudioProcessor p;
    prepare (p);
    p.reset();

    juce::MidiBuffer midi;
    juce::AudioBuffer<float> buffer (2, blockSize);
    for (int block = 0; block < 10; ++block)
    {
        buffer.clear();
        p.processBlock (buffer, midi);
        CHECK (buffer.getMagnitude (0, blockSize) < 1.0e-7f);
    }
}

TEST_CASE_METHOD (JuceFixture, "Plugin: editor opens, resizes and closes", "[plugin][gui]")
{
#if JUCE_LINUX
    if (std::getenv ("DISPLAY") == nullptr)
        SKIP ("No display available");
#endif
    ShobeatsAudioProcessor p;
    prepare (p);

    std::unique_ptr<juce::AudioProcessorEditor> editor (p.createEditorAndMakeActive());
    REQUIRE (editor != nullptr);
    CHECK (editor->getWidth() == sfx::MainView::designWidth * 85 / 100);
    CHECK (editor->getHeight() == sfx::MainView::designHeight * 85 / 100);

    editor->setSize (sfx::MainView::designWidth, sfx::MainView::designHeight);
    auto snapshot = editor->createComponentSnapshot (editor->getLocalBounds());
    CHECK (snapshot.getWidth() == sfx::MainView::designWidth);

    // Optional: SHOBEATS_SNAPSHOT=/path/prefix saves renderings of the UI (one per selected pedal).
    if (const auto* prefix = std::getenv ("SHOBEATS_SNAPSHOT"))
    {
        auto& view = dynamic_cast<sfx::ShobeatsAudioProcessorEditor&> (*editor).getView();
        for (int i = 0; i < sfx::numEffects; ++i)
        {
            setParam (p, sfx::effectInfo[static_cast<size_t> (i)].enabledId, i % 3 == 0 ? 0.0f : 1.0f);
            view.getPedalboard().select (static_cast<sfx::EffectId> (i));
            juce::File file (juce::String::fromUTF8 (prefix) + juce::String (i) + ".png");
            file.deleteFile();
            juce::FileOutputStream out (file);
            REQUIRE (out.openedOk());
            CHECK (juce::PNGImageFormat().writeImageToStream (editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f), out));
        }
    }

    editor.reset();
}

TEST_CASE_METHOD (JuceFixture, "Plugin: every pedal can be selected and moved from the editor", "[plugin][gui]")
{
#if JUCE_LINUX
    if (std::getenv ("DISPLAY") == nullptr)
        SKIP ("No display available");
#endif
    ShobeatsAudioProcessor p;
    prepare (p);
    std::unique_ptr<juce::AudioProcessorEditor> editor (p.createEditorAndMakeActive());
    auto& view = dynamic_cast<sfx::ShobeatsAudioProcessorEditor&> (*editor).getView();

    for (int i = 0; i < sfx::numEffects; ++i)
    {
        const auto id = static_cast<sfx::EffectId> (i);
        view.getPedalboard().select (id);
        CHECK (view.getDetailPanel().getEffect() == id);
        juce::ignoreUnused (editor->createComponentSnapshot (editor->getLocalBounds()));
    }

    // Move the selected pedal (reverb, last) to the front, one step at a time.
    for (int i = 0; i < sfx::numEffects; ++i)
        view.getPedalboard().nudgeSelected (-1);
    CHECK (p.getChainOrder().get()[0] == static_cast<int> (sfx::EffectId::reverb));
    CHECK (sfx::ChainOrder::isValid (p.getChainOrder().get()));

    editor.reset();
}

TEST_CASE_METHOD (JuceFixture, "Plugin: editor survives parameter changes from a background thread", "[plugin][gui]")
{
#if JUCE_LINUX
    if (std::getenv ("DISPLAY") == nullptr)
        SKIP ("No display available");
#endif
    // Mirrors pluginval's "Editor Automation" test: a non-message thread hammers every
    // parameter while the editor is open and the message loop is running.
    ShobeatsAudioProcessor p;
    prepare (p);
    std::unique_ptr<juce::AudioProcessorEditor> editor (p.createEditorAndMakeActive());
    REQUIRE (editor != nullptr);
    // Render the UI repeatedly on the message thread while parameters change underneath it.
    int renders = 0;
    juce::TimedCallback painter ([&] {
        juce::ignoreUnused (editor->createComponentSnapshot (editor->getLocalBounds()));
        ++renders;
    });
    painter.startTimer (15);

    std::atomic<bool> done { false };
    std::thread automation ([&] {
        juce::Random r (99);
        for (int block = 0; block < 300; ++block)
        {
            for (auto* param : p.getParameters())
                param->setValueNotifyingHost (r.nextFloat());
            juce::Thread::sleep (2);
        }
        done = true;
        // Let pending async UI updates drain, then leave the message loop.
        juce::Thread::sleep (200);
        juce::MessageManager::getInstance()->stopDispatchLoop();
    });

    juce::MessageManager::getInstance()->runDispatchLoop();
    automation.join();
    painter.stopTimer();
    CHECK (done);
    CHECK (renders > 10);

    editor.reset();
    SUCCEED();
}
