#include "PluginEditor.h"
#include "PluginProcessor.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdlib>
#include <random>
#include <thread>

using trio::TrioAudioProcessor;
namespace ParamID = trio::ParamID;

namespace
{
constexpr double sampleRate = 48000.0;
constexpr int blockSize = 512;

struct JuceFixture
{
    juce::ScopedJuceInitialiser_GUI juce;
};

void setParam (TrioAudioProcessor& p, const char* id, float value)
{
    auto* param = p.getState().getParameter (id);
    REQUIRE (param != nullptr);
    param->setValueNotifyingHost (param->convertTo0to1 (value));
}

void prepare (TrioAudioProcessor& p, double sr = sampleRate, int block = blockSize, int channels = 2)
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

void disableAllModules (TrioAudioProcessor& p)
{
    setParam (p, ParamID::satEnabled, 0.0f);
    setParam (p, ParamID::chorusEnabled, 0.0f);
    setParam (p, ParamID::reverbEnabled, 0.0f);
}
} // namespace

TEST_CASE_METHOD (JuceFixture, "Plugin: exposes every expected parameter", "[plugin]")
{
    TrioAudioProcessor p;
    for (auto* id : { ParamID::inputGain, ParamID::outputGain,
                      ParamID::satEnabled, ParamID::satDrive, ParamID::satTone, ParamID::satMode, ParamID::satMix,
                      ParamID::chorusEnabled, ParamID::chorusRate, ParamID::chorusDepth, ParamID::chorusDelay, ParamID::chorusMix,
                      ParamID::reverbEnabled, ParamID::reverbSize, ParamID::reverbDecay, ParamID::reverbDamping,
                      ParamID::reverbPredelay, ParamID::reverbWidth, ParamID::reverbMix })
    {
        INFO (id);
        CHECK (p.getState().getParameter (id) != nullptr);
    }
    CHECK (p.getParameters().size() == 19);
}

TEST_CASE_METHOD (JuceFixture, "Plugin: supports mono and stereo, rejects other layouts", "[plugin]")
{
    TrioAudioProcessor p;
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
    TrioAudioProcessor a;
    setParam (a, ParamID::satDrive, 27.5f);
    setParam (a, ParamID::satMode, 2.0f);
    setParam (a, ParamID::chorusRate, 3.3f);
    setParam (a, ParamID::reverbDecay, 7.0f);
    setParam (a, ParamID::reverbEnabled, 0.0f);
    setParam (a, ParamID::outputGain, -6.0f);

    juce::MemoryBlock blob;
    a.getStateInformation (blob);
    REQUIRE (blob.getSize() > 0);

    TrioAudioProcessor b;
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
}

TEST_CASE_METHOD (JuceFixture, "Plugin: garbage state data is ignored safely", "[plugin]")
{
    TrioAudioProcessor p;
    const char junk[] = "definitely not a plugin state";
    p.setStateInformation (junk, static_cast<int> (sizeof (junk)));
    CHECK (p.getState().getParameter (ParamID::satDrive)->getValue()
           == p.getState().getParameter (ParamID::satDrive)->getDefaultValue());
}

TEST_CASE_METHOD (JuceFixture, "Plugin: reports oversampling latency", "[plugin]")
{
    TrioAudioProcessor p;
    prepare (p);
    CHECK (p.getLatencySamples() > 0);
    CHECK (p.getLatencySamples() < 64);
    CHECK (p.getTailLengthSeconds() > 1.0);
}

TEST_CASE_METHOD (JuceFixture, "Plugin: with every module off the signal passes at unity gain", "[plugin]")
{
    TrioAudioProcessor p;
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
    TrioAudioProcessor p;
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
    TrioAudioProcessor p;
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

TEST_CASE_METHOD (JuceFixture, "Plugin: survives randomised automation, odd block sizes and sample rates", "[plugin]")
{
    for (double sr : { 44100.0, 96000.0 })
        for (int channels : { 1, 2 })
        {
            TrioAudioProcessor p;
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
    TrioAudioProcessor p;
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
    TrioAudioProcessor p;
    prepare (p);

    std::unique_ptr<juce::AudioProcessorEditor> editor (p.createEditorAndMakeActive());
    REQUIRE (editor != nullptr);
    CHECK (editor->getWidth() == trio::MainView::designWidth);
    CHECK (editor->getHeight() == trio::MainView::designHeight);

    editor->setSize (trio::MainView::designWidth * 3 / 2, trio::MainView::designHeight * 3 / 2);
    auto snapshot = editor->createComponentSnapshot (editor->getLocalBounds());
    CHECK (snapshot.getWidth() == trio::MainView::designWidth * 3 / 2);

    // Optional: TRIOFX_SNAPSHOT=/path/to/file.png saves a rendering of the UI.
    if (const auto* path = std::getenv ("TRIOFX_SNAPSHOT"))
    {
        juce::File file (juce::String::fromUTF8 (path));
        file.deleteFile();
        juce::FileOutputStream out (file);
        REQUIRE (out.openedOk());
        CHECK (juce::PNGImageFormat().writeImageToStream (snapshot, out));
    }

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
    TrioAudioProcessor p;
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
