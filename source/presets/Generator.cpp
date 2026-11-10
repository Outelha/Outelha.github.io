#include "Generator.h"

namespace sfx
{
namespace
{
using Vibe = Generator::Vibe;
using E = EffectId;

// Small deterministic RNG wrapper.
struct Rng
{
    explicit Rng (juce::uint32 seed) : r (static_cast<juce::int64> (seed) * 2654435761LL + 12345) {}
    float range (float lo, float hi) { return lo + (hi - lo) * r.nextFloat(); }
    bool chance (float p) { return r.nextFloat() < p; }
    int index (int n) { return r.nextInt (n); }
    template <typename T>
    const T& pick (std::initializer_list<T> list) { return *(list.begin() + index (static_cast<int> (list.size()))); }
    juce::Random r;
};

// Canonical signal-flow rank: colour -> modulation -> reverse -> echo -> space.
int flowRank (E id)
{
    switch (id)
    {
        case E::resampler: return 0;
        case E::tape: return 1;
        case E::saturation: return 2;
        case E::chorus: return 3;
        case E::tremolo: return 4;
        case E::reverse: return 5;
        case E::delay: return 6;
        case E::reverb: return 7;
    }
    return 0;
}

struct Recipe
{
    std::vector<std::pair<E, float>> candidates; // effect, probability
    E signature;                                  // always present
    float driftLo, driftHi, unstableLo, unstableHi;
    float outputTrim;                             // dB, base
    bool shuffleOrder = false;
};

Recipe recipeFor (Vibe v)
{
    switch (v)
    {
        case Vibe::vintage:
            return { { { E::tape, 0.85f }, { E::saturation, 0.6f }, { E::chorus, 0.3f }, { E::tremolo, 0.15f }, { E::delay, 0.35f }, { E::reverb, 0.5f } },
                     E::tape, 0.1f, 0.35f, 0.0f, 0.15f, 0.0f };
        case Vibe::dreamy:
            return { { { E::chorus, 0.7f }, { E::delay, 0.6f }, { E::reverse, 0.3f }, { E::tremolo, 0.2f }, { E::tape, 0.25f }, { E::reverb, 1.0f } },
                     E::reverb, 0.2f, 0.5f, 0.0f, 0.1f, -1.5f };
        case Vibe::dark:
            return { { { E::saturation, 0.6f }, { E::reverse, 0.4f }, { E::delay, 0.6f }, { E::resampler, 0.3f }, { E::chorus, 0.25f }, { E::tape, 0.3f }, { E::reverb, 0.85f } },
                     E::reverb, 0.1f, 0.4f, 0.0f, 0.2f, -1.0f };
        case Vibe::lofi:
            return { { { E::resampler, 0.9f }, { E::tape, 0.85f }, { E::saturation, 0.4f }, { E::chorus, 0.25f }, { E::reverb, 0.35f } },
                     E::resampler, 0.1f, 0.4f, 0.1f, 0.5f, -1.0f };
        case Vibe::space:
            return { { { E::delay, 0.8f }, { E::reverse, 0.6f }, { E::chorus, 0.5f }, { E::resampler, 0.15f }, { E::reverb, 1.0f } },
                     E::reverb, 0.3f, 0.7f, 0.1f, 0.4f, -2.5f };
        case Vibe::experimental:
            return { { { E::saturation, 0.45f }, { E::resampler, 0.45f }, { E::tape, 0.3f }, { E::chorus, 0.4f }, { E::tremolo, 0.55f },
                       { E::reverse, 0.55f }, { E::delay, 0.5f }, { E::reverb, 0.4f } },
                     E::reverse, 0.2f, 0.8f, 0.2f, 0.8f, -2.0f, true };
    }
    return {};
}

// Curated parameter ranges per effect, shaped by the vibe.
void configure (E id, Vibe v, Rng& r, juce::NamedValueSet& p)
{
    const bool dark = v == Vibe::dark, dreamy = v == Vibe::dreamy, space = v == Vibe::space, lofi = v == Vibe::lofi,
               exp = v == Vibe::experimental, vintage = v == Vibe::vintage;
    auto set = [&p] (const char* key, juce::var value) { p.set (key, value); };

    switch (id)
    {
        case E::saturation:
            set (ParamID::satMode, exp ? r.pick<juce::String> ({ "Fuzz", "Fold", "Hard", "Warm" })
                                       : lofi ? r.pick<juce::String> ({ "Warm", "Hard" }) : r.pick<juce::String> ({ "Warm", "Soft" }));
            set (ParamID::satDrive, r.range (exp ? 4.0f : 4.0f, exp ? 16.0f : 14.0f));
            set (ParamID::satTone, dark ? r.range (-0.7f, -0.3f) : vintage ? r.range (-0.3f, 0.1f) : r.range (-0.3f, 0.3f));
            set (ParamID::satMix, r.range (0.35f, 0.8f));
            break;

        case E::resampler:
            set (ParamID::rsRate, lofi ? r.range (6000.0f, 22050.0f) : exp ? r.range (2500.0f, 12000.0f) : r.range (12000.0f, 24000.0f));
            set (ParamID::rsBits, lofi ? r.range (6.0f, 12.0f) : exp ? r.range (4.0f, 10.0f) : r.range (10.0f, 16.0f));
            set (ParamID::rsFilter, dark ? r.range (2500.0f, 5000.0f) : r.range (3000.0f, 9000.0f));
            set (ParamID::rsJitter, exp || space ? r.range (0.2f, 0.7f) : r.range (0.0f, 0.4f));
            set (ParamID::rsMix, r.range (0.45f, 0.85f));
            break;

        case E::tape:
            set (ParamID::tapeDrive, r.range (2.0f, vintage ? 12.0f : 8.0f));
            set (ParamID::tapeWow, lofi ? r.range (0.2f, 0.6f) : r.range (0.08f, 0.35f));
            set (ParamID::tapeFlutter, lofi ? r.range (0.1f, 0.5f) : r.range (0.05f, 0.25f));
            set (ParamID::tapeNoise, lofi ? r.range (0.15f, 0.45f) : r.range (0.0f, 0.2f));
            set (ParamID::tapeAge, lofi || dark ? r.range (0.4f, 0.8f) : r.range (0.15f, 0.5f));
            break;

        case E::chorus:
            set (ParamID::chorusRate, exp ? r.range (0.5f, 3.0f) : r.range (0.15f, 0.8f));
            set (ParamID::chorusDepth, dreamy || space ? r.range (0.45f, 0.9f) : r.range (0.2f, 0.6f));
            set (ParamID::chorusDelay, r.range (8.0f, 22.0f));
            set (ParamID::chorusFeedback, exp || space ? r.range (0.2f, 0.6f) : r.range (0.0f, 0.3f));
            set (ParamID::chorusMix, r.range (0.2f, 0.5f));
            break;

        case E::tremolo:
            set (ParamID::tremSync, true);
            set (ParamID::tremDivision, exp ? r.pick<juce::String> ({ "1/16", "1/32", "1/16 T", "1/8 T" })
                                            : dreamy ? r.pick<juce::String> ({ "1/2", "1/1", "1/4 D" })
                                                     : r.pick<juce::String> ({ "1/8", "1/4", "1/8 T" }));
            set (ParamID::tremShape, exp ? r.pick<juce::String> ({ "Square", "Saw", "Triangle" }) : r.pick<juce::String> ({ "Sine", "Triangle" }));
            set (ParamID::tremDepth, exp ? r.range (0.5f, 0.9f) : r.range (0.15f, 0.45f));
            set (ParamID::tremStereo, r.range (0.3f, 1.0f));
            break;

        case E::reverse:
            set (ParamID::revSync, true);
            set (ParamID::revDivision, exp ? r.pick<juce::String> ({ "1/8", "1/16", "1/4", "1/8 D" }) : r.pick<juce::String> ({ "1/4", "1/2", "1/1" }));
            set (ParamID::revPitch, dark ? r.pick<float> ({ -12.0f, -7.0f, -5.0f })
                                         : space || exp ? r.pick<float> ({ 12.0f, 7.0f, -12.0f, -5.0f, 0.0f }) : r.pick<float> ({ 0.0f, 12.0f }));
            set (ParamID::revMix, r.range (0.2f, exp ? 0.55f : 0.4f));
            break;

        case E::delay:
            set (ParamID::delaySync, true);
            set (ParamID::delayDivision, exp ? r.pick<juce::String> ({ "1/16", "1/16 D", "1/8 T", "1/8 D" })
                                             : r.pick<juce::String> ({ "1/4 D", "1/8 D", "1/4", "1/2" }));
            set (ParamID::delayFeedback, space ? r.range (0.5f, 0.72f) : r.range (0.25f, 0.55f));
            set (ParamID::delayLowCut, r.range (150.0f, 500.0f));
            set (ParamID::delayHighCut, dark ? r.range (1800.0f, 3000.0f) : vintage ? r.range (2500.0f, 5000.0f) : r.range (3500.0f, 8000.0f));
            set (ParamID::delayPingPong, r.chance (dreamy || space || exp ? 0.65f : 0.25f));
            set (ParamID::delayMix, r.range (0.15f, space ? 0.38f : 0.3f));
            break;

        case E::reverb:
            if (space || dreamy)
            {
                set (ParamID::reverbSize, r.range (0.75f, 1.0f));
                set (ParamID::reverbDecay, space ? r.range (7.0f, 16.0f) : r.range (3.5f, 9.0f));
                set (ParamID::reverbPredelay, r.range (20.0f, 70.0f));
                set (ParamID::reverbMix, r.range (0.38f, 0.6f));
            }
            else if (lofi || vintage)
            {
                set (ParamID::reverbSize, r.range (0.3f, 0.6f));
                set (ParamID::reverbDecay, r.range (0.8f, 2.5f));
                set (ParamID::reverbPredelay, r.range (5.0f, 25.0f));
                set (ParamID::reverbMix, r.range (0.12f, 0.25f));
            }
            else
            {
                set (ParamID::reverbSize, r.range (0.55f, 0.95f));
                set (ParamID::reverbDecay, r.range (2.5f, 7.0f));
                set (ParamID::reverbPredelay, r.range (10.0f, 50.0f));
                set (ParamID::reverbMix, r.range (0.28f, 0.45f));
            }
            set (ParamID::reverbDamping, dark || lofi ? r.range (0.65f, 0.85f) : r.range (0.2f, 0.6f));
            break;
    }
}

juce::String nameFor (Vibe v, Rng& r)
{
    static const std::array<std::pair<std::vector<const char*>, std::vector<const char*>>, Generator::numVibes> words { {
        { { "Golden", "Faded", "Smoky", "Velvet", "Amber", "Rusty", "Mellow", "Sepia" }, { "Reel", "Postcard", "Console", "Lounge", "Valve", "Memoir", "Session", "Jukebox" } },
        { { "Floating", "Silver", "Hazy", "Soft", "Drifting", "Pastel", "Weightless", "Moonlit" }, { "Cloud", "Lagoon", "Petal", "Horizon", "Veil", "Lantern", "Tide", "Daydream" } },
        { { "Shadow", "Midnight", "Obsidian", "Hollow", "Cold", "Ashen", "Haunted", "Black" }, { "Ritual", "Alley", "Omen", "Crypt", "Signal", "Mirror", "Choir", "Smoke" } },
        { { "Dusty", "Worn", "Grainy", "Crumpled", "Thrift", "Cracked", "Muffled", "Static" }, { "Cassette", "Polaroid", "Crate", "Tape", "Bedroom", "Sampler", "Radio", "Notebook" } },
        { { "Orbital", "Zero-G", "Distant", "Cosmic", "Infinite", "Stellar", "Lunar", "Deep" }, { "Nebula", "Drift", "Station", "Horizon", "Echo", "Void", "Comet", "Field" } },
        { { "Glitched", "Inverted", "Fractured", "Mutant", "Elastic", "Folded", "Scrambled", "Prism" }, { "Machine", "Signal", "Loop", "Circuit", "Stutter", "Maze", "Engine", "Splice" } },
    } };
    const auto& [adj, noun] = words[static_cast<size_t> (v)];
    return juce::String (adj[static_cast<size_t> (r.index (static_cast<int> (adj.size())))]) + " "
           + noun[static_cast<size_t> (r.index (static_cast<int> (noun.size())))];
}
} // namespace

juce::StringArray Generator::vibeNames()
{
    return { "Vintage", "Dreamy", "Dark", "Lo-Fi", "Space", "Experimental" };
}

Preset Generator::generate (Vibe vibe, juce::uint32 seed)
{
    Rng r (seed + static_cast<juce::uint32> (vibe) * 7919u);
    const auto recipe = recipeFor (vibe);

    // Pick pedals: signature always, others by probability, 2..4 in total.
    std::vector<E> chosen { recipe.signature };
    auto candidates = recipe.candidates;
    for (size_t i = candidates.size(); i > 1; --i) // shuffle so the cap is fair
        std::swap (candidates[i - 1], candidates[static_cast<size_t> (r.index (static_cast<int> (i)))]);
    for (const auto& [id, probability] : candidates)
        if (id != recipe.signature && chosen.size() < 4 && r.chance (probability))
            chosen.push_back (id);
    while (chosen.size() < 2)
    {
        const auto& extra = candidates[static_cast<size_t> (r.index (static_cast<int> (candidates.size())))].first;
        if (std::find (chosen.begin(), chosen.end(), extra) == chosen.end())
            chosen.push_back (extra);
    }

    if (recipe.shuffleOrder)
    {
        for (size_t i = chosen.size(); i > 1; --i)
            std::swap (chosen[i - 1], chosen[static_cast<size_t> (r.index (static_cast<int> (i)))]);
    }
    else
    {
        std::sort (chosen.begin(), chosen.end(), [] (E a, E b) { return flowRank (a) < flowRank (b); });
    }

    Preset preset;
    preset.isFactory = false;
    for (auto id : chosen)
    {
        preset.chain.push_back (static_cast<int> (id));
        configure (id, vibe, r, preset.params);
    }

    // Full order: chain, then the unused pedals in default order.
    size_t slot = 0;
    for (auto id : preset.chain)
        preset.order[slot++] = id;
    for (int id = 0; id < numEffects; ++id)
        if (std::find (preset.chain.begin(), preset.chain.end(), id) == preset.chain.end())
            preset.order[slot++] = id;

    // Analog movement and loudness trim.
    preset.params.set (ParamID::drift, r.range (recipe.driftLo, recipe.driftHi));
    preset.params.set (ParamID::unstable, r.range (recipe.unstableLo, recipe.unstableHi));

    auto trim = recipe.outputTrim;
    auto has = [&chosen] (E id) { return std::find (chosen.begin(), chosen.end(), id) != chosen.end(); };
    if (has (E::delay) && static_cast<float> (preset.params[ParamID::delayFeedback]) > 0.6f)
        trim -= 1.0f;
    if (has (E::saturation) && has (E::tape))
        trim -= 0.5f;
    if (has (E::tremolo) && static_cast<float> (preset.params[ParamID::tremDepth]) > 0.6f)
        trim += 1.5f; // deep chopping removes energy
    preset.params.set (ParamID::outputGain, trim);

    preset.name = nameFor (vibe, r);
    preset.category = "Generated / " + vibeNames()[static_cast<int> (vibe)];
    juce::StringArray chainNames;
    for (auto id : chosen)
        chainNames.add (infoFor (id).name);
    preset.description = "Generated " + vibeNames()[static_cast<int> (vibe)] + " idea: " + chainNames.joinIntoString (" > ");
    return preset;
}
} // namespace sfx
