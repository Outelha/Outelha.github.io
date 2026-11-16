// SHOBEATS SOUND LAB: generation, validity, duplicates, shipped Lab banks, curation
// persistence, export / promote, DAW recall of a Lab preset and the browser's curation UI.

#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "presets/SoundLab.h"

#include <catch2/catch_test_macros.hpp>

#include <set>

using sfx::Preset;
using sfx::PresetManager;
using sfx::ShobeatsAudioProcessor;
namespace lab = sfx::lab;

namespace
{
struct Fixture
{
    juce::ScopedJuceInitialiser_GUI juce;
    juce::TemporaryFile root;
    juce::File curation() const { return root.getFile().getChildFile ("curation.json"); }

    // Points every on-disk location of a manager at the temporary folder.
    void isolate (PresetManager& pm) const
    {
        pm.setUserDirectory (root.getFile().getChildFile ("User"));
        pm.setImportDirectory (root.getFile().getChildFile ("Imported"));
        pm.setApprovedDirectory (root.getFile().getChildFile ("Approved"));
        pm.setCurationFile (curation());
    }
    ~Fixture() { root.getFile().deleteRecursively(); }
};

bool haveDisplay()
{
#if JUCE_LINUX
    return std::getenv ("DISPLAY") != nullptr;
#else
    return true;
#endif
}

int firstLab (const PresetManager& pm, const juce::String& subCategory = {})
{
    for (int i = pm.getNumFactoryPresets(); i < pm.getNumBuiltInPresets(); ++i)
        if (subCategory.isEmpty() || pm.getPresets()[static_cast<size_t> (i)].category.endsWith (subCategory))
            return i;
    return -1;
}
} // namespace

TEST_CASE_METHOD (Fixture, "Sound Lab: generation is deterministic for (category, seed)", "[lab]")
{
    for (int category = 0; category < lab::numCategories; ++category)
    {
        const auto a = lab::generate (category, 1234u + static_cast<juce::uint32> (category));
        const auto b = lab::generate (category, 1234u + static_cast<juce::uint32> (category));
        CHECK (juce::JSON::toString (a.preset.toVar()) == juce::JSON::toString (b.preset.toVar()));
        CHECK (a.architecture == b.architecture);
        const auto other = lab::generate (category, 99999u);
        CHECK (lab::fingerprint (other.preset) != lab::fingerprint (a.preset));
    }
    // Batches too.
    const auto x = lab::generateBatch (2, 3, 7u);
    const auto y = lab::generateBatch (2, 3, 7u);
    REQUIRE (x.size() == 30);
    for (size_t i = 0; i < x.size(); ++i)
        CHECK (lab::fingerprint (x[i].preset) == lab::fingerprint (y[i].preset));
}

TEST_CASE_METHOD (Fixture, "Sound Lab: every generated candidate is valid, named, described and in its category", "[lab]")
{
    ShobeatsAudioProcessor ranges;
    juce::StringArray taken;
    const auto batch = lab::generateBatch (0, 20, 2026u, &taken);
    REQUIRE (batch.size() == 200);
    std::set<juce::String> names;
    for (const auto& c : batch)
    {
        INFO (c.id << " " << c.preset.name << " " << c.architecture);
        CHECK (lab::validateParameters (c.preset, ranges.getState()).isEmpty());
        CHECK (! c.preset.chain.empty());
        CHECK (c.preset.chain.size() <= 4);
        CHECK (sfx::ChainOrder::isValid (c.preset.order));
        CHECK (c.preset.name.isNotEmpty());
        CHECK (c.preset.description.isNotEmpty());
        CHECK (c.purpose.isNotEmpty());
        CHECK (c.instruments.isNotEmpty());
        CHECK (c.preset.category == "SHOBEATS Lab / " + lab::categoryNames()[c.category]);
        CHECK (lab::describeChain (c.preset).isNotEmpty());
        CHECK (names.insert (c.preset.name.toLowerCase()).second); // unique names
        // The processing order starts with the active chain, in the architecture's order.
        for (size_t k = 0; k < c.preset.chain.size(); ++k)
            CHECK (c.preset.order[k] == c.preset.chain[k]);
    }
}

TEST_CASE_METHOD (Fixture, "Sound Lab: structural distance and duplicate rules", "[lab]")
{
    ShobeatsAudioProcessor ranges;
    auto& state = ranges.getState();
    const auto a = lab::generate (0, 11u).preset;
    CHECK (lab::structuralDistance (a, a, state) == 0.0);
    CHECK (lab::isDuplicate (lab::structuralDistance (a, a, state), 0.0));

    // A different effect set is far apart.
    auto b = a;
    b.chain = { static_cast<int> (sfx::EffectId::tremolo) };
    CHECK (lab::structuralDistance (a, b, state) > 0.3);

    // Same set, reversed order: further than identical.
    auto c = a;
    std::reverse (c.chain.begin(), c.chain.end());
    CHECK (lab::structuralDistance (a, c, state) > 0.05);

    // Two settings that sound different (audio > threshold) are not duplicates unless near-identical.
    CHECK (! lab::isDuplicate (0.1, lab::duplicateAudioDb + 1.0));
    CHECK (lab::isDuplicate (0.1, lab::duplicateAudioDb * 0.5));
    CHECK (lab::isDuplicate (lab::duplicateStructural * 0.5, 20.0));
    CHECK (lab::isTooCloseToReference (0.1, lab::referenceAudioDb * 0.5));
    CHECK (! lab::isTooCloseToReference (0.4, 0.0));
}

TEST_CASE_METHOD (Fixture, "Sound Lab: quality control level-matches good candidates and rejects invalid ones", "[lab][audio]")
{
    ShobeatsAudioProcessor ranges;
    const auto input = sfx::audit::testSignal();

    auto good = lab::generate (2, 5u).preset; // Vintage & Analog
    const auto q = lab::qualityCheck (good, input, ranges.getState());
    INFO (q.reason);
    CHECK (q.passed);
    CHECK (q.metrics.finite);
    CHECK (q.metrics.peakDb <= lab::maxPeakDb);
    CHECK (std::abs (q.metrics.levelChangeDb) < 3.0);
    CHECK (static_cast<double> (good.params[sfx::ParamID::outputGain]) == q.outputGainDb);

    auto bad = lab::generate (2, 5u).preset;
    bad.params.set (sfx::ParamID::delayFeedback, 1.7); // outside the parameter's range
    CHECK (! lab::qualityCheck (bad, input, ranges.getState()).passed);
    auto badChoice = lab::generate (2, 5u).preset;
    badChoice.params.set (sfx::ParamID::satMode, "Overdrive"); // not a choice
    CHECK (lab::validateParameters (badChoice, ranges.getState()).isNotEmpty());
}

TEST_CASE_METHOD (Fixture, "Sound Lab: shipped Lab banks load separately from the 60 factory presets", "[lab][presets]")
{
    ShobeatsAudioProcessor p;
    auto& pm = p.getPresetManager();
    CHECK (pm.getLoadErrors().isEmpty());
    REQUIRE (pm.getNumFactoryPresets() == 60);
    REQUIRE (pm.getNumLabPresets() > 300);
    CHECK (pm.subCategories (PresetManager::Group::lab) == lab::categoryNames());

    std::set<juce::String> keys;
    for (int i = pm.getNumFactoryPresets(); i < pm.getNumBuiltInPresets(); ++i)
    {
        const auto& preset = pm.getPresets()[static_cast<size_t> (i)];
        INFO (preset.name);
        CHECK (pm.groupOf (i) == PresetManager::Group::lab);
        CHECK (preset.isLab);
        CHECK (! preset.isFactory);
        CHECK (preset.category.startsWith ("SHOBEATS Lab / "));
        CHECK (preset.lab["status"].toString() == "technically validated, not artistically approved");
        CHECK (static_cast<int> (preset.lab["generatorVersion"]) == lab::generatorVersion);
        CHECK (lab::validateParameters (preset, p.getState()).isEmpty());
        CHECK (keys.insert (preset.name.toLowerCase()).second);
        CHECK (pm.deleteUserPreset (i) == false); // built in: read-only
    }
}

TEST_CASE_METHOD (Fixture, "Sound Lab: shipped candidates are reproducible from their seed", "[lab][presets]")
{
    ShobeatsAudioProcessor p;
    auto& pm = p.getPresetManager();
    int checked = 0;
    for (int i = pm.getNumFactoryPresets(); i < pm.getNumBuiltInPresets(); i += 7)
    {
        auto shipped = pm.getPresets()[static_cast<size_t> (i)];
        const auto category = lab::categoryNames().indexOf (shipped.lab["labCategory"].toString());
        REQUIRE (category >= 0);
        const auto seed = static_cast<juce::uint32> (static_cast<juce::int64> (shipped.lab["seed"]));
        auto regenerated = lab::generate (category, seed).preset;
        INFO (shipped.name);
        CHECK (regenerated.chain == shipped.chain);
        CHECK (lab::fingerprint (regenerated) == lab::fingerprint (shipped)); // outputGain (level match) excluded
        CHECK (lab::fingerprint (shipped) == shipped.lab["fingerprint"].toString());
        ++checked;
    }
    CHECK (checked > 40);
}

TEST_CASE_METHOD (Fixture, "Sound Lab: ratings, favourites and hidden presets persist and hidden ones are skipped", "[lab][curation]")
{
    {
        ShobeatsAudioProcessor p;
        auto& pm = p.getPresetManager();
        isolate (pm);
        const auto first = firstLab (pm);
        REQUIRE (first >= 0);
        pm.setRating (first, 4);
        pm.setFavorite (first + 1, true);
        pm.setRejected (first + 2, true);
        pm.setRating (3, 5); // a factory preset can be rated too
        CHECK (pm.getCuration (first).rating == 4);
        CHECK (curation().existsAsFile());

        // Previous / next skip the hidden preset.
        REQUIRE (pm.load (first + 1));
        REQUIRE (pm.loadNext (1));
        CHECK (pm.getCurrentIndex() == first + 3);
        REQUIRE (pm.loadNext (-1));
        CHECK (pm.getCurrentIndex() == first + 1);
    }
    // A new instance (e.g. reopening the DAW project) reads the same curation.
    ShobeatsAudioProcessor q;
    auto& pm = q.getPresetManager();
    isolate (pm);
    const auto first = firstLab (pm);
    CHECK (pm.getCuration (first).rating == 4);
    CHECK (pm.getCuration (first + 1).favorite);
    CHECK (pm.getCuration (first + 2).rejected);
    CHECK (pm.getCuration (3).rating == 5);
    CHECK (pm.getCuration (first + 5).rating == 0);

    // The file records the fingerprint the generator uses to keep rejected sounds out of new batches.
    const auto json = juce::JSON::parse (curation());
    bool found = false;
    for (const auto& e : *json["presets"].getArray())
        if (static_cast<bool> (e["rejected"]))
            found = e["fingerprint"].toString() == lab::fingerprint (pm.getPresets()[static_cast<size_t> (first + 2)]);
    CHECK (found);

    pm.setRejected (first + 2, false);
    CHECK (! pm.getCuration (first + 2).rejected);
}

TEST_CASE_METHOD (Fixture, "Sound Lab: export writes a native collection and promote creates the Approved collection", "[lab][curation]")
{
    ShobeatsAudioProcessor p;
    auto& pm = p.getPresetManager();
    isolate (pm);
    const auto a = firstLab (pm, "Dark & Cinematic");
    const auto b = firstLab (pm, "Space & Ambient");
    REQUIRE (a >= 0);
    REQUIRE (b >= 0);
    pm.setRating (a, 5);

    const auto zipFile = root.getFile().getChildFile ("My Picks.zip");
    REQUIRE (pm.exportCollection ({ a, b, 0 }, zipFile, "My Picks"));
    juce::ZipFile zip (zipFile);
    REQUIRE (zip.getNumEntries() == 4);
    int presetsInZip = 0;
    for (int i = 0; i < zip.getNumEntries(); ++i)
    {
        std::unique_ptr<juce::InputStream> in (zip.createStreamForEntry (i));
        REQUIRE (in != nullptr);
        const auto json = juce::JSON::parse (in->readEntireStreamAsString());
        if (zip.getEntry (i)->filename == "collection.json")
        {
            CHECK (json["format"].toString() == "shobeats-fx-collection");
            CHECK (json["presets"].getArray()->size() == 3);
            CHECK (static_cast<int> ((*json["presets"].getArray())[0]["rating"]) == 5);
        }
        else
        {
            CHECK (zip.getEntry (i)->filename.endsWith (".sfxpreset"));
            CHECK (Preset::fromVar (json, "x").has_value());
            ++presetsInZip;
        }
    }
    CHECK (presetsInZip == 3);

    const auto before = pm.getPresets().size();
    CHECK (pm.promoteToApproved ({ a, b }) == 2);
    CHECK (pm.getPresets().size() == before + 2);
    CHECK (pm.subCategories (PresetManager::Group::approved).contains ("Dark & Cinematic"));
    const auto approved = pm.indexOf (pm.getPresets()[static_cast<size_t> (a)].name, "Approved / Dark & Cinematic");
    REQUIRE (approved >= 0);
    CHECK (pm.groupOf (approved) == PresetManager::Group::approved);
    CHECK (pm.getCuration (approved).rating == 5);
    CHECK (pm.getPresets()[static_cast<size_t> (approved)].file.isAChildOf (pm.getApprovedDirectory()));
    // Promoting again overwrites, it does not duplicate; approved presets can be deleted.
    CHECK (pm.promoteToApproved ({ a }) == 1);
    CHECK (pm.getPresets().size() == before + 2);
    CHECK (pm.deleteUserPreset (pm.indexOf (pm.getPresets()[static_cast<size_t> (b)].name, "Approved / Space & Ambient")));
    CHECK (pm.getPresets().size() == before + 1);
}

TEST_CASE_METHOD (Fixture, "Sound Lab: a Lab preset is recalled exactly from the DAW project state", "[lab][presets]")
{
    ShobeatsAudioProcessor p;
    const auto index = firstLab (p.getPresetManager(), "Rhythmic & Reverse");
    REQUIRE (p.getPresetManager().load (index));
    const auto& preset = p.getPresetManager().getPresets()[static_cast<size_t> (index)];

    juce::MemoryBlock state;
    p.getStateInformation (state);

    ShobeatsAudioProcessor q;
    q.setStateInformation (state.getData(), static_cast<int> (state.getSize()));
    for (auto* id : sfx::allParameterIds)
        CHECK (q.getState().getParameter (id)->getValue() == p.getState().getParameter (id)->getValue());
    CHECK (q.getChainOrder().get() == p.getChainOrder().get());
    const auto [name, category] = q.getCurrentPresetLabel();
    CHECK (name == preset.name);
    CHECK (category == preset.category);
    CHECK (q.getPresetManager().getCurrentIndex() == index);
}

TEST_CASE_METHOD (Fixture, "Sound Lab: browser groups, filters and per-row rating / favourite / hide", "[lab][gui]")
{
    if (! haveDisplay())
        SKIP ("No display available");
    ShobeatsAudioProcessor p;
    auto& pm = p.getPresetManager();
    isolate (pm);
    sfx::ui::PresetBrowser browser (pm);
    browser.setSize (1200, 800);

    browser.showCategory ("Factory");
    CHECK (browser.getNumShown() == 60);
    browser.showCategory (PresetManager::labCategory);
    CHECK (browser.getNumShown() == pm.getNumLabPresets());
    browser.setSubCategory ("Drums & Percussion");
    const auto drums = browser.getNumShown();
    CHECK (drums > 0);
    CHECK (drums < pm.getNumLabPresets());
    browser.setSubCategory ("All categories");

    // Click the 3rd star, the heart and the hide cross of the first row.
    using Browser = sfx::ui::PresetBrowser;
    const auto first = browser.getShownIndex (0);
    const auto w = browser.getRowWidth();
    browser.clickRow (0, Browser::starArea (w, 2).getCentre(), {});
    CHECK (pm.getCuration (first).rating == 3);
    browser.clickRow (0, Browser::starArea (w, 2).getCentre(), {}); // same star again clears
    CHECK (pm.getCuration (first).rating == 0);
    browser.clickRow (0, Browser::starArea (w, 4).getCentre(), {});
    CHECK (pm.getCuration (first).rating == 5);
    browser.clickRow (0, Browser::heartArea (w).getCentre(), {});
    CHECK (pm.getCuration (first).favorite);
    CHECK (pm.getCurrentIndex() != first); // rating controls do not load the preset

    browser.setMinRating (5);
    CHECK (browser.getNumShown() == 1);
    browser.setMinRating (0);
    browser.showCategory (Browser::favoritesGroup);
    CHECK (browser.getNumShown() == 1);
    CHECK (browser.getShownIndex (0) == first);

    browser.showCategory (PresetManager::labCategory);
    const auto shown = browser.getNumShown();
    browser.clickRow (0, Browser::hideArea (w).getCentre(), {});
    CHECK (pm.getCuration (first).rejected);
    CHECK (browser.getNumShown() == shown - 1); // hidden presets disappear...
    browser.setShowHidden (true);
    CHECK (browser.getNumShown() == shown); // ...unless asked for

    // A plain click on the name loads; search covers suggested instruments.
    browser.clickRow (1, { 40, 15 }, {});
    CHECK (pm.getCurrentIndex() == browser.getShownIndex (1));
    browser.setSearchText ("808");
    CHECK (browser.getNumShown() > 0);
    browser.setSearchText ({});

    // Multi-select -> export and approve.
    browser.selectAllShown();
    CHECK (browser.selectedIndices().size() == static_cast<size_t> (browser.getNumShown()));
    const auto zip = root.getFile().getChildFile ("all.zip");
    CHECK (browser.exportSelectedTo (zip));
    CHECK (zip.existsAsFile());
}

TEST_CASE_METHOD (Fixture, "Sound Lab: editor with the Lab browser open survives repeated open / close", "[lab][gui][lifecycle]")
{
    if (! haveDisplay())
        SKIP ("No display available");
    for (int i = 0; i < 10; ++i)
    {
        auto p = std::make_unique<ShobeatsAudioProcessor>();
        p->setRateAndBufferSizeDetails (48000.0, 256);
        p->prepareToPlay (48000.0, 256);
        isolate (p->getPresetManager());
        std::unique_ptr<juce::AudioProcessorEditor> editor (p->createEditorAndMakeActive());
        REQUIRE (editor != nullptr);
        auto& view = dynamic_cast<sfx::ShobeatsAudioProcessorEditor&> (*editor).getView();
        view.showBrowser (true);
        view.getBrowser().showCategory (PresetManager::labCategory);
        view.getBrowser().clickRow (i, { 40, 15 }, {});
        juce::ignoreUnused (editor->createComponentSnapshot (editor->getLocalBounds()));
        p->editorBeingDeleted (editor.get());
        editor.reset();
        p->releaseResources();
        p.reset();
    }
    SUCCEED();
}
