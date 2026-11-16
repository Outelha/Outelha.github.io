#include "PresetImporter.h"

#include "dsp/DspCommon.h"
#include "dsp/Saturation.h"

#include <map>

namespace sfx
{
namespace
{
using juce::String;

//==============================================================================
// Source parameter access.
//
// FFWD FX writes <PARAM id="..."/> without a value for parameters it left at their
// default. The source plugin's defaults are not published; the values below are the
// most common explicit values across a 650-preset sample (all round "factory" numbers:
// 0.5, 0.25, 375 ms, 2 s, 9999.999 Hz, -35 dB ...). Uses of them are reported.
const std::map<String, double>& assumedDefaults()
{
    static const std::map<String, double> table {
        { "Distortion_Drive", 1.0 }, { "Distortion_Tone", 0.0 }, { "Distortion_Mix", 1.0 }, { "Distortion_Mode", 1.0 },
        { "Distortion_Frequency", 500.0 }, { "Distortion_Drift", 0.0 }, { "Distortion_Instability", 0.0 },
        { "Resampler_SampleRate", 44100.0 }, { "Resampler_Jitter", 0.0 }, { "Resampler_Drive", 1.0 },
        { "Resampler_PostFilter", 1.0 }, { "Resampler_PreFilter", 1.0 }, { "Resampler_Drift", 0.0 }, { "Resampler_Instability", 0.0 },
        { "Tape_Range", 0.1 }, { "Tape_Balance", 0.1 }, { "Tape_WowRate", 0.6 }, { "Tape_FlutterRate", 11.0 },
        { "Tape_NoiseGain", -35.0 }, { "Tape_NoiseMode", 0.0 }, { "Tape_LPF", 22000.0 }, { "Tape_HPF", 20.0 },
        { "Tape_DropOutDepth", 0.0 }, { "Tape_DropOutRate", 2.3 }, { "Tape_Flux", 0.0 }, { "Tape_ScrapeLevel", 0.0 },
        { "Tape_Drift", 0.0 }, { "Tape_Instability", 0.0 },
        { "Chorus_Rate", 0.25 }, { "Chorus_Depth", 0.4 }, { "Chorus_Mix", 0.3 }, { "Chorus_Width", 0.3 }, { "Chorus_Stereo", 1.0 },
        { "Chorus_Saturation", 3.0 }, { "Chorus_Waveform", 0.0 }, { "Chorus_Clock", 0.0 },
        { "Modulation_Depth", 0.5 }, { "Modulation_Rate", 0.5 }, { "Modulation_Sync", 1.0 }, { "Modulation_NoteDivision", 3.0 },
        { "Modulation_NoteTiming", 0.0 }, { "Modulation_Shape", 0.5 }, { "Modulation_Stereo", 0.0 }, { "Modulation_Mode", 0.0 },
        { "Modulation_Drift", 0.0 }, { "Modulation_Instability", 0.0 },
        { "ReverseDelay_Mix", 0.5 }, { "ReverseDelay_NoteDivision", 5.0 }, { "ReverseDelay_NoteTiming", 0.0 },
        { "ReverseDelay_Pitch", 0.0 }, { "ReverseDelay_PitchActive", 1.0 }, { "ReverseDelay_Feedback", 0.25 },
        { "ReverseDelay_HPF", 10.0 }, { "ReverseDelay_LPF", 9999.999 }, { "ReverseDelay_Type", 0.0 },
        { "Delay_Mix", 0.5 }, { "Delay_Feedback", 0.5 }, { "Delay_Sync", 1.0 }, { "Delay_NoteDivision", 3.0 }, { "Delay_NoteTiming", 0.0 },
        { "Delay_Time", 375.0 }, { "Delay_Adjust", 1.0 }, { "Delay_HPF", 10.0 }, { "Delay_LPF", 9999.999 }, { "Delay_Type", 0.0 },
        { "Delay_Saturation", 3.0 }, { "Delay_ModDepth", 0.0 }, { "Delay_Freeze", 0.0 }, { "Delay_Clock", 0.0 },
        { "Delay_Drift", 0.0 }, { "Delay_Instability", 0.0 },
        { "Reverb_Mix", 0.25 }, { "Reverb_Decay", 2.0 }, { "Reverb_Size", 0.5 }, { "Reverb_PreDelay", 0.0 }, { "Reverb_HighDamp", 9999.999 },
        { "Reverb_LowDamp", 10.0 }, { "Reverb_Blend", 0.5 }, { "Reverb_Type", 1.0 }, { "Reverb_ModDepth", 0.0 }, { "Reverb_Saturation", 0.0 },
        { "Reverb_Drift", 0.0 }, { "Reverb_Instability", 0.0 },
        { "Pitch_Semitones", 0.0 }, { "Pitch_Cents", 0.0 }, { "Width_Width", 1.0 },
        { "EQ_LowGain", 0.0 }, { "EQ_MidGain", 0.0 }, { "EQ_HighGain", 0.0 }, { "EQ_Active", 1.0 },
        { "Shifter_Active", 0.0 }, { "MOD1_Active", 1.0 }, { "MOD2_Active", 1.0 },
    };
    return table;
}

// Older TONEFLX versions prefix tape parameters with "Tape" (Core0_Tape_TapeRange ...).
const std::map<String, String>& legacyAliases()
{
    static const std::map<String, String> table {
        { "Tape_Range", "Tape_TapeRange" }, { "Tape_Balance", "Tape_TapeBalance" }, { "Tape_WowRate", "Tape_TapeWowRate" },
        { "Tape_FlutterRate", "Tape_TapeFlutterRate" }, { "Tape_NoiseGain", "Tape_TapeNoiseGain" }, { "Tape_NoiseMode", "Tape_TapeNoiseMode" },
        { "Tape_LPF", "Tape_TapeLPF" }, { "Tape_HPF", "Tape_TapeHPF" }, { "Tape_DropOutDepth", "Tape_TapeDropOutDepth" },
        { "Tape_DropOutRate", "Tape_TapeDropOutRate" }, { "Tape_Flux", "Tape_TapeFlux" }, { "Tape_ScrapeLevel", "Tape_TapeScrapeLevel" },
    };
    return table;
}

struct Source
{
    std::map<String, double> values; // explicit values, keyed without the "Core0_" prefix
    std::map<String, bool> present;  // key -> appears in the file (with or without a value)
    juce::StringArray assumed;       // keys whose default had to be assumed

    bool has (const String& key) const { return values.count (key) > 0; }

    double get (const String& key)
    {
        if (auto it = values.find (key); it != values.end())
            return it->second;
        if (auto alias = legacyAliases().find (key); alias != legacyAliases().end())
            if (auto it = values.find (alias->second); it != values.end())
                return it->second;
        if (auto it = assumedDefaults().find (key); it != assumedDefaults().end())
        {
            if (present.count (key) > 0)
                assumed.addIfNotAlreadyThere (key);
            return it->second;
        }
        return 0.0;
    }

    // A module is active only if its switch is explicitly on. When the switch has no value,
    // the module's parameters are (almost always) untouched as well, so it is treated as off.
    bool active (const String& module, juce::StringArray& notes) const
    {
        const auto key = module + "_Active";
        if (auto it = values.find (key); it != values.end())
            return it->second >= 0.5;
        if (module == "Tape")
            if (auto it = values.find ("Tape_TapeActive"); it != values.end())
                return it->second >= 0.5;
        if (present.count (key) > 0)
            notes.add (module + ": on/off state not stored in the file - treated as off");
        return false;
    }
};

std::optional<Source> parse (const String& xmlText, String& product, String& error, juce::StringArray& graph)
{
    auto xml = juce::XmlDocument::parse (xmlText);
    if (xml == nullptr)
    {
        error = "not valid XML";
        return std::nullopt;
    }

    Source s;
    const auto tag = xml->getTagName();
    auto stripCore = [] (const String& id) { return id.startsWith ("Core0_") ? id.substring (6) : id; };

    if (tag == "SonixAudio" || tag == "FFWD")
    {
        product = xml->getStringAttribute ("product", tag) + " " + xml->getStringAttribute ("version");
        auto* core = xml->getChildByName ("Core0");
        if (core == nullptr)
        {
            error = "no Core0 element";
            return std::nullopt;
        }
        if (auto* fx = core->getChildByName ("FXGraph"))
            for (auto* m : fx->getChildWithTagNameIterator ("Module"))
                graph.add (m->getStringAttribute ("id"));
        if (auto* params = core->getChildByName ("Parameters"))
            for (auto* p : params->getChildWithTagNameIterator ("PARAM"))
            {
                const auto key = stripCore (p->getStringAttribute ("id"));
                s.present[key] = true;
                if (p->hasAttribute ("value"))
                    s.values[key] = p->getDoubleAttribute ("value");
            }
        if (auto* globals = core->getChildByName ("GlobalParameters"))
            for (int i = 0; i < globals->getNumAttributes(); ++i)
                s.values[globals->getAttributeName (i)] = globals->getAttributeValue (i).getDoubleValue();
    }
    else if (tag.startsWith ("Core0_"))
    {
        // Single-module snippet: parameters are attributes of the root element.
        product = "module snippet";
        for (int i = 0; i < xml->getNumAttributes(); ++i)
        {
            const auto key = stripCore (xml->getAttributeName (i));
            s.present[key] = true;
            s.values[key] = xml->getAttributeValue (i).getDoubleValue();
        }
        // A snippet describes one module: it is in use even if its switch is not stored.
        const auto module = tag.substring (6);
        if (s.values.count (module + "_Active") == 0)
            s.values[module + "_Active"] = 1.0;
    }
    else
    {
        error = "unsupported root element <" + tag + ">";
        return std::nullopt;
    }

    if (s.present.empty() && s.values.empty())
    {
        error = "no parameters found";
        return std::nullopt;
    }
    return s;
}

// Source note division: 0=1/64 1=1/32 2=1/16 3=1/8 4=1/4 5=1/2 6=1 bar (decoded from the
// "BASIC FX" presets named by their division); timing: 0=straight 1=triplet 2=dotted.
String mapDivision (double divisionIndex, double timing, const String& what, juce::StringArray& approx)
{
    const auto index = juce::jlimit (0, 6, juce::roundToInt (divisionIndex));
    const auto t = juce::jlimit (0, 2, juce::roundToInt (timing));
    const double beats = 4.0 / std::pow (2.0, 6 - index) * (t == 1 ? 2.0 / 3.0 : t == 2 ? 1.5 : 1.0);

    int best = 0;
    double bestError = 1.0e9;
    for (int i = 0; i < dsp::numDivisions; ++i)
    {
        const auto e = std::abs (std::log (dsp::divisionBeats[static_cast<size_t> (i)] / beats));
        if (e < bestError)
        {
            bestError = e;
            best = i;
        }
    }
    if (bestError > 1.0e-3)
    {
        static const char* names[] = { "1/64", "1/32", "1/16", "1/8", "1/4", "1/2", "1/1" };
        approx.add (what + ": note value " + String (names[index]) + (t == 1 ? " triplet" : t == 2 ? " dotted" : "")
                    + " not available, using " + dsp::divisionNames[static_cast<size_t> (best)]);
    }
    return dsp::divisionNames[static_cast<size_t> (best)];
}

String moduleToKey (const String& module)
{
    if (module == "Distortion") return "sat";
    if (module == "Resampler") return "rs";
    if (module == "Tape") return "tape";
    if (module == "Chorus") return "chorus";
    if (module == "Modulation") return "trem";
    if (module == "ReverseDelay") return "rev";
    if (module == "Delay") return "delay";
    if (module == "Reverb") return "reverb";
    return {};
}

bool differs (double a, double b, double tolerance = 1.0e-3) { return std::abs (a - b) > tolerance; }
} // namespace

//==============================================================================
String toString (PresetImporter::Status s)
{
    switch (s)
    {
        case PresetImporter::Status::converted: return "converted";
        case PresetImporter::Status::partial: return "partial";
        case PresetImporter::Status::failed: return "failed";
    }
    return {};
}

bool PresetImporter::isSupportedFile (const juce::File& f)
{
    return f.hasFileExtension ("fwd;fmod");
}

String PresetImporter::categoryFromFolder (const String& folderPath)
{
    juce::StringArray parts;
    for (auto part : juce::StringArray::fromTokens (folderPath.replaceCharacter ('\\', '/'), "/", {}))
    {
        part = part.trim();
        // "2. CREATIVE FX" -> "CREATIVE FX"
        if (part.containsChar ('.') && part.upToFirstOccurrenceOf (".", false, false).trim().containsOnly ("0123456789"))
            part = part.fromFirstOccurrenceOf (".", false, false).trim();
        // "CHORUS (CH-0RU5)" / "SIGNATURE FX (START HERE)" -> drop the parenthetical
        if (part.containsChar ('('))
            part = part.upToFirstOccurrenceOf ("(", false, false).trim();
        if (part.isEmpty() || part.equalsIgnoreCase ("Presets"))
            continue;
        // Title case, keeping short all-caps words like "FX".
        juce::StringArray words;
        for (auto w : juce::StringArray::fromTokens (part, " ", {}))
            words.add (w.length() <= 2 ? w.toUpperCase() : w.substring (0, 1).toUpperCase() + w.substring (1).toLowerCase());
        parts.add (words.joinIntoString (" "));
    }
    return parts.joinIntoString (" / ");
}

void PresetImporter::splitCreator (const String& fileStem, String& creator, String& name)
{
    auto s = fileStem.trim();
    while (s.startsWithChar ('!'))
        s = s.substring (1).trim();
    if (s.startsWithChar ('@') && s.contains (" - "))
    {
        creator = s.upToFirstOccurrenceOf (" - ", false, false).trim();
        name = s.fromFirstOccurrenceOf (" - ", false, false).trim();
    }
    else
    {
        creator = {};
        name = s;
    }
    if (name.isEmpty())
        name = fileStem.trim();
}

PresetImporter::Result PresetImporter::convert (const String& xmlText, const String& relativePath)
{
    Result r;
    r.sourcePath = relativePath;

    String product, error;
    juce::StringArray graph;
    auto parsed = parse (xmlText, product, error, graph);
    if (! parsed)
    {
        r.error = error;
        return r;
    }
    auto& s = *parsed;

    juce::StringArray approx, missing; // approximations / unsupported features in use
    auto& p = r.preset.params;

    const auto normalised = relativePath.replaceCharacter ('\\', '/');
    const auto folder = normalised.upToLastOccurrenceOf ("/", false, false);
    const auto stem = normalised.fromLastOccurrenceOf ("/", false, false).upToLastOccurrenceOf (".", false, false);
    splitCreator (stem, r.preset.creator, r.preset.name);
    const auto sub = categoryFromFolder (folder);
    r.preset.category = sub.isEmpty() ? String ("Imported") : "Imported / " + sub;
    r.preset.isFactory = false;
    r.preset.isImported = true;
    r.preset.sourceProduct = product.trim();
    r.preset.sourceFile = normalised;
    // Content fingerprint used to skip files that were already imported.
    r.preset.sourceHash = String::toHexString (xmlText.hashCode64()) + "-" + String (xmlText.length());

    // ---- Processing order: FXGraph lists every module; keep the mapped ones in that order.
    static const juce::StringArray defaultGraph { "Distortion", "Resampler", "Tape", "Chorus", "Modulation", "ReverseDelay", "Delay", "Reverb" };
    if (graph.isEmpty())
    {
        graph = defaultGraph;
        approx.add ("Processing order not stored in the file - default order used");
    }

    double driftMax = 0.0, unstableMax = 0.0;
    auto movement = [&] (const String& module) {
        driftMax = std::max (driftMax, s.get (module + "_Drift"));
        unstableMax = std::max (unstableMax, s.get (module + "_Instability"));
    };

    std::vector<int> chain;
    for (const auto& module : graph)
    {
        const auto key = moduleToKey (module);
        if (key.isEmpty() || ! s.active (module, approx))
            continue;

        if (module == "Distortion")
        {
            // Drive is a linear gain factor (min 1.0 = unity in every sample) -> dB.
            const auto drive = std::max (1.0, s.get ("Distortion_Drive"));
            p.set (ParamID::satDrive, juce::jlimit (0.0, (double) dsp::Saturation::maxDriveDb, 20.0 * std::log10 (drive)));
            p.set (ParamID::satTone, juce::jlimit (-1.0, 1.0, s.get ("Distortion_Tone")));
            p.set (ParamID::satMix, juce::jlimit (0.0, 1.0, s.get ("Distortion_Mix")));
            p.set (ParamID::satMode, s.get ("Distortion_Mode") >= 0.5 ? "Hard" : "Soft");
            approx.add ("Distortion: character mode mapped to " + String (s.get ("Distortion_Mode") >= 0.5 ? "Hard" : "Soft") + " (source modes differ)");
            if (differs (s.get ("Distortion_Frequency"), 500.0, 1.0))
                missing.add ("Distortion: frequency control (" + String (juce::roundToInt (s.get ("Distortion_Frequency"))) + " Hz) has no equivalent");
            movement ("Distortion");
        }
        else if (module == "Resampler")
        {
            const auto rate = s.get ("Resampler_SampleRate");
            p.set (ParamID::rsRate, juce::jlimit (500.0, 48000.0, rate));
            if (rate < 500.0)
                approx.add ("Resampler: " + String (rate, 0) + " Hz is below the 500 Hz minimum - set to 500 Hz");
            p.set (ParamID::rsBits, 24.0); // the source has no bit-depth control
            p.set (ParamID::rsJitter, juce::jlimit (0.0, 1.0, s.get ("Resampler_Jitter")));
            p.set (ParamID::rsFilter, s.get ("Resampler_PostFilter") >= 0.5 ? juce::jlimit (200.0, 20000.0, 0.45 * rate) : 20000.0);
            p.set (ParamID::rsMix, 1.0);
            if (differs (s.get ("Resampler_Drive"), 1.0, 0.05))
                missing.add ("Resampler: drive (" + String (s.get ("Resampler_Drive"), 2) + ") has no equivalent");
            if (s.get ("Resampler_PreFilter") >= 0.5)
                approx.add ("Resampler: anti-alias pre-filter not available (more aliasing)");
            movement ("Resampler");
        }
        else if (module == "Tape")
        {
            // Range = overall pitch-modulation depth, Balance = share of flutter (units unknown).
            const auto range = juce::jlimit (0.0, 1.0, s.get ("Tape_Range"));
            const auto balance = juce::jlimit (0.0, 1.0, s.get ("Tape_Balance"));
            p.set (ParamID::tapeWow, juce::jlimit (0.0, 1.0, 2.5 * range * (1.0 - balance)));
            p.set (ParamID::tapeFlutter, juce::jlimit (0.0, 1.0, 2.5 * range * balance));
            const auto noiseMode = juce::roundToInt (s.get ("Tape_NoiseMode"));
            const auto noiseGain = std::pow (10.0, s.get ("Tape_NoiseGain") / 20.0);
            p.set (ParamID::tapeNoise, noiseMode == 0 ? 0.0 : juce::jlimit (0.0, 1.0, noiseGain / 0.03));
            if (noiseMode > 1)
                approx.add ("Tape: noise type " + String (noiseMode) + " approximated by plain hiss");
            // Head-wear filter -> Age (inverse of the Age -> cutoff curve), dropouts raise it.
            const auto lpf = juce::jlimit (1000.0, 22000.0, s.get ("Tape_LPF"));
            const auto ageFromLpf = juce::jlimit (0.0, 1.0, std::log (lpf / 18000.0) / std::log (3500.0 / 18000.0));
            const auto ageFromDropouts = std::sqrt (juce::jlimit (0.0, 1.0, s.get ("Tape_DropOutDepth") / 0.6));
            p.set (ParamID::tapeAge, std::max (ageFromLpf, ageFromDropouts));
            p.set (ParamID::tapeDrive, 0.0);
            p.set (ParamID::tapeMix, 1.0);
            approx.add ("Tape: wow/flutter depth, noise and age scaled approximately (source units differ)");
            if (s.get ("Tape_Flux") > 0.01 || s.get ("Tape_ScrapeLevel") > 0.01)
                missing.add ("Tape: flux / scrape character has no equivalent");
            if (s.get ("Tape_HPF") > 30.0)
                missing.add ("Tape: low-cut filter (" + String (juce::roundToInt (s.get ("Tape_HPF"))) + " Hz) has no equivalent");
            approx.add ("Tape: wow and flutter rates are fixed in SHOBEATS");
            movement ("Tape");
        }
        else if (module == "Chorus")
        {
            const auto rate = s.get ("Chorus_Rate");
            p.set (ParamID::chorusRate, juce::jlimit (0.05, 5.0, rate));
            p.set (ParamID::chorusDepth, juce::jlimit (0.0, 1.0, s.get ("Chorus_Depth")));
            p.set (ParamID::chorusMix, juce::jlimit (0.0, 1.0, s.get ("Chorus_Mix")));
            p.set (ParamID::chorusWidth, s.get ("Chorus_Stereo") >= 0.5 ? juce::jlimit (0.0, 1.0, s.get ("Chorus_Width") > 0.0 ? s.get ("Chorus_Width") : 1.0) : 0.0);
            if (rate < 0.05)
                approx.add ("Chorus: rate below 0.05 Hz raised to 0.05 Hz");
            if (s.get ("Chorus_Saturation") > 3.5)
                missing.add ("Chorus: saturation (" + String (s.get ("Chorus_Saturation"), 1) + ") has no equivalent");
            if (s.get ("Chorus_Clock") > 0.01)
                missing.add ("Chorus: clock/character control has no equivalent");
            if (s.get ("Chorus_Waveform") >= 0.5)
                approx.add ("Chorus: alternative LFO waveform not available (sine used)");
        }
        else if (module == "Modulation")
        {
            const bool sync = s.get ("Modulation_Sync") >= 0.5;
            p.set (ParamID::tremSync, sync);
            if (sync)
                p.set (ParamID::tremDivision, mapDivision (s.get ("Modulation_NoteDivision"), s.get ("Modulation_NoteTiming"), "Tremolo", approx));
            p.set (ParamID::tremRate, juce::jlimit (0.1, 20.0, s.get ("Modulation_Rate")));
            p.set (ParamID::tremDepth, juce::jlimit (0.0, 1.0, s.get ("Modulation_Depth")));
            const auto shape = s.get ("Modulation_Shape");
            p.set (ParamID::tremShape, shape >= 0.8 ? "Square" : shape <= 0.2 ? "Triangle" : "Sine");
            p.set (ParamID::tremStereo, s.get ("Modulation_Stereo") >= 0.5 ? 1.0 : 0.0);
            approx.add ("Modulation -> Tremolo: continuous shape mapped to the nearest of Sine/Triangle/Square; source mode " + String (juce::roundToInt (s.get ("Modulation_Mode"))) + " not interpreted");
            movement ("Modulation");
        }
        else if (module == "ReverseDelay")
        {
            p.set (ParamID::revSync, true);
            p.set (ParamID::revDivision, mapDivision (s.get ("ReverseDelay_NoteDivision"), s.get ("ReverseDelay_NoteTiming"), "Reverse", approx));
            const bool pitchOn = s.get ("ReverseDelay_PitchActive") >= 0.5;
            p.set (ParamID::revPitch, pitchOn ? juce::jlimit (-12.0, 12.0, s.get ("ReverseDelay_Pitch")) : 0.0);
            p.set (ParamID::revMix, juce::jlimit (0.0, 1.0, s.get ("ReverseDelay_Mix")));
            if (s.get ("ReverseDelay_Feedback") > 0.05)
                missing.add ("Reverse: feedback (" + String (juce::roundToInt (100.0 * s.get ("ReverseDelay_Feedback"))) + " %) has no equivalent");
            if (s.get ("ReverseDelay_HPF") > 20.0 || s.get ("ReverseDelay_LPF") < 9000.0)
                missing.add ("Reverse: filters have no equivalent");
            if (s.get ("ReverseDelay_Type") >= 0.5)
                missing.add ("Reverse: alternative reverse type not available");
        }
        else if (module == "Delay")
        {
            const bool sync = s.get ("Delay_Sync") >= 0.5;
            p.set (ParamID::delaySync, sync);
            if (sync)
                p.set (ParamID::delayDivision, mapDivision (s.get ("Delay_NoteDivision"), s.get ("Delay_NoteTiming"), "Delay", approx));
            else
            {
                const auto ms = s.get ("Delay_Time") * s.get ("Delay_Adjust");
                p.set (ParamID::delayTime, juce::jlimit (1.0, 2000.0, ms));
                if (ms > 2000.0)
                    approx.add ("Delay: " + String (juce::roundToInt (ms)) + " ms shortened to the 2000 ms maximum");
            }
            const auto fb = s.get ("Delay_Feedback");
            p.set (ParamID::delayFeedback, juce::jlimit (0.0, 0.9, fb));
            if (fb > 0.9)
                approx.add ("Delay: feedback " + String (juce::roundToInt (100.0 * fb)) + " % limited to 90 % (safe level)");
            p.set (ParamID::delayLowCut, juce::jlimit (20.0, 2000.0, s.get ("Delay_HPF")));
            const auto lpf = s.get ("Delay_LPF");
            p.set (ParamID::delayHighCut, juce::jlimit (1000.0, 20000.0, lpf));
            if (lpf < 1000.0)
                approx.add ("Delay: high-cut " + String (juce::roundToInt (lpf)) + " Hz raised to the 1 kHz minimum");
            p.set (ParamID::delayMix, juce::jlimit (0.0, 1.0, s.get ("Delay_Mix")));
            p.set (ParamID::delayPingPong, false);
            if (s.get ("Delay_Type") >= 0.5)
                missing.add ("Delay: alternative delay type not available (standard stereo delay used)");
            if (s.get ("Delay_Freeze") >= 0.5)
                missing.add ("Delay: freeze has no equivalent");
            if (s.get ("Delay_ModDepth") > 0.01)
                missing.add ("Delay: time modulation has no equivalent");
            if (s.get ("Delay_Saturation") > 3.5)
                missing.add ("Delay: feedback saturation (" + String (s.get ("Delay_Saturation"), 1) + ") has no equivalent");
            if (s.get ("Delay_Clock") > 0.01)
                missing.add ("Delay: clock/character control has no equivalent");
            movement ("Delay");
        }
        else if (module == "Reverb")
        {
            const auto decay = s.get ("Reverb_Decay");
            p.set (ParamID::reverbDecay, juce::jlimit (0.2, 20.0, decay));
            if (decay > 20.0)
                approx.add ("Reverb: decay " + String (decay, 1) + " s shortened to 20 s");
            p.set (ParamID::reverbSize, juce::jlimit (0.0, 1.0, s.get ("Reverb_Size")));
            p.set (ParamID::reverbPredelay, juce::jlimit (0.0, 250.0, s.get ("Reverb_PreDelay")));
            p.set (ParamID::reverbMix, juce::jlimit (0.0, 1.0, s.get ("Reverb_Mix")));
            // High damping cutoff -> Damping (inverse of the Damping -> cutoff curve).
            const auto damp = juce::jlimit (1200.0, 18000.0, s.get ("Reverb_HighDamp"));
            p.set (ParamID::reverbDamping, juce::jlimit (0.0, 1.0, std::log (damp / 18000.0) / std::log (1200.0 / 18000.0)));
            static const char* types[] = { "room", "plate", "hall" };
            approx.add ("Reverb: " + String (types[juce::jlimit (0, 2, juce::roundToInt (s.get ("Reverb_Type")))]) + " type approximated by the SHOBEATS reverb");
            if (s.get ("Reverb_LowDamp") > 20.0)
                missing.add ("Reverb: low-frequency damping has no equivalent");
            if (s.get ("Reverb_ModDepth") > 0.01)
                approx.add ("Reverb: modulation depth not mapped (SHOBEATS reverb uses fixed modulation plus Drift)");
            if (s.get ("Reverb_Saturation") > 0.5)
                missing.add ("Reverb: saturation has no equivalent");
            if (differs (s.get ("Reverb_Blend"), 0.5, 0.05))
                missing.add ("Reverb: early/late blend has no equivalent");
            movement ("Reverb");
        }

        const auto id = effectIdForKey (key);
        if (id >= 0)
            chain.push_back (id);
    }

    // ---- Global modules and settings.
    if (s.has ("Global_InputGain"))
        p.set (ParamID::inputGain, juce::jlimit (-24.0, 24.0, s.get ("Global_InputGain")));
    if (s.has ("Global_OutputGain"))
    {
        const auto out = s.get ("Global_OutputGain");
        p.set (ParamID::outputGain, juce::jlimit (-24.0, 6.0, out));
        if (out > 6.0)
            approx.add ("Output gain limited to +6 dB (safe level)");
    }
    if (differs (s.get ("Pitch_Semitones"), 0.0, 0.01) || differs (s.get ("Pitch_Cents"), 0.0, 0.5))
        missing.add ("Pitch shift (" + String (s.get ("Pitch_Semitones"), 1) + " st) has no equivalent - the preset will not be transposed");
    if (s.get ("Shifter_Active") >= 0.5)
        missing.add ("Shifter module has no equivalent");
    if (s.get ("EQ_Active") >= 0.5
        && (differs (s.get ("EQ_LowGain"), 0.0, 0.1) || differs (s.get ("EQ_MidGain"), 0.0, 0.1) || differs (s.get ("EQ_HighGain"), 0.0, 0.1)))
        missing.add ("EQ has no equivalent");
    if (differs (s.get ("Width_Width"), 1.0, 0.02))
        missing.add ("Stereo width module (" + String (juce::roundToInt (100.0 * s.get ("Width_Width"))) + " %) has no equivalent");
    for (const auto& [key, value] : s.values)
    {
        if (key.startsWith ("ModMatrix_") && std::abs (value) > 1.0e-3)
        {
            const auto lfo = key.fromFirstOccurrenceOf ("ModMatrix_", false, false).upToFirstOccurrenceOf ("_", false, false);
            if (s.get (lfo + "_Active") >= 0.5)
            {
                missing.addIfNotAlreadyThere ("LFO modulation matrix (" + lfo + ") has no equivalent");
            }
        }
        if (key.startsWith ("Mixer_") && key != "Mixer_Active" && differs (value, 0.0, 0.01))
            missing.addIfNotAlreadyThere ("Internal mixer levels/sends have no equivalent");
        if (key.startsWith ("Macro") && ! key.contains ("Target") && differs (value, 0.0, 1.0e-3))
            missing.addIfNotAlreadyThere ("Macro control has no equivalent");
    }

    // ---- Level safety. Measured on the 650-preset sample: positive Tone on a saturated
    // (harmonic-rich) signal and Hard mode at very high drive make the SHOBEATS saturation
    // louder than its level compensation expects, and several fully-wet space effects
    // stacked add up. Trim those cases so loading a preset never jumps in level.
    if (std::find (chain.begin(), chain.end(), static_cast<int> (EffectId::saturation)) != chain.end())
    {
        const auto tone = static_cast<double> (p[ParamID::satTone]);
        const auto driveDb = static_cast<double> (p[ParamID::satDrive]);
        auto trim = -3.0 * std::max (0.0, tone);
        if (p[ParamID::satMode].toString() == "Hard")
            trim -= std::min (4.0, std::max (0.0, driveDb - 12.0) * 0.25);
        if (trim < -0.05)
        {
            p.set (ParamID::satOutput, trim);
            approx.add ("Saturation output trimmed by " + String (trim, 1) + " dB to keep the level safe");
        }
    }
    {
        int wet = 0;
        for (auto* id : { ParamID::reverbMix, ParamID::revMix, ParamID::delayMix, ParamID::chorusMix })
            if (p.contains (id) && static_cast<double> (p[id]) >= 0.9)
                ++wet;
        if (wet >= 2)
        {
            const auto out = (p.contains (ParamID::outputGain) ? static_cast<double> (p[ParamID::outputGain]) : 0.0) - 3.0;
            p.set (ParamID::outputGain, juce::jlimit (-24.0, 6.0, out));
            approx.add ("Output trimmed by 3 dB: several effects are fully wet");
        }
    }

    p.set (ParamID::drift, juce::jlimit (0.0, 1.0, driftMax));
    p.set (ParamID::unstable, juce::jlimit (0.0, 1.0, unstableMax));
    if (driftMax > 0.0 || unstableMax > 0.0)
        approx.add ("Per-module drift/instability combined into the global Drift/Unstable controls");

    if (! s.assumed.isEmpty())
        approx.add (String (s.assumed.size()) + " value(s) not stored in the file - source defaults assumed");

    // ---- Chain and order.
    r.preset.chain = chain;
    size_t slot = 0;
    for (auto id : chain)
        r.preset.order[slot++] = id;
    for (int id = 0; id < numEffects; ++id)
        if (std::find (chain.begin(), chain.end(), id) == chain.end())
            r.preset.order[slot++] = id;

    if (chain.empty())
        missing.add ("No supported effect was active in this preset");

    r.status = missing.isEmpty() ? Status::converted : Status::partial;
    r.preset.conversionStatus = toString (r.status);
    for (const auto& m : missing)
        r.preset.conversionNotes.add ("Missing: " + m);
    for (const auto& a : approx)
        r.preset.conversionNotes.add ("Approx: " + a);

    juce::StringArray chainNames;
    for (auto id : chain)
        chainNames.add (effectInfo[static_cast<size_t> (id)].name);
    r.preset.description = "Imported from " + r.preset.sourceProduct + (r.preset.creator.isNotEmpty() ? " by " + r.preset.creator : String())
                           + ". Chain: " + (chainNames.isEmpty() ? String ("none") : chainNames.joinIntoString (" > "))
                           + (r.status == Status::partial ? ". Partially converted: " + String (missing.size()) + " unsupported feature(s)." : ".");
    return r;
}

//==============================================================================
PresetImporter::BatchReport PresetImporter::importInto (const juce::File& source, const juce::File& importRoot)
{
    BatchReport report;
    importRoot.createDirectory();

    // Everything already imported (same source path and same content) is skipped, so repeated
    // imports do not create duplicates. Identical files stored in two different folders of an
    // archive are both kept, because they belong to two categories.
    auto keyOf = [] (const Preset& p) { return p.sourceFile + "|" + p.sourceHash; };
    std::map<String, bool> known;
    for (const auto& f : importRoot.findChildFiles (juce::File::findFiles, true, "*.sfxpreset"))
        if (auto p = Preset::fromVar (juce::JSON::parse (f), {}))
            if (p->sourceHash.isNotEmpty())
                known[keyOf (*p)] = true;

    auto handle = [&] (const String& text, const String& relativePath) {
        const auto result = convert (text, relativePath);
        if (result.status == Status::failed)
        {
            ++report.failed;
            report.lines.add ("FAILED     " + relativePath + "  (" + result.error + ")");
            return;
        }
        if (known.count (keyOf (result.preset)) > 0)
        {
            ++report.duplicates;
            return;
        }
        known[keyOf (result.preset)] = true;

        // Folder structure mirrors the source categories.
        auto dir = importRoot;
        for (const auto& part : juce::StringArray::fromTokens (result.preset.category.fromFirstOccurrenceOf ("Imported", false, false), "/", {}))
            if (part.trim().isNotEmpty())
                dir = dir.getChildFile (juce::File::createLegalFileName (part.trim()));
        dir.createDirectory();
        auto base = juce::File::createLegalFileName ((result.preset.creator.isNotEmpty() ? result.preset.creator + " - " : String()) + result.preset.name);
        auto file = dir.getChildFile (base + ".sfxpreset");
        for (int n = 2; file.existsAsFile(); ++n)
            file = dir.getChildFile (base + " (" + String (n) + ").sfxpreset");
        file.replaceWithText (juce::JSON::toString (result.preset.toVar()));

        (result.status == Status::converted ? report.converted : report.partial)++;
        report.lines.add ((result.status == Status::converted ? "CONVERTED  " : "PARTIAL    ") + relativePath);
        for (const auto& note : result.preset.conversionNotes)
            report.lines.add ("           - " + note);
    };

    if (source.hasFileExtension ("zip"))
    {
        juce::ZipFile zip (source);
        for (int i = 0; i < zip.getNumEntries(); ++i)
        {
            const auto* entry = zip.getEntry (i);
            if (entry == nullptr || entry->isSymbolicLink || entry->filename.endsWithChar ('/'))
                continue;
            const auto name = entry->filename.replaceCharacter ('\\', '/');
            if (name.startsWith ("__MACOSX/") || name.fromLastOccurrenceOf ("/", false, false).startsWithChar ('.'))
                continue;
            if (! (name.endsWithIgnoreCase (".fwd") || name.endsWithIgnoreCase (".fmod")))
                continue;
            std::unique_ptr<juce::InputStream> in (zip.createStreamForEntry (i));
            if (in != nullptr)
                handle (in->readEntireStreamAsString(), name);
        }
    }
    else if (source.isDirectory())
    {
        for (const auto& f : source.findChildFiles (juce::File::findFiles, true))
            if (isSupportedFile (f))
                handle (f.loadFileAsString(), source.getFileName() + "/" + f.getRelativePathFrom (source).replaceCharacter ('\\', '/'));
    }
    else if (isSupportedFile (source))
    {
        handle (source.loadFileAsString(), source.getFileName());
    }

    // Report file.
    juce::StringArray header {
        "SHOBEATS FX preset import report",
        "Source: " + source.getFullPathName(),
        "Date: " + juce::Time::getCurrentTime().toString (true, true),
        "Converted: " + String (report.converted) + "   Partially converted: " + String (report.partial)
            + "   Failed: " + String (report.failed) + "   Skipped (already imported): " + String (report.duplicates),
        "",
        "Converted presets approximate the original sound with SHOBEATS effects; they are not identical.",
        "Missing = a source feature in use that SHOBEATS cannot reproduce. Approx = an approximate translation.",
        ""
    };
    header.addArray (report.lines);
    report.reportFile = importRoot.getChildFile ("import-report-" + juce::Time::getCurrentTime().formatted ("%Y%m%d-%H%M%S") + ".txt");
    report.reportFile.replaceWithText (header.joinIntoString ("\n"));
    return report;
}
} // namespace sfx
