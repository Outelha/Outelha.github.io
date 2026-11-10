#include "PresetBrowser.h"

#include "PluginProcessor.h"

namespace sfx::ui
{
namespace
{
void styleEditor (juce::TextEditor& e, const juce::String& placeholder)
{
    e.setFont (uiFont (14.0f));
    e.setTextToShowWhenEmpty (placeholder, Palette::textFaint);
    e.setColour (juce::TextEditor::backgroundColourId, Palette::display);
    e.setColour (juce::TextEditor::outlineColourId, Palette::panelEdge);
    e.setColour (juce::TextEditor::focusedOutlineColourId, Palette::brand);
    e.setColour (juce::TextEditor::textColourId, Palette::text);
    e.setIndents (10, 7);
}
} // namespace

//==============================================================================
PresetBar::PresetBar (ShobeatsAudioProcessor& p) : processor (p)
{
    for (auto* b : { &prev, &next, &save })
        addAndMakeVisible (*b);
    prev.setTooltip ("Previous preset");
    next.setTooltip ("Next preset");
    save.setTooltip ("Save the current sound as a user preset");
    prev.onClick = [this] { processor.getPresetManager().loadNext (-1); timerCallback(); };
    next.onClick = [this] { processor.getPresetManager().loadNext (1); timerCallback(); };
    save.onClick = [this] { if (onSave) onSave(); };
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    startTimerHz (8);
    timerCallback();
}

juce::Rectangle<int> PresetBar::nameArea() const
{
    return getLocalBounds().withTrimmedLeft (36).withTrimmedRight (36 + 70);
}

void PresetBar::timerCallback()
{
    const auto [name, category] = processor.getCurrentPresetLabel();
    if (name != shownName || category != shownCategory)
    {
        shownName = name;
        shownCategory = category;
        repaint();
    }
}

void PresetBar::paint (juce::Graphics& g)
{
    const auto area = nameArea().toFloat();
    g.setColour (Palette::display);
    g.fillRoundedRectangle (area, 6.0f);
    g.setColour (isMouseOver() ? Palette::brand.withAlpha (0.7f) : Palette::panelEdge);
    g.drawRoundedRectangle (area.reduced (0.5f), 6.0f, 1.0f);

    auto text = nameArea().reduced (12, 0);
    g.setColour (Palette::text);
    g.setFont (uiFont (15.0f, true));
    g.drawText (shownName.isEmpty() ? juce::String ("Init") : shownName, text, juce::Justification::centredLeft);
    g.setColour (Palette::textDim);
    g.setFont (uiFont (11.0f, true));
    g.drawText ((shownCategory.isEmpty() ? juce::String ("Browse presets") : shownCategory.toUpperCase()) + "   v", text, juce::Justification::centredRight);
}

void PresetBar::resized()
{
    auto area = getLocalBounds();
    prev.setBounds (area.removeFromLeft (32));
    save.setBounds (area.removeFromRight (66));
    area.removeFromRight (4);
    next.setBounds (area.removeFromRight (32));
}

void PresetBar::mouseUp (const juce::MouseEvent& e)
{
    if (nameArea().contains (e.getPosition()) && onBrowse)
        onBrowse();
}

//==============================================================================
PresetBrowser::PresetBrowser (PresetManager& m) : manager (m)
{
    juce::StringArray categories { "All" };
    categories.addArray (PresetManager::factoryCategories());
    categories.add (PresetManager::userCategory);
    for (const auto& c : categories)
    {
        auto* b = categoryButtons.add (new juce::TextButton (c));
        b->setClickingTogglesState (false);
        b->onClick = [this, c] { selectCategory (c); };
        addAndMakeVisible (b);
    }

    styleEditor (search, "Search presets, descriptions or instruments...");
    search.onTextChange = [this] { refresh(); };
    search.onEscapeKey = [this] { if (onClose) onClose(); };
    addAndMakeVisible (search);

    list.setRowHeight (52);
    list.setColour (juce::ListBox::backgroundColourId, Palette::display);
    list.setColour (juce::ListBox::outlineColourId, Palette::panelEdge);
    list.setOutlineThickness (1);
    addAndMakeVisible (list);

    close.onClick = [this] { if (onClose) onClose(); };
    remove.setTooltip ("Delete the selected user preset");
    remove.onClick = [this] {
        const auto row = list.getSelectedRow();
        if (row >= 0 && row < static_cast<int> (filtered.size()))
        {
            const auto index = filtered[static_cast<size_t> (row)];
            const auto& preset = manager.getPresets()[static_cast<size_t> (index)];
            juce::NativeMessageBox::showOkCancelBox (juce::MessageBoxIconType::WarningIcon, "Delete preset",
                                                     "Delete the user preset \"" + preset.name + "\"?", this,
                                                     juce::ModalCallbackFunction::create ([this, index] (int result) {
                                                         if (result != 0)
                                                         {
                                                             manager.deleteUserPreset (index);
                                                             refresh();
                                                         }
                                                     }));
        }
    };
    addAndMakeVisible (close);
    addAndMakeVisible (remove);

    selectCategory ("All");
}

void PresetBrowser::selectCategory (const juce::String& c)
{
    category = c;
    for (auto* b : categoryButtons)
        b->setToggleState (b->getButtonText() == c, juce::dontSendNotification);
    refresh();
}

void PresetBrowser::refresh()
{
    filtered.clear();
    const auto terms = juce::StringArray::fromTokens (search.getText(), true);
    const auto& presets = manager.getPresets();
    for (size_t i = 0; i < presets.size(); ++i)
    {
        const auto& p = presets[i];
        if (category != "All" && p.category != category)
            continue;
        bool match = true;
        for (const auto& t : terms)
            match = match && (p.name.containsIgnoreCase (t) || p.description.containsIgnoreCase (t) || p.category.containsIgnoreCase (t));
        if (match)
            filtered.push_back (static_cast<int> (i));
    }
    list.updateContent();
    list.deselectAllRows();
    for (size_t r = 0; r < filtered.size(); ++r)
        if (filtered[r] == manager.getCurrentIndex())
        {
            list.selectRow (static_cast<int> (r));
            break;
        }
    remove.setEnabled (category == PresetManager::userCategory || category == "All");
    repaint();
}

int PresetBrowser::getNumRows()
{
    return static_cast<int> (filtered.size());
}

void PresetBrowser::paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected)
{
    if (row < 0 || row >= static_cast<int> (filtered.size()))
        return;
    const auto index = filtered[static_cast<size_t> (row)];
    const auto& p = manager.getPresets()[static_cast<size_t> (index)];
    const bool current = index == manager.getCurrentIndex();

    auto area = juce::Rectangle<int> (0, 0, width, height).reduced (6, 3);
    if (selected || current)
    {
        g.setColour (Palette::brand.withAlpha (selected ? 0.22f : 0.1f));
        g.fillRoundedRectangle (area.toFloat(), 6.0f);
    }
    area.reduce (10, 4);
    auto top = area.removeFromTop (22);
    g.setColour (current ? Palette::brand : Palette::text);
    g.setFont (uiFont (15.0f, true));
    g.drawText (p.name, top, juce::Justification::centredLeft);
    g.setColour (Palette::textDim);
    g.setFont (uiFont (10.5f, true));
    g.drawText (p.category.toUpperCase(), top, juce::Justification::centredRight);
    g.setFont (uiFont (12.0f));
    g.setColour (Palette::textDim);
    g.drawText (p.description, area, juce::Justification::centredLeft, true);
}

void PresetBrowser::listBoxItemClicked (int row, const juce::MouseEvent&)
{
    if (row >= 0 && row < static_cast<int> (filtered.size()))
    {
        manager.load (filtered[static_cast<size_t> (row)]);
        list.repaint();
    }
}

void PresetBrowser::listBoxItemDoubleClicked (int row, const juce::MouseEvent&)
{
    loadRowAndClose (row);
}

void PresetBrowser::returnKeyPressed (int row)
{
    loadRowAndClose (row);
}

void PresetBrowser::loadRowAndClose (int row)
{
    if (row >= 0 && row < static_cast<int> (filtered.size()))
        manager.load (filtered[static_cast<size_t> (row)]);
    if (onClose)
        onClose();
}

bool PresetBrowser::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)
    {
        if (onClose)
            onClose();
        return true;
    }
    return false;
}

void PresetBrowser::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (0.55f));
    const auto panel = getLocalBounds().reduced (60, 40).toFloat();
    g.setColour (Palette::panel);
    g.fillRoundedRectangle (panel, 14.0f);
    g.setColour (Palette::panelEdge);
    g.drawRoundedRectangle (panel.reduced (0.5f), 14.0f, 1.0f);

    auto header = panel.reduced (24.0f, 18.0f).removeFromTop (30.0f);
    g.setColour (Palette::text);
    g.setFont (uiFont (22.0f, true));
    g.drawText ("PRESETS", header, juce::Justification::centredLeft);
    g.setColour (Palette::textDim);
    g.setFont (uiFont (12.0f));
    g.drawText (juce::String (filtered.size()) + " shown  /  " + juce::String (manager.getPresets().size()) + " total", header,
                juce::Justification::centredRight);
}

void PresetBrowser::resized()
{
    auto panel = getLocalBounds().reduced (60, 40).reduced (24, 18);
    panel.removeFromTop (40);
    auto bottom = panel.removeFromBottom (34);
    close.setBounds (bottom.removeFromRight (110));
    bottom.removeFromRight (10);
    remove.setBounds (bottom.removeFromRight (110));
    panel.removeFromBottom (12);

    auto left = panel.removeFromLeft (210);
    for (auto* b : categoryButtons)
    {
        b->setBounds (left.removeFromTop (34));
        left.removeFromTop (6);
    }
    panel.removeFromLeft (18);
    search.setBounds (panel.removeFromTop (36));
    panel.removeFromTop (10);
    list.setBounds (panel);
}

//==============================================================================
GeneratorStrip::GeneratorStrip (PresetManager& m) : manager (m)
{
    const auto names = Generator::vibeNames();
    for (int i = 0; i < names.size(); ++i)
    {
        auto* b = vibes.add (new juce::TextButton (names[i]));
        b->setConnectedEdges ((i > 0 ? juce::Button::ConnectedOnLeft : 0) | (i < names.size() - 1 ? juce::Button::ConnectedOnRight : 0));
        b->setTooltip ("Vibe for the generator: " + names[i]);
        b->onClick = [this, i] {
            manager.setSelectedVibe (static_cast<Generator::Vibe> (i));
            refreshSelection();
        };
        addAndMakeVisible (b);
    }
    // Always drawn "lit" so it reads as the primary action.
    generate.setColour (juce::TextButton::buttonOnColourId, Palette::brand);
    generate.setToggleState (true, juce::dontSendNotification);
    generate.setTooltip ("Create a new effect chain in the selected vibe (save it with Save if you like it)");
    generate.onClick = [this] { manager.generate (manager.getSelectedVibe()); };
    addAndMakeVisible (generate);
    refreshSelection();
}

void GeneratorStrip::refreshSelection()
{
    for (int i = 0; i < vibes.size(); ++i)
        vibes[i]->setToggleState (i == static_cast<int> (manager.getSelectedVibe()), juce::dontSendNotification);
}

void GeneratorStrip::resized()
{
    auto area = getLocalBounds();
    generate.setBounds (area.removeFromRight (juce::jmax (80, area.getWidth() / 5)));
    area.removeFromRight (6);
    const auto w = area.getWidth() / juce::jmax (1, vibes.size());
    for (int i = 0; i < vibes.size(); ++i)
        vibes[i]->setBounds (i == vibes.size() - 1 ? area : area.removeFromLeft (w));
}

//==============================================================================
SavePresetPanel::SavePresetPanel (PresetManager& m) : manager (m)
{
    styleEditor (name, "Preset name");
    styleEditor (description, "Description (optional): sound, instruments...");
    name.onReturnKey = [this] { doSave(); };
    name.onEscapeKey = [this] { if (onClose) onClose(); };
    save.onClick = [this] { doSave(); };
    cancel.onClick = [this] { if (onClose) onClose(); };
    for (juce::Component* c : { (juce::Component*) &name, (juce::Component*) &description, (juce::Component*) &save, (juce::Component*) &cancel })
        addAndMakeVisible (*c);
}

void SavePresetPanel::open (const juce::String& suggestedName)
{
    name.setText (suggestedName, false);
    description.clear();
    message.clear();
    name.selectAll();
    name.grabKeyboardFocus();
    repaint();
}

void SavePresetPanel::doSave()
{
    const auto n = name.getText().trim();
    if (n.isEmpty())
    {
        message = "Please enter a name.";
        repaint();
        return;
    }
    if (manager.saveUserPreset (n, description.getText().trim()))
    {
        if (onClose)
            onClose();
    }
    else
    {
        message = "Could not write to " + manager.getUserDirectory().getFullPathName();
        repaint();
    }
}

void SavePresetPanel::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (0.55f));
    const auto panel = getLocalBounds().withSizeKeepingCentre (520, 260).toFloat();
    g.setColour (Palette::panel);
    g.fillRoundedRectangle (panel, 14.0f);
    g.setColour (Palette::panelEdge);
    g.drawRoundedRectangle (panel.reduced (0.5f), 14.0f, 1.0f);
    g.setColour (Palette::text);
    g.setFont (uiFont (20.0f, true));
    g.drawText ("SAVE USER PRESET", panel.reduced (24.0f, 18.0f).removeFromTop (28.0f), juce::Justification::centredLeft);
    if (message.isNotEmpty())
    {
        g.setColour (Palette::ledOn);
        g.setFont (uiFont (12.0f));
        g.drawText (message, panel.reduced (24.0f, 18.0f).removeFromBottom (34.0f).withTrimmedRight (240.0f), juce::Justification::centredLeft);
    }
}

void SavePresetPanel::resized()
{
    auto panel = getLocalBounds().withSizeKeepingCentre (520, 260).reduced (24, 18);
    panel.removeFromTop (40);
    name.setBounds (panel.removeFromTop (36));
    panel.removeFromTop (10);
    description.setBounds (panel.removeFromTop (36));
    auto bottom = panel.removeFromBottom (34);
    save.setBounds (bottom.removeFromRight (110));
    bottom.removeFromRight (10);
    cancel.setBounds (bottom.removeFromRight (110));
}
} // namespace sfx::ui
