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

// Full preset browser: groups (Factory, SHOBEATS Lab, User, Imported, Approved,
// Favorites), sub-categories, search, rating filter, star ratings / favourites / hide per
// row, multi-select export as a native collection and promotion to the Approved collection.
class PresetBrowser final : public juce::Component, private juce::ListBoxModel
{
public:
    explicit PresetBrowser (PresetManager& manager);

    static constexpr const char* allGroup = "All";
    static constexpr const char* factoryGroup = "Factory";
    static constexpr const char* favoritesGroup = "Favorites";
    static juce::StringArray groupNames(); // All, Factory, SHOBEATS Lab, User, Imported, Approved, Favorites

    std::function<void()> onClose;
    void refresh();

    // Converts presets from another plugin (zip / folder / file); shows a summary.
    void importFrom (const juce::File& source);
    // Accepts a group name, or the old category names "User" / "Imported".
    void showCategory (const juce::String& groupName) { selectGroup (groupName); }
    void setSubCategory (const juce::String& sub);
    void setSearchText (const juce::String& text) { search.setText (text, true); }
    void setMinRating (int stars); // 0 = any, -1 = unrated only
    void setShowHidden (bool show) { showHidden.setToggleState (show, juce::sendNotification); }
    int getNumShown() const noexcept { return static_cast<int> (filtered.size()); }
    int getRowWidth() const { return list.getVisibleRowWidth(); }
    int getShownIndex (int row) const { return row >= 0 && row < getNumShown() ? filtered[static_cast<size_t> (row)] : -1; }

    // Row-level actions (also used by the mouse handling).
    void clickRow (int row, juce::Point<int> positionInRow, juce::ModifierKeys mods);
    std::vector<int> selectedIndices() const;
    void selectAllShown();
    bool exportSelectedTo (const juce::File& zip);
    int promoteSelected();

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

    // Row geometry (row-local), shared by painting and hit-testing.
    static juce::Rectangle<int> starArea (int width, int star);
    static juce::Rectangle<int> heartArea (int width);
    static juce::Rectangle<int> hideArea (int width);

private:
    int getNumRows() override;
    void paintListBoxItem (int row, juce::Graphics&, int width, int height, bool selected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override;
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;
    void returnKeyPressed (int row) override;
    void selectGroup (const juce::String& group);
    void loadRowAndClose (int row);
    bool matchesGroup (int index) const;

    PresetManager& manager;
    juce::OwnedArray<juce::TextButton> groupButtons;
    juce::TextEditor search;
    juce::ListBox list { "Presets", this };
    juce::TextButton close { "Close" }, remove { "Delete" }, import { "Import..." }, exportButton { "Export..." }, promote { "Approve" };
    juce::ComboBox subCategory, ratingFilter;
    juce::ToggleButton showHidden { "Show hidden" };
    juce::String group { allGroup };
    std::vector<int> filtered;
    std::unique_ptr<juce::FileChooser> chooser;
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
