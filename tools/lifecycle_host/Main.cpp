// SHOBEATS FX plugin lifecycle stress host.
//
// Loads the real built VST3 through JUCE's VST3 hosting (which unloads the module
// with ExitDll + FreeLibrary/dlclose once the last instance is gone) and runs host-like
// lifecycle sequences many times. Prints every step so a crash log shows exactly where
// it stopped. Exit code 0 = every scenario completed.
//
// This approximates what a DAW does when inserting/removing a plugin; it is not FL Studio.

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

#if JUCE_WINDOWS
 #include <windows.h>
#endif

#include <atomic>
#include <thread>

namespace
{
using namespace juce;

constexpr double sampleRate = 48000.0;
constexpr int blockSize = 512;

void log (const String& s)
{
    std::fprintf (stdout, "%s\n", s.toRawUTF8());
    std::fflush (stdout);
}

void pump (int ms)
{
    MessageManager::getInstance()->runDispatchLoopUntil (ms);
}

bool moduleLoaded()
{
#if JUCE_WINDOWS
    return GetModuleHandleW (L"ShobeatsFX.vst3") != nullptr;
#else
    return false;
#endif
}

struct Host
{
    AudioPluginFormatManager formats;
    PluginDescription description;

    explicit Host (const File& bundle)
    {
        formats.addFormat (std::make_unique<VST3PluginFormat>());
        OwnedArray<PluginDescription> found;
        VST3PluginFormat vst3;
        vst3.findAllTypesForFile (found, bundle.getFullPathName());
        if (found.isEmpty())
        {
            log ("ERROR: no plugin found in " + bundle.getFullPathName());
            std::exit (2);
        }
        description = *found[0];
        log ("Plugin: " + description.name + " (" + description.fileOrIdentifier + ")");
    }

    std::unique_ptr<AudioPluginInstance> create()
    {
        String error;
        auto instance = formats.createPluginInstance (description, sampleRate, blockSize, error);
        if (instance == nullptr)
        {
            log ("ERROR: createPluginInstance failed: " + error);
            std::exit (3);
        }
        return instance;
    }
};

void prepare (AudioPluginInstance& p)
{
    p.setPlayConfigDetails (2, 2, sampleRate, blockSize);
    p.prepareToPlay (sampleRate, blockSize);
}

void process (AudioPluginInstance& p, int blocks, int seed = 1)
{
    AudioBuffer<float> buffer (2, blockSize);
    MidiBuffer midi;
    Random rng (seed);
    for (int b = 0; b < blocks; ++b)
    {
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < blockSize; ++i)
                buffer.setSample (ch, i, (rng.nextFloat() * 2.0f - 1.0f) * 0.3f);
        p.processBlock (buffer, midi);
    }
}

// Switch on every pedal and load a few settings so all DSP and UI paths are active.
void enableEverything (AudioPluginInstance& p)
{
    for (auto* param : p.getParameters())
        if (param->getName (64).endsWithIgnoreCase (" On"))
            param->setValueNotifyingHost (1.0f);
}

// Real desktop windows by default. "--offscreen" keeps editors off the desktop (for
// headless X servers without a window manager); they still paint and run their timers.
bool useDesktopWindows = true;

struct EditorWindow final : public DocumentWindow
{
    explicit EditorWindow (AudioProcessorEditor* editor)
        : DocumentWindow ("SHOBEATS FX lifecycle", Colours::black, DocumentWindow::closeButton, useDesktopWindows)
    {
        setUsingNativeTitleBar (true);
        setContentOwned (editor, true);
        if (useDesktopWindows)
            setVisible (true);
        else
            setBounds (0, 0, editor->getWidth(), editor->getHeight());
    }
};

// Opens the editor in a visible window, lets it paint and run its timers, then closes it.
void showEditor (AudioPluginInstance& p, int pumpMs, bool closeWithoutPumping = false)
{
    auto* editor = p.createEditorAndMakeActive();
    if (editor == nullptr)
        return;
    auto window = std::make_unique<EditorWindow> (editor);
    // Force at least one full paint even if the window manager delays exposure.
    { auto snapshot = editor->createComponentSnapshot (editor->getLocalBounds()); }
    pump (pumpMs);
    window.reset(); // deletes the editor (content owned)
    if (! closeWithoutPumping)
        pump (20);
}

void finishInstance (std::unique_ptr<AudioPluginInstance> p)
{
    p->releaseResources();
    p.reset();
}

} // namespace

int main (int argc, char* argv[])
{
    if (argc < 2)
    {
        std::fprintf (stderr, "usage: %s <path to ShobeatsFX.vst3> [scale] [--offscreen]\n", argv[0]);
        return 1;
    }
    for (int a = 2; a < argc; ++a)
        if (String (argv[a]) == "--offscreen")
            useDesktopWindows = false;
    ScopedJuceInitialiser_GUI juceInit;
    const File bundle (String::fromUTF8 (argv[1]));
    const auto scale = argc > 2 ? jmax (1, String (argv[2]).getIntValue()) : 1;
    Host host (bundle);

    auto scenario = [] (const String& name) { log ("=== Scenario " + name); };
    auto progress = [] (const String& name, int i) {
        if (i % 10 == 0)
            log ("  " + name + " #" + String (i) + "  module loaded: " + String (moduleLoaded() ? "yes" : "no"));
    };

    scenario ("A: create / process / destroy, no editor");
    for (int i = 0; i < 100 * scale; ++i)
    {
        progress ("A", i);
        auto p = host.create();
        prepare (*p);
        enableEverything (*p);
        process (*p, 40, i);
        finishInstance (std::move (p));
        pump (5);
    }

    scenario ("B: create / process / editor shown / editor closed / destroy");
    for (int i = 0; i < 100 * scale; ++i)
    {
        progress ("B", i);
        auto p = host.create();
        prepare (*p);
        enableEverything (*p);
        process (*p, 20, i);
        showEditor (*p, 120);
        process (*p, 20, i + 1);
        finishInstance (std::move (p));
        pump (5);
    }

    scenario ("C: editor opened, closed and reopened, then destroy");
    for (int i = 0; i < 30 * scale; ++i)
    {
        progress ("C", i);
        auto p = host.create();
        prepare (*p);
        process (*p, 10, i);
        showEditor (*p, 80);
        showEditor (*p, 80);
        showEditor (*p, 40);
        finishInstance (std::move (p));
        pump (5);
    }

    scenario ("D: editor closed and instance destroyed immediately (no message processing in between)");
    for (int i = 0; i < 100 * scale; ++i)
    {
        progress ("D", i);
        auto p = host.create();
        prepare (*p);
        enableEverything (*p);
        process (*p, 10, i);
        showEditor (*p, 60, true);
        finishInstance (std::move (p));
    }
    pump (50);

    scenario ("E: rapid create / destroy");
    for (int i = 0; i < 200 * scale; ++i)
    {
        progress ("E", i);
        auto p = host.create();
        p.reset();
    }

    scenario ("F: several instances, editors open, removed in mixed order");
    for (int i = 0; i < 20 * scale; ++i)
    {
        progress ("F", i);
        std::vector<std::unique_ptr<AudioPluginInstance>> instances;
        for (int k = 0; k < 3; ++k)
        {
            instances.push_back (host.create());
            prepare (*instances.back());
            enableEverything (*instances.back());
            process (*instances.back(), 10, i + k);
        }
        std::vector<std::unique_ptr<EditorWindow>> windows;
        for (int k = 0; k < 2; ++k)
            if (auto* e = instances[static_cast<size_t> (k)]->createEditorAndMakeActive())
                windows.push_back (std::make_unique<EditorWindow> (e));
        pump (100);
        windows[1].reset();
        finishInstance (std::move (instances[1]));
        pump (20);
        windows[0].reset();
        finishInstance (std::move (instances[0]));
        finishInstance (std::move (instances[2]));
        pump (10);
    }

    scenario ("G: audio thread running while the editor opens and closes, then stop and destroy");
    for (int i = 0; i < 30 * scale; ++i)
    {
        progress ("G", i);
        auto p = host.create();
        prepare (*p);
        enableEverything (*p);
        std::atomic<bool> running { true };
        std::thread audio ([&] {
            int seed = 0;
            while (running.load())
                process (*p, 1, ++seed);
        });
        showEditor (*p, 150);
        showEditor (*p, 50, true);
        running = false;
        audio.join();
        finishInstance (std::move (p));
        pump (10);
    }

    log ("Module loaded after all scenarios: " + String (moduleLoaded() ? "yes" : "no"));
    log ("ALL LIFECYCLE SCENARIOS COMPLETED");
    return 0;
}
