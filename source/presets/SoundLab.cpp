#include "SoundLab.h"

#include "Parameters.h"

namespace sfx::lab
{
namespace
{
using juce::String;
using E = EffectId;
namespace P = ParamID;

struct Rng
{
    explicit Rng (juce::uint32 seed) : r (static_cast<juce::int64> (seed) * 6364136223846793005LL + 1442695040888963407LL) {}
    float range (float lo, float hi) { return lo + (hi - lo) * r.nextFloat(); }
    float logRange (float lo, float hi) { return lo * std::pow (hi / lo, r.nextFloat()); }
    bool chance (float p) { return r.nextFloat() < p; }
    int index (int n) { return r.nextInt (n); }
    template <typename T>
    T pick (std::initializer_list<T> list) { return *(list.begin() + index (static_cast<int> (list.size()))); }
    juce::Random r;
};

// Parameter-writing helpers used by the architecture rules.
struct Ctx
{
    Rng& r;
    juce::NamedValueSet& p;
    std::vector<E>& chain;

    void set (const char* id, float lo, float hi) { p.set (id, r.range (lo, hi)); }
    void setLog (const char* id, float lo, float hi) { p.set (id, r.logRange (lo, hi)); }
    void value (const char* id, juce::var v) { p.set (id, v); }
    void choose (const char* id, std::initializer_list<const char*> options) { p.set (id, String (r.pick (options))); }
    bool has (E e) const { return std::find (chain.begin(), chain.end(), e) != chain.end(); }
    // Inserts an optional pedal before 'before' (or at the end).
    void insertBefore (E e, E before)
    {
        auto it = std::find (chain.begin(), chain.end(), before);
        chain.insert (it, e);
    }
    void append (E e) { chain.push_back (e); }
};

// --- Per-effect building blocks (ranges reflect what SHOBEATS can do musically) ---
void satWarm (Ctx& c, float driveLo, float driveHi) { c.choose (P::satMode, { "Warm", "Soft" }); c.set (P::satDrive, driveLo, driveHi); c.set (P::satTone, -0.5f, 0.1f); c.set (P::satMix, 0.45f, 0.9f); }
void satDark (Ctx& c) { c.choose (P::satMode, { "Warm", "Soft" }); c.set (P::satDrive, 6.0f, 16.0f); c.set (P::satTone, -0.85f, -0.35f); c.set (P::satMix, 0.5f, 0.9f); }
void satAggro (Ctx& c) { c.choose (P::satMode, { "Hard", "Fuzz", "Fold" }); c.set (P::satDrive, 8.0f, 22.0f); c.set (P::satTone, -0.3f, 0.3f); c.set (P::satMix, 0.3f, 0.7f); }
void reverbHall (Ctx& c, float decayLo, float decayHi) { c.set (P::reverbSize, 0.7f, 1.0f); c.set (P::reverbDecay, decayLo, decayHi); c.set (P::reverbDamping, 0.3f, 0.7f); c.set (P::reverbPredelay, 15.0f, 70.0f); c.set (P::reverbMix, 0.3f, 0.55f); }
void reverbRoom (Ctx& c) { c.set (P::reverbSize, 0.2f, 0.45f); c.set (P::reverbDecay, 0.4f, 1.4f); c.set (P::reverbDamping, 0.3f, 0.6f); c.set (P::reverbPredelay, 0.0f, 12.0f); c.set (P::reverbMix, 0.12f, 0.28f); }
void delaySynced (Ctx& c, std::initializer_list<const char*> divisions, float fbLo, float fbHi)
{
    c.value (P::delaySync, true);
    c.choose (P::delayDivision, divisions);
    c.set (P::delayFeedback, fbLo, fbHi);
    c.setLog (P::delayLowCut, 120.0f, 600.0f);
    c.setLog (P::delayHighCut, 2500.0f, 9000.0f);
    c.set (P::delayMix, 0.18f, 0.38f);
}
void tremSynced (Ctx& c, std::initializer_list<const char*> divisions, std::initializer_list<const char*> shapes, float depthLo, float depthHi)
{
    c.value (P::tremSync, true);
    c.choose (P::tremDivision, divisions);
    c.choose (P::tremShape, shapes);
    c.set (P::tremDepth, depthLo, depthHi);
    c.set (P::tremStereo, 0.0f, 1.0f);
}
void reverseSynced (Ctx& c, std::initializer_list<const char*> divisions, std::initializer_list<float> pitches, float mixLo, float mixHi)
{
    c.value (P::revSync, true);
    c.choose (P::revDivision, divisions);
    c.value (P::revPitch, c.r.pick (pitches));
    c.set (P::revMix, mixLo, mixHi);
}
void tapeWorn (Ctx& c, float ageLo, float ageHi) { c.set (P::tapeDrive, 2.0f, 9.0f); c.set (P::tapeWow, 0.15f, 0.5f); c.set (P::tapeFlutter, 0.05f, 0.35f); c.set (P::tapeNoise, 0.05f, 0.35f); c.set (P::tapeAge, ageLo, ageHi); }
void chorusLush (Ctx& c) { c.setLog (P::chorusRate, 0.08f, 0.6f); c.set (P::chorusDepth, 0.5f, 0.95f); c.set (P::chorusDelay, 10.0f, 24.0f); c.set (P::chorusFeedback, 0.0f, 0.4f); c.set (P::chorusWidth, 0.7f, 1.0f); c.set (P::chorusMix, 0.3f, 0.55f); }
void crusher (Ctx& c, float rateLo, float rateHi, float bitsLo, float bitsHi) { c.setLog (P::rsRate, rateLo, rateHi); c.set (P::rsBits, bitsLo, bitsHi); c.setLog (P::rsFilter, 0.25f * rateLo, 0.45f * rateHi); c.set (P::rsJitter, 0.0f, 0.3f); c.set (P::rsMix, 0.45f, 0.85f); }

struct Architecture
{
    const char* id;
    const char* name;
    std::vector<E> chain; // processing order (by design, not canonical)
    const char* purpose;
    const char* instruments;
    const char* character;
    std::function<void (Ctx&)> rules;
    float driftLo, driftHi, unstableLo, unstableHi;
};

std::vector<Architecture> architecturesFor (int category)
{
    switch (category)
    {
        case 0: // Dark & Cinematic
            return {
                { "dark-suboctave-shadow", "Sub-Octave Shadow", { E::reverse, E::saturation, E::reverb },
                  "Turn plain lines into ominous underscore", "piano, strings, pads",
                  "Octave-down reverse ghosts pushed through a dark tube stage into a long, damped hall.",
                  [] (Ctx& c) { reverseSynced (c, { "1/2", "1/1", "1/4 D" }, { -12.0f, -7.0f }, 0.25f, 0.5f); satDark (c); reverbHall (c, 5.0f, 12.0f); c.set (P::reverbDamping, 0.65f, 0.9f); },
                  0.15f, 0.4f, 0.0f, 0.15f },
                { "dark-distant-siren", "Distant Siren", { E::chorus, E::delay, E::reverb },
                  "Make leads sound far away and threatening", "lead synths, vocal chops, guitar",
                  "A slow, resonant chorus feeding dark ping-pong echoes and a cavernous tail.",
                  [] (Ctx& c) { chorusLush (c); c.set (P::chorusFeedback, 0.4f, 0.65f); delaySynced (c, { "1/4 D", "1/2", "1/4" }, 0.5f, 0.7f); c.value (P::delayPingPong, true); c.setLog (P::delayHighCut, 1600.0f, 3200.0f); reverbHall (c, 7.0f, 15.0f); },
                  0.2f, 0.5f, 0.0f, 0.1f },
                { "dark-iron-pulse", "Iron Pulse", { E::tremolo, E::saturation, E::reverb },
                  "Menacing rhythmic stabs for trailers and drill", "brass, strings, stabs, 808 melodies",
                  "Slow square-wave gating hits a hard-driven stage, then a short metallic room.",
                  [] (Ctx& c) { tremSynced (c, { "1/2 D", "1/4 D", "1/2" }, { "Square" }, 0.5f, 0.8f); satAggro (c); c.set (P::satTone, -0.5f, 0.0f); reverbRoom (c); c.set (P::reverbDecay, 1.2f, 2.5f); },
                  0.0f, 0.2f, 0.05f, 0.25f },
                { "dark-tape-dread", "Tape Dread", { E::tape, E::reverb, E::resampler },
                  "Decayed, haunted textures from clean sources", "piano, choir, music box",
                  "Worn tape into a long hall, then the whole tail degraded by a crushed sampler.",
                  [] (Ctx& c) { tapeWorn (c, 0.55f, 0.9f); reverbHall (c, 6.0f, 14.0f); c.set (P::reverbDamping, 0.7f, 0.95f); crusher (c, 9000.0f, 20000.0f, 10.0f, 14.0f); },
                  0.25f, 0.55f, 0.05f, 0.3f },
                { "dark-bloom-rumble", "Saturated Bloom", { E::reverb, E::saturation, E::tremolo },
                  "Swelling cinematic drones", "pads, drones, sustained chords",
                  "A wet reverb driven into warm saturation, its tail breathing with a slow tremolo.",
                  [] (Ctx& c) { reverbHall (c, 8.0f, 18.0f); c.set (P::reverbMix, 0.5f, 0.7f); satWarm (c, 8.0f, 16.0f); c.set (P::satTone, -0.6f, -0.2f); tremSynced (c, { "1/1", "1/2 D", "1/2" }, { "Sine", "Triangle" }, 0.2f, 0.4f); },
                  0.3f, 0.6f, 0.0f, 0.1f },
            };
        case 1: // Melody Transformations
            return {
                { "melody-octave-mirror", "Octave Mirror", { E::reverse, E::delay, E::reverb },
                  "Add an octave-up reversed answer to every phrase", "plucks, keys, bells, guitar",
                  "Short octave-up reverse slices bouncing across ping-pong echoes.",
                  [] (Ctx& c) { reverseSynced (c, { "1/8", "1/4", "1/8 D" }, { 12.0f }, 0.3f, 0.5f); delaySynced (c, { "1/8 D", "1/4", "1/8" }, 0.3f, 0.5f); c.value (P::delayPingPong, true); reverbRoom (c); },
                  0.0f, 0.2f, 0.0f, 0.1f },
                { "melody-fifth-ghost", "Fifth Ghost", { E::reverse, E::chorus, E::delay },
                  "Harmonise melodies with a reversed fifth", "leads, flutes, vocal chops",
                  "A reversed fifth (or fourth below) smeared by chorus and repeated in time.",
                  [] (Ctx& c) { reverseSynced (c, { "1/4", "1/2", "1/4 D" }, { 7.0f, -5.0f }, 0.25f, 0.45f); chorusLush (c); delaySynced (c, { "1/4", "1/8 D", "1/2" }, 0.3f, 0.5f); },
                  0.1f, 0.3f, 0.0f, 0.1f },
                { "melody-glide-wobble", "Glide Wobble", { E::tape, E::chorus, E::reverb },
                  "Detuned, seasick versions of a hook", "keys, synth leads, samples",
                  "Heavy tape wow and a quick chorus make every note bend and drift.",
                  [] (Ctx& c) { tapeWorn (c, 0.2f, 0.5f); c.set (P::tapeWow, 0.6f, 0.95f); c.setLog (P::chorusRate, 0.8f, 2.2f); c.set (P::chorusDepth, 0.5f, 0.85f); c.set (P::chorusMix, 0.4f, 0.65f); reverbRoom (c); c.set (P::reverbMix, 0.15f, 0.3f); },
                  0.4f, 0.75f, 0.1f, 0.35f },
                { "melody-crystal-pluck", "Crystal Pluck", { E::resampler, E::delay, E::reverb },
                  "Turn soft melodies into glassy digital plucks", "bells, plucks, music box, harp",
                  "A light sample-rate crush with bright dotted echoes in a clear space.",
                  [] (Ctx& c) { crusher (c, 16000.0f, 30000.0f, 11.0f, 16.0f); delaySynced (c, { "1/16 D", "1/8 D", "1/16" }, 0.35f, 0.55f); c.value (P::delayPingPong, true); c.setLog (P::delayHighCut, 7000.0f, 14000.0f); reverbHall (c, 2.0f, 4.0f); c.set (P::reverbDamping, 0.1f, 0.35f); c.set (P::reverbMix, 0.2f, 0.35f); },
                  0.0f, 0.15f, 0.0f, 0.1f },
                { "melody-stutter-lead", "Stutter Lead", { E::tremolo, E::delay, E::saturation },
                  "Rhythmic chopping that turns pads into riffs", "pads, held chords, synth leads",
                  "Fast square chopping, triplet echoes, then gentle drive to glue the pieces.",
                  [] (Ctx& c) { tremSynced (c, { "1/16", "1/16 T", "1/8 T" }, { "Square", "Saw" }, 0.6f, 0.9f); delaySynced (c, { "1/8 T", "1/16 D", "1/8" }, 0.2f, 0.4f); satWarm (c, 3.0f, 9.0f); },
                  0.0f, 0.15f, 0.0f, 0.15f },
            };
        case 2: // Vintage & Analog
            return {
                { "vintage-cassette-deck", "Cassette Deck", { E::tape, E::saturation, E::reverb },
                  "Nostalgic cassette colour", "Rhodes, piano, samples, full loops",
                  "Warm, worn tape with soft valve drive and a small room.",
                  [] (Ctx& c) { tapeWorn (c, 0.35f, 0.7f); satWarm (c, 3.0f, 10.0f); reverbRoom (c); },
                  0.15f, 0.4f, 0.05f, 0.2f },
                { "vintage-spring-combo", "Spring Combo", { E::saturation, E::tremolo, E::reverb },
                  "Surf and soul amp character", "guitar, organ, Wurlitzer",
                  "Driven amp, free-running tremolo and a bright, splashy spring-like tank.",
                  [] (Ctx& c) { c.choose (P::satMode, { "Hard", "Warm" }); c.set (P::satDrive, 6.0f, 13.0f); c.set (P::satTone, 0.0f, 0.3f); c.set (P::satMix, 0.4f, 0.75f); c.value (P::tremSync, false); c.set (P::tremRate, 3.5f, 7.0f); c.value (P::tremShape, "Sine"); c.set (P::tremDepth, 0.2f, 0.45f); c.set (P::reverbSize, 0.25f, 0.45f); c.set (P::reverbDecay, 1.6f, 3.2f); c.set (P::reverbDamping, 0.1f, 0.3f); c.set (P::reverbPredelay, 4.0f, 14.0f); c.set (P::reverbMix, 0.2f, 0.35f); },
                  0.05f, 0.25f, 0.0f, 0.1f },
                { "vintage-bbd-echo", "Bucket Echo", { E::chorus, E::delay, E::tape },
                  "Analog bucket-brigade style echo", "guitar, vocals, synth leads",
                  "Subtle chorus into dark free-time echoes, all printed to worn tape.",
                  [] (Ctx& c) { c.setLog (P::chorusRate, 0.25f, 0.7f); c.set (P::chorusDepth, 0.15f, 0.35f); c.set (P::chorusMix, 0.15f, 0.3f); c.value (P::delaySync, false); c.set (P::delayTime, 220.0f, 460.0f); c.set (P::delayFeedback, 0.4f, 0.62f); c.setLog (P::delayLowCut, 180.0f, 450.0f); c.setLog (P::delayHighCut, 2000.0f, 3600.0f); c.set (P::delayMix, 0.2f, 0.35f); tapeWorn (c, 0.2f, 0.45f); c.set (P::tapeNoise, 0.0f, 0.15f); },
                  0.2f, 0.45f, 0.05f, 0.2f },
                { "vintage-console-glue", "Console Glue", { E::saturation, E::tape },
                  "Bus glue and harmonic density", "drum bus, full loops, bass",
                  "Parallel console saturation into gentle tape compression.",
                  [] (Ctx& c) { c.value (P::satMode, "Soft"); c.set (P::satDrive, 8.0f, 15.0f); c.set (P::satTone, -0.1f, 0.2f); c.set (P::satMix, 0.55f, 0.85f); c.set (P::tapeDrive, 4.0f, 11.0f); c.set (P::tapeWow, 0.02f, 0.08f); c.set (P::tapeFlutter, 0.02f, 0.08f); c.value (P::tapeNoise, 0.0f); c.set (P::tapeAge, 0.05f, 0.3f); c.set (P::tapeMix, 0.6f, 0.9f); },
                  0.0f, 0.15f, 0.0f, 0.05f },
                { "vintage-radio-memory", "Radio Memory", { E::resampler, E::saturation, E::tape },
                  "Old broadcast and gramophone tone", "vocals, samples, melodies",
                  "Narrow band-limited crush, driven valve tone and hissing, aged tape.",
                  [] (Ctx& c) { crusher (c, 5000.0f, 11025.0f, 8.0f, 12.0f); c.setLog (P::rsFilter, 2200.0f, 4500.0f); c.set (P::rsMix, 0.8f, 1.0f); c.value (P::satMode, "Warm"); c.set (P::satDrive, 9.0f, 16.0f); c.set (P::satTone, 0.1f, 0.4f); tapeWorn (c, 0.6f, 0.85f); c.set (P::tapeNoise, 0.25f, 0.5f); },
                  0.1f, 0.3f, 0.1f, 0.3f },
            };
        case 3: // Dreamy & Atmospheric
            return {
                { "dream-halo-wash", "Halo Wash", { E::chorus, E::reverb, E::tremolo },
                  "Soft, glowing backdrops", "pads, piano, clean guitar",
                  "Lush chorus into a big soft hall that swells with a slow auto-pan.",
                  [] (Ctx& c) { chorusLush (c); reverbHall (c, 5.0f, 10.0f); tremSynced (c, { "1/2", "1/1", "1/2 D" }, { "Sine" }, 0.15f, 0.3f); c.set (P::tremStereo, 0.7f, 1.0f); },
                  0.2f, 0.45f, 0.0f, 0.1f },
                { "dream-cloud-garden", "Cloud Echo Garden", { E::delay, E::reverb, E::chorus },
                  "Floating echoes that melt into space", "keys, plucks, vocals",
                  "Ping-pong echoes washed into reverb, with chorus applied to the whole tail.",
                  [] (Ctx& c) { delaySynced (c, { "1/4 D", "1/4", "1/2" }, 0.45f, 0.65f); c.value (P::delayPingPong, true); reverbHall (c, 4.0f, 8.0f); c.setLog (P::chorusRate, 0.08f, 0.25f); c.set (P::chorusDepth, 0.5f, 0.8f); c.set (P::chorusMix, 0.2f, 0.4f); },
                  0.2f, 0.45f, 0.0f, 0.1f },
                { "dream-reverse-bloom", "Reverse Bloom", { E::reverse, E::reverb },
                  "Swells that lead into each note", "piano, guitar, vocal phrases",
                  "Reversed half-bars (often an octave up) blooming into a long hall.",
                  [] (Ctx& c) { reverseSynced (c, { "1/2", "1/4 D", "1/1" }, { 0.0f, 12.0f }, 0.3f, 0.5f); reverbHall (c, 6.0f, 12.0f); },
                  0.15f, 0.4f, 0.0f, 0.1f },
                { "dream-underwater-glass", "Underwater Glass", { E::resampler, E::chorus, E::reverb },
                  "Muffled, submerged dream textures", "keys, vocals, music box",
                  "A soft low-rate filter, deep chorus and a heavily damped space.",
                  [] (Ctx& c) { crusher (c, 12000.0f, 22000.0f, 12.0f, 16.0f); c.setLog (P::rsFilter, 1800.0f, 3500.0f); chorusLush (c); c.set (P::chorusDepth, 0.7f, 1.0f); reverbHall (c, 4.0f, 9.0f); c.set (P::reverbDamping, 0.75f, 0.95f); },
                  0.3f, 0.55f, 0.0f, 0.15f },
                { "dream-breathing-pad", "Breathing Pad", { E::reverb, E::tremolo, E::chorus },
                  "Pads that inhale and exhale", "pads, strings, choirs",
                  "A wet reverb modulated by a slow triangle tremolo, softened by chorus.",
                  [] (Ctx& c) { reverbHall (c, 3.0f, 6.0f); c.set (P::reverbMix, 0.4f, 0.6f); tremSynced (c, { "1/1", "1/2 D", "1/2" }, { "Triangle", "Sine" }, 0.25f, 0.45f); c.set (P::tremStereo, 0.5f, 1.0f); c.setLog (P::chorusRate, 0.1f, 0.3f); c.set (P::chorusDepth, 0.4f, 0.7f); c.set (P::chorusMix, 0.15f, 0.3f); },
                  0.15f, 0.4f, 0.0f, 0.1f },
            };
        case 4: // Experimental & Glitch
            return {
                { "glitch-bit-rain", "Bit Rain", { E::resampler, E::tremolo, E::delay },
                  "Glitchy digital rain from any sound", "synths, percussion, vocals",
                  "Starved, jittery sample clock chopped into tiny pieces and scattered.",
                  [] (Ctx& c) { crusher (c, 1800.0f, 6000.0f, 4.0f, 8.0f); c.set (P::rsJitter, 0.5f, 0.9f); tremSynced (c, { "1/32", "1/16 T", "1/16" }, { "Square" }, 0.7f, 1.0f); delaySynced (c, { "1/16", "1/16 D", "1/32" }, 0.4f, 0.6f); c.value (P::delayPingPong, true); },
                  0.1f, 0.3f, 0.4f, 0.8f },
                { "glitch-folded-circuit", "Folded Circuit", { E::saturation, E::resampler, E::chorus },
                  "Metallic, circuit-bent tones", "synth bass, leads, drums",
                  "Wavefolding into a bit-crusher and a fast, resonant chorus.",
                  [] (Ctx& c) { c.value (P::satMode, "Fold"); c.set (P::satDrive, 6.0f, 18.0f); c.set (P::satMix, 0.45f, 0.8f); crusher (c, 7000.0f, 16000.0f, 6.0f, 10.0f); c.setLog (P::chorusRate, 2.0f, 4.5f); c.set (P::chorusDepth, 0.3f, 0.6f); c.set (P::chorusFeedback, 0.55f, 0.8f); c.set (P::chorusDelay, 5.0f, 9.0f); c.set (P::chorusMix, 0.3f, 0.5f); },
                  0.1f, 0.4f, 0.2f, 0.5f },
                { "glitch-reverse-shredder", "Reverse Shredder", { E::reverse, E::saturation, E::delay },
                  "Shredded, backwards stutters", "vocals, drums, loops",
                  "Tiny pitched reverse slices hit a fuzz stage and repeat in dotted time.",
                  [] (Ctx& c) { reverseSynced (c, { "1/16", "1/32", "1/16 D" }, { 12.0f, -12.0f, 7.0f }, 0.45f, 0.7f); c.value (P::satMode, "Fuzz"); c.set (P::satDrive, 6.0f, 14.0f); c.set (P::satMix, 0.3f, 0.5f); delaySynced (c, { "1/16 D", "1/8 T", "1/16" }, 0.3f, 0.5f); },
                  0.1f, 0.3f, 0.2f, 0.5f },
                { "glitch-elastic-time", "Elastic Time", { E::tape, E::delay, E::reverse },
                  "Warping, rubbery time effects", "loops, vocals, synth lines",
                  "Extreme tape instability feeding short, high-feedback echoes and reversed fragments.",
                  [] (Ctx& c) { tapeWorn (c, 0.3f, 0.6f); c.set (P::tapeWow, 0.8f, 1.0f); c.set (P::tapeFlutter, 0.4f, 0.8f); c.value (P::delaySync, false); c.set (P::delayTime, 35.0f, 140.0f); c.set (P::delayFeedback, 0.6f, 0.78f); c.setLog (P::delayHighCut, 2200.0f, 4500.0f); c.set (P::delayMix, 0.25f, 0.4f); reverseSynced (c, { "1/8", "1/8 D", "1/4" }, { 0.0f, -12.0f }, 0.2f, 0.4f); },
                  0.5f, 0.8f, 0.4f, 0.8f },
                { "glitch-gated-fuzz-grid", "Gated Fuzz Grid", { E::saturation, E::tremolo, E::reverb, E::resampler },
                  "Hard-edged rhythmic noise beds", "drums, synth stabs, noise",
                  "Fuzz chopped into a grid, a short room, then crushed together.",
                  [] (Ctx& c) { c.value (P::satMode, "Fuzz"); c.set (P::satDrive, 10.0f, 20.0f); c.set (P::satMix, 0.35f, 0.6f); tremSynced (c, { "1/16", "1/8 T", "1/16 T" }, { "Square" }, 0.75f, 1.0f); reverbRoom (c); c.set (P::reverbMix, 0.25f, 0.4f); crusher (c, 6000.0f, 14000.0f, 6.0f, 10.0f); },
                  0.0f, 0.2f, 0.2f, 0.5f },
            };
        case 5: // Trap & R&B Textures
            return {
                { "rnb-late-night-keys", "Late Night Keys", { E::tape, E::chorus, E::reverb },
                  "Smooth, warm keys for R&B", "Rhodes, piano, guitar",
                  "Gentle tape warmth, slow chorus and a dark, intimate room.",
                  [] (Ctx& c) { tapeWorn (c, 0.2f, 0.45f); c.set (P::tapeWow, 0.08f, 0.25f); c.set (P::tapeNoise, 0.0f, 0.12f); c.setLog (P::chorusRate, 0.2f, 0.5f); c.set (P::chorusDepth, 0.3f, 0.55f); c.set (P::chorusMix, 0.25f, 0.4f); c.set (P::reverbSize, 0.45f, 0.7f); c.set (P::reverbDecay, 2.0f, 4.0f); c.set (P::reverbDamping, 0.5f, 0.75f); c.set (P::reverbMix, 0.2f, 0.35f); },
                  0.1f, 0.3f, 0.0f, 0.1f },
                { "trap-bell-shadow", "Bell Shadow", { E::reverse, E::delay, E::reverb },
                  "Dark trap bells with a ghost layer", "bells, plucks, celesta",
                  "An octave-down reversed shadow, dark dotted echoes and a damped hall.",
                  [] (Ctx& c) { reverseSynced (c, { "1/2", "1/4" }, { -12.0f }, 0.2f, 0.35f); delaySynced (c, { "1/8 D", "1/4 D", "1/8" }, 0.35f, 0.55f); c.value (P::delayPingPong, true); c.setLog (P::delayHighCut, 2400.0f, 4000.0f); reverbHall (c, 3.0f, 5.5f); c.set (P::reverbDamping, 0.6f, 0.85f); },
                  0.05f, 0.25f, 0.0f, 0.1f },
                { "trap-velvet-808", "Velvet 808", { E::saturation, E::tape },
                  "Make 808s and bass audible on small speakers", "808, sub bass, bass synths",
                  "Warm harmonic saturation and tape density, no time effects to blur the low end.",
                  [] (Ctx& c) { c.value (P::satMode, "Warm"); c.set (P::satDrive, 12.0f, 22.0f); c.set (P::satTone, -0.35f, 0.0f); c.set (P::satMix, 0.55f, 0.85f); c.set (P::tapeDrive, 4.0f, 10.0f); c.set (P::tapeWow, 0.0f, 0.03f); c.set (P::tapeFlutter, 0.0f, 0.03f); c.value (P::tapeNoise, 0.0f); c.set (P::tapeAge, 0.1f, 0.35f); },
                  0.0f, 0.1f, 0.0f, 0.05f },
                { "trap-pluck-haze", "Pluck Haze", { E::resampler, E::chorus, E::delay, E::reverb },
                  "Hazy, slightly lo-fi trap plucks", "plucks, guitar, keys",
                  "A soft crush, chorus shimmer, sixteenth echoes and a short space.",
                  [] (Ctx& c) { crusher (c, 18000.0f, 26000.0f, 11.0f, 14.0f); c.set (P::rsMix, 0.3f, 0.55f); c.setLog (P::chorusRate, 0.3f, 0.8f); c.set (P::chorusDepth, 0.3f, 0.5f); c.set (P::chorusMix, 0.25f, 0.4f); delaySynced (c, { "1/16 D", "1/8", "1/16" }, 0.25f, 0.45f); c.set (P::delayMix, 0.12f, 0.25f); reverbRoom (c); c.set (P::reverbDecay, 1.0f, 2.0f); },
                  0.05f, 0.25f, 0.0f, 0.1f },
                { "rnb-vocal-air", "Vocal Air", { E::delay, E::reverb, E::tremolo },
                  "Polished space for vocals and chops", "vocals, vocal chops, flutes",
                  "Bright quarter-note ping-pong, a pre-delayed plate-like space and a subtle auto-pan.",
                  [] (Ctx& c) { delaySynced (c, { "1/4", "1/8 D", "1/4 D" }, 0.2f, 0.4f); c.value (P::delayPingPong, true); c.setLog (P::delayHighCut, 6000.0f, 10000.0f); c.set (P::reverbSize, 0.5f, 0.7f); c.set (P::reverbDecay, 1.8f, 3.2f); c.set (P::reverbDamping, 0.2f, 0.4f); c.set (P::reverbPredelay, 30.0f, 60.0f); c.set (P::reverbMix, 0.2f, 0.32f); tremSynced (c, { "1/4", "1/2", "1/8" }, { "Sine" }, 0.1f, 0.2f); c.value (P::tremStereo, 1.0f); },
                  0.0f, 0.15f, 0.0f, 0.05f },
            };
        case 6: // Rhythmic & Reverse
            return {
                { "rhythm-backward-pulse", "Backward Pulse", { E::reverse, E::tremolo },
                  "Reverse loops that still groove", "drum loops, chords, full beats",
                  "Quarter-note reverse slices pulsing on a square gate.",
                  [] (Ctx& c) { reverseSynced (c, { "1/4", "1/8", "1/4 D" }, { 0.0f }, 0.5f, 0.75f); tremSynced (c, { "1/8", "1/16", "1/8 T" }, { "Square", "Triangle" }, 0.5f, 0.85f); },
                  0.0f, 0.15f, 0.0f, 0.1f },
                { "rhythm-swing-gate", "Swing Gate", { E::tremolo, E::delay },
                  "Triplet-feel gating and echoes", "pads, chords, guitar",
                  "Triplet square chopping answered by dotted echoes for a swung feel.",
                  [] (Ctx& c) { tremSynced (c, { "1/8 T", "1/16 T", "1/4 T" }, { "Square" }, 0.7f, 0.95f); delaySynced (c, { "1/8 D", "1/16 D", "1/4 T" }, 0.25f, 0.45f); },
                  0.0f, 0.1f, 0.0f, 0.1f },
                { "rhythm-reverse-ladder", "Reverse Echo Ladder", { E::delay, E::reverse },
                  "Echoes that come back reversed", "vocals, guitar, synth leads",
                  "Ping-pong echoes fed into the reverser, so every repeat plays backwards.",
                  [] (Ctx& c) { delaySynced (c, { "1/4 D", "1/8 D", "1/4" }, 0.45f, 0.65f); c.value (P::delayPingPong, true); c.set (P::delayMix, 0.35f, 0.55f); reverseSynced (c, { "1/4", "1/2", "1/8" }, { 0.0f, 12.0f }, 0.35f, 0.55f); },
                  0.0f, 0.2f, 0.0f, 0.1f },
                { "rhythm-gated-hall", "Gated Hall", { E::reverb, E::tremolo },
                  "Gated reverb pulses", "snares, vocals, pads",
                  "A bright hall chopped by a fast square gate into rhythmic bursts.",
                  [] (Ctx& c) { reverbHall (c, 2.5f, 5.0f); c.set (P::reverbDamping, 0.1f, 0.35f); c.set (P::reverbMix, 0.45f, 0.65f); tremSynced (c, { "1/16", "1/8", "1/16 T" }, { "Square" }, 0.75f, 0.95f); },
                  0.0f, 0.1f, 0.0f, 0.1f },
                { "rhythm-saw-ducker", "Saw Ducker", { E::tremolo, E::saturation, E::delay },
                  "Sidechain-style pumping without a sidechain", "pads, chords, bass synths",
                  "A saw-shaped duck on every beat, soft drive and short echoes.",
                  [] (Ctx& c) { tremSynced (c, { "1/4", "1/8" }, { "Saw" }, 0.55f, 0.85f); c.set (P::tremStereo, 0.0f, 0.2f); satWarm (c, 3.0f, 8.0f); delaySynced (c, { "1/8", "1/16 D" }, 0.15f, 0.3f); c.set (P::delayMix, 0.1f, 0.2f); },
                  0.0f, 0.1f, 0.0f, 0.05f },
            };
        case 7: // Space & Ambient
            return {
                { "space-orbit-field", "Orbit Field", { E::delay, E::reverb, E::chorus },
                  "Endless ambient fields", "pads, guitar, drones",
                  "Long ping-pong feedback into a huge reverb, slowly orbiting with chorus.",
                  [] (Ctx& c) { delaySynced (c, { "1/2", "1/4 D", "1/1" }, 0.55f, 0.72f); c.value (P::delayPingPong, true); c.setLog (P::delayHighCut, 3000.0f, 5500.0f); reverbHall (c, 10.0f, 20.0f); c.set (P::reverbMix, 0.45f, 0.6f); c.setLog (P::chorusRate, 0.05f, 0.15f); c.set (P::chorusDepth, 0.6f, 1.0f); c.set (P::chorusMix, 0.2f, 0.4f); },
                  0.3f, 0.6f, 0.0f, 0.15f },
                { "space-frozen-horizon", "Frozen Horizon", { E::reverb, E::resampler },
                  "A degraded, frozen ambient tail", "pads, piano, field recordings",
                  "A near-endless reverb whose tail is softly crushed, like a distant transmission.",
                  [] (Ctx& c) { reverbHall (c, 14.0f, 20.0f); c.set (P::reverbMix, 0.55f, 0.7f); crusher (c, 14000.0f, 26000.0f, 11.0f, 15.0f); c.set (P::rsMix, 0.35f, 0.6f); },
                  0.3f, 0.6f, 0.1f, 0.3f },
                { "space-starlight", "Starlight Shimmer", { E::reverse, E::reverb, E::delay },
                  "Shimmering octave-up ambience", "pads, guitar, vocals",
                  "Octave-up reverse grains in a long hall, the shimmer repeated in half notes.",
                  [] (Ctx& c) { reverseSynced (c, { "1/4", "1/2" }, { 12.0f }, 0.25f, 0.4f); reverbHall (c, 9.0f, 16.0f); delaySynced (c, { "1/2", "1/4 D" }, 0.4f, 0.55f); c.set (P::delayMix, 0.15f, 0.3f); },
                  0.2f, 0.5f, 0.0f, 0.15f },
                { "space-drifting-station", "Drifting Station", { E::tape, E::chorus, E::reverb },
                  "Slowly drifting, warbling space", "synths, keys, drones",
                  "Wobbling tape, a very slow deep chorus and a long, soft hall.",
                  [] (Ctx& c) { tapeWorn (c, 0.35f, 0.65f); c.set (P::tapeWow, 0.4f, 0.7f); c.setLog (P::chorusRate, 0.05f, 0.12f); c.set (P::chorusDepth, 0.8f, 1.0f); c.set (P::chorusMix, 0.35f, 0.55f); reverbHall (c, 9.0f, 15.0f); },
                  0.5f, 0.8f, 0.05f, 0.25f },
                { "space-deep-drone", "Deep Field Drone", { E::chorus, E::reverb, E::tremolo },
                  "Drones and long transitions", "drones, risers, sustained synths",
                  "High-feedback chorus into a maximal reverb with a very slow swell.",
                  [] (Ctx& c) { c.setLog (P::chorusRate, 0.06f, 0.2f); c.set (P::chorusDepth, 0.7f, 1.0f); c.set (P::chorusFeedback, 0.6f, 0.8f); c.set (P::chorusMix, 0.35f, 0.5f); reverbHall (c, 15.0f, 20.0f); c.set (P::reverbMix, 0.5f, 0.65f); tremSynced (c, { "1/1", "1/2 D" }, { "Triangle" }, 0.2f, 0.35f); },
                  0.4f, 0.7f, 0.0f, 0.15f },
            };
        case 8: // Drums & Percussion
            return {
                { "drums-crushed-room", "Crushed Room", { E::saturation, E::reverb },
                  "Punchy, gritty drum ambience", "drum loops, snares, percussion",
                  "Parallel hard drive with a short, tight room.",
                  [] (Ctx& c) { c.value (P::satMode, "Hard"); c.set (P::satDrive, 6.0f, 14.0f); c.set (P::satTone, -0.1f, 0.25f); c.set (P::satMix, 0.3f, 0.5f); reverbRoom (c); c.set (P::reverbDecay, 0.4f, 1.0f); },
                  0.0f, 0.05f, 0.0f, 0.05f },
                { "drums-sampler-grit", "Sampler Grit", { E::resampler, E::saturation, E::tape },
                  "Vintage sampler crunch for drums", "drum breaks, kicks, snares",
                  "Classic 12-bit sampler resolution, warm drive and tape weight.",
                  [] (Ctx& c) { crusher (c, 18000.0f, 28000.0f, 10.0f, 13.0f); c.set (P::rsJitter, 0.0f, 0.08f); c.set (P::rsMix, 0.5f, 0.8f); satWarm (c, 5.0f, 11.0f); c.set (P::tapeDrive, 3.0f, 9.0f); c.set (P::tapeWow, 0.0f, 0.05f); c.set (P::tapeFlutter, 0.0f, 0.05f); c.set (P::tapeNoise, 0.0f, 0.15f); c.set (P::tapeAge, 0.2f, 0.45f); },
                  0.0f, 0.1f, 0.0f, 0.1f },
                { "drums-slap-bounce", "Slap Bounce", { E::delay, E::saturation },
                  "Wide slapback for percussion", "percussion, hi-hats, claps",
                  "Short free-time ping-pong slaps with a touch of drive.",
                  [] (Ctx& c) { c.value (P::delaySync, false); c.set (P::delayTime, 45.0f, 130.0f); c.set (P::delayFeedback, 0.1f, 0.3f); c.value (P::delayPingPong, true); c.setLog (P::delayLowCut, 200.0f, 600.0f); c.setLog (P::delayHighCut, 5000.0f, 12000.0f); c.set (P::delayMix, 0.15f, 0.3f); satWarm (c, 2.0f, 7.0f); },
                  0.0f, 0.05f, 0.0f, 0.05f },
                { "drums-lofi-kit", "Lo-Fi Kit", { E::resampler, E::tape },
                  "Dusty, worn drum kits", "drum loops, full beats",
                  "Low-rate, low-bit crush with heavily aged tape.",
                  [] (Ctx& c) { crusher (c, 7000.0f, 13000.0f, 7.0f, 10.0f); c.set (P::rsJitter, 0.0f, 0.15f); tapeWorn (c, 0.5f, 0.8f); c.set (P::tapeWow, 0.0f, 0.1f); c.set (P::tapeFlutter, 0.0f, 0.1f); },
                  0.0f, 0.15f, 0.05f, 0.2f },
                { "drums-gated-snare", "Gated Snare Space", { E::reverb, E::tremolo },
                  "Eighties-style gated snare space", "snares, toms, claps",
                  "A medium room cut off by a sixteenth-note gate.",
                  [] (Ctx& c) { c.set (P::reverbSize, 0.5f, 0.75f); c.set (P::reverbDecay, 1.2f, 2.5f); c.set (P::reverbDamping, 0.2f, 0.45f); c.set (P::reverbPredelay, 0.0f, 10.0f); c.set (P::reverbMix, 0.3f, 0.45f); tremSynced (c, { "1/16", "1/8" }, { "Square" }, 0.4f, 0.65f); },
                  0.0f, 0.05f, 0.0f, 0.05f },
                { "drums-transient-fuzz", "Transient Fuzz", { E::saturation, E::reverb },
                  "Aggressive parallel fuzz for drums", "kicks, snares, drum bus",
                  "A small amount of fuzz blended under the drums with a short room.",
                  [] (Ctx& c) { c.value (P::satMode, "Fuzz"); c.set (P::satDrive, 4.0f, 10.0f); c.set (P::satTone, 0.0f, 0.3f); c.set (P::satMix, 0.15f, 0.35f); reverbRoom (c); },
                  0.0f, 0.05f, 0.0f, 0.05f },
            };
        default: // Original Wildcards: unusual orders built from a safe effect pool.
            return {
                { "wild-inside-out-room", "Inside-Out Room", { E::reverb, E::saturation, E::delay },
                  "Distorted reverb as an instrument", "drums, plucks, vocals",
                  "The room is distorted, not the source: reverb into a wavefolder, then echoes.",
                  [] (Ctx& c) { reverbHall (c, 2.0f, 6.0f); c.set (P::reverbMix, 0.45f, 0.7f); c.choose (P::satMode, { "Fold", "Fuzz", "Hard" }); c.set (P::satDrive, 6.0f, 16.0f); c.set (P::satMix, 0.35f, 0.65f); delaySynced (c, { "1/8 D", "1/4", "1/16 D" }, 0.3f, 0.5f); },
                  0.1f, 0.4f, 0.1f, 0.3f },
                { "wild-granular-tape", "Granular Tape", { E::reverse, E::tape, E::chorus },
                  "Grain-like, unstable textures", "vocals, guitar, field recordings",
                  "Tiny reverse grains printed to unstable tape and smeared by chorus.",
                  [] (Ctx& c) { reverseSynced (c, { "1/16", "1/16 D", "1/8" }, { 0.0f, 12.0f, -12.0f }, 0.5f, 0.8f); tapeWorn (c, 0.3f, 0.7f); c.set (P::tapeWow, 0.6f, 1.0f); chorusLush (c); },
                  0.3f, 0.7f, 0.3f, 0.6f },
                { "wild-pingpong-crusher", "Ping-Pong Crusher", { E::delay, E::resampler, E::reverb },
                  "Echoes that degrade with each repeat", "synths, vocals, percussion",
                  "Ping-pong echoes crushed after the fact, so the repeats sound digital and dusty.",
                  [] (Ctx& c) { delaySynced (c, { "1/8 D", "1/4", "1/16 D" }, 0.45f, 0.65f); c.value (P::delayPingPong, true); c.set (P::delayMix, 0.4f, 0.6f); crusher (c, 3000.0f, 9000.0f, 6.0f, 10.0f); reverbRoom (c); },
                  0.1f, 0.3f, 0.1f, 0.4f },
                { "wild-pulse-choir", "Pulse Choir", { E::chorus, E::tremolo, E::reverb },
                  "Choir-like resonance in rhythm", "vocals, pads, strings",
                  "High-feedback chorus resonance chopped in triplets and diffused in a hall.",
                  [] (Ctx& c) { c.setLog (P::chorusRate, 0.3f, 1.2f); c.set (P::chorusDepth, 0.4f, 0.8f); c.set (P::chorusFeedback, 0.65f, 0.85f); c.set (P::chorusDelay, 6.0f, 14.0f); c.set (P::chorusMix, 0.4f, 0.6f); tremSynced (c, { "1/8 T", "1/16 T", "1/4 T" }, { "Triangle", "Square" }, 0.5f, 0.8f); reverbHall (c, 3.0f, 7.0f); },
                  0.1f, 0.4f, 0.0f, 0.2f },
                { "wild-tape-after-space", "Tape After Space", { E::delay, E::reverb, E::tape },
                  "Ambient tails printed to old tape", "keys, guitar, vocals",
                  "Echo and reverb recorded onto a warbling, aged tape at the very end.",
                  [] (Ctx& c) { delaySynced (c, { "1/4", "1/8 D", "1/2" }, 0.35f, 0.6f); reverbHall (c, 3.0f, 8.0f); tapeWorn (c, 0.5f, 0.85f); c.set (P::tapeWow, 0.3f, 0.7f); },
                  0.3f, 0.6f, 0.1f, 0.4f },
                { "wild-tremolo-reverser", "Chopped Reverser", { E::tremolo, E::reverse, E::saturation },
                  "Gated sound played backwards", "chords, drums, vocals",
                  "The gate is applied before reversing, so chopped envelopes play backwards and swell in.",
                  [] (Ctx& c) { tremSynced (c, { "1/16", "1/8", "1/8 T" }, { "Square", "Saw" }, 0.6f, 0.95f); reverseSynced (c, { "1/4", "1/2", "1/4 D" }, { 0.0f, 12.0f, -5.0f }, 0.5f, 0.8f); satWarm (c, 3.0f, 10.0f); },
                  0.0f, 0.3f, 0.0f, 0.3f },
            };
    }
}

// Global musical constraints applied after an architecture's rules.
void applyCouplingRules (juce::NamedValueSet& p, const std::vector<E>& chain)
{
    auto has = [&chain] (E e) { return std::find (chain.begin(), chain.end(), e) != chain.end(); };
    auto get = [&p] (const char* id, double def) { return p.contains (id) ? static_cast<double> (p[id]) : def; };

    if (has (E::reverb))
    {
        // Long tails need damping and a lower mix to stay usable.
        if (get (P::reverbDecay, 2.5) > 8.0)
        {
            p.set (P::reverbMix, std::min (get (P::reverbMix, 0.3), 0.6));
            p.set (P::reverbDamping, std::max (get (P::reverbDamping, 0.4), 0.3));
        }
        // Small rooms with huge decays sound metallic: scale size with decay.
        if (get (P::reverbDecay, 2.5) > 6.0)
            p.set (P::reverbSize, std::max (get (P::reverbSize, 0.6), 0.6));
    }
    if (has (E::delay) && get (P::delayFeedback, 0.4) > 0.6)
    {
        // High feedback: darker repeats and a lower mix keep it from washing out.
        p.set (P::delayHighCut, std::min (get (P::delayHighCut, 8000.0), 6500.0));
        p.set (P::delayMix, std::min (get (P::delayMix, 0.3), 0.45));
    }
    if (has (E::chorus) && get (P::chorusFeedback, 0.0) > 0.6)
        p.set (P::chorusMix, std::min (get (P::chorusMix, 0.5), 0.55));
    if (has (E::saturation) && get (P::satDrive, 12.0) > 18.0)
        p.set (P::satMix, std::min (get (P::satMix, 1.0), 0.85));
    // Bright tone on heavy saturation is louder than the level compensation assumes.
    if (has (E::saturation))
    {
        auto trim = -3.0 * std::max (0.0, get (P::satTone, 0.0));
        const auto mode = p[P::satMode].toString();
        if (mode == "Hard" || mode == "Fuzz")
            trim -= std::min (4.0, std::max (0.0, get (P::satDrive, 12.0) - 12.0) * 0.25);
        p.set (P::satOutput, trim + 0.0); // + 0.0: no "-0" in files
    }
}

std::array<std::pair<std::vector<const char*>, std::vector<const char*>>, numCategories> nameWords()
{
    return { {
        { { "Obsidian", "Hollow", "Ashen", "Grave", "Iron", "Midnight", "Sable", "Umbral", "Cold", "Black", "Silent", "Brooding", "Shrouded", "Feral" },
          { "Monolith", "Requiem", "Cathedral", "Vigil", "Abyss", "Procession", "Eclipse", "Siren", "Omen", "Citadel", "Tide", "Verdict", "Crypt", "Horizon" } },
        { { "Mirrored", "Folded", "Ghosted", "Inverted", "Bent", "Echoed", "Twisted", "Answering", "Shadowed", "Doubled", "Liquid", "Melted", "Spiral", "Phantom" },
          { "Phrase", "Hook", "Motif", "Refrain", "Lullaby", "Riff", "Cadence", "Arpeggio", "Line", "Theme", "Counterpoint", "Whistle", "Chime", "Figure" } },
        { { "Amber", "Sepia", "Velvet", "Golden", "Faded", "Smoky", "Walnut", "Brass", "Mellow", "Dusty", "Copper", "Tweed", "Polaroid", "Honeyed" },
          { "Console", "Reel", "Lounge", "Jukebox", "Valve", "Matinee", "Diner", "Cabinet", "Studio", "Postcard", "Turntable", "Wireless", "Parlour", "Ballroom" } },
        { { "Floating", "Pastel", "Silk", "Moonlit", "Hazy", "Weightless", "Drowsy", "Pearl", "Gentle", "Dawn", "Lilac", "Cotton", "Glimmering", "Slow" },
          { "Petals", "Lagoon", "Veil", "Daydream", "Mist", "Lantern", "Reverie", "Meadow", "Cloud", "Haze", "Aurora", "Feather", "Harbour", "Garden" } },
        { { "Scrambled", "Fractured", "Mutant", "Shattered", "Buffered", "Elastic", "Corrupt", "Stuttered", "Pixel", "Circuit", "Rogue", "Static", "Broken", "Quantum" },
          { "Machine", "Glitch", "Splice", "Engine", "Error", "Swarm", "Lattice", "Signal", "Shard", "Protocol", "Maze", "Packet", "Kernel", "Sprite" } },
        { { "Velvet", "After-Hours", "Slow-Burn", "Neon", "Satin", "Rosewood", "Low-Lit", "Smoked", "Late", "Silk-Road", "Candle", "Honey", "Plush", "Cruise" },
          { "Keys", "Bells", "Slide", "Haze", "Glow", "Confession", "Ride", "Groove", "Serenade", "Pulse", "Heat", "Interlude", "Afterparty", "Tint" } },
        { { "Backward", "Swung", "Pumping", "Chopped", "Syncopated", "Offbeat", "Rolling", "Gated", "Rewound", "Bouncing", "Shuffled", "Clipped", "Strobing", "Ducking" },
          { "Pulse", "Ladder", "Groove", "Engine", "Clockwork", "Shuffle", "Strut", "Gate", "Stepper", "Rewind", "Pendulum", "Motor", "Bounce", "Metronome" } },
        { { "Orbital", "Stellar", "Distant", "Infinite", "Lunar", "Zenith", "Celestial", "Polar", "Endless", "Nebular", "Silent", "Vast", "Frozen", "Cosmic" },
          { "Field", "Station", "Horizon", "Expanse", "Drift", "Observatory", "Tundra", "Comet", "Void", "Satellite", "Glacier", "Corridor", "Nebula", "Plateau" } },
        { { "Crushed", "Punchy", "Gritty", "Slammed", "Tight", "Dusty", "Snappy", "Boxy", "Fat", "Thumping", "Raw", "Knocking", "Cracked", "Heavy" },
          { "Kit", "Room", "Break", "Snare", "Bus", "Groove", "Stomp", "Clap", "Kick", "Loop", "Rimshot", "Tom", "Backbeat", "Shaker" } },
        { { "Inside-Out", "Upside", "Paradox", "Wandering", "Curious", "Chimeric", "Kaleido", "Tangled", "Velcro", "Origami", "Mobius", "Rubber", "Topsy", "Prism" },
          { "Theory", "Creature", "Contraption", "Puzzle", "Machine", "Garden", "Engine", "Riddle", "Oddity", "Carousel", "Labyrinth", "Gadget", "Experiment", "Bloom" } },
    } };
}
} // namespace

const juce::StringArray& categoryNames()
{
    static const juce::StringArray names { "Dark & Cinematic", "Melody Transformations", "Vintage & Analog", "Dreamy & Atmospheric",
                                           "Experimental & Glitch", "Trap & R&B Textures", "Rhythmic & Reverse", "Space & Ambient",
                                           "Drums & Percussion", "Original Wildcards" };
    return names;
}

String makeName (int category, juce::uint32 seed, const juce::StringArray& taken)
{
    const auto words = nameWords()[static_cast<size_t> (juce::jlimit (0, numCategories - 1, category))];
    const auto& adj = words.first;
    const auto& noun = words.second;
    Rng r (seed ^ 0x9e3779b9u);
    const auto start = r.index (static_cast<int> (adj.size() * noun.size()));
    for (int k = 0; k < static_cast<int> (adj.size() * noun.size()); ++k)
    {
        const auto idx = (start + k * 37) % static_cast<int> (adj.size() * noun.size()); // 37 is coprime with the grid size
        const auto name = String (adj[static_cast<size_t> (idx % static_cast<int> (adj.size()))]) + " "
                          + noun[static_cast<size_t> (idx / static_cast<int> (adj.size()))];
        if (! taken.contains (name.toLowerCase()))
            return name;
    }
    return "Lab " + String::toHexString (static_cast<juce::int64> (seed));
}

String describeChain (const Preset& preset)
{
    const auto& p = preset.params;
    auto num = [&p] (const char* id) { return static_cast<double> (p[id]); };
    juce::StringArray parts;
    for (auto id : preset.chain)
    {
        switch (static_cast<E> (id))
        {
            case E::saturation: parts.add (p[P::satMode].toString() + " drive +" + String (juce::roundToInt (num (P::satDrive))) + " dB"); break;
            case E::resampler: parts.add (String (num (P::rsRate) / 1000.0, 1) + " kHz / " + String (juce::roundToInt (num (P::rsBits))) + "-bit"); break;
            case E::tape: parts.add ("tape (wow " + String (juce::roundToInt (100.0 * num (P::tapeWow))) + " %, age " + String (juce::roundToInt (100.0 * num (P::tapeAge))) + " %)"); break;
            case E::chorus: parts.add (String (num (P::chorusRate), 2) + " Hz chorus"); break;
            case E::tremolo: parts.add ((static_cast<bool> (p[P::tremSync]) ? p[P::tremDivision].toString() : String (num (P::tremRate), 1) + " Hz") + " " + p[P::tremShape].toString().toLowerCase() + " tremolo"); break;
            case E::reverse:
            {
                const auto st = juce::roundToInt (num (P::revPitch));
                parts.add (p[P::revDivision].toString() + " reverse" + (st != 0 ? " " + String (st > 0 ? "+" : "") + String (st) + " st" : String()));
                break;
            }
            case E::delay: parts.add ((static_cast<bool> (p[P::delaySync]) ? p[P::delayDivision].toString() : String (juce::roundToInt (num (P::delayTime))) + " ms")
                                      + (static_cast<bool> (p[P::delayPingPong]) ? " ping-pong" : String()) + " delay"); break;
            case E::reverb: parts.add (String (num (P::reverbDecay), 1) + " s reverb"); break;
        }
    }
    return parts.joinIntoString (" > ");
}

Candidate generate (int category, juce::uint32 seed)
{
    category = juce::jlimit (0, numCategories - 1, category);
    Rng r (seed);
    const auto archs = architecturesFor (category);
    const auto& arch = archs[static_cast<size_t> (r.index (static_cast<int> (archs.size())))];

    Candidate c;
    c.category = category;
    c.seed = seed;
    c.architecture = arch.id;
    c.purpose = arch.purpose;
    c.instruments = arch.instruments;

    std::vector<E> chain = arch.chain;
    juce::NamedValueSet params;
    Ctx ctx { r, params, chain };
    arch.rules (ctx);

    // Variation axis 1 - intensity: subtle / as designed / bold. Scales the wet amounts
    // (mix and depth controls) so variants of one architecture differ in character.
    const auto intensity = r.index (3);
    if (intensity != 1)
    {
        const auto scale = intensity == 0 ? 0.65 : 1.3;
        for (int i = 0; i < params.size(); ++i)
        {
            const auto key = params.getName (i).toString();
            const auto value = params.getValueAt (i);
            if ((key.endsWith ("Mix") || key.endsWith ("Depth")) && (value.isDouble() || value.isInt()))
                params.set (params.getName (i), juce::jlimit (0.05, 1.0, static_cast<double> (value) * scale));
        }
    }
    c.architecture << (intensity == 0 ? "/subtle" : intensity == 2 ? "/bold" : "");

    // Variation axis 2 - colour pedal: sometimes add one complementary pedal, before or
    // after the core chain, where it makes musical sense.
    const bool protectLowEnd = String (arch.id) == "trap-velvet-808";
    if (chain.size() < 4 && ! protectLowEnd && r.chance (0.45f))
    {
        switch (r.index (4))
        {
            case 0:
                if (! ctx.has (E::tape) && category != 8)
                { tapeWorn (ctx, 0.15f, 0.45f); ctx.set (P::tapeMix, 0.4f, 0.8f); chain.insert (chain.begin(), E::tape); c.architecture << "+tape"; }
                break;
            case 1:
                if (! ctx.has (E::chorus) && category != 8)
                { chorusLush (ctx); ctx.insertBefore (E::chorus, chain.back()); c.architecture << "+chorus"; }
                break;
            case 2:
                if (! ctx.has (E::saturation))
                { satWarm (ctx, 2.0f, 8.0f); chain.insert (chain.begin(), E::saturation); c.architecture << "+sat"; }
                break;
            default:
                if (! ctx.has (E::reverb) && category != 8)
                { reverbRoom (ctx); chain.push_back (E::reverb); c.architecture << "+room"; }
                else if (! ctx.has (E::resampler))
                { crusher (ctx, 14000.0f, 30000.0f, 10.0f, 16.0f); ctx.set (P::rsMix, 0.2f, 0.45f); chain.push_back (E::resampler); c.architecture << "+crush"; }
                break;
        }
    }

    applyCouplingRules (params, chain);
    params.set (P::drift, r.range (arch.driftLo, arch.driftHi));
    params.set (P::unstable, r.range (arch.unstableLo, arch.unstableHi));

    // Readable, stable values: 4 decimals (1 for values in Hz / ms above 100).
    for (int i = 0; i < params.size(); ++i)
        if (const auto v = params.getValueAt (i); v.isDouble())
        {
            const auto x = static_cast<double> (v);
            const auto q = std::abs (x) >= 100.0 ? 10.0 : 10000.0;
            params.set (params.getName (i), std::round (x * q) / q + 0.0);
        }

    auto& preset = c.preset;
    preset.isFactory = false;
    preset.params = params;
    for (auto e : chain)
        preset.chain.push_back (static_cast<int> (e));
    size_t slot = 0;
    for (auto id : preset.chain)
        preset.order[slot++] = id;
    for (int id = 0; id < numEffects; ++id)
        if (std::find (preset.chain.begin(), preset.chain.end(), id) == preset.chain.end())
            preset.order[slot++] = id;

    preset.name = makeName (category, seed, {});
    preset.category = "SHOBEATS Lab / " + categoryNames()[category];
    preset.description = String (arch.character) + " For " + arch.instruments + ".";
    return c;
}

std::vector<Candidate> generateBatch (int batch, int perCategory, juce::uint32 baseSeed, juce::StringArray* taken)
{
    juce::StringArray local;
    auto& names = taken != nullptr ? *taken : local;
    std::vector<Candidate> out;
    for (int category = 0; category < numCategories; ++category)
        for (int k = 0; k < perCategory; ++k)
        {
            // Seed = hash of (base, batch, category, slot): reproducible and independent per candidate.
            const auto key = String (baseSeed) + "/" + String (batch) + "/" + String (category) + "/" + String (k);
            const auto seed = static_cast<juce::uint32> (key.hashCode64() & 0xffffffff);
            auto c = generate (category, seed);
            c.batch = batch;
            c.preset.name = makeName (category, seed, names);
            names.add (c.preset.name.toLowerCase());
            c.id = "B" + String (batch + 1) + "-C" + String (category + 1).paddedLeft ('0', 2) + "-" + String (k + 1).paddedLeft ('0', 2);
            out.push_back (std::move (c));
        }
    return out;
}

String fingerprint (const Preset& preset)
{
    String s;
    for (auto id : preset.chain)
        s << id << ",";
    juce::StringArray keys;
    for (const auto& nv : preset.params)
        if (nv.name.toString() != P::outputGain) // level matching is not part of the sound's identity
            keys.add (nv.name.toString());
    keys.sort (true);
    for (const auto& k : keys)
    {
        const auto& v = preset.params[juce::Identifier (k)];
        s << k << "=" << (v.isString() || v.isBool() ? v.toString() : String (static_cast<double> (v), 2)) << ";";
    }
    return String::toHexString (s.hashCode64());
}

double structuralDistance (const Preset& a, const Preset& b, juce::AudioProcessorValueTreeState& ranges)
{
    std::array<bool, numEffects> inA {}, inB {};
    for (auto id : a.chain) inA[static_cast<size_t> (id)] = true;
    for (auto id : b.chain) inB[static_cast<size_t> (id)] = true;

    int both = 0, either = 0;
    for (int e = 0; e < numEffects; ++e)
    {
        both += (inA[static_cast<size_t> (e)] && inB[static_cast<size_t> (e)]) ? 1 : 0;
        either += (inA[static_cast<size_t> (e)] || inB[static_cast<size_t> (e)]) ? 1 : 0;
    }
    const auto setDistance = either > 0 ? 1.0 - static_cast<double> (both) / either : 0.0;

    // Order: fraction of shared effect pairs whose relative order differs.
    int pairs = 0, swapped = 0;
    auto pos = [] (const Preset& p, int id) { return static_cast<int> (std::find (p.chain.begin(), p.chain.end(), id) - p.chain.begin()); };
    for (int x = 0; x < numEffects; ++x)
        for (int y = x + 1; y < numEffects; ++y)
            if (inA[static_cast<size_t> (x)] && inB[static_cast<size_t> (x)] && inA[static_cast<size_t> (y)] && inB[static_cast<size_t> (y)])
            {
                ++pairs;
                swapped += ((pos (a, x) < pos (a, y)) != (pos (b, x) < pos (b, y))) ? 1 : 0;
            }
    const auto orderDistance = pairs > 0 ? static_cast<double> (swapped) / pairs : 0.0;

    // Parameters of the shared active effects (normalised 0..1), plus global movement.
    double paramSum = 0.0;
    int paramCount = 0;
    auto compare = [&] (const char* id) {
        auto* param = ranges.getParameter (id);
        if (param == nullptr)
            return;
        auto norm = [&] (const Preset& p) {
            if (! p.params.contains (id))
                return param->getDefaultValue();
            const auto& v = p.params[id];
            if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (param); choice != nullptr && v.isString())
                return choice->convertTo0to1 (static_cast<float> (std::max (0, choice->choices.indexOf (v.toString()))));
            return param->convertTo0to1 (static_cast<float> (static_cast<double> (v)));
        };
        paramSum += std::abs (norm (a) - norm (b));
        ++paramCount;
    };
    for (auto* id : allParameterIds)
    {
        const String key (id);
        int owner = -1;
        for (int e = 0; e < numEffects; ++e)
            if (key.startsWith (effectInfo[static_cast<size_t> (e)].key))
                owner = e;
        if (key.endsWith ("Enabled"))
            continue;
        if (owner >= 0 && ! (inA[static_cast<size_t> (owner)] && inB[static_cast<size_t> (owner)]))
            continue;
        if (owner < 0 && key != P::drift && key != P::unstable)
            continue;
        // Only parameters a preset actually sets: untouched defaults would dilute the distance.
        if (! a.params.contains (id) && ! b.params.contains (id))
            continue;
        compare (id);
    }
    const auto paramDistance = paramCount > 0 ? paramSum / paramCount : 0.0;

    return 0.45 * setDistance + 0.15 * orderDistance + 0.40 * paramDistance;
}

String validateParameters (const Preset& preset, juce::AudioProcessorValueTreeState& ranges)
{
    if (preset.chain.empty())
        return "empty chain";
    if (! ChainOrder::isValid (preset.order))
        return "invalid order";
    for (const auto& nv : preset.params)
    {
        auto* param = ranges.getParameter (nv.name.toString());
        if (param == nullptr)
            return "unknown parameter " + nv.name.toString();
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (param))
        {
            if (! nv.value.isString() || ! choice->choices.contains (nv.value.toString()))
                return "invalid choice " + nv.name.toString() + "=" + nv.value.toString();
            continue;
        }
        if (nv.value.isBool())
            continue;
        if (! (nv.value.isInt() || nv.value.isInt64() || nv.value.isDouble()))
            return "non-numeric " + nv.name.toString();
        const auto v = static_cast<float> (static_cast<double> (nv.value));
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param))
        {
            const auto& r = ranged->getNormalisableRange();
            if (! std::isfinite (v) || v < r.start - 1.0e-4f || v > r.end + 1.0e-4f)
                return "out of range " + nv.name.toString() + "=" + String (v);
        }
    }
    return {};
}

QcResult qualityCheck (Preset& preset, const juce::AudioBuffer<float>& input, juce::AudioProcessorValueTreeState& ranges)
{
    QcResult q;
    if (auto error = validateParameters (preset, ranges); error.isNotEmpty())
    {
        q.reason = "invalid parameters: " + error;
        return q;
    }

    preset.params.set (P::outputGain, 0.0);
    auto m = audit::measure (preset, input);
    if (! m.finite)
    {
        q.reason = "non-finite output";
        q.metrics = m;
        return q;
    }
    if (m.levelChangeDb < silentChangeDb)
    {
        q.reason = "silent output";
        q.metrics = m;
        return q;
    }

    // Level match to the dry signal, then keep the peak under the ceiling.
    auto gain = juce::jlimit (minOutputGainDb, maxOutputGainDb, -m.levelChangeDb);
    gain = std::min (gain, maxPeakDb - 0.5 - (static_cast<double> (m.peakDb) - 0.0));
    gain = juce::jlimit (minOutputGainDb, maxOutputGainDb, std::floor (gain * 2.0) / 2.0);
    preset.params.set (P::outputGain, gain);
    q.outputGainDb = gain;
    m = audit::measure (preset, input);
    q.metrics = m;

    if (! m.finite)
        q.reason = "non-finite output";
    else if (m.peakDb > maxPeakDb)
        q.reason = "clipping: peak " + String (m.peakDb, 1) + " dBFS";
    else if (m.levelChangeDb < minLevelChangeDb)
        q.reason = "too quiet: " + String (m.levelChangeDb, 1) + " dB";
    else if (m.levelChangeDb > maxLevelChangeDb)
        q.reason = "too loud: " + String (m.levelChangeDb, 1) + " dB";
    else if (m.tailGrowthDb > maxTailGrowthDb)
    {
        // Long reverse windows and pre-delays legitimately put energy late in a short tail:
        // confirm on a longer render before calling it runaway feedback.
        const auto longer = audit::measure (preset, audit::testSignal (2.0, 8.0));
        if (! longer.finite || longer.tailGrowthDb > maxTailGrowthDb)
            q.reason = "unstable: tail grows " + String (longer.tailGrowthDb, 1) + " dB over 6 s";
        else
            q.passed = true;
    }
    else if (m.realtimeFactor > maxRealtimeFactor)
        q.reason = "CPU: realtime factor " + String (m.realtimeFactor, 3);
    else
        q.passed = true;
    return q;
}

bool isDuplicate (double structural, double audioDb)
{
    return structural < duplicateStructural || (structural < duplicateStructuralWithAudio && audioDb < duplicateAudioDb);
}

bool isTooCloseToReference (double structural, double audioDb)
{
    return structural < referenceStructural || (structural < referenceStructuralWithAudio && audioDb < referenceAudioDb);
}
} // namespace sfx::lab
