#include "PluginProcessor.h"

#include <catch2/catch_test_macros.hpp>

#include <set>

using sfx::Generator;
using sfx::Preset;
using sfx::ShobeatsAudioProcessor;

namespace
{
constexpr double sr = 48000.0;
constexpr int block = 512;

struct Fixture
{
    juce::ScopedJuceInitialiser_GUI juce;
};

juce::AudioBuffer<float> testSignal()
{
    const auto n = static_cast<int> (sr * 2.5);
    juce::AudioBuffer<float> b (2, n);
    b.clear();
    juce::Random rng (7);
    for (int i = 0; i < static_cast<int> (sr * 1.8); ++i)
    {
        const auto t = static_cast<float> (i) / static_cast<float> (sr);
        float s = 0.0f;
        for (auto f : { 196.0f, 246.94f, 293.66f })
            s += std::sin (juce::MathConstants<float>::twoPi * f * t) + 0.3f * std::sin (juce::MathConstants<float>::twoPi * 2.0f * f * t);
        s *= 0.1f * std::exp (-1.5f * std::fmod (t, 0.9f));
        const auto hit = (rng.nextFloat() * 2.0f - 1.0f) * 0.3f * std::exp (-45.0f * std::fmod (t, 0.3f));
        b.setSample (0, i, s + hit);
        b.setSample (1, i, s * 0.92f + hit);
    }
    return b;
}

juce::AudioBuffer<float> render (const Preset& preset, const juce::AudioBuffer<float>& in)
{
    ShobeatsAudioProcessor p;
    preset.applyTo (p.getState(), p.getChainOrder());
    p.setRateAndBufferSizeDetails (sr, block);
    p.prepareToPlay (sr, block);
    juce::AudioBuffer<float> out (in);
    juce::MidiBuffer midi;
    for (int start = 0; start < out.getNumSamples(); start += block)
    {
        juce::AudioBuffer<float> chunk (out.getArrayOfWritePointers(), 2, start, std::min (block, out.getNumSamples() - start));
        p.processBlock (chunk, midi);
    }
    return out;
}

double rmsDb (const juce::AudioBuffer<float>& b, int to)
{
    double acc = 0.0;
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < to; ++i)
            acc += b.getSample (ch, i) * b.getSample (ch, i);
    return 10.0 * std::log10 (acc / (2.0 * to) + 1.0e-12);
}

juce::String signature (const Preset& p)
{
    juce::String s;
    for (auto id : p.chain)
        s << id << ",";
    for (const auto& nv : p.params)
        s << nv.name.toString() << "=" << nv.value.toString() << ";";
    return s;
}
} // namespace

TEST_CASE_METHOD (Fixture, "Generator: deterministic per seed, varied across seeds", "[generator]")
{
    for (int v = 0; v < Generator::numVibes; ++v)
    {
        const auto vibe = static_cast<Generator::Vibe> (v);
        CHECK (signature (Generator::generate (vibe, 1234)) == signature (Generator::generate (vibe, 1234)));

        std::set<juce::String> results;
        for (juce::uint32 seed = 0; seed < 40; ++seed)
            results.insert (signature (Generator::generate (vibe, seed * 977u + 3u)));
        INFO (Generator::vibeNames()[v]);
        CHECK (results.size() == 40); // every press is a new idea
    }
}

TEST_CASE_METHOD (Fixture, "Generator: results follow the vibe rules and stay in range", "[generator]")
{
    ShobeatsAudioProcessor probe;
    for (int v = 0; v < Generator::numVibes; ++v)
    {
        const auto vibe = static_cast<Generator::Vibe> (v);
        std::set<int> chainSizes;
        for (juce::uint32 seed = 0; seed < 60; ++seed)
        {
            const auto preset = Generator::generate (vibe, seed * 31u + 11u);
            INFO (Generator::vibeNames()[v] << " seed " << seed << " " << preset.name);
            CHECK (preset.chain.size() >= 2);
            CHECK (preset.chain.size() <= 4);
            CHECK (sfx::ChainOrder::isValid (preset.order));
            CHECK (preset.name.isNotEmpty());
            CHECK (preset.category.startsWith ("Generated"));
            chainSizes.insert (static_cast<int> (preset.chain.size()));

            for (const auto& nv : preset.params)
            {
                auto* param = probe.getState().getParameter (nv.name.toString());
                REQUIRE (param != nullptr);
                if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (param))
                    CHECK (choice->choices.contains (nv.value.toString()));
                else if (! nv.value.isBool())
                {
                    const auto range = probe.getState().getParameterRange (nv.name.toString());
                    const auto x = static_cast<float> (static_cast<double> (nv.value));
                    CHECK (x >= range.start);
                    CHECK (x <= range.end);
                }
            }

            // Signature pedals per vibe.
            auto has = [&preset] (sfx::EffectId id) { return std::find (preset.chain.begin(), preset.chain.end(), static_cast<int> (id)) != preset.chain.end(); };
            switch (vibe)
            {
                case Generator::Vibe::vintage: CHECK (has (sfx::EffectId::tape)); break;
                case Generator::Vibe::lofi: CHECK (has (sfx::EffectId::resampler)); break;
                case Generator::Vibe::experimental: CHECK (has (sfx::EffectId::reverse)); break;
                default: CHECK (has (sfx::EffectId::reverb)); break;
            }
        }
        CHECK (chainSizes.size() >= 2); // not always the same number of pedals
    }
}

TEST_CASE_METHOD (Fixture, "Generator: audio checks - safe level and audible for every vibe", "[generator][audio]")
{
    const auto input = testSignal();
    const auto end = static_cast<int> (sr * 1.8);
    const auto inDb = rmsDb (input, end);
    juce::String report;
    for (int v = 0; v < Generator::numVibes; ++v)
        for (juce::uint32 seed = 0; seed < 10; ++seed)
        {
            const auto preset = Generator::generate (static_cast<Generator::Vibe> (v), seed * 101u + 5u);
            const auto out = render (preset, input);
            const auto peak = out.getMagnitude (0, out.getNumSamples());
            const auto change = rmsDb (out, end) - inDb;
            report << Generator::vibeNames()[v] << " " << preset.name << ": peak " << juce::String (juce::Decibels::gainToDecibels (peak), 1)
                   << " dBFS, level " << juce::String (change, 1) << " dB\n";
            INFO (Generator::vibeNames()[v] << " seed " << seed << " " << preset.name << " (" << preset.description << ")");
            bool finite = true;
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < out.getNumSamples(); ++i)
                    finite = finite && std::isfinite (out.getSample (ch, i));
            CHECK (finite);
            CHECK (peak < 1.0f);
            CHECK (change > -14.0);
            CHECK (change < 5.0);
        }
    if (const auto* path = std::getenv ("SHOBEATS_GENERATOR_REPORT"))
        juce::File (juce::String::fromUTF8 (path)).replaceWithText (report);
}

TEST_CASE_METHOD (Fixture, "Generator: a generated idea can be saved and reloaded as a user preset", "[generator]")
{
    juce::TemporaryFile dir;
    ShobeatsAudioProcessor p;
    auto& pm = p.getPresetManager();
    pm.setUserDirectory (dir.getFile());

    const auto& idea = pm.generate (Generator::Vibe::dreamy);
    CHECK (pm.getCurrentName() == idea.name);
    CHECK (p.getChainOrder().get() == idea.order);
    const auto snapshot = Preset::capture (p.getState(), p.getChainOrder());

    REQUIRE (pm.saveUserPreset ("Keeper", "favourite generated idea").has_value());
    pm.generate (Generator::Vibe::lofi); // something else
    REQUIRE (pm.load (pm.indexOf ("Keeper", sfx::PresetManager::userCategory)));
    CHECK (p.getChainOrder().get() == snapshot.order);
    // The saved preset reproduces the generated settings.
    const auto reloaded = Preset::capture (p.getState(), p.getChainOrder());
    for (const auto& nv : snapshot.params)
    {
        INFO (nv.name.toString());
        if (nv.value.isString() || nv.value.isBool())
            CHECK (reloaded.params[nv.name].toString() == nv.value.toString());
        else
            CHECK (std::abs (static_cast<double> (reloaded.params[nv.name]) - static_cast<double> (nv.value)) < 1.0e-3 * std::max (1.0, std::abs (static_cast<double> (nv.value))));
    }
    dir.getFile().deleteRecursively();
}
