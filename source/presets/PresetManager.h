#pragma once

#include "Generator.h"
#include "Preset.h"
#include "PresetImporter.h"

#include <map>

namespace sfx
{
class ShobeatsAudioProcessor;

// Factory library (compiled in) + user presets on disk. Message thread only.
class PresetManager
{
public:
    // Plain literals: no static JUCE objects that would be destroyed at DLL unload.
    static constexpr const char* userCategory = "User";
    static constexpr const char* importedCategory = "Imported";
    static constexpr const char* labCategory = "SHOBEATS Lab";
    static constexpr const char* approvedCategory = "Approved";
    static constexpr const char* fileExtension = ".sfxpreset";

    explicit PresetManager (ShobeatsAudioProcessor& processor);

    static juce::StringArray factoryCategories();
    static juce::File defaultUserDirectory();
    static juce::File defaultImportDirectory();
    static juce::File defaultApprovedDirectory();
    static juce::File defaultCurationFile();

    void setUserDirectory (const juce::File& dir);
    const juce::File& getUserDirectory() const noexcept { return userDirectory; }
    void setImportDirectory (const juce::File& dir);
    const juce::File& getImportDirectory() const noexcept { return importDirectory; }
    void setApprovedDirectory (const juce::File& dir);
    const juce::File& getApprovedDirectory() const noexcept { return approvedDirectory; }
    void rescanUserPresets();

    // Converts presets from another plugin (a .zip, a folder or a single .fwd/.fmod file)
    // into native presets stored in the import directory, then rescans.
    PresetImporter::BatchReport importPresets (const juce::File& source);

    // Distinct imported sub-categories, e.g. "Creative FX / Ambient".
    juce::StringArray importedFolders() const;

    const std::vector<Preset>& getPresets() const noexcept { return presets; }
    int getNumFactoryPresets() const noexcept { return numFactory; }
    // Built in = factory + SHOBEATS Lab candidates (compiled in, read-only).
    int getNumBuiltInPresets() const noexcept { return numBuiltIn; }
    int getNumLabPresets() const noexcept { return numBuiltIn - numFactory; }

    enum class Group { factory, lab, user, imported, approved };
    Group groupOf (int index) const;
    // Distinct sub-categories of a group, e.g. "Dark & Cinematic" for the Lab.
    juce::StringArray subCategories (Group group) const;

    // --- Curation (ratings, favourites, hidden) - persisted in a JSON file. ---------------
    struct Curation
    {
        int rating = 0; // 0 = unrated, 1..5 stars
        bool favorite = false;
        bool rejected = false; // hidden from browsing / next-previous; excluded from new Lab batches
    };
    static juce::String curationKey (const Preset& p) { return p.category + "|" + p.name; }
    void setCurationFile (const juce::File& file); // loads it
    const juce::File& getCurationFile() const noexcept { return curationFile; }
    Curation getCuration (int index) const;
    void setRating (int index, int stars);
    void setFavorite (int index, bool favorite);
    void setRejected (int index, bool rejected);

    // Writes the presets as a native collection: a .zip with one .sfxpreset per preset and a
    // collection.json listing them (with ratings). Returns false on a write error.
    bool exportCollection (const std::vector<int>& indices, const juce::File& zipFile, const juce::String& collectionName) const;
    // Copies the presets into the Approved collection folder; returns how many were written.
    int promoteToApproved (const std::vector<int>& indices);
    int indexOf (const juce::String& name, const juce::String& category) const;

    // Applies a preset with a click-free transition. Returns false for a bad index.
    bool load (int index);
    bool loadNext (int direction); // skips hidden (rejected) presets

    // Creates and applies a new idea for the vibe (click-free), and remembers it so it
    // can be saved as a user preset. Returns the generated preset.
    const Preset& generate (Generator::Vibe vibe);
    const std::optional<Preset>& getLastGenerated() const noexcept { return lastGenerated; }

    Generator::Vibe getSelectedVibe() const noexcept { return selectedVibe; }
    void setSelectedVibe (Generator::Vibe v) noexcept { selectedVibe = v; }

    // Saves the current settings as a user preset (overwrites a user preset of the same name).
    std::optional<int> saveUserPreset (const juce::String& name, const juce::String& description);
    bool deleteUserPreset (int index);

    // Name shown in the UI: the loaded preset (empty when nothing has been loaded).
    juce::String getCurrentName() const;
    int getCurrentIndex() const noexcept { return currentIndex; }

    // Called after state restore, and by the generator, to label the current sound.
    void setCurrentLabel (const juce::String& name, const juce::String& category);
    const juce::String& getCurrentCategory() const noexcept { return currentCategory; }

    // Parse errors in the factory library (should be empty; checked by tests).
    const juce::StringArray& getLoadErrors() const noexcept { return loadErrors; }

    std::function<void()> onPresetListChanged, onCurrentPresetChanged;

private:
    void loadFactoryPresets();
    void loadCuration();
    bool saveCuration() const;
    void updateCuration (int index, const std::function<void (Curation&)>& change);
    static juce::String safeFileName (const juce::String& name);

    ShobeatsAudioProcessor& processor;
    std::vector<Preset> presets; // factory, lab, then user, imported, approved
    int numFactory = 0, numBuiltIn = 0;
    int currentIndex = -1;
    juce::String currentName, currentCategory;
    juce::File userDirectory, importDirectory, approvedDirectory, curationFile;
    struct CurationEntry
    {
        Curation c;
        juce::String fingerprint;
    };
    std::map<juce::String, CurationEntry> curation;
    juce::StringArray loadErrors;
    std::optional<Preset> lastGenerated;
    Generator::Vibe selectedVibe = Generator::Vibe::vintage;
    juce::uint32 generatorSeed = static_cast<juce::uint32> (juce::Time::currentTimeMillis());
};
} // namespace sfx
