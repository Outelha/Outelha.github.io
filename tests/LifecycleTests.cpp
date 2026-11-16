// In-process lifecycle stress tests (processor + editor creation/destruction).
// The out-of-process VST3 load/unload cycle is covered by tools/lifecycle_host in CI.

#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "ui/Textures.h"

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <cstdlib>
#include <thread>

using sfx::ShobeatsAudioProcessor;

namespace
{
constexpr double sr = 48000.0;
constexpr int block = 256;

struct Fixture
{
    juce::ScopedJuceInitialiser_GUI juce;
};

bool haveDisplay()
{
#if JUCE_LINUX
    return std::getenv ("DISPLAY") != nullptr;
#else
    return true;
#endif
}

void runAudio (ShobeatsAudioProcessor& p, int blocks)
{
    juce::AudioBuffer<float> buffer (2, block);
    juce::MidiBuffer midi;
    juce::Random rng (3);
    for (int b = 0; b < blocks; ++b)
    {
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < block; ++i)
                buffer.setSample (ch, i, (rng.nextFloat() * 2.0f - 1.0f) * 0.3f);
        p.processBlock (buffer, midi);
    }
}

void switchEverythingOn (ShobeatsAudioProcessor& p)
{
    for (const auto& info : sfx::effectInfo)
        p.getState().getParameter (info.enabledId)->setValueNotifyingHost (1.0f);
}

// Closes an editor the way JUCE's plugin wrappers do: tell the processor first, then delete.
void closeEditor (ShobeatsAudioProcessor& p, std::unique_ptr<juce::AudioProcessorEditor>& editor)
{
    p.editorBeingDeleted (editor.get());
    editor.reset();
}
} // namespace

TEST_CASE_METHOD (Fixture, "Lifecycle: 100 x create, process, destroy (no editor)", "[lifecycle]")
{
    for (int i = 0; i < 100; ++i)
    {
        auto p = std::make_unique<ShobeatsAudioProcessor>();
        p->setRateAndBufferSizeDetails (sr, block);
        p->prepareToPlay (sr, block);
        switchEverythingOn (*p);
        runAudio (*p, 20);
        p->releaseResources();
        p.reset();
    }
    SUCCEED();
}

TEST_CASE_METHOD (Fixture, "Lifecycle: 100 x create, process, open and paint editor, close, destroy", "[lifecycle][gui]")
{
    if (! haveDisplay())
        SKIP ("No display available");

    for (int i = 0; i < 100; ++i)
    {
        auto p = std::make_unique<ShobeatsAudioProcessor>();
        p->setRateAndBufferSizeDetails (sr, block);
        p->prepareToPlay (sr, block);
        switchEverythingOn (*p);
        runAudio (*p, 5);

        std::unique_ptr<juce::AudioProcessorEditor> editor (p->createEditorAndMakeActive());
        REQUIRE (editor != nullptr);
        if (i % 2 == 1)
            dynamic_cast<sfx::ShobeatsAudioProcessorEditor&> (*editor).getView().setAdvanced (true);
        juce::ignoreUnused (editor->createComponentSnapshot (editor->getLocalBounds())); // full paint
        runAudio (*p, 5);
        closeEditor (*p, editor);

        p->releaseResources();
        p.reset();
    }
    SUCCEED();
}

TEST_CASE_METHOD (Fixture, "Lifecycle: editor reopened many times on one instance, then destroyed", "[lifecycle][gui]")
{
    if (! haveDisplay())
        SKIP ("No display available");

    auto p = std::make_unique<ShobeatsAudioProcessor>();
    p->setRateAndBufferSizeDetails (sr, block);
    p->prepareToPlay (sr, block);
    for (int i = 0; i < 30; ++i)
    {
        std::unique_ptr<juce::AudioProcessorEditor> editor (p->createEditorAndMakeActive());
        REQUIRE (editor != nullptr);
        juce::ignoreUnused (editor->createComponentSnapshot (editor->getLocalBounds()));
        runAudio (*p, 2);
        closeEditor (*p, editor);
        CHECK (p->getActiveEditor() == nullptr);
    }
    p->releaseResources();
    p.reset();
}

TEST_CASE_METHOD (Fixture, "Lifecycle: textures live only while an editor exists (no static JUCE images)", "[lifecycle][gui]")
{
    if (! haveDisplay())
        SKIP ("No display available");

    // Regression for the plugin-removal crash: texture images used to be function-level
    // statics, i.e. Direct2D images on Windows destroyed only at DLL unload.
    using sfx::ui::Textures;
    CHECK (Textures::liveInstances() == 0);
    {
        ShobeatsAudioProcessor p, q;
        std::unique_ptr<juce::AudioProcessorEditor> a (p.createEditorAndMakeActive());
        juce::ignoreUnused (a->createComponentSnapshot (a->getLocalBounds()));
        CHECK (Textures::liveInstances() == 1);

        std::unique_ptr<juce::AudioProcessorEditor> b (q.createEditorAndMakeActive());
        juce::ignoreUnused (b->createComponentSnapshot (b->getLocalBounds()));
        CHECK (Textures::liveInstances() == 1); // shared between editors

        closeEditor (p, a);
        CHECK (Textures::liveInstances() == 1); // still used by the second editor
        closeEditor (q, b);
        CHECK (Textures::liveInstances() == 0); // released with the last editor
    }
    CHECK (Textures::liveInstances() == 0);
}

TEST_CASE_METHOD (Fixture, "Lifecycle: audio thread runs while editors open and close, then stops before destroy", "[lifecycle][gui]")
{
    if (! haveDisplay())
        SKIP ("No display available");

    for (int i = 0; i < 20; ++i)
    {
        auto p = std::make_unique<ShobeatsAudioProcessor>();
        p->setRateAndBufferSizeDetails (sr, block);
        p->prepareToPlay (sr, block);
        switchEverythingOn (*p);

        std::atomic<bool> running { true };
        std::thread audio ([&] {
            while (running.load())
                runAudio (*p, 1);
        });
        for (int k = 0; k < 3; ++k)
        {
            std::unique_ptr<juce::AudioProcessorEditor> editor (p->createEditorAndMakeActive());
            REQUIRE (editor != nullptr);
            juce::ignoreUnused (editor->createComponentSnapshot (editor->getLocalBounds()));
            // Presets and generated ideas switch while audio runs.
            p->getPresetManager().loadNext (1);
            p->getPresetManager().generate (static_cast<sfx::Generator::Vibe> (k));
            closeEditor (*p, editor);
        }
        running = false;
        audio.join();
        p->releaseResources();
        p.reset();
    }
    SUCCEED();
}

TEST_CASE_METHOD (Fixture, "Lifecycle: processing before prepareToPlay or after releaseResources is safe", "[lifecycle]")
{
    // JUCE's VST3 wrapper forwards process() without checking that the plugin is active, and
    // FL Studio is documented (juce_audio_plugin_client_VST3.cpp) to call process() before or
    // during setActive(). The processor must not touch unprepared DSP state.
    juce::MidiBuffer midi;
    for (int numChannels : { 1, 2 })
    {
        ShobeatsAudioProcessor p;
        switchEverythingOn (p);
        juce::AudioBuffer<float> buffer (numChannels, 512);
        buffer.clear();
        buffer.setSample (0, 0, 0.5f);

        p.processBlock (buffer, midi);         // never prepared
        p.processBlockBypassed (buffer, midi); // never prepared
        for (int ch = 0; ch < numChannels; ++ch)
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                REQUIRE (std::isfinite (buffer.getSample (ch, i)));

        p.setRateAndBufferSizeDetails (sr, block);
        p.prepareToPlay (sr, block);
        runAudio (p, 4);
        p.releaseResources();
        p.processBlock (buffer, midi);         // after release
        p.processBlockBypassed (buffer, midi);
        p.prepareToPlay (sr, block);           // re-activation works
        runAudio (p, 4);
    }
    SUCCEED();
}
