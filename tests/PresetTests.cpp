#include "PluginProcessor.h"

#include <catch2/catch_test_macros.hpp>

#include <map>
#include <set>

using sfx::Preset;
using sfx::PresetManager;
using sfx::ShobeatsAudioProcessor;

namespace
{
constexpr double sr = 48000.0;
constexpr int block = 512;

struct Fixture
{
    juce::ScopedJuceInitialiser_GUI juce;
    juce::TemporaryFile userDir;
};

void prepare (ShobeatsAudioProcessor& p)
{
    p.setRateAndBufferSizeDetails (sr, block);
    p.prepareToPlay (sr, block);
}

// A short "musical" test signal: a decaying chord (piano-like partials) and
// a few drum-like noise hits, peaking around -6 dBFS, followed by silence.
juce::AudioBuffer<float> testSignal (double seconds = 3.0)
{
    const auto n = static_cast<int> (sr * seconds);
    juce::AudioBuffer<float> b (2, n);
    b.clear();
    juce::Random rng (42);
    const float notes[] = { 220.0f, 277.18f, 329.63f, 440.0f };
    for (int i = 0; i < static_cast<int> (sr * 2.0); ++i)
    {
        const auto t = static_cast<float> (i) / static_cast<float> (sr);
        float chord = 0.0f;
        for (auto f : notes)
            for (int h = 1; h <= 4; ++h)
                chord += std::sin (juce::MathConstants<float>::twoPi * f * static_cast<float> (h) * t) / static_cast<float> (h * h);
        chord *= 0.09f * std::exp (-1.6f * std::fmod (t, 1.0f));

        // Hits every 250 ms.
        const auto beatT = std::fmod (t, 0.25f);
        const auto hit = (rng.nextFloat() * 2.0f - 1.0f) * 0.35f * std::exp (-40.0f * beatT);

        b.setSample (0, i, chord + hit);
        b.setSample (1, i, chord * 0.9f + hit);
    }
    return b;
}

juce::AudioBuffer<float> render (ShobeatsAudioProcessor& p, const juce::AudioBuffer<float>& in)
{
    juce::AudioBuffer<float> out (in);
    juce::MidiBuffer midi;
    for (int start = 0; start < out.getNumSamples(); start += block)
    {
        const auto len = std::min (block, out.getNumSamples() - start);
        juce::AudioBuffer<float> chunk (out.getArrayOfWritePointers(), 2, start, len);
        p.processBlock (chunk, midi);
    }
    return out;
}

// Per-frame features (dB): level L, level R, side level, brightness (first difference).
std::vector<float> features (const juce::AudioBuffer<float>& b)
{
    constexpr int frame = 2400; // 50 ms
    std::vector<float> f;
    for (int start = 0; start + frame <= b.getNumSamples(); start += frame)
    {
        double l = 0, r = 0, side = 0, bright = 0;
        for (int i = start; i < start + frame; ++i)
        {
            const auto a = b.getSample (0, i), c = b.getSample (1, i);
            l += a * a;
            r += c * c;
            side += (a - c) * (a - c);
            const auto d = i > 0 ? a - b.getSample (0, i - 1) : 0.0f;
            bright += d * d;
        }
        for (auto v : { l, r, side, bright })
            f.push_back (static_cast<float> (10.0 * std::log10 (v / frame + 1.0e-9)));
    }
    return f;
}

double featureDistance (const std::vector<float>& a, const std::vector<float>& b)
{
    double d = 0.0;
    for (size_t i = 0; i < a.size(); ++i)
        d += std::abs (a[i] - b[i]);
    return d / static_cast<double> (a.size());
}

double rmsDb (const juce::AudioBuffer<float>& b, int from, int to)
{
    double acc = 0.0;
    for (int ch = 0; ch < 2; ++ch)
        for (int i = from; i < to; ++i)
            acc += b.getSample (ch, i) * b.getSample (ch, i);
    return 10.0 * std::log10 (acc / (2.0 * (to - from)) + 1.0e-12);
}
} // namespace

TEST_CASE_METHOD (Fixture, "Presets: factory library is complete and well formed", "[presets]")
{
    ShobeatsAudioProcessor p;
    auto& pm = p.getPresetManager();
    INFO (pm.getLoadErrors().joinIntoString ("\n"));
    CHECK (pm.getLoadErrors().isEmpty());
    REQUIRE (pm.getNumFactoryPresets() == 60);

    std::map<juce::String, int> perCategory;
    std::set<juce::String> names;
    for (int i = 0; i < pm.getNumFactoryPresets(); ++i)
    {
        const auto& preset = pm.getPresets()[static_cast<size_t> (i)];
        INFO (preset.name);
        CHECK (preset.isFactory);
        CHECK (preset.description.length() > 30);
        CHECK (names.insert (preset.name).second);
        CHECK (PresetManager::factoryCategories().contains (preset.category));
        ++perCategory[preset.category];

        // A real chain: at least one pedal, never everything at once.
        CHECK (! preset.chain.empty());
        CHECK (preset.chain.size() < static_cast<size_t> (sfx::numEffects));
        CHECK (sfx::ChainOrder::isValid (preset.order));

        // Every stored value names a real parameter and lies in its range.
        for (const auto& nv : preset.params)
        {
            INFO (nv.name.toString());
            auto* param = p.getState().getParameter (nv.name.toString());
            REQUIRE (param != nullptr);
            if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (param))
                CHECK (choice->choices.contains (nv.value.toString()));
            else if (! nv.value.isBool())
            {
                const auto range = p.getState().getParameterRange (nv.name.toString());
                const auto v = static_cast<float> (static_cast<double> (nv.value));
                CHECK (v >= range.start);
                CHECK (v <= range.end);
            }
        }
    }
    for (const auto& category : PresetManager::factoryCategories())
    {
        INFO (category);
        CHECK (perCategory[category] == 10);
    }
}

TEST_CASE_METHOD (Fixture, "Presets: every preset applies exactly to parameters, switches and order", "[presets]")
{
    ShobeatsAudioProcessor p;
    auto& pm = p.getPresetManager();
    for (int i = 0; i < pm.getNumFactoryPresets(); ++i)
    {
        // Start from a messy state to prove the preset fully defines the sound.
        for (auto* param : p.getParameters())
            param->setValueNotifyingHost (0.77f);
        p.getChainOrder().move (0, 7);

        REQUIRE (pm.load (i));
        const auto& preset = pm.getPresets()[static_cast<size_t> (i)];
        INFO (preset.name);
        CHECK (pm.getCurrentName() == preset.name);
        CHECK (p.getChainOrder().get() == preset.order);

        for (int e = 0; e < sfx::numEffects; ++e)
        {
            const bool shouldBeOn = std::find (preset.chain.begin(), preset.chain.end(), e) != preset.chain.end();
            CHECK ((p.getState().getRawParameterValue (sfx::effectInfo[static_cast<size_t> (e)].enabledId)->load() > 0.5f) == shouldBeOn);
        }

        for (auto* id : sfx::allParameterIds)
        {
            if (juce::String (id).endsWith ("Enabled"))
                continue;
            auto* param = p.getState().getParameter (id);
            const auto* stored = preset.params.getVarPointer (id);
            float expected = param->getDefaultValue();
            if (stored != nullptr)
            {
                if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (param); choice != nullptr && stored->isString())
                    expected = choice->convertTo0to1 (static_cast<float> (choice->choices.indexOf (stored->toString())));
                else
                    expected = param->convertTo0to1 (static_cast<float> (static_cast<double> (*stored)));
            }
            INFO (id);
            CHECK (std::abs (param->getValue() - expected) < 1.0e-3f);
        }
    }
}

TEST_CASE_METHOD (Fixture, "Presets: audio checks - safe level, audible effect, all distinguishable", "[presets][audio]")
{
    const auto input = testSignal();
    const auto signalEnd = static_cast<int> (sr * 2.0);
    const auto inputDb = rmsDb (input, 0, signalEnd);

    ShobeatsAudioProcessor dryProc;
    prepare (dryProc);
    const auto dryOut = render (dryProc, input); // same latency as the processed renders
    const auto dryFeatures = features (dryOut);

    // Energy of (processed - dry) relative to the dry signal, in dB.
    auto residualDb = [&dryOut, signalEnd] (const juce::AudioBuffer<float>& out) {
        double diff = 0.0, ref = 0.0;
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < signalEnd; ++i)
            {
                const auto d = dryOut.getSample (ch, i);
                diff += (out.getSample (ch, i) - d) * (out.getSample (ch, i) - d);
                ref += d * d;
            }
        return 10.0 * std::log10 (diff / ref + 1.0e-12);
    };

    ShobeatsAudioProcessor probe;
    const auto numPresets = probe.getPresetManager().getNumFactoryPresets();
    std::vector<std::vector<float>> allFeatures;
    std::vector<juce::String> names;

    juce::String report = "\npreset | peak dBFS | level change dB | residual vs dry dB | feature distance from dry\n";
    for (int i = 0; i < numPresets; ++i)
    {
        ShobeatsAudioProcessor p;
        p.getPresetManager().load (i);
        prepare (p); // fresh instance: no transition fade in the measurement
        const auto out = render (p, input);
        const auto& name = p.getPresetManager().getPresets()[static_cast<size_t> (i)].name;

        float peak = 0.0f;
        bool finite = true;
        for (int ch = 0; ch < 2; ++ch)
            for (int s = 0; s < out.getNumSamples(); ++s)
            {
                const auto v = out.getSample (ch, s);
                finite = finite && std::isfinite (v);
                peak = std::max (peak, std::abs (v));
            }
        const auto levelChange = rmsDb (out, 0, signalEnd) - inputDb;
        auto f = features (out);
        const auto fromDry = featureDistance (f, dryFeatures);
        const auto residual = residualDb (out);

        report << name << " | " << juce::String (juce::Decibels::gainToDecibels (peak), 1) << " | "
               << juce::String (levelChange, 1) << " | " << juce::String (residual, 1) << " | " << juce::String (fromDry, 2) << "\n";

        INFO (name);
        CHECK (finite);
        CHECK (peak < 1.0f);                       // input peaks near -6 dBFS: no preset may clip
        CHECK (levelChange > -12.0);               // still clearly audible
        CHECK (levelChange < 4.0);                 // no big jump in loudness
        CHECK (residual > -20.0);                  // the processing changes the waveform substantially

        allFeatures.push_back (std::move (f));
        names.push_back (name);
    }

    // Every pair of presets must differ measurably.
    double closest = 1.0e9;
    juce::String closestPair;
    for (size_t a = 0; a < allFeatures.size(); ++a)
        for (size_t b = a + 1; b < allFeatures.size(); ++b)
        {
            const auto d = featureDistance (allFeatures[a], allFeatures[b]);
            if (d < closest)
            {
                closest = d;
                closestPair = names[a] + " / " + names[b];
            }
        }
    report << "closest pair: " << closestPair << " (" << juce::String (closest, 2) << ")\n";
    UNSCOPED_INFO (report);
    CHECK (closest > 0.5);

    if (const auto* path = std::getenv ("SHOBEATS_PRESET_REPORT"))
        juce::File (juce::String::fromUTF8 (path)).replaceWithText (report);
}

TEST_CASE_METHOD (Fixture, "Presets: switching while playing is click- and spike-free", "[presets][audio]")
{
    ShobeatsAudioProcessor p;
    auto& pm = p.getPresetManager();
    const auto input = testSignal (6.0);

    // Measure the loudest steady-state peak among the presets involved.
    const int a = pm.indexOf ("Lost in Clouds", "Dreamy / Ambient");
    const int b = pm.indexOf ("Bitcrushed Basement", "Lo-Fi / Texture");
    const int c = pm.indexOf ("Wormhole Echo", "Space / Experimental");
    REQUIRE (a >= 0);
    REQUIRE (b >= 0);
    REQUIRE (c >= 0);

    float steadyPeak = 0.0f;
    for (int idx : { a, b, c })
    {
        ShobeatsAudioProcessor q;
        q.getPresetManager().load (idx);
        prepare (q);
        const auto out = render (q, input);
        steadyPeak = std::max (steadyPeak, out.getMagnitude (0, out.getNumSamples()));
    }

    // Now switch a -> b -> c -> a while audio runs, every 0.5 s, mid-note.
    juce::AudioBuffer<float> out (input);
    juce::MidiBuffer midi;
    const int switchEvery = static_cast<int> (sr * 0.5);
    int nextSwitch = switchEvery, which = 0;
    const int sequence[] = { a, b, c, a, b, c, a, b, c, a };
    pm.load (sequence[which++]);
    prepare (p); // playback starts with the first preset already active
    std::vector<int> switchPoints;
    for (int start = 0; start < out.getNumSamples(); start += block)
    {
        if (start >= nextSwitch && which < 10)
        {
            pm.load (sequence[which++]);
            switchPoints.push_back (start);
            nextSwitch += switchEvery;
        }
        const auto len = std::min (block, out.getNumSamples() - start);
        juce::AudioBuffer<float> chunk (out.getArrayOfWritePointers(), 2, start, len);
        p.processBlock (chunk, midi);
    }

    const auto peak = out.getMagnitude (0, out.getNumSamples());
    int peakAt = 0;
    for (int i = 0; i < out.getNumSamples(); ++i)
        if (std::abs (out.getSample (0, i)) >= peak * 0.999f || std::abs (out.getSample (1, i)) >= peak * 0.999f)
        {
            peakAt = i;
            break;
        }
    juce::String sw;
    for (auto s2 : switchPoints)
        sw << s2 << " ";
    INFO ("steady peak " << steadyPeak << ", peak with switching " << peak << " at sample " << peakAt << " switches: " << sw);
    CHECK (peak <= steadyPeak * 1.1f + 0.01f);

    // No hard discontinuity around the switch points.
    for (auto sp : switchPoints)
    {
        float maxStep = 0.0f;
        for (int i = std::max (1, sp - 64); i < std::min (out.getNumSamples(), sp + static_cast<int> (sr * 0.06)); ++i)
            maxStep = std::max (maxStep, std::abs (out.getSample (0, i) - out.getSample (0, i - 1)));
        INFO ("switch at " << sp << " max step " << maxStep);
        CHECK (maxStep < 0.35f);
    }
}

TEST_CASE_METHOD (Fixture, "Presets: user presets save, list, load and delete separately from factory", "[presets]")
{
    ShobeatsAudioProcessor p;
    auto& pm = p.getPresetManager();
    const auto dir = userDir.getFile();
    dir.deleteRecursively();
    pm.setUserDirectory (dir);
    const auto factoryCount = pm.getNumFactoryPresets();
    REQUIRE (pm.getPresets().size() == static_cast<size_t> (factoryCount));

    // Shape a sound and save it.
    pm.load (pm.indexOf ("Midnight Ritual", "Dark / Trap"));
    p.getState().getParameter (sfx::ParamID::satDrive)->setValueNotifyingHost (0.9f);
    p.getChainOrder().move (0, 3);
    const auto expectedOrder = p.getChainOrder().get();
    const auto saved = pm.saveUserPreset ("My Ritual", "darker");
    REQUIRE (saved.has_value());
    REQUIRE (pm.getPresets().size() == static_cast<size_t> (factoryCount + 1));
    CHECK (! pm.getPresets().back().isFactory);
    CHECK (pm.getPresets().back().category == PresetManager::userCategory);
    CHECK (dir.getChildFile ("My Ritual.sfxpreset").existsAsFile());

    // A fresh instance finds and loads it identically.
    ShobeatsAudioProcessor q;
    q.getPresetManager().setUserDirectory (dir);
    const auto idx = q.getPresetManager().indexOf ("My Ritual", PresetManager::userCategory);
    REQUIRE (idx >= 0);
    REQUIRE (q.getPresetManager().load (idx));
    CHECK (q.getChainOrder().get() == expectedOrder);
    for (auto* id : sfx::allParameterIds)
    {
        INFO (id);
        CHECK (std::abs (q.getState().getParameter (id)->getValue() - p.getState().getParameter (id)->getValue()) < 1.0e-3f);
    }

    // Factory presets cannot be deleted; user presets can.
    CHECK_FALSE (pm.deleteUserPreset (0));
    CHECK (pm.deleteUserPreset (static_cast<int> (pm.getPresets().size()) - 1));
    CHECK (pm.getPresets().size() == static_cast<size_t> (factoryCount));
    dir.deleteRecursively();
}

TEST_CASE_METHOD (Fixture, "Presets: DAW state recalls the preset name and sound", "[presets]")
{
    ShobeatsAudioProcessor a;
    auto& pm = a.getPresetManager();
    REQUIRE (pm.load (pm.indexOf ("Gravity Zero", "Space / Experimental")));
    juce::MemoryBlock blob;
    a.getStateInformation (blob);

    ShobeatsAudioProcessor b;
    b.setStateInformation (blob.getData(), static_cast<int> (blob.getSize()));
    CHECK (b.getPresetManager().getCurrentName() == "Gravity Zero");
    CHECK (b.getCurrentPresetLabel().second == "Space / Experimental");
    CHECK (b.getChainOrder().get() == a.getChainOrder().get());
    for (auto* id : sfx::allParameterIds)
    {
        INFO (id);
        CHECK (std::abs (b.getState().getParameter (id)->getValue() - a.getState().getParameter (id)->getValue()) < 1.0e-6f);
    }
}

TEST_CASE_METHOD (Fixture, "Presets: next/previous navigation wraps around", "[presets]")
{
    ShobeatsAudioProcessor p;
    auto& pm = p.getPresetManager();
    pm.setUserDirectory (userDir.getFile());
    REQUIRE (pm.loadNext (1));
    CHECK (pm.getCurrentIndex() == 0);
    REQUIRE (pm.loadNext (-1));
    CHECK (pm.getCurrentIndex() == static_cast<int> (pm.getPresets().size()) - 1);
    REQUIRE (pm.loadNext (1));
    CHECK (pm.getCurrentIndex() == 0);
}
