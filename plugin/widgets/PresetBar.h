// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// The presets of a guitar: a menu of them (the factory's, acoustic and electric, then the user's), a button to keep
// this guitar's sound as a preset of one's own (a dialog asks its name) and one to delete one's own. What a preset
// is made of and where it goes is the owner's: it wires capture, apply and the guitar's current preset name.
#pragma once

#include <functional>
#include <memory>
#include <vector>

#include "../../constants/ui.hpp"
#include "../../models/preset.hpp"
#include "../services/PresetLibrary.h"
#include "IconButton.h"
#include "Modal.h"

class PresetBar : public juce::Component
{
public:
    PresetBar();

    std::function<grmg::models::Preset(const std::string &name)> capture; // this guitar's sound, named
    std::function<void(const grmg::models::Preset &)> apply;              // given to this guitar
    std::function<juce::String()> currentName;                            // the preset this guitar was last given
    std::function<void(std::unique_ptr<Modal>)> showDialog;               // shown by the owner, over the window

    // the menu again (another instance may have saved one), on this guitar's preset
    void refresh();

    // what the menu and the dialogs do
    void choose(int index); // the index-th preset of the menu
    bool save(const juce::String &name);
    bool remove(const grmg::models::Preset &preset);

    const std::vector<grmg::models::Preset> &getShown() const { return shown; }
    const juce::ComboBox &getMenu() const { return menu; }
    PresetLibrary &getLibrary() { return *library; }

    void resized() override;
    void mouseEnter(const juce::MouseEvent &) override;

private:
    juce::SharedResourcePointer<PresetLibrary> library;
    juce::ComboBox menu;
    IconButton saveButton{ui::IconId::floppyDisk, "Save preset: this guitar's sound (every setting but its patterns and its mixer strip), for every project"};
    IconButton deleteButton{ui::IconId::trash, "Delete this preset (only one of your own)"};
    std::vector<grmg::models::Preset> shown; // the menu's items: item id = index + 1

    const grmg::models::Preset *selected() const;
    void openSave();
    void openDelete();
    void openFailure(const juce::String &what);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetBar)
};
