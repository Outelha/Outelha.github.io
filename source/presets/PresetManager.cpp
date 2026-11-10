#include "PresetManager.h"

#include "PluginProcessor.h"
#include "ShobeatsPresetData.h"

namespace sfx
{
PresetManager::PresetManager (ShobeatsAudioProcessor& p) : processor (p)
{
    loadFactoryPresets();
    userDirectory = defaultUserDirectory();
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

void PresetManager::loadFactoryPresets()
{
    std::vector<Preset> factory;
    for (int i = 0; i < ShobeatsPresetData::namedResourceListSize; ++i)
    {
        int size = 0;
        const auto* data = ShobeatsPresetData::getNamedResource (ShobeatsPresetData::namedResourceList[i], size);
        if (data == nullptr)
            continue;

        const auto json = juce::JSON::parse (juce::String::fromUTF8 (data, size));
        const auto category = json["category"].toString();
        auto* list = json["presets"].getArray();
        if (list == nullptr || category.isEmpty())
        {
            loadErrors.add (juce::String (ShobeatsPresetData::originalFilenames[i]) + ": not a preset bank");
            continue;
        }
        for (auto& entry : *list)
        {
            juce::String error;
            if (auto preset = Preset::fromVar (entry, category, &error))
                factory.push_back (std::move (*preset));
            else
                loadErrors.add (error);
        }
    }

    // Keep the bank order of the categories.
    const auto order = factoryCategories();
    std::stable_sort (factory.begin(), factory.end(), [&order] (const Preset& a, const Preset& b) {
        return order.indexOf (a.category) < order.indexOf (b.category);
    });

    presets = std::move (factory);
    numFactory = static_cast<int> (presets.size());
}

void PresetManager::setUserDirectory (const juce::File& dir)
{
    userDirectory = dir;
    rescanUserPresets();
}

void PresetManager::rescanUserPresets()
{
    const auto currentKey = std::make_pair (currentName, currentCategory);
    presets.erase (presets.begin() + numFactory, presets.end());

    std::vector<Preset> user;
    for (const auto& file : userDirectory.findChildFiles (juce::File::findFiles, false, "*" + fileExtension))
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
    const auto start = currentIndex < 0 ? (direction > 0 ? -1 : 0) : currentIndex;
    return load (((start + direction) % n + n) % n);
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
    if (index < numFactory || index >= static_cast<int> (presets.size()))
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
