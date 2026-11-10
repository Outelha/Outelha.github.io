#pragma once

#include "Controls.h"
#include "presets/PresetManager.h"

namespace sfx::ui
{
// Compact navigation strip: previous / current preset (click to browse) / next / save.
class PresetBar final : public juce::Component, private juce::Timer
{
public:
    PresetBar (ShobeatsAudioProcessor& processor);

    std::function<void()> onBrowse, onSave;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    juce::Rectangle<int> nameArea() const;

    ShobeatsAudioProcessor& processor;
    juce::TextButton prev { "<" }, next { ">" }, save { "Save" };
    juce::String shownName, shownCategory;
};

// Full preset browser: categories, search, list with descriptions, load / delete.
class PresetBrowser final : public juce::Component, private juce::ListBoxModel
{
public:
    explicit PresetBrowser (PresetManager& manager);

    std::function<void()> onClose;
    void refresh();

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    int getNumRows() override;
    void paintListBoxItem (int row, juce::Graphics&, int width, int height, bool selected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override;
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;
    void returnKeyPressed (int row) override;
    void selectCategory (const juce::String& category);
    void loadRowAndClose (int row);

    PresetManager& manager;
    juce::OwnedArray<juce::TextButton> categoryButtons;
    juce::TextEditor search;
    juce::ListBox list { "Presets", this };
    juce::TextButton close { "Close" }, remove { "Delete" };
    juce::String category { "All" };
    std::vector<int> filtered;
};

// Vibe selector + GENERATE button for the FX idea generator.
class GeneratorStrip final : public juce::Component
{
public:
    explicit GeneratorStrip (PresetManager& manager);
    void resized() override;
    void refreshSelection();

private:
    PresetManager& manager;
    juce::OwnedArray<juce::TextButton> vibes;
    juce::TextButton generate { "Generate" };
};

// Small modal-style panel to name and save a user preset.
class SavePresetPanel final : public juce::Component
{
public:
    explicit SavePresetPanel (PresetManager& manager);
    void open (const juce::String& suggestedName);

    std::function<void()> onClose;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void doSave();

    PresetManager& manager;
    juce::TextEditor name, description;
    juce::TextButton save { "Save" }, cancel { "Cancel" };
    juce::String message;
};
} // namespace sfx::ui
