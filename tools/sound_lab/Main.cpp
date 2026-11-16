// SHOBEATS SOUND LAB - offline candidate generator and quality control.
//
//   shobeats_sound_lab generate --out <dir> [--reference <dir>] [--exclude <curation.json>]
//                               [--batches 5] [--per-category 10] [--seed 2026]
//
// Generates batches of original candidates (sfx::lab::generate), renders each one through
// the real processor, level-matches it, rejects technical failures, near-duplicates and
// candidates too close to a local reference library (--reference: a folder of .sfxpreset
// files, e.g. a private import; never written to the output), and writes:
//   <out>/NN_<category>.json   accepted banks (format "shobeats-fx-lab")
//   <out>/manifest.json/.csv   every candidate: status, reason, metrics, seed
// Fully deterministic for the same arguments and generator version. Runs on this thread
// only; nothing here touches an audio callback.

#include "PluginProcessor.h"
#include "presets/PresetManager.h"
#include "presets/SoundLab.h"

#include <cstdio>
#include <map>

namespace
{
using juce::String;

String arg (const juce::StringArray& args, const String& name, const String& fallback = {})
{
    const auto i = args.indexOf (name);
    return i >= 0 && i + 1 < args.size() ? args[i + 1] : fallback;
}

String slug (const String& s)
{
    return s.replace ("R&B", "RnB").toLowerCase().replace ("&", "and").retainCharacters ("abcdefghijklmnopqrstuvwxyz0123456789 ").trim().replace (" ", "_");
}

struct Ref
{
    sfx::Preset preset;
    std::vector<float> features;
};

// Fingerprints of presets the user hid (rejected) in the browser's curation file.
juce::StringArray rejectedFingerprints (const juce::File& file)
{
    juce::StringArray out;
    if (! file.existsAsFile())
        return out;
    const auto json = juce::JSON::parse (file);
    if (auto* list = json["presets"].getArray())
        for (const auto& e : *list)
            if (static_cast<bool> (e["rejected"]) && e["fingerprint"].toString().isNotEmpty())
                out.add (e["fingerprint"].toString());
    return out;
}
} // namespace

int main (int argc, char* argv[])
{
    juce::StringArray args;
    for (int i = 1; i < argc; ++i)
        args.add (String::fromUTF8 (argv[i]));
    if (args.isEmpty() || args[0] != "generate" || arg (args, "--out").isEmpty())
    {
        std::fprintf (stderr, "usage: %s generate --out <dir> [--reference <dir>] [--exclude <curation.json>] [--include-rejected] [--batches 5] [--per-category 10] [--seed 2026]\n", argv[0]);
        return 1;
    }

    juce::ScopedJuceInitialiser_GUI juceInit;
    const auto cwd = juce::File::getCurrentWorkingDirectory();
    const auto out = cwd.getChildFile (arg (args, "--out"));
    const auto batches = arg (args, "--batches", "5").getIntValue();
    const auto perCategory = arg (args, "--per-category", "10").getIntValue();
    const auto baseSeed = static_cast<juce::uint32> (arg (args, "--seed", "2026").getLargeIntValue());
    // Presets hidden in the browser never come back unless --include-rejected is given.
    const auto curationFile = arg (args, "--exclude").isNotEmpty() ? cwd.getChildFile (arg (args, "--exclude"))
                                                                    : sfx::PresetManager::defaultCurationFile();
    const auto excluded = args.contains ("--include-rejected") ? juce::StringArray() : rejectedFingerprints (curationFile);
    std::printf ("Curation file: %s (%d rejected fingerprints excluded)\n", curationFile.getFullPathName().toRawUTF8(), excluded.size());

    sfx::ShobeatsAudioProcessor rangesOwner; // parameter ranges for validation / distances
    auto& ranges = rangesOwner.getState();
    const auto input = sfx::audit::testSignal();

    // Reference library (local only): measured once, compared against, never copied.
    std::vector<Ref> reference;
    if (const auto refArg = arg (args, "--reference"); refArg.isNotEmpty())
    {
        for (const auto& f : cwd.getChildFile (refArg).findChildFiles (juce::File::findFiles, true, "*.sfxpreset"))
            if (auto p = sfx::Preset::fromVar (juce::JSON::parse (f), "Reference"))
            {
                auto m = sfx::audit::measure (*p, input);
                reference.push_back ({ std::move (*p), std::move (m.features) });
            }
        std::printf ("Reference library: %d presets (used for similarity only)\n", static_cast<int> (reference.size()));
    }

    struct Row
    {
        sfx::lab::Candidate c;
        String status, reason;
        sfx::lab::QcResult qc;
        double nearestCandidate = 1.0, nearestReference = 1.0;
    };
    std::vector<Row> rows;
    std::vector<size_t> accepted;
    juce::StringArray taken;

    for (int b = 0; b < batches; ++b)
    {
        int passed = 0;
        for (auto& c : sfx::lab::generateBatch (b, perCategory, baseSeed, &taken))
        {
            Row row { std::move (c), {}, {}, {} };
            auto& preset = row.c.preset;
            const auto fp = sfx::lab::fingerprint (preset);
            if (excluded.contains (fp))
            {
                row.status = "rejected";
                row.reason = "rejected by user curation";
                rows.push_back (std::move (row));
                continue;
            }

            row.qc = sfx::lab::qualityCheck (preset, input, ranges);
            if (! row.qc.passed)
            {
                row.status = "rejected";
                row.reason = row.qc.reason;
                rows.push_back (std::move (row));
                continue;
            }

            for (auto idx : accepted)
            {
                const auto& other = rows[idx];
                const auto s = sfx::lab::structuralDistance (preset, other.c.preset, ranges);
                const auto a = sfx::audit::featureDistance (row.qc.metrics.features, other.qc.metrics.features);
                row.nearestCandidate = std::min (row.nearestCandidate, s);
                if (row.reason.isEmpty() && sfx::lab::isDuplicate (s, a))
                    row.reason = "duplicate of " + other.c.id + " (structural " + String (s, 3) + ", audio " + String (a, 2) + " dB)";
            }
            for (const auto& ref : reference)
            {
                const auto s = sfx::lab::structuralDistance (preset, ref.preset, ranges);
                row.nearestReference = std::min (row.nearestReference, s);
                if (row.reason.isEmpty() && s < sfx::lab::referenceStructuralWithAudio)
                {
                    const auto a = sfx::audit::featureDistance (row.qc.metrics.features, ref.features);
                    if (sfx::lab::isTooCloseToReference (s, a))
                        row.reason = "too close to the reference library (structural " + String (s, 3) + ", audio " + String (a, 2) + " dB)";
                }
            }

            row.status = row.reason.isEmpty() ? "accepted" : "rejected";
            if (row.status == "accepted")
            {
                accepted.push_back (rows.size());
                ++passed;
            }
            rows.push_back (std::move (row));
        }
        std::printf ("Batch %d: %d candidates, %d accepted\n", b + 1, perCategory * sfx::lab::numCategories, passed);
    }

    // Banks of accepted candidates, one per category, plus the manifest of everything.
    out.createDirectory();
    for (const auto& f : out.findChildFiles (juce::File::findFiles, false, "*.json"))
        f.deleteFile();
    std::map<int, juce::Array<juce::var>> banks;
    juce::Array<juce::var> manifest;
    String csv = "id,status,reason,category,name,architecture,seed,batch,chain,output_gain_db,peak_dbfs,level_change_db,tail_growth_db,realtime_factor,nearest_candidate,nearest_reference\n";
    int nAccepted = 0, nInvalid = 0, nDuplicate = 0, nReference = 0, nCuration = 0;

    for (auto& row : rows)
    {
        auto& c = row.c;
        auto* meta = new juce::DynamicObject();
        meta->setProperty ("id", c.id);
        meta->setProperty ("generatorVersion", sfx::lab::generatorVersion);
        meta->setProperty ("seed", static_cast<juce::int64> (c.seed));
        meta->setProperty ("baseSeed", static_cast<juce::int64> (baseSeed));
        meta->setProperty ("batch", c.batch + 1);
        meta->setProperty ("labCategory", sfx::lab::categoryNames()[c.category]);
        meta->setProperty ("architecture", c.architecture);
        meta->setProperty ("purpose", c.purpose);
        meta->setProperty ("instruments", c.instruments);
        meta->setProperty ("chainSummary", sfx::lab::describeChain (c.preset));
        meta->setProperty ("fingerprint", sfx::lab::fingerprint (c.preset));
        meta->setProperty ("status", row.status == "accepted" ? "technically validated, not artistically approved" : "rejected");
        if (row.qc.metrics.features.size() > 0)
        {
            auto* qc = new juce::DynamicObject();
            qc->setProperty ("peakDbfs", std::round (row.qc.metrics.peakDb * 10.0) / 10.0);
            qc->setProperty ("levelChangeDb", std::round (row.qc.metrics.levelChangeDb * 10.0) / 10.0);
            qc->setProperty ("tailGrowthDb", std::round (row.qc.metrics.tailGrowthDb * 10.0) / 10.0);
            qc->setProperty ("nearestCandidate", std::round (row.nearestCandidate * 1000.0) / 1000.0);
            if (! reference.empty())
                qc->setProperty ("nearestReference", std::round (row.nearestReference * 1000.0) / 1000.0);
            meta->setProperty ("qc", juce::var (qc));
        }
        c.preset.lab = juce::var (meta);
        c.preset.isLab = true;

        if (row.status == "accepted")
        {
            ++nAccepted;
            banks[c.category].add (c.preset.toVar());
        }
        else if (row.reason.startsWith ("duplicate"))
            ++nDuplicate;
        else if (row.reason.startsWith ("too close"))
            ++nReference;
        else if (row.reason.startsWith ("rejected by user"))
            ++nCuration;
        else
            ++nInvalid;

        auto* m = new juce::DynamicObject();
        m->setProperty ("status", row.status);
        m->setProperty ("reason", row.reason);
        m->setProperty ("name", c.preset.name);
        m->setProperty ("lab", c.preset.lab);
        m->setProperty ("realtimeFactor", std::round (row.qc.metrics.realtimeFactor * 1000.0) / 1000.0); // timing: varies by machine
        manifest.add (juce::var (m));

        auto q = [] (const String& s) { return "\"" + s.replace ("\"", "'") + "\""; };
        csv << c.id << "," << row.status << "," << q (row.reason) << "," << q (sfx::lab::categoryNames()[c.category]) << "," << q (c.preset.name) << ","
            << c.architecture << "," << String (static_cast<juce::int64> (c.seed)) << "," << (c.batch + 1) << "," << q (sfx::lab::describeChain (c.preset)) << ","
            << String (row.qc.outputGainDb, 1) << "," << String (row.qc.metrics.peakDb, 1) << "," << String (row.qc.metrics.levelChangeDb, 1) << ","
            << String (row.qc.metrics.tailGrowthDb, 1) << "," << String (row.qc.metrics.realtimeFactor, 3) << "," << String (row.nearestCandidate, 3) << ","
            << (reference.empty() ? String() : String (row.nearestReference, 3)) << "\n";
    }

    for (const auto& [category, list] : banks)
    {
        auto* bank = new juce::DynamicObject();
        bank->setProperty ("format", "shobeats-fx-lab");
        bank->setProperty ("version", 1);
        bank->setProperty ("category", "SHOBEATS Lab / " + sfx::lab::categoryNames()[category]);
        bank->setProperty ("presets", list);
        const auto file = out.getChildFile (String (category + 1).paddedLeft ('0', 2) + "_" + slug (sfx::lab::categoryNames()[category]) + ".json");
        file.replaceWithText (juce::JSON::toString (juce::var (bank)));
    }

    auto* summary = new juce::DynamicObject();
    summary->setProperty ("generatorVersion", sfx::lab::generatorVersion);
    summary->setProperty ("baseSeed", static_cast<juce::int64> (baseSeed));
    summary->setProperty ("batches", batches);
    summary->setProperty ("perCategory", perCategory);
    summary->setProperty ("generated", static_cast<int> (rows.size()));
    summary->setProperty ("accepted", nAccepted);
    summary->setProperty ("rejectedInvalid", nInvalid);
    summary->setProperty ("rejectedDuplicate", nDuplicate);
    summary->setProperty ("rejectedTooCloseToReference", nReference);
    summary->setProperty ("rejectedByCuration", nCuration);
    summary->setProperty ("referenceCompared", ! reference.empty());
    summary->setProperty ("candidates", manifest);
    out.getChildFile ("manifest.json").replaceWithText (juce::JSON::toString (juce::var (summary)));
    out.getChildFile ("manifest.csv").replaceWithText (csv);

    std::printf ("Generated %d, accepted %d, rejected: invalid/QC %d, duplicates %d, too close to reference %d, curation %d\n",
                 static_cast<int> (rows.size()), nAccepted, nInvalid, nDuplicate, nReference, nCuration);
    return 0;
}
