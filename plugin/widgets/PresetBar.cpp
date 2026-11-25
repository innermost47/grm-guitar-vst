// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
#include "PresetBar.h"

using grmg::models::Preset;

PresetBar::PresetBar()
{
    menu.setTextWhenNothingSelected("Presets");
    menu.setTooltip("Presets: a whole guitar's sound at once (its patterns and its mixer strip stay)");
    menu.onChange = [this]
    { choose(menu.getSelectedId() - 1); };
    saveButton.onClick = [this]
    { openSave(); };
    deleteButton.onClick = [this]
    { openDelete(); };
    for (auto *c : std::initializer_list<juce::Component *>{&menu, &saveButton, &deleteButton})
        addAndMakeVisible(c);
    addMouseListener(this, true);
    refresh();
}

void PresetBar::refresh()
{
    shown = library->list();
    const auto name = currentName ? currentName() : juce::String();
    menu.clear(juce::dontSendNotification);
    std::string category;
    int current = 0;
    for (size_t k = 0; k < shown.size(); ++k)
    {
        const auto &preset = shown[k];
        if (preset.category != category)
        {
            category = preset.category;
            menu.addSectionHeading(juce::String(category));
        }
        menu.addItem(juce::String(preset.name), (int)k + 1);
        // the guitar's preset; one of the user's of that name wins over the factory's (it was saved over it)
        if (juce::String(preset.name) == name && (current == 0 || !preset.factory))
            current = (int)k + 1;
    }
    if (current > 0)
        menu.setSelectedId(current, juce::dontSendNotification);
    else
        menu.setText(name, juce::dontSendNotification); // empty: "Presets"; a deleted one: its name
    deleteButton.setEnabled(selected() != nullptr && !selected()->factory);
}

const Preset *PresetBar::selected() const
{
    const int k = menu.getSelectedId() - 1;
    return k >= 0 && k < (int)shown.size() ? &shown[(size_t)k] : nullptr;
}

void PresetBar::choose(int index)
{
    if (index < 0 || index >= (int)shown.size())
        return;
    const auto preset = shown[(size_t)index];
    if (apply)
        apply(preset);
    refresh();
}

bool PresetBar::save(const juce::String &name)
{
    const auto trimmed = name.trim();
    if (trimmed.isEmpty() || !capture)
        return false;
    const auto preset = capture(trimmed.toStdString());
    if (!library->save(preset))
        return false;
    if (apply)
        apply(preset); // the guitar is that preset now (nothing it sounds changes)
    refresh();
    return true;
}

bool PresetBar::remove(const Preset &preset)
{
    if (preset.factory || !library->remove(preset.name))
        return false;
    refresh();
    return true;
}

void PresetBar::openSave()
{
    if (!showDialog)
        return;
    auto dialog = std::make_unique<Modal>("Save the preset");
    dialog->setMessage("This guitar's sound: every setting but its patterns and its mixer strip. It is kept on this "
                       "computer and found again in every project. A preset of your own with the same name is replaced.");
    auto &field = dialog->addTextField("Preset name", ui::PRESET_NAME_MAX);
    if (const auto *mine = selected(); mine != nullptr && !mine->factory)
        field.setText(juce::String(mine->name), false);
    dialog->addButton("Cancel");
    auto &ok = dialog->addButton("Save", [this, &field]
                                 {
        const auto name = field.getText().trim();
        if (!save(name))
            openFailure("The preset \"" + name + "\" could not be written in " + library->getFolder().getFullPathName() + "."); });
    ok.setEnabled(field.getText().trim().isNotEmpty());
    field.onTextChange = [&field, &ok]
    { ok.setEnabled(field.getText().trim().isNotEmpty()); };
    field.onReturnKey = [&ok]
    {
        if (ok.isEnabled())
            ok.onClick();
    };
    showDialog(std::move(dialog));
}

void PresetBar::openDelete()
{
    const auto *preset = selected();
    if (!showDialog || preset == nullptr || preset->factory)
        return;
    auto dialog = std::make_unique<Modal>("Delete the preset");
    dialog->setMessage("\"" + juce::String(preset->name) + "\" will be gone from the menu of every project. The guitars "
                       "that use it keep their sound.");
    dialog->addButton("Cancel");
    dialog->addButton("Delete", [this, target = *preset]
                      {
        if (!remove(target))
            openFailure("The preset \"" + juce::String(target.name) + "\" could not be deleted from " + library->getFolder().getFullPathName() + "."); });
    showDialog(std::move(dialog));
}

void PresetBar::openFailure(const juce::String &what)
{
    auto dialog = std::make_unique<Modal>("The preset is not changed");
    dialog->setMessage(what);
    dialog->addButton("Close");
    showDialog(std::move(dialog));
}

void PresetBar::mouseEnter(const juce::MouseEvent &)
{
    refresh(); // a preset saved by another instance of the plugin
}

void PresetBar::resized()
{
    auto area = getLocalBounds();
    const int icon = area.getHeight();
    deleteButton.setBounds(area.removeFromRight(icon));
    area.removeFromRight(4);
    saveButton.setBounds(area.removeFromRight(icon));
    area.removeFromRight(6);
    menu.setBounds(area);
}
