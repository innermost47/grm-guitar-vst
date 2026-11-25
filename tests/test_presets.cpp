// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// The presets: the factory's are embedded, sorted, and name only real settings of a guitar within their ranges; one
// of the user's is written, found again (by another library: another instance, another project), replaced, deleted;
// a guitar's sound goes into a preset and comes back from it, its mixer strip stays, a setting the preset does not
// name goes to its default; the guitar's preset is in the session; the menu shows them and applies the one chosen.
#include <cstdio>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../plugin/PluginProcessor.h"
#include "../plugin/services/PresetLibrary.h"
#include "../plugin/widgets/PresetBar.h"

static int failures = 0, checks = 0;

static void check(bool ok, const char *what)
{
    ++checks;
    if (!ok)
    {
        ++failures;
        std::printf("FAIL  %s\n", what);
    }
}

static float plain(GRMGuitarProcessor &p, const char *id)
{
    auto *param = p.params.getParameter(id);
    return param->convertFrom0to1(param->getValue());
}

static void set(GRMGuitarProcessor &p, const char *id, float value)
{
    auto *param = p.params.getParameter(id);
    param->setValueNotifyingHost(param->convertTo0to1(value));
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    const auto folder = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("grm-guitar-presets-test");
    folder.deleteRecursively();

    // ---- the factory's
    PresetLibrary library(folder);
    const auto all = library.list();
    int acoustic = 0, electric = 0;
    for (const auto &p : all)
    {
        acoustic += p.factory && p.category == "Acoustic";
        electric += p.factory && p.category == "Electric";
    }
    check(acoustic >= 5 && electric >= 5, "the factory has acoustic and electric presets");
    check(!all.empty() && all.front().category == "Acoustic" && all.back().category == "Electric", "the menu goes by category");

    GRMGuitarProcessor processor;
    bool real = true, inRange = true, noMixer = true;
    for (const auto &p : all)
        for (const auto &[id, value] : p.values)
        {
            auto *param = processor.params.getParameter(id);
            real &= param != nullptr;
            noMixer &= !grmg::is_mixer_param(id);
            if (param != nullptr)
            {
                const auto range = param->getNormalisableRange();
                inRange &= value >= range.start - 1e-4f && value <= range.end + 1e-4f;
            }
            if (param == nullptr)
                std::printf("      unknown setting %s in %s\n", id.c_str(), p.name.c_str());
        }
    check(real, "the factory presets name only real settings");
    check(inRange, "the factory presets stay within the settings' ranges");
    check(noMixer, "the factory presets leave the mixer strip");

    // ---- a guitar's sound into a preset and back; its mixer strip stays
    set(processor, "force", 4.0f);
    set(processor, "room_distance", 3.0f);
    set(processor, "mix_gain", 6.0f);
    const auto mine = processor.capturePreset(0, "Mine");
    check(mine.values.count("force") && !mine.values.count("mix_gain"), "a preset holds the sound, not the mixer strip");
    check(library.save(mine), "a preset of one's own is written");
    PresetLibrary other(folder); // another instance, another project
    const auto found = other.find("Mine", false);
    check(found && std::abs(found->values.at("force") - 4.0f) < 1e-4f && found->category == ui::PRESET_USER_CATEGORY,
          "another library finds it, as it was");
    check(!other.list().empty() && other.list().back().name == "Mine", "the user's come after the factory's");

    set(processor, "force", 1.0f);
    set(processor, "mix_gain", -3.0f);
    processor.applyPreset(0, *found);
    check(std::abs(plain(processor, "force") - 4.0f) < 1e-3f && std::abs(plain(processor, "room_distance") - 3.0f) < 1e-3f,
          "the preset gives the guitar its sound back");
    check(std::abs(plain(processor, "mix_gain") + 3.0f) < 1e-3f, "the mixer strip stays as it is");
    check(processor.getPresetName(0) == "Mine", "the guitar knows its preset");

    const auto piezo = library.find("Piezo on stage", true);
    check(piezo.has_value(), "a factory preset is found by its name");
    if (piezo)
    {
        processor.applyPreset(0, *piezo);
        auto *force = processor.params.getParameter("force");
        check((int)plain(processor, "sensor") == 2 && plain(processor, "room_on") < 0.5f, "a factory preset sets what it names");
        check(std::abs(force->getValue() - force->getDefaultValue()) < 1e-6f, "a setting it does not name is at its default");
    }

    // ---- replaced, then deleted
    auto louder = mine;
    louder.values["force"] = 5.0f;
    check(library.save(louder) && library.list().size() == all.size() + 1, "saving the same name replaces it");
    check(std::abs(library.find("Mine", false)->values.at("force") - 5.0f) < 1e-4f, "with its new sound");
    check(library.remove("Mine") && !library.userHas("Mine") && library.list().size() == all.size(), "a preset of one's own is deleted");
    check(!library.remove("Piezo on stage"), "a factory preset cannot be deleted");

    // ---- the guitar's preset is in the session
    {
        juce::MemoryBlock state;
        processor.getStateInformation(state);
        GRMGuitarProcessor restored;
        restored.setStateInformation(state.getData(), (int)state.getSize());
        check(restored.getPresetName(0) == "Piezo on stage", "the guitar's preset comes back with the session");
    }

    // ---- the menu
    {
        PresetBar bar;
        juce::String applied;
        bar.apply = [&](const grmg::models::Preset &p)
        {
            applied = juce::String(p.name);
            processor.applyPreset(0, p);
        };
        bar.currentName = [&]
        { return processor.getPresetName(0); };
        bar.refresh();
        const auto &shown = bar.getShown();
        int crunch = -1;
        for (size_t k = 0; k < shown.size(); ++k)
            if (shown[k].name == "British crunch")
                crunch = (int)k;
        check(crunch >= 0, "the menu shows the factory presets");
        bar.choose(crunch);
        check(applied == "British crunch" && bar.getMenu().getText() == "British crunch" && (int)plain(processor, "amp_lead_model") == 1,
              "choosing one in the menu gives it to the guitar");
    }

    folder.deleteRecursively();
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
