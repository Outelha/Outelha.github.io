#pragma once

#include "Preset.h"

namespace sfx
{
class ShobeatsAudioProcessor;

// Factory library (compiled in) + user presets on disk. Message thread only.
class PresetManager
{
public:
    static inline const juce::String userCategory { "User" };
    static inline const juce::String fileExtension { ".sfxpreset" };

    explicit PresetManager (ShobeatsAudioProcessor& processor);

    static juce::StringArray factoryCategories();
    static juce::File defaultUserDirectory();

    void setUserDirectory (const juce::File& dir);
    const juce::File& getUserDirectory() const noexcept { return userDirectory; }
    void rescanUserPresets();

    const std::vector<Preset>& getPresets() const noexcept { return presets; }
    int getNumFactoryPresets() const noexcept { return numFactory; }
    int indexOf (const juce::String& name, const juce::String& category) const;

    // Applies a preset with a click-free transition. Returns false for a bad index.
    bool load (int index);
    bool loadNext (int direction);

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
    static juce::String safeFileName (const juce::String& name);

    ShobeatsAudioProcessor& processor;
    std::vector<Preset> presets; // factory first, then user
    int numFactory = 0;
    int currentIndex = -1;
    juce::String currentName, currentCategory;
    juce::File userDirectory;
    juce::StringArray loadErrors;
};
} // namespace sfx
