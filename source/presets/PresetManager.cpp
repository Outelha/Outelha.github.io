#include "PresetManager.h"

#include "PluginProcessor.h"
#include "ShobeatsLabData.h"
#include "ShobeatsPresetData.h"
#include "SoundLab.h"

namespace sfx
{
PresetManager::PresetManager (ShobeatsAudioProcessor& p) : processor (p)
{
    loadFactoryPresets();
    userDirectory = defaultUserDirectory();
    importDirectory = defaultImportDirectory();
    approvedDirectory = defaultApprovedDirectory();
    curationFile = defaultCurationFile();
    loadCuration();
    rescanUserPresets();
}

juce::StringArray PresetManager::factoryCategories()
{
    return { "Vintage / Analog", "Dreamy / Ambient", "Dark / Trap", "Lo-Fi / Texture", "Space / Experimental", "Creative / Movement" };
}

juce::File PresetManager::defaultUserDirectory()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("SHOBEATS")
        .getChildFile ("SHOBEATS FX")
        .getChildFile ("Presets");
}

juce::File PresetManager::defaultImportDirectory()
{
    return defaultUserDirectory().getParentDirectory().getChildFile ("Imported");
}

juce::File PresetManager::defaultApprovedDirectory()
{
    return defaultUserDirectory().getParentDirectory().getChildFile ("Approved");
}

juce::File PresetManager::defaultCurationFile()
{
    return defaultUserDirectory().getParentDirectory().getChildFile ("curation.json");
}

void PresetManager::setApprovedDirectory (const juce::File& dir)
{
    approvedDirectory = dir;
    rescanUserPresets();
}

void PresetManager::setImportDirectory (const juce::File& dir)
{
    importDirectory = dir;
    rescanUserPresets();
}

PresetImporter::BatchReport PresetManager::importPresets (const juce::File& source)
{
    auto report = PresetImporter::importInto (source, importDirectory);
    rescanUserPresets();
    return report;
}

juce::StringArray PresetManager::importedFolders() const
{
    juce::StringArray folders;
    for (const auto& p : presets)
        if (p.isImported)
            folders.addIfNotAlreadyThere (p.category.fromFirstOccurrenceOf ("Imported / ", false, false));
    folders.removeEmptyStrings();
    folders.sort (true);
    return folders;
}

namespace
{
// Reads every bank of a juce_add_binary_data target ("shobeats-fx-presets" / "shobeats-fx-lab").
template <typename GetResource>
std::vector<Preset> readBanks (int count, const char* const* names, const char* const* files, GetResource getResource, juce::StringArray& errors)
{
    std::vector<Preset> out;
    for (int i = 0; i < count; ++i)
    {
        int size = 0;
        const auto* data = getResource (names[i], size);
        if (data == nullptr)
            continue;

        const auto json = juce::JSON::parse (juce::String::fromUTF8 (data, size));
        const auto category = json["category"].toString();
        auto* list = json["presets"].getArray();
        if (list == nullptr || category.isEmpty())
        {
            errors.add (juce::String (files[i]) + ": not a preset bank");
            continue;
        }
        for (auto& entry : *list)
        {
            juce::String error;
            if (auto preset = Preset::fromVar (entry, category, &error))
                out.push_back (std::move (*preset));
            else
                errors.add (error);
        }
    }
    return out;
}
} // namespace

void PresetManager::loadFactoryPresets()
{
    auto factory = readBanks (ShobeatsPresetData::namedResourceListSize, ShobeatsPresetData::namedResourceList,
                              ShobeatsPresetData::originalFilenames, ShobeatsPresetData::getNamedResource, loadErrors);

    // Keep the bank order of the categories.
    const auto order = factoryCategories();
    std::stable_sort (factory.begin(), factory.end(), [&order] (const Preset& a, const Preset& b) {
        return order.indexOf (a.category) < order.indexOf (b.category);
    });

    presets = std::move (factory);
    numFactory = static_cast<int> (presets.size());

    // SHOBEATS Lab candidates (generated offline by tools/sound_lab), in category order.
    auto lab = readBanks (ShobeatsLabData::namedResourceListSize, ShobeatsLabData::namedResourceList,
                          ShobeatsLabData::originalFilenames, ShobeatsLabData::getNamedResource, loadErrors);
    const auto& labOrder = lab::categoryNames();
    std::stable_sort (lab.begin(), lab.end(), [&labOrder] (const Preset& a, const Preset& b) {
        auto rank = [&labOrder] (const Preset& p) { return labOrder.indexOf (p.category.fromFirstOccurrenceOf (" / ", false, false)); };
        return rank (a) < rank (b);
    });
    for (auto& p : lab)
    {
        p.isFactory = false;
        p.isLab = true;
        presets.push_back (std::move (p));
    }
    numBuiltIn = static_cast<int> (presets.size());
}

void PresetManager::setUserDirectory (const juce::File& dir)
{
    userDirectory = dir;
    rescanUserPresets();
}

void PresetManager::rescanUserPresets()
{
    const auto currentKey = std::make_pair (currentName, currentCategory);
    presets.erase (presets.begin() + numBuiltIn, presets.end());

    std::vector<Preset> user;
    for (const auto& file : userDirectory.findChildFiles (juce::File::findFiles, false, juce::String ("*") + fileExtension))
    {
        if (auto preset = Preset::fromVar (juce::JSON::parse (file), userCategory))
        {
            preset->isFactory = false;
            preset->category = userCategory;
            preset->file = file;
            user.push_back (std::move (*preset));
        }
    }
    std::sort (user.begin(), user.end(), [] (const Preset& a, const Preset& b) { return a.name.compareIgnoreCase (b.name) < 0; });
    for (auto& p : user)
        presets.push_back (std::move (p));

    // Imported presets (converted from other plugins), kept separate from user presets.
    std::vector<Preset> imported;
    for (const auto& file : importDirectory.findChildFiles (juce::File::findFiles, true, juce::String ("*") + fileExtension))
    {
        if (auto preset = Preset::fromVar (juce::JSON::parse (file), importedCategory))
        {
            preset->isFactory = false;
            preset->isImported = true;
            if (! preset->category.startsWith (importedCategory))
                preset->category = importedCategory;
            preset->file = file;
            imported.push_back (std::move (*preset));
        }
    }
    std::sort (imported.begin(), imported.end(), [] (const Preset& a, const Preset& b) {
        const auto c = a.category.compareIgnoreCase (b.category);
        return c != 0 ? c < 0 : a.name.compareIgnoreCase (b.name) < 0;
    });
    for (auto& p : imported)
        presets.push_back (std::move (p));

    // Approved collection: presets promoted from any group, kept in their own folder.
    std::vector<Preset> approved;
    for (const auto& file : approvedDirectory.findChildFiles (juce::File::findFiles, true, juce::String ("*") + fileExtension))
    {
        if (auto preset = Preset::fromVar (juce::JSON::parse (file), approvedCategory))
        {
            preset->isFactory = false;
            preset->isImported = false;
            if (! preset->category.startsWith (approvedCategory))
                preset->category = approvedCategory;
            preset->file = file;
            approved.push_back (std::move (*preset));
        }
    }
    std::sort (approved.begin(), approved.end(), [] (const Preset& a, const Preset& b) { return a.name.compareIgnoreCase (b.name) < 0; });
    for (auto& p : approved)
        presets.push_back (std::move (p));

    currentIndex = indexOf (currentKey.first, currentKey.second);
    if (onPresetListChanged)
        onPresetListChanged();
}

int PresetManager::indexOf (const juce::String& name, const juce::String& category) const
{
    for (size_t i = 0; i < presets.size(); ++i)
        if (presets[i].name == name && presets[i].category == category)
            return static_cast<int> (i);
    return -1;
}

bool PresetManager::load (int index)
{
    if (index < 0 || index >= static_cast<int> (presets.size()))
        return false;

    const auto& preset = presets[static_cast<size_t> (index)];
    // Fade out, swap, clear old tails, fade in: no clicks or level spikes while playing.
    processor.requestTransition (true);
    preset.applyTo (processor.getState(), processor.getChainOrder());

    currentIndex = index;
    currentName = preset.name;
    currentCategory = preset.category;
    processor.setCurrentPresetLabel (currentName, currentCategory);
    if (onCurrentPresetChanged)
        onCurrentPresetChanged();
    return true;
}

bool PresetManager::loadNext (int direction)
{
    if (presets.empty())
        return false;
    const auto n = static_cast<int> (presets.size());
    auto index = currentIndex < 0 ? (direction > 0 ? -1 : 0) : currentIndex;
    for (int step = 0; step < n; ++step)
    {
        index = ((index + direction) % n + n) % n;
        if (! getCuration (index).rejected)
            return load (index);
    }
    return false; // everything is hidden
}

PresetManager::Group PresetManager::groupOf (int index) const
{
    if (index < numFactory)
        return Group::factory;
    if (index < numBuiltIn)
        return Group::lab;
    const auto& p = presets[static_cast<size_t> (index)];
    if (p.isImported)
        return Group::imported;
    if (p.category.startsWith (approvedCategory))
        return Group::approved;
    return Group::user;
}

juce::StringArray PresetManager::subCategories (Group group) const
{
    juce::StringArray out;
    for (size_t i = 0; i < presets.size(); ++i)
        if (groupOf (static_cast<int> (i)) == group)
            out.addIfNotAlreadyThere (presets[i].category.fromFirstOccurrenceOf (" / ", false, false));
    out.removeEmptyStrings();
    if (group != Group::lab && group != Group::factory)
        out.sort (true);
    return out;
}

//==============================================================================
void PresetManager::setCurationFile (const juce::File& file)
{
    curationFile = file;
    loadCuration();
    if (onPresetListChanged)
        onPresetListChanged();
}

void PresetManager::loadCuration()
{
    curation.clear();
    if (! curationFile.existsAsFile())
        return;
    const auto json = juce::JSON::parse (curationFile);
    if (auto* list = json["presets"].getArray())
        for (const auto& e : *list)
        {
            const auto key = e["key"].toString();
            if (key.isEmpty())
                continue;
            CurationEntry entry;
            entry.c.rating = juce::jlimit (0, 5, static_cast<int> (e["rating"]));
            entry.c.favorite = static_cast<bool> (e["favorite"]);
            entry.c.rejected = static_cast<bool> (e["rejected"]);
            entry.fingerprint = e["fingerprint"].toString();
            curation[key] = entry;
        }
}

bool PresetManager::saveCuration() const
{
    juce::Array<juce::var> list;
    for (const auto& [key, entry] : curation)
    {
        if (entry.c.rating == 0 && ! entry.c.favorite && ! entry.c.rejected)
            continue;
        auto* o = new juce::DynamicObject();
        o->setProperty ("key", key);
        o->setProperty ("rating", entry.c.rating);
        o->setProperty ("favorite", entry.c.favorite);
        o->setProperty ("rejected", entry.c.rejected);
        o->setProperty ("fingerprint", entry.fingerprint);
        list.add (juce::var (o));
    }
    auto* root = new juce::DynamicObject();
    root->setProperty ("format", "shobeats-fx-curation");
    root->setProperty ("version", 1);
    root->setProperty ("presets", list);
    if (! curationFile.getParentDirectory().createDirectory())
        return false;
    // Write a temporary file first so a crash never leaves a truncated curation file.
    const juce::TemporaryFile temp (curationFile);
    return temp.getFile().replaceWithText (juce::JSON::toString (juce::var (root))) && temp.overwriteTargetFileWithTemporary();
}

PresetManager::Curation PresetManager::getCuration (int index) const
{
    if (index < 0 || index >= static_cast<int> (presets.size()))
        return {};
    const auto it = curation.find (curationKey (presets[static_cast<size_t> (index)]));
    return it != curation.end() ? it->second.c : Curation {};
}

void PresetManager::updateCuration (int index, const std::function<void (Curation&)>& change)
{
    if (index < 0 || index >= static_cast<int> (presets.size()))
        return;
    const auto& preset = presets[static_cast<size_t> (index)];
    auto& entry = curation[curationKey (preset)];
    change (entry.c);
    entry.fingerprint = lab::fingerprint (preset);
    saveCuration();
}

void PresetManager::setRating (int index, int stars)
{
    updateCuration (index, [stars] (Curation& c) { c.rating = juce::jlimit (0, 5, stars); });
}

void PresetManager::setFavorite (int index, bool favorite)
{
    updateCuration (index, [favorite] (Curation& c) { c.favorite = favorite; });
}

void PresetManager::setRejected (int index, bool rejected)
{
    updateCuration (index, [rejected] (Curation& c) { c.rejected = rejected; });
}

bool PresetManager::exportCollection (const std::vector<int>& indices, const juce::File& zipFile, const juce::String& collectionName) const
{
    juce::ZipFile::Builder builder;
    juce::Array<juce::var> entries;
    juce::StringArray usedNames;
    for (auto index : indices)
    {
        if (index < 0 || index >= static_cast<int> (presets.size()))
            continue;
        auto preset = presets[static_cast<size_t> (index)];
        auto fileName = safeFileName (preset.name);
        for (int n = 2; usedNames.contains (fileName, true); ++n)
            fileName = safeFileName (preset.name) + " " + juce::String (n);
        usedNames.add (fileName);

        const auto text = juce::JSON::toString (preset.toVar());
        builder.addEntry (std::make_unique<juce::MemoryInputStream> (text.toRawUTF8(), text.getNumBytesAsUTF8(), true), 9, fileName + fileExtension,
                          juce::Time::getCurrentTime());
        const auto c = getCuration (index);
        auto* e = new juce::DynamicObject();
        e->setProperty ("file", fileName + fileExtension);
        e->setProperty ("name", preset.name);
        e->setProperty ("category", preset.category);
        e->setProperty ("rating", c.rating);
        e->setProperty ("favorite", c.favorite);
        if (preset.lab.isObject())
            e->setProperty ("labId", preset.lab["id"]);
        entries.add (juce::var (e));
    }
    if (entries.isEmpty())
        return false;

    auto* root = new juce::DynamicObject();
    root->setProperty ("format", "shobeats-fx-collection");
    root->setProperty ("version", 1);
    root->setProperty ("name", collectionName);
    root->setProperty ("presets", entries);
    const auto manifest = juce::JSON::toString (juce::var (root));
    builder.addEntry (std::make_unique<juce::MemoryInputStream> (manifest.toRawUTF8(), manifest.getNumBytesAsUTF8(), true), 9, "collection.json",
                      juce::Time::getCurrentTime());

    zipFile.deleteFile();
    juce::FileOutputStream out (zipFile);
    return out.openedOk() && builder.writeToStream (out, nullptr);
}

int PresetManager::promoteToApproved (const std::vector<int>& indices)
{
    if (! approvedDirectory.createDirectory())
        return 0;
    int written = 0;
    for (auto index : indices)
    {
        if (index < 0 || index >= static_cast<int> (presets.size()) || groupOf (index) == Group::approved)
            continue;
        auto preset = presets[static_cast<size_t> (index)];
        auto source = preset.category;
        if (source.startsWith ("SHOBEATS Lab / "))
            source = source.fromFirstOccurrenceOf ("SHOBEATS Lab / ", false, false);
        preset.category = juce::String (approvedCategory) + " / " + source;
        const auto file = approvedDirectory.getChildFile (safeFileName (source + " - " + preset.name) + fileExtension);
        if (file.replaceWithText (juce::JSON::toString (preset.toVar())))
        {
            ++written;
            // Carry the rating / favourite over to the approved copy.
            auto& entry = curation[curationKey (preset)];
            entry.c = getCuration (index);
            entry.c.rejected = false;
            entry.fingerprint = lab::fingerprint (preset);
        }
    }
    saveCuration();
    rescanUserPresets();
    return written;
}

const Preset& PresetManager::generate (Generator::Vibe vibe)
{
    // A new seed each time so every press gives a different idea.
    generatorSeed = generatorSeed * 1664525u + 1013904223u;
    lastGenerated = Generator::generate (vibe, generatorSeed);

    processor.requestTransition (true);
    lastGenerated->applyTo (processor.getState(), processor.getChainOrder());
    currentIndex = -1;
    currentName = lastGenerated->name;
    currentCategory = lastGenerated->category;
    processor.setCurrentPresetLabel (currentName, currentCategory);
    if (onCurrentPresetChanged)
        onCurrentPresetChanged();
    return *lastGenerated;
}

juce::String PresetManager::safeFileName (const juce::String& name)
{
    auto s = juce::File::createLegalFileName (name.trim());
    return s.isEmpty() ? juce::String ("Untitled") : s;
}

std::optional<int> PresetManager::saveUserPreset (const juce::String& name, const juce::String& description)
{
    if (name.trim().isEmpty())
        return std::nullopt;

    auto preset = Preset::capture (processor.getState(), processor.getChainOrder());
    preset.name = name.trim();
    preset.category = userCategory;
    preset.description = description;

    if (! userDirectory.createDirectory())
        return std::nullopt;
    const auto file = userDirectory.getChildFile (safeFileName (preset.name) + fileExtension);
    if (! file.replaceWithText (juce::JSON::toString (preset.toVar())))
        return std::nullopt;

    currentName = preset.name;
    currentCategory = userCategory;
    processor.setCurrentPresetLabel (currentName, currentCategory);
    rescanUserPresets();
    if (onCurrentPresetChanged)
        onCurrentPresetChanged();
    return currentIndex >= 0 ? std::optional<int> (currentIndex) : std::nullopt;
}

bool PresetManager::deleteUserPreset (int index)
{
    if (index < numBuiltIn || index >= static_cast<int> (presets.size()))
        return false;
    const auto& preset = presets[static_cast<size_t> (index)];
    if (! preset.file.deleteFile())
        return false;
    if (index == currentIndex)
    {
        currentName = {};
        currentCategory = {};
        processor.setCurrentPresetLabel ({}, {});
    }
    rescanUserPresets();
    if (onCurrentPresetChanged)
        onCurrentPresetChanged();
    return true;
}

juce::String PresetManager::getCurrentName() const
{
    return currentName;
}

void PresetManager::setCurrentLabel (const juce::String& name, const juce::String& category)
{
    currentName = name;
    currentCategory = category;
    currentIndex = indexOf (name, category);
    processor.setCurrentPresetLabel (name, category);
    // No UI callback here: this can be called from the host's state-restore thread.
    // The editor polls the label.
}
} // namespace sfx
