// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// The presets: the factory's (embedded, acoustic and electric) and the user's (one file each in the plugin's
// folder of the user's application data, so that every project, in every host, sees them). Shared by every instance
// of the plugin (juce::SharedResourcePointer): a preset saved in one is in the menu of the others.
#pragma once

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include <JuceHeader.h>
#include "../../constants/ui.hpp"
#include "../../models/preset.hpp"

class PresetLibrary
{
public:
    PresetLibrary(); // the user's folder
    explicit PresetLibrary(const juce::File &folder);

    // every preset, by category (PRESET_CATEGORIES' order), then by name; the user's are read again
    std::vector<grmg::models::Preset> list() const;
    std::optional<grmg::models::Preset> find(const std::string &name, bool factory) const;
    bool userHas(const std::string &name) const;

    // a user preset (it replaces the one of the same name); false when it could not be written
    bool save(grmg::models::Preset preset);
    bool remove(const std::string &name);

    const juce::File &getFolder() const { return folder; }
    static juce::File defaultFolder();

    // the preset file's XML
    static std::unique_ptr<juce::XmlElement> toXml(const grmg::models::Preset &preset);
    static std::optional<grmg::models::Preset> fromXml(const juce::XmlElement &xml);

private:
    juce::File folder;
    std::vector<grmg::models::Preset> factory;

    juce::File fileFor(const std::string &name) const;
    std::vector<std::pair<juce::File, grmg::models::Preset>> userFiles() const; // with the file each is in
    std::vector<grmg::models::Preset> user() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetLibrary)
};
