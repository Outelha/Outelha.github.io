#include "PluginEditor.h"
#include "PluginProcessor.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdlib>
#include <cstring>
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

TEST_CASE_METHOD (JuceFixture, "ParameterReader: finds parameters by id text, not by pointer", "[plugin]")
{
    // Regression: MSVC does not merge identical string literals across files, so an id
    // pointer from another translation unit can differ from the cached one.
    ShobeatsAudioProcessor p;
    setParam (p, ParamID::delayFeedback, 0.77f);
    sfx::ParameterReader reader (p.getState());

    char copy[32] = {};
    std::strncpy (copy, ParamID::delayFeedback, sizeof (copy) - 1);
    REQUIRE (copy != ParamID::delayFeedback);
    CHECK (std::abs (reader.get (copy) - 0.77f) < 1.0e-3f);
    CHECK (std::abs (reader.get (ParamID::delayFeedback) - 0.77f) < 1.0e-3f);
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
        auto save = [&] (const juce::String& suffix) {
            juce::File file (juce::String::fromUTF8 (prefix) + suffix + ".png");
            file.deleteFile();
            juce::FileOutputStream out (file);
            REQUIRE (out.openedOk());
            CHECK (juce::PNGImageFormat().writeImageToStream (editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f), out));
        };
        p.getPresetManager().load (p.getPresetManager().indexOf ("Dusty Memories", "Vintage / Analog"));
        // Play ~1.5 s of a beat-like test signal so the audio-reactive visuals have real data.
        double clockTime = 1.0;
        auto playAudio = [&] {
            juce::AudioBuffer<float> buffer (2, blockSize);
            juce::MidiBuffer midi;
            for (int b = 0; b < static_cast<int> (1.5 * sampleRate) / blockSize; ++b)
            {
                for (int i = 0; i < blockSize; ++i)
                {
                    const auto n = b * blockSize + i;
                    const auto t = static_cast<float> (n) / static_cast<float> (sampleRate);
                    const auto beat = std::fmod (t, 0.25f);
                    const auto env = std::exp (-beat * 14.0f) * (0.4f + 0.6f * std::abs (std::sin (t * 1.7f)));
                    const auto v = env * (0.6f * std::sin (juce::MathConstants<float>::twoPi * 110.0f * t) + 0.25f * std::sin (juce::MathConstants<float>::twoPi * 1320.0f * t));
                    buffer.setSample (0, i, v);
                    buffer.setSample (1, i, v);
                }
                p.processBlock (buffer, midi);
                if (b % 8 == 0)
                    view.getFrameClock().tick (clockTime += 1.0 / 60.0);
            }
            for (int i = 0; i < 4; ++i)
                view.getFrameClock().tick (clockTime += 1.0 / 60.0);
        };
        playAudio();
        view.getMainPanel().refreshDisplay();
        view.refresh();
        save ("main");
        view.setAdvanced (true);
        for (int i = 0; i < sfx::numEffects; ++i)
        {
            setParam (p, sfx::effectInfo[static_cast<size_t> (i)].enabledId, i % 3 == 0 ? 0.0f : 1.0f);
            view.getPedalboard().select (static_cast<sfx::EffectId> (i));
            playAudio();
            juce::File file (juce::String::fromUTF8 (prefix) + juce::String (i) + ".png");
            file.deleteFile();
            juce::FileOutputStream out (file);
            REQUIRE (out.openedOk());
            CHECK (juce::PNGImageFormat().writeImageToStream (editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f), out));
        }

        p.getPresetManager().load (p.getPresetManager().indexOf ("Lost in Clouds", "Dreamy / Ambient"));
        view.showBrowser (true);

        // Optional: SHOBEATS_SNAPSHOT_IMPORT=<zip> renders the browser's IMPORTED view (local check only).
        if (const auto* archive = std::getenv ("SHOBEATS_SNAPSHOT_IMPORT"))
        {
            juce::TemporaryFile importDir;
            p.getPresetManager().setImportDirectory (importDir.getFile());
            p.getPresetManager().importPresets (juce::File (juce::String::fromUTF8 (archive)));
            view.showBrowser (false);
            view.showBrowser (true);
            view.getBrowser().showCategory (sfx::PresetManager::importedCategory);
            juce::File imported (juce::String::fromUTF8 (prefix) + "imported.png");
            imported.deleteFile();
            juce::FileOutputStream importedOut (imported);
            CHECK (juce::PNGImageFormat().writeImageToStream (editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f), importedOut));
            importDir.getFile().deleteRecursively();
        }
        juce::File file (juce::String::fromUTF8 (prefix) + "browser.png");
        file.deleteFile();
        juce::FileOutputStream out (file);
        CHECK (juce::PNGImageFormat().writeImageToStream (editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f), out));

        view.getBrowser().showCategory (sfx::PresetManager::labCategory);
        juce::File labFile (juce::String::fromUTF8 (prefix) + "lab.png");
        labFile.deleteFile();
        juce::FileOutputStream labOut (labFile);
        CHECK (juce::PNGImageFormat().writeImageToStream (editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f), labOut));
        view.showBrowser (false);
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
    std::atomic<int> renders { 0 };
    juce::TimedCallback painter ([&] {
        juce::ignoreUnused (editor->createComponentSnapshot (editor->getLocalBounds()));
        ++renders;
    });
    painter.startTimer (15);

    std::atomic<bool> done { false };
    std::thread automation ([&] {
        juce::Random r (99);
        // At least 300 rounds, and keep going until the UI has rendered at least 10 times
        // under the changing parameters (independent of how busy the machine is).
        for (int block = 0; block < 300 || renders.load() < 10; ++block)
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
    CHECK (renders.load() >= 10);

    editor.reset();
    SUCCEED();
}

namespace
{
struct AnimatedEditor
{
    ShobeatsAudioProcessor& p;
    std::unique_ptr<juce::AudioProcessorEditor> editor { p.createEditorAndMakeActive() };
    sfx::MainView& view = dynamic_cast<sfx::ShobeatsAudioProcessorEditor&> (*editor).getView();
    double clockTime = 1.0;

    // One 60 Hz UI frame, optionally with audio processed since the previous frame.
    void frame (bool processAudio)
    {
        if (processAudio)
            processSine (p, 2);
        view.getFrameClock().tick (clockTime += 1.0 / 60.0);
    }
    float reel() { return view.getDetailPanel().getDisplay().getReelPhase(); }
};
} // namespace

TEST_CASE_METHOD (JuceFixture, "UI: tape reels turn only while audio is processed and the pedal is on", "[plugin][gui][animation]")
{
#if JUCE_LINUX
    if (std::getenv ("DISPLAY") == nullptr)
        SKIP ("No display available");
#endif
    ShobeatsAudioProcessor p;
    prepare (p);
    disableAllModules (p);
    setParam (p, ParamID::tapeEnabled, 1.0f);
    AnimatedEditor ui { p };
    ui.view.setAdvanced (true);
    ui.view.getPedalboard().select (sfx::EffectId::tape);

    // Host not calling processBlock (transport stopped / plugin idle): reels stay still.
    for (int i = 0; i < 30; ++i)
        ui.frame (false);
    const auto idle = ui.reel();
    for (int i = 0; i < 30; ++i)
        ui.frame (false);
    CHECK (ui.reel() == idle);

    // Audio running: reels turn.
    for (int i = 0; i < 60; ++i)
        ui.frame (true);
    const auto running = ui.reel();
    CHECK (running > idle + 0.5f);

    // Audio stops: reels coast down and settle.
    for (int i = 0; i < 240; ++i)
        ui.frame (false);
    const auto settled = ui.reel();
    for (int i = 0; i < 30; ++i)
        ui.frame (false);
    CHECK (ui.reel() == settled);

    // Pedal bypassed: audio runs, reels stay settled.
    setParam (p, ParamID::tapeEnabled, 0.0f);
    for (int i = 0; i < 240; ++i)
        ui.frame (true);
    const auto bypassed = ui.reel();
    for (int i = 0; i < 30; ++i)
        ui.frame (true);
    CHECK (ui.reel() == bypassed);

    // Pedal on but the Advanced view hidden: nothing animates.
    setParam (p, ParamID::tapeEnabled, 1.0f);
    ui.view.setAdvanced (false);
    const auto hidden = ui.reel();
    for (int i = 0; i < 60; ++i)
        ui.frame (true);
    CHECK (ui.reel() == hidden);
}

TEST_CASE_METHOD (JuceFixture, "UI: full-editor render cost stays within budget", "[plugin][gui][perf]")
{
#if JUCE_LINUX
    if (std::getenv ("DISPLAY") == nullptr)
        SKIP ("No display available");
#endif
    ShobeatsAudioProcessor p;
    prepare (p);
    enableAllModules (p);
    AnimatedEditor ui { p };
    ui.editor->setSize (sfx::MainView::designWidth, sfx::MainView::designHeight);
    ui.view.getPedalboard().select (sfx::EffectId::delay); // the panel with the most artwork

    // Software-rendered full repaints (worst case: every pixel, no cached layers reused by the
    // host). Real frames only repaint the animated regions, so this is a loose upper bound.
    auto measure = [&] (bool advanced, float scale) {
        ui.view.setAdvanced (advanced);
        for (int i = 0; i < 4; ++i)
            ui.frame (true);
        juce::ignoreUnused (ui.editor->createComponentSnapshot (ui.editor->getLocalBounds(), true, scale)); // warm caches
        constexpr int runs = 6;
        const auto start = juce::Time::getMillisecondCounterHiRes();
        for (int i = 0; i < runs; ++i)
        {
            ui.frame (true);
            juce::ignoreUnused (ui.editor->createComponentSnapshot (ui.editor->getLocalBounds(), true, scale));
        }
        return (juce::Time::getMillisecondCounterHiRes() - start) / runs;
    };
    for (auto scale : { 1.0f, 1.25f, 1.5f, 2.0f })
    {
        const auto mainMs = measure (false, scale);
        const auto advMs = measure (true, scale);
        WARN ("scale " << scale << ": main " << mainMs << " ms, advanced " << advMs << " ms per full repaint");
        CHECK (mainMs < 400.0);
        CHECK (advMs < 400.0);
    }
}

TEST_CASE_METHOD (JuceFixture, "UI: editor scales cleanly at 100, 125, 150 and 200 %", "[plugin][gui][scaling]")
{
#if JUCE_LINUX
    if (std::getenv ("DISPLAY") == nullptr)
        SKIP ("No display available");
#endif
    ShobeatsAudioProcessor p;
    prepare (p);
    AnimatedEditor ui { p };
    auto& editor = *ui.editor;

    // Every visible control must lie inside its parent at design size; scaling is one transform.
    std::function<void (juce::Component&)> checkInside = [&] (juce::Component& c) {
        for (auto* child : c.getChildren())
        {
            if (! child->isVisible() || child->getWidth() == 0)
                continue;
            INFO (typeid (*child).name() << " '" << child->getName() << "' " << child->getBounds().toString() << " in " << c.getLocalBounds().toString());
            CHECK (c.getLocalBounds().expanded (2).contains (child->getBounds()));
            checkInside (*child);
        }
    };

    for (auto percent : { 100, 125, 150, 200 })
    {
        const auto w = sfx::MainView::designWidth * percent / 100;
        const auto h = sfx::MainView::designHeight * percent / 100;
        editor.setSize (w, h);
        CHECK (editor.getWidth() == w);
        CHECK (editor.getHeight() == h);
        const auto viewArea = ui.view.getBoundsInParent();
        CHECK (std::abs (viewArea.getWidth() - w) <= 1);
        CHECK (std::abs (viewArea.getHeight() - h) <= 1);

        for (auto advanced : { false, true })
        {
            ui.view.setAdvanced (advanced);
            checkInside (ui.view);
            ui.frame (true);
            const auto image = editor.createComponentSnapshot (editor.getLocalBounds());
            CHECK (image.getWidth() == w);
            if (const auto* prefix = std::getenv ("SHOBEATS_SNAPSHOT"))
            {
                juce::File file (juce::String::fromUTF8 (prefix) + "scale" + juce::String (percent) + (advanced ? "_adv" : "_main") + ".png");
                file.deleteFile();
                juce::FileOutputStream out (file);
                CHECK (juce::PNGImageFormat().writeImageToStream (image, out));
            }
        }
    }
}

//==============================================================================
// DELAY bitmap skin (assets/images/delay, docs/ui-redesign/DELAY_ASSETS.md)
//==============================================================================
#include "ui/PedalArt.h"
#include "ui/Skin.h"

TEST_CASE_METHOD (JuceFixture, "DELAY skin: all 20 assets are bundled at their exact @2x size", "[gui][skin]")
{
    using sfx::ui::Skin;
    struct Expect
    {
        const char* name;
        int w, h;
        bool opaque, rotates;
    };
    const Expect expected[] = {
        { "delay_adv_enclosure", 308, 544, false, false }, { "delay_adv_window_glass", 212, 92, false, false },
        { "delay_adv_selected_glow", 308, 544, false, false }, { "delay_main_enclosure", 208, 448, false, false },
        { "delay_main_window_glass", 160, 80, false, false }, { "pedal_knob_shadow", 88, 88, false, true },
        { "pedal_knob_body", 88, 88, false, true }, { "pedal_knob_gloss", 88, 88, false, true },
        { "footswitch_up", 88, 88, false, false }, { "footswitch_down", 88, 88, false, false },
        { "led_amber_off", 48, 48, false, true }, { "led_amber_on", 48, 48, false, true },
        { "panel_background", 2480, 816, true, false }, { "display_frame", 700, 688, false, false },
        { "display_glass", 700, 688, false, false }, { "panel_knob_shadow", 144, 144, false, true },
        { "panel_knob_skirt", 144, 144, false, true }, { "panel_knob_cap", 144, 144, false, true },
        { "panel_knob_pointer", 144, 144, false, true }, { "art_delay", 600, 324, false, false },
    };
    REQUIRE (Skin::delayLayerNames().size() == 20);

    Skin skin;
    REQUIRE (skin.hasDelay());
    for (const auto& e : expected)
    {
        INFO (e.name);
        CHECK (Skin::delayLayerNames().contains (e.name));
        const auto& layer = skin.get (e.name);
        REQUIRE (layer.isValid());
        CHECK (layer.full.getWidth() == e.w);
        CHECK (layer.full.getHeight() == e.h);
        // The @1x copy is an exact half.
        CHECK (layer.half.getWidth() == e.w / 2);
        CHECK (layer.half.getHeight() == e.h / 2);
        if (e.rotates)
            CHECK (e.w == e.h); // rotates about the canvas centre

        const juce::Point<int> corners[] = { { 0, 0 }, { e.w - 1, 0 }, { 0, e.h - 1 }, { e.w - 1, e.h - 1 } };
        if (e.opaque)
        {
            // panel_background: opaque, outer corners in chassis black.
            for (auto c : corners)
            {
                const auto px = layer.full.getPixelAt (c.x, c.y);
                CHECK (px.getAlpha() == 255);
                CHECK (px.getBrightness() < 0.1f);
            }
        }
        else
        {
            CHECK (layer.full.hasAlphaChannel());
            for (auto c : corners)
                CHECK (layer.full.getPixelAt (c.x, c.y).getAlpha() == 0);
        }
    }

    // Glass overlays must let the live content through (spec: at least 80 % transparent).
    for (auto name : { "delay_adv_window_glass", "delay_main_window_glass", "display_glass" })
    {
        INFO (name);
        const auto& img = skin.get (name).full;
        int maxAlpha = 0;
        for (int y = 0; y < img.getHeight(); ++y)
            for (int x = 0; x < img.getWidth(); ++x)
                maxAlpha = std::max (maxAlpha, static_cast<int> (img.getPixelAt (x, y).getAlpha()));
        CHECK (maxAlpha <= 51);
    }

    // Only DELAY is skinned; the other seven keep their procedural drawing.
    for (int i = 0; i < sfx::numEffects; ++i)
        CHECK (sfx::ui::pedal::hasSkin (static_cast<sfx::EffectId> (i)) == (static_cast<sfx::EffectId> (i) == sfx::EffectId::delay));
}

TEST_CASE_METHOD (JuceFixture, "DELAY skin: knob layers turn with the value (7:30 at minimum, 4:30 at maximum)", "[gui][skin]")
{
#if JUCE_LINUX
    if (std::getenv ("DISPLAY") == nullptr)
        SKIP ("No display available");
#endif
    ShobeatsAudioProcessor p;
    sfx::ui::ShobeatsLookAndFeel laf;
    sfx::ui::Knob knob (p.getState(), ParamID::delayFeedback, "", sfx::ui::accentColour (sfx::EffectId::delay), sfx::ui::Knob::Style::pedal);
    knob.hideCaption();
    knob.setLookAndFeel (&laf);
    knob.getSlider().getProperties().set ("skin", "delay");
    knob.getSlider().getProperties().set ("skinCanvas", 44.0f);
    knob.setBounds (0, 0, 37, 37);

    // Angle of the white pointer line, clockwise from 12 o'clock, in degrees.
    auto pointerAngle = [&] (double proportion) {
        auto& s = knob.getSlider();
        s.setValue (s.proportionOfLengthToValue (proportion), juce::sendNotificationSync);
        constexpr float scale = 4.0f;
        const auto img = knob.createComponentSnapshot (knob.getLocalBounds(), true, scale);
        const auto c = juce::Point<float> (img.getWidth() * 0.5f, img.getHeight() * 0.5f);
        double sx = 0.0, sy = 0.0;
        int count = 0;
        for (int y = 0; y < img.getHeight(); ++y)
            for (int x = 0; x < img.getWidth(); ++x)
            {
                const auto px = img.getPixelAt (x, y);
                const auto d = juce::Point<float> (static_cast<float> (x), static_cast<float> (y)).getDistanceFrom (c);
                if (px.getAlpha() > 200 && px.getBrightness() > 0.8f && d < 15.0f * scale)
                {
                    sx += x - c.x;
                    sy += y - c.y;
                    ++count;
                }
            }
        REQUIRE (count > 10);
        return juce::radiansToDegrees (std::atan2 (sx, -sy));
    };

    CHECK (std::abs (pointerAngle (0.0) - (-135.0)) < 8.0);
    CHECK (std::abs (pointerAngle (0.5) - 0.0) < 8.0);
    CHECK (std::abs (pointerAngle (1.0) - 135.0) < 8.0);
    knob.setLookAndFeel (nullptr);
}

TEST_CASE_METHOD (JuceFixture, "DELAY skin: images live only while an editor exists (no static JUCE images)", "[gui][skin][lifecycle]")
{
#if JUCE_LINUX
    if (std::getenv ("DISPLAY") == nullptr)
        SKIP ("No display available");
#endif
    using sfx::ui::Skin;
    CHECK (Skin::liveInstances() == 0);
    {
        ShobeatsAudioProcessor p;
        prepare (p);
        std::unique_ptr<juce::AudioProcessorEditor> editor (p.createEditorAndMakeActive());
        auto& view = dynamic_cast<sfx::ShobeatsAudioProcessorEditor&> (*editor).getView();
        view.setAdvanced (true);
        view.getPedalboard().select (sfx::EffectId::delay);
        juce::ignoreUnused (editor->createComponentSnapshot (editor->getLocalBounds()));
        CHECK (Skin::liveInstances() == 1);
        p.editorBeingDeleted (editor.get());
        editor.reset();
        CHECK (Skin::liveInstances() == 0);
    }
    CHECK (Skin::liveInstances() == 0);
}

TEST_CASE_METHOD (JuceFixture, "DELAY skin: controls keep driving their parameters", "[gui][skin]")
{
#if JUCE_LINUX
    if (std::getenv ("DISPLAY") == nullptr)
        SKIP ("No display available");
#endif
    ShobeatsAudioProcessor p;
    prepare (p);
    std::unique_ptr<juce::AudioProcessorEditor> editor (p.createEditorAndMakeActive());
    auto& view = dynamic_cast<sfx::ShobeatsAudioProcessorEditor&> (*editor).getView();
    view.setAdvanced (true);
    view.getPedalboard().select (sfx::EffectId::delay);

    // Every skinned slider is still attached: moving it moves the parameter, and the
    // parameter (automation, preset recall) moves it back.
    std::vector<juce::Slider*> skinned;
    std::function<void (juce::Component&)> collect = [&] (juce::Component& c) {
        if (auto* s = dynamic_cast<juce::Slider*> (&c); s != nullptr && s->getProperties()["skin"].toString() == "delay")
            skinned.push_back (s);
        for (auto* child : c.getChildren())
            collect (*child);
    };
    collect (view);
    // Advanced pedal: Time, Division, Feedback. Panel: Time, Division, Feedback, Low Cut, High Cut, Width, Mix.
    CHECK (skinned.size() == 10);
    for (auto* s : skinned)
    {
        const auto before = s->getValue();
        s->setValue (s->proportionOfLengthToValue (s->valueToProportionOfLength (before) > 0.5 ? 0.2 : 0.8), juce::sendNotificationSync);
        CHECK (s->getValue() != before);
    }
    setParam (p, ParamID::delayFeedback, 0.0f);
    for (auto* s : skinned)
        if (s->getTooltip().startsWith ("Delay Feedback"))
            CHECK (s->getValue() == 0.0);

    // Footswitch: still a button bound to Delay On.
    setParam (p, ParamID::delayEnabled, 0.0f);
    juce::Button* footswitch = nullptr;
    std::function<void (juce::Component&)> findSwitch = [&] (juce::Component& c) {
        if (c.getName() == "Footswitch" && c.getParentComponent() != nullptr && c.getParentComponent()->getTitle() == "Delay")
            footswitch = dynamic_cast<juce::Button*> (&c);
        for (auto* child : c.getChildren())
            findSwitch (*child);
    };
    findSwitch (view);
    REQUIRE (footswitch != nullptr);
    footswitch->setToggleState (true, juce::sendNotificationSync); // what a stomp does
    CHECK (p.getState().getRawParameterValue (ParamID::delayEnabled)->load() > 0.5f);

    p.editorBeingDeleted (editor.get());
    editor.reset();
}

TEST_CASE_METHOD (JuceFixture, "DELAY skin: Main and Advanced render at every editor size", "[gui][skin][scaling]")
{
#if JUCE_LINUX
    if (std::getenv ("DISPLAY") == nullptr)
        SKIP ("No display available");
#endif
    ShobeatsAudioProcessor p;
    prepare (p);
    disableAllModules (p);
    setParam (p, ParamID::delayEnabled, 1.0f);
    AnimatedEditor ui { p };
    ui.view.getPedalboard().select (sfx::EffectId::delay);
    for (int i = 0; i < 4; ++i)
        ui.frame (true);

    // Optional: SHOBEATS_SNAPSHOT_DELAY=/path/prefix saves each rendering (local visual check).
    const auto* prefix = std::getenv ("SHOBEATS_SNAPSHOT_DELAY");
    for (auto percent : { 85, 100, 125, 150, 200 })
    {
        const auto w = sfx::MainView::designWidth * percent / 100;
        const auto h = sfx::MainView::designHeight * percent / 100;
        ui.editor->setSize (w, h);
        for (auto advanced : { false, true })
            for (auto on : { true, false })
            {
                setParam (p, ParamID::delayEnabled, on ? 1.0f : 0.0f);
                ui.view.setAdvanced (advanced);
                ui.frame (true);
                const auto start = juce::Time::getMillisecondCounterHiRes();
                const auto image = ui.editor->createComponentSnapshot (ui.editor->getLocalBounds());
                const auto ms = juce::Time::getMillisecondCounterHiRes() - start;
                CHECK (image.getWidth() == w);
                WARN ((advanced ? "advanced " : "main ") << (on ? "on " : "off ") << percent << " %: " << ms << " ms");
                if (prefix != nullptr)
                {
                    juce::File file (juce::String::fromUTF8 (prefix) + (advanced ? "adv" : "main") + (on ? "_on_" : "_off_") + juce::String (percent) + ".png");
                    file.deleteFile();
                    juce::FileOutputStream out (file);
                    CHECK (juce::PNGImageFormat().writeImageToStream (image, out));
                }
            }
    }
    p.editorBeingDeleted (ui.editor.get());
}
