#include "PresetBrowser.h"

#include "Hardware.h"
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
    return getLocalBounds().withTrimmedLeft (56).withTrimmedRight (56 + 102).reduced (0, 2);
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
    const auto orange = juce::Colour (0xffff8a1f);
    hw::drawTray (g, area, 9.0f);
    // Warm light rising from the bottom edge of the preset well.
    {
        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (area.toNearestInt());
        hw::drawLightSpill (g, juce::Rectangle<float> (area.getWidth() * 0.8f, 24.0f).withCentre ({ area.getCentreX(), area.getBottom() }), orange,
                            isMouseOver() ? 0.9f : 0.6f);
    }
    g.setColour (isMouseOver() ? orange.withAlpha (0.7f) : juce::Colour (0xff3a3632));
    g.drawRoundedRectangle (area.reduced (0.5f), 9.0f, 1.0f);

    auto text = nameArea().reduced (18, 0).toFloat();
    g.setColour (orange.brighter (0.2f));
    g.setFont (font (Type::labelBold, 23.0f, 0.04f));
    g.drawText ((shownName.isEmpty() ? juce::String ("Init") : shownName).toUpperCase(), text, juce::Justification::centredLeft, true);

    // BROWSE with a menu glyph; the category is shown in the central display.
    auto browse = text.removeFromRight (96.0f);
    auto glyph = browse.removeFromRight (16.0f).withSizeKeepingCentre (14.0f, 11.0f);
    g.setColour (juce::Colour (0xffffb347));
    for (int i = 0; i < 3; ++i)
        g.fillRect (glyph.getX(), glyph.getY() + static_cast<float> (i) * 4.5f, glyph.getWidth(), 1.8f);
    g.setFont (font (Type::labelBold, 16.0f, 0.06f));
    g.drawText ("BROWSE", browse.withTrimmedRight (6.0f), juce::Justification::centredRight, false);
}

void PresetBar::resized()
{
    auto area = getLocalBounds();
    prev.setBounds (area.removeFromLeft (46));
    save.setBounds (area.removeFromRight (88));
    area.removeFromRight (14);
    next.setBounds (area.removeFromRight (46));
}

void PresetBar::mouseUp (const juce::MouseEvent& e)
{
    if (nameArea().contains (e.getPosition()) && onBrowse)
        onBrowse();
}

//==============================================================================
juce::StringArray PresetBrowser::groupNames()
{
    return { allGroup, factoryGroup, PresetManager::labCategory, PresetManager::userCategory, PresetManager::importedCategory,
             PresetManager::approvedCategory, favoritesGroup };
}

PresetBrowser::PresetBrowser (PresetManager& m) : manager (m)
{
    for (const auto& g : groupNames())
    {
        auto* b = groupButtons.add (new juce::TextButton (g.toUpperCase()));
        b->setClickingTogglesState (false);
        b->onClick = [this, g] { selectGroup (g); };
        addAndMakeVisible (b);
    }

    styleEditor (search, "Search names, descriptions, categories, creators or instruments...");
    search.onTextChange = [this] { refresh(); };
    search.onEscapeKey = [this] { if (onClose) onClose(); };
    addAndMakeVisible (search);

    list.setRowHeight (52);
    list.setMultipleSelectionEnabled (true);
    list.setColour (juce::ListBox::backgroundColourId, Palette::display);
    list.setColour (juce::ListBox::outlineColourId, Palette::panelEdge);
    list.setOutlineThickness (1);
    addAndMakeVisible (list);

    for (auto* box : { &subCategory, &ratingFilter })
    {
        box->setColour (juce::ComboBox::backgroundColourId, Palette::display);
        box->setColour (juce::ComboBox::outlineColourId, Palette::panelEdge);
        box->setColour (juce::ComboBox::textColourId, Palette::text);
        box->onChange = [this] { refresh(); };
    }
    addChildComponent (subCategory);
    ratingFilter.addItem ("Any rating", 1);
    for (int stars = 1; stars <= 5; ++stars)
        ratingFilter.addItem (juce::String (stars) + (stars == 5 ? " stars" : "+ stars"), stars + 1);
    ratingFilter.addItem ("Unrated", 7);
    ratingFilter.setSelectedId (1, juce::dontSendNotification);
    ratingFilter.setTooltip ("Filter by your star rating");
    addAndMakeVisible (ratingFilter);

    showHidden.setColour (juce::ToggleButton::textColourId, Palette::textDim);
    showHidden.setColour (juce::ToggleButton::tickColourId, Palette::brand);
    showHidden.setTooltip ("Show presets you have hidden (rejected)");
    showHidden.onClick = [this] { refresh(); };
    addAndMakeVisible (showHidden);

    close.onClick = [this] { if (onClose) onClose(); };
    remove.setTooltip ("Delete the selected user, imported or approved preset");
    remove.onClick = [this] {
        const auto indices = selectedIndices();
        if (indices.size() != 1 || indices.front() < manager.getNumBuiltInPresets())
            return;
        const auto index = indices.front();
        const auto& preset = manager.getPresets()[static_cast<size_t> (index)];
        // The dialog is asynchronous: the editor may be closed (and this browser destroyed)
        // before the user answers, so only act if the browser still exists.
        juce::Component::SafePointer<PresetBrowser> safeThis (this);
        juce::NativeMessageBox::showOkCancelBox (juce::MessageBoxIconType::WarningIcon, "Delete preset",
                                                 "Delete the preset \"" + preset.name + "\"?", this,
                                                 juce::ModalCallbackFunction::create ([safeThis, index] (int result) {
                                                     if (result != 0 && safeThis != nullptr)
                                                     {
                                                         safeThis->manager.deleteUserPreset (index);
                                                         safeThis->refresh();
                                                     }
                                                 }));
    };

    exportButton.setTooltip ("Export the selected presets as a SHOBEATS collection (.zip)");
    exportButton.onClick = [this] {
        if (selectedIndices().empty())
            return;
        chooser = std::make_unique<juce::FileChooser> ("Export collection", juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                                                                                .getChildFile ("SHOBEATS Collection.zip"), "*.zip");
        juce::Component::SafePointer<PresetBrowser> safeThis (this);
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                              [safeThis] (const juce::FileChooser& fc) {
                                  if (safeThis == nullptr || fc.getResult() == juce::File())
                                      return;
                                  const auto file = fc.getResult().withFileExtension ("zip");
                                  const auto ok = safeThis->exportSelectedTo (file);
                                  juce::AlertWindow::showMessageBoxAsync (ok ? juce::MessageBoxIconType::InfoIcon : juce::MessageBoxIconType::WarningIcon,
                                                                          "Export collection",
                                                                          ok ? "Exported to\n" + file.getFullPathName() : "Could not write " + file.getFullPathName(),
                                                                          "OK", safeThis.getComponent());
                              });
    };
    promote.setTooltip ("Copy the selected presets into your Approved collection");
    promote.onClick = [this] {
        const auto n = promoteSelected();
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Approved collection",
                                                juce::String (n) + " preset(s) copied to the Approved collection:\n"
                                                    + manager.getApprovedDirectory().getFullPathName(),
                                                "OK", this);
    };

    import.setTooltip ("Convert presets from another plugin (.zip, .fwd or .fmod) into SHOBEATS presets");
    import.onClick = [this] {
        chooser = std::make_unique<juce::FileChooser> ("Import presets (.zip, .fwd, .fmod)", juce::File::getSpecialLocation (juce::File::userHomeDirectory),
                                                       "*.zip;*.fwd;*.fmod");
        juce::Component::SafePointer<PresetBrowser> safeThis (this);
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [safeThis] (const juce::FileChooser& fc) {
                                  if (safeThis != nullptr && fc.getResult().existsAsFile())
                                      safeThis->importFrom (fc.getResult());
                              });
    };
    for (auto* b : { &close, &remove, &import, &exportButton, &promote })
        addAndMakeVisible (*b);

    selectGroup (allGroup);
}

void PresetBrowser::selectGroup (const juce::String& g)
{
    group = groupNames().contains (g) ? g : juce::String (allGroup);
    for (int i = 0; i < groupButtons.size(); ++i)
        groupButtons[i]->setToggleState (groupNames()[i] == group, juce::dontSendNotification);

    // Sub-categories: factory categories, Lab categories, imported folders, approved sources.
    std::optional<PresetManager::Group> sub;
    if (group == factoryGroup) sub = PresetManager::Group::factory;
    else if (group == PresetManager::labCategory) sub = PresetManager::Group::lab;
    else if (group == PresetManager::importedCategory) sub = PresetManager::Group::imported;
    else if (group == PresetManager::approvedCategory) sub = PresetManager::Group::approved;

    if (sub.has_value())
    {
        const auto previous = subCategory.getText();
        subCategory.clear (juce::dontSendNotification);
        subCategory.addItem ("All categories", 1);
        const auto names = *sub == PresetManager::Group::factory ? PresetManager::factoryCategories() : manager.subCategories (*sub);
        for (int i = 0; i < names.size(); ++i)
            subCategory.addItem (names[i], i + 2);
        const auto index = names.indexOf (previous);
        subCategory.setSelectedId (index >= 0 ? index + 2 : 1, juce::dontSendNotification);
    }
    subCategory.setVisible (sub.has_value());
    resized();
    refresh();
}

void PresetBrowser::setSubCategory (const juce::String& sub)
{
    for (int i = 0; i < subCategory.getNumItems(); ++i)
        if (subCategory.getItemText (i) == sub)
            subCategory.setSelectedItemIndex (i, juce::sendNotificationSync);
}

void PresetBrowser::setMinRating (int stars)
{
    ratingFilter.setSelectedId (stars < 0 ? 7 : juce::jlimit (0, 5, stars) + 1, juce::sendNotificationSync);
}

void PresetBrowser::importFrom (const juce::File& source)
{
    const auto report = manager.importPresets (source);
    selectGroup (PresetManager::importedCategory);

    juce::String message;
    message << "Converted: " << report.converted << "\n"
            << "Partially converted: " << report.partial << "\n"
            << "Failed: " << report.failed << "\n"
            << "Already imported (skipped): " << report.duplicates << "\n\n"
            << "Converted presets approximate the original sounds with SHOBEATS effects. "
            << "Partially converted presets use source features SHOBEATS does not have; each preset lists them.\n\n"
            << "Full report:\n" << report.reportFile.getFullPathName();
    juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Preset import", message, "OK", this);
}

bool PresetBrowser::matchesGroup (int index) const
{
    using G = PresetManager::Group;
    const auto g = manager.groupOf (index);
    if (group == favoritesGroup)
        return manager.getCuration (index).favorite;
    if (group == factoryGroup && g != G::factory) return false;
    if (group == PresetManager::labCategory && g != G::lab) return false;
    if (group == PresetManager::userCategory && g != G::user) return false;
    if (group == PresetManager::importedCategory && g != G::imported) return false;
    if (group == PresetManager::approvedCategory && g != G::approved) return false;

    if (subCategory.isVisible() && subCategory.getSelectedId() > 1)
    {
        const auto& category = manager.getPresets()[static_cast<size_t> (index)].category;
        const auto sub = subCategory.getText();
        if (category != sub && ! category.endsWith (" / " + sub))
            return false;
    }
    return true;
}

void PresetBrowser::refresh()
{
    filtered.clear();
    const auto terms = juce::StringArray::fromTokens (search.getText(), true);
    const auto& presets = manager.getPresets();
    const auto ratingId = ratingFilter.getSelectedId();
    for (size_t i = 0; i < presets.size(); ++i)
    {
        const auto index = static_cast<int> (i);
        if (! matchesGroup (index))
            continue;
        const auto c = manager.getCuration (index);
        if (c.rejected && ! showHidden.getToggleState())
            continue;
        if (ratingId == 7 ? c.rating != 0 : (ratingId > 1 && c.rating < ratingId - 1))
            continue;

        const auto& p = presets[i];
        const auto instruments = p.lab.isObject() ? p.lab["instruments"].toString() + " " + p.lab["purpose"].toString() : juce::String();
        bool match = true;
        for (const auto& t : terms)
            match = match && (p.name.containsIgnoreCase (t) || p.description.containsIgnoreCase (t) || p.category.containsIgnoreCase (t)
                              || p.creator.containsIgnoreCase (t) || instruments.containsIgnoreCase (t));
        if (match)
            filtered.push_back (index);
    }
    list.updateContent();
    list.deselectAllRows();
    for (size_t r = 0; r < filtered.size(); ++r)
        if (filtered[r] == manager.getCurrentIndex())
        {
            list.selectRow (static_cast<int> (r));
            break;
        }
    remove.setEnabled (group != factoryGroup && group != PresetManager::labCategory);
    repaint();
}

std::vector<int> PresetBrowser::selectedIndices() const
{
    std::vector<int> out;
    const auto rows = list.getSelectedRows();
    for (int i = 0; i < rows.size(); ++i)
        if (const auto index = getShownIndex (rows[i]); index >= 0)
            out.push_back (index);
    return out;
}

void PresetBrowser::selectAllShown()
{
    if (! filtered.empty())
        list.selectRangeOfRows (0, getNumShown() - 1, true);
}

bool PresetBrowser::exportSelectedTo (const juce::File& zip)
{
    return manager.exportCollection (selectedIndices(), zip, zip.getFileNameWithoutExtension());
}

int PresetBrowser::promoteSelected()
{
    const auto n = manager.promoteToApproved (selectedIndices());
    selectGroup (group); // the Approved list (and its sub-categories) changed
    return n;
}

int PresetBrowser::getNumRows()
{
    return static_cast<int> (filtered.size());
}

juce::Rectangle<int> PresetBrowser::starArea (int width, int star)
{
    // Top line, right side: [5 stars][heart][hide]
    return { width - 16 - 44 - 5 * 18 + star * 18, 7, 18, 22 };
}

juce::Rectangle<int> PresetBrowser::heartArea (int width) { return { width - 16 - 44, 7, 22, 22 }; }
juce::Rectangle<int> PresetBrowser::hideArea (int width) { return { width - 16 - 22, 7, 22, 22 }; }

void PresetBrowser::paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected)
{
    if (row < 0 || row >= static_cast<int> (filtered.size()))
        return;
    const auto index = filtered[static_cast<size_t> (row)];
    const auto& p = manager.getPresets()[static_cast<size_t> (index)];
    const bool current = index == manager.getCurrentIndex();
    const auto c = manager.getCuration (index);
    const float alpha = c.rejected ? 0.45f : 1.0f;

    auto area = juce::Rectangle<int> (0, 0, width, height).reduced (6, 3);
    if (selected || current)
    {
        g.setColour (Palette::brand.withAlpha (selected ? 0.22f : 0.1f));
        g.fillRoundedRectangle (area.toFloat(), 6.0f);
    }

    // Curation controls.
    for (int s = 0; s < 5; ++s)
    {
        juce::Path star;
        star.addStar (starArea (width, s).toFloat().getCentre(), 5, 3.2f, 7.5f, 0.0f);
        g.setColour (s < c.rating ? Palette::brand : Palette::textFaint.withAlpha (0.6f));
        if (s < c.rating)
            g.fillPath (star);
        else
            g.strokePath (star, juce::PathStrokeType (1.0f));
    }
    {
        const auto r = heartArea (width).toFloat().withSizeKeepingCentre (14.0f, 13.0f);
        juce::Path heart;
        heart.startNewSubPath (r.getCentreX(), r.getBottom());
        heart.cubicTo (r.getX() - 2.0f, r.getCentreY(), r.getX() + 1.0f, r.getY() - 3.0f, r.getCentreX(), r.getY() + 3.0f);
        heart.cubicTo (r.getRight() - 1.0f, r.getY() - 3.0f, r.getRight() + 2.0f, r.getCentreY(), r.getCentreX(), r.getBottom());
        heart.closeSubPath();
        g.setColour (c.favorite ? Palette::ledOn : Palette::textFaint.withAlpha (0.6f));
        if (c.favorite)
            g.fillPath (heart);
        else
            g.strokePath (heart, juce::PathStrokeType (1.0f));
    }
    {
        const auto r = hideArea (width).toFloat().withSizeKeepingCentre (10.0f, 10.0f);
        g.setColour (c.rejected ? Palette::brand : Palette::textFaint.withAlpha (0.6f));
        g.drawLine ({ r.getTopLeft(), r.getBottomRight() }, 1.5f);
        g.drawLine ({ r.getTopRight(), r.getBottomLeft() }, 1.5f);
    }

    area.reduce (10, 4);
    area.removeFromRight (16 + 44 + 5 * 18 - 10);
    auto top = area.removeFromTop (22);
    g.setColour ((current ? Palette::brand : Palette::text).withMultipliedAlpha (alpha));
    g.setFont (uiFont (15.0f, true));
    g.drawText (p.name, top, juce::Justification::centredLeft);
    g.setColour (Palette::textDim.withMultipliedAlpha (alpha));
    g.setFont (uiFont (10.5f, true));
    auto tag = top.removeFromRight (juce::jmin (top.getWidth() / 2, 360));
    auto badge = [&] (const juce::String& text) {
        auto b = tag.removeFromRight (62).reduced (0, 3).toFloat();
        g.setColour (Palette::brand.withAlpha (0.25f));
        g.fillRoundedRectangle (b, 3.0f);
        g.setColour (Palette::brand);
        g.drawText (text, b, juce::Justification::centred);
        tag.removeFromRight (8);
        g.setColour (Palette::textDim.withMultipliedAlpha (alpha));
    };
    if (c.rejected)
        badge ("HIDDEN");
    if (p.isImported && p.conversionStatus == "partial")
        badge ("PARTIAL");
    g.drawText (p.category.toUpperCase(), tag, juce::Justification::centredRight);
    g.setFont (uiFont (12.0f));
    juce::String line = p.description;
    if (p.creator.isNotEmpty())
        line = "by " + p.creator + "  -  " + line;
    if (p.isLab && p.lab.isObject())
        line = p.lab["chainSummary"].toString() + "  -  " + line;
    g.drawText (line, area, juce::Justification::centredLeft, true);
}

void PresetBrowser::clickRow (int row, juce::Point<int> pos, juce::ModifierKeys mods)
{
    const auto index = getShownIndex (row);
    if (index < 0)
        return;
    const auto width = list.getVisibleRowWidth();
    const auto c = manager.getCuration (index);
    for (int s = 0; s < 5; ++s)
        if (starArea (width, s).contains (pos))
        {
            manager.setRating (index, c.rating == s + 1 ? 0 : s + 1); // click the same star again to clear
            list.repaintRow (row);
            return;
        }
    if (heartArea (width).contains (pos))
    {
        manager.setFavorite (index, ! c.favorite);
        if (group == favoritesGroup)
            refresh();
        else
            list.repaintRow (row);
        return;
    }
    if (hideArea (width).contains (pos))
    {
        manager.setRejected (index, ! c.rejected);
        refresh();
        return;
    }
    // Ctrl/Cmd/Shift clicks only build a selection (for export / approve).
    if (! (mods.isCommandDown() || mods.isShiftDown()))
    {
        manager.load (index);
        list.repaint();
    }
}

void PresetBrowser::listBoxItemClicked (int row, const juce::MouseEvent& e)
{
    clickRow (row, e.getPosition(), e.mods);
}

void PresetBrowser::listBoxItemDoubleClicked (int row, const juce::MouseEvent& e)
{
    const auto width = list.getVisibleRowWidth();
    if (e.getPosition().x >= starArea (width, 0).getX())
        return; // double clicks on the rating controls are just two clicks
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
    if (key == juce::KeyPress ('a', juce::ModifierKeys::commandModifier, 0))
    {
        selectAllShown();
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
    if (group == PresetManager::labCategory)
    {
        g.setColour (Palette::textFaint);
        g.setFont (uiFont (11.0f));
        g.drawText ("Generated candidates: technically validated, not artistically approved. Rate, hide or approve them.",
                    header.withTrimmedLeft (130.0f).withTrimmedRight (170.0f), juce::Justification::centredLeft);
    }
}

void PresetBrowser::resized()
{
    auto panel = getLocalBounds().reduced (60, 40).reduced (24, 18);
    panel.removeFromTop (40);
    auto bottom = panel.removeFromBottom (34);
    close.setBounds (bottom.removeFromRight (100));
    bottom.removeFromRight (10);
    remove.setBounds (bottom.removeFromRight (100));
    import.setBounds (bottom.removeFromLeft (120));
    bottom.removeFromLeft (10);
    exportButton.setBounds (bottom.removeFromLeft (110));
    bottom.removeFromLeft (10);
    promote.setBounds (bottom.removeFromLeft (110));
    panel.removeFromBottom (12);

    auto left = panel.removeFromLeft (190);
    for (auto* b : groupButtons)
    {
        b->setBounds (left.removeFromTop (34));
        left.removeFromTop (4);
    }
    panel.removeFromLeft (18);
    auto searchRow = panel.removeFromTop (36);
    showHidden.setBounds (searchRow.removeFromRight (120));
    searchRow.removeFromRight (8);
    ratingFilter.setBounds (searchRow.removeFromRight (120));
    searchRow.removeFromRight (8);
    if (subCategory.isVisible())
    {
        subCategory.setBounds (searchRow.removeFromRight (260));
        searchRow.removeFromRight (8);
    }
    search.setBounds (searchRow);
    panel.removeFromTop (10);
    list.setBounds (panel);
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
