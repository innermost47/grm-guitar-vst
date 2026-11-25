// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
#include "PresetLibrary.h"

#include <algorithm>
#include <iterator>

#include "../style/Assets.h"

using grmg::models::Preset;

PresetLibrary::PresetLibrary() : PresetLibrary(defaultFolder()) {}

PresetLibrary::PresetLibrary(const juce::File &f) : folder(f)
{
    for (const auto &xml : Assets::xmlFiles(ui::PRESET_EXTENSION))
        if (auto preset = fromXml(*xml))
        {
            preset->factory = true;
            factory.push_back(std::move(*preset));
        }
}

juce::File PresetLibrary::defaultFolder()
{
    // next to the plugin's settings (UpdateChecker's): AppData/Roaming, ~/Library/Application Support, ~/.config
    auto base = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
#if JUCE_MAC
    base = base.getChildFile("Application Support");
#endif
    return base.getChildFile(ui::SETTINGS_NAME).getChildFile(ui::PRESET_FOLDER);
}

std::unique_ptr<juce::XmlElement> PresetLibrary::toXml(const Preset &preset)
{
    auto xml = std::make_unique<juce::XmlElement>(ui::PRESET_TAG);
    xml->setAttribute("name", juce::String(preset.name));
    xml->setAttribute("category", juce::String(preset.category));
    xml->setAttribute("version", JucePlugin_VersionString);
    for (const auto &[id, value] : preset.values)
    {
        auto *param = xml->createNewChildElement("PARAM");
        param->setAttribute("id", juce::String(id));
        param->setAttribute("value", value);
    }
    return xml;
}

std::optional<Preset> PresetLibrary::fromXml(const juce::XmlElement &xml)
{
    if (!xml.hasTagName(ui::PRESET_TAG) || xml.getStringAttribute("name").trim().isEmpty())
        return std::nullopt;
    Preset preset;
    preset.name = xml.getStringAttribute("name").trim().toStdString();
    preset.category = xml.getStringAttribute("category", ui::PRESET_USER_CATEGORY).toStdString();
    for (const auto *param : xml.getChildWithTagNameIterator("PARAM"))
        if (param->hasAttribute("id") && param->hasAttribute("value"))
            preset.values[param->getStringAttribute("id").toStdString()] = (float)param->getDoubleAttribute("value");
    return preset;
}

juce::File PresetLibrary::fileFor(const std::string &name) const
{
    return folder.getChildFile(juce::File::createLegalFileName(juce::String(name)) + ui::PRESET_EXTENSION);
}

std::vector<std::pair<juce::File, Preset>> PresetLibrary::userFiles() const
{
    std::vector<std::pair<juce::File, Preset>> found;
    for (const auto &file : folder.findChildFiles(juce::File::findFiles, false, juce::String("*") + ui::PRESET_EXTENSION))
        if (const auto xml = juce::parseXML(file))
            if (auto preset = fromXml(*xml))
            {
                preset->category = ui::PRESET_USER_CATEGORY;
                found.emplace_back(file, std::move(*preset));
            }
    return found;
}

std::vector<Preset> PresetLibrary::user() const
{
    std::vector<Preset> found;
    for (auto &entry : userFiles())
        found.push_back(std::move(entry.second));
    return found;
}

std::vector<Preset> PresetLibrary::list() const
{
    auto all = factory;
    auto mine = user();
    all.insert(all.end(), std::make_move_iterator(mine.begin()), std::make_move_iterator(mine.end()));
    auto rank = [](const Preset &p)
    {
        const auto *begin = std::begin(ui::PRESET_CATEGORIES), *end = std::end(ui::PRESET_CATEGORIES);
        return (int)(std::find_if(begin, end, [&](const char *c)
                                  { return p.category == c; }) -
                     begin);
    };
    std::stable_sort(all.begin(), all.end(), [&](const Preset &a, const Preset &b)
                     { return rank(a) != rank(b) ? rank(a) < rank(b)
                                                 : juce::String(a.name).compareNatural(juce::String(b.name)) < 0; });
    return all;
}

std::optional<Preset> PresetLibrary::find(const std::string &name, bool fromFactory) const
{
    for (auto &preset : fromFactory ? factory : user())
        if (preset.name == name)
            return preset;
    return std::nullopt;
}

bool PresetLibrary::userHas(const std::string &name) const
{
    return find(name, false).has_value();
}

bool PresetLibrary::save(Preset preset)
{
    preset.category = ui::PRESET_USER_CATEGORY;
    preset.factory = false;
    if (preset.name.empty() || !folder.createDirectory().wasOk())
        return false;
    // another file of the same preset name (written by hand) goes: one name, one preset
    const auto target = fileFor(preset.name);
    for (const auto &[file, other] : userFiles())
        if (other.name == preset.name && file != target)
            file.deleteFile();
    return toXml(preset)->writeTo(target);
}

bool PresetLibrary::remove(const std::string &name)
{
    bool removed = false;
    for (const auto &[file, preset] : userFiles())
        if (preset.name == name)
            removed |= file.deleteFile();
    return removed;
}
