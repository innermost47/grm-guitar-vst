// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
#pragma once
#include <array>
#include <JuceHeader.h>
#include "theme.hpp"
#include "fonts.hpp"
#include "sizes.hpp"
#include "../models/ui.hpp"

namespace ui
{
    // Phosphor icons (fill style, MIT license) and the glyphs drawn for GRM Guitar (strum, arpeggio)
    inline constexpr std::array<IconFile, ICON_COUNT> ICON_FILES = {{
        {IconId::play, "play.svg"},
        {IconId::stop, "stop.svg"},
        {IconId::arrowsClockwise, "arrows-clockwise.svg"},
        {IconId::exportFile, "export.svg"},
        {IconId::strum, "strum.svg"},
        {IconId::arpeggio, "arpeggio.svg"},
        {IconId::eraser, "eraser.svg"},
        {IconId::arrowCounterClockwise, "arrow-counter-clockwise.svg"},
        {IconId::arrowClockwise, "arrow-clockwise.svg"},
        {IconId::musicNotes, "music-notes.svg"},
        {IconId::filePlus, "file-plus.svg"},
        {IconId::copySimple, "copy-simple.svg"},
        {IconId::copy, "copy.svg"},
        {IconId::clipboard, "clipboard.svg"},
        {IconId::trash, "trash.svg"},
        {IconId::plus, "plus.svg"},
        {IconId::minus, "minus.svg"},
        {IconId::ghost, "ghost.svg"},
        {IconId::crosshair, "crosshair.svg"},
        {IconId::speakerSlash, "speaker-slash.svg"},
        {IconId::headphones, "headphones.svg"},
        {IconId::caretUp, "caret-up.svg"},
        {IconId::caretDown, "caret-down.svg"},
        {IconId::caretLeft, "caret-left.svg"},
        {IconId::caretRight, "caret-right.svg"},
        {IconId::floppyDisk, "floppy-disk.svg"},
    }};

    // ---- the plugin on the net: its releases, the GRM it shows off
    inline constexpr const char *RELEASES_API = "https://api.github.com/repos/innermost47/grm-guitar-vst/releases/latest";
    inline constexpr const char *RELEASES_PAGE = "https://github.com/innermost47/grm-guitar-vst/releases/latest";
    inline constexpr const char *GRM_CORE_PAGE = "https://github.com/innermost47/grm-core";
    inline constexpr const char *HTTP_USER_AGENT = "GRM-Guitar";
    inline constexpr int UPDATE_TIMEOUT_MS = 5000;
    // the plugin's own settings, shared by every project (a file in the user's application data)
    inline constexpr const char *SETTINGS_NAME = "GRM Guitar";
    inline constexpr const char *SKIPPED_VERSION_KEY = "skipped_version";

    // ---- presets: a guitar's sound, kept for every project (the user's in the plugin's folder, the factory's embedded)
    inline constexpr const char *PRESET_EXTENSION = ".grmpreset";
    inline constexpr const char *PRESET_FOLDER = "Presets";
    inline constexpr const char *PRESET_TAG = "GRMGuitarPreset";
    inline constexpr const char *PRESET_USER_CATEGORY = "User";
    inline constexpr const char *PRESET_CATEGORIES[] = {"Acoustic", "Electric", PRESET_USER_CATEGORY}; // the menu's order
    inline constexpr int PRESET_NAME_MAX = 48;

    // ---- the dialogs: the equations a GRM network brought up, discreet on their left
    inline constexpr const char *EQUATIONS_FILE = "grm-equations.svg";
    inline constexpr float EQUATIONS_ALPHA = 0.3f;
    inline constexpr float MODAL_LINK_ALPHA = 0.55f;
    inline constexpr float MODAL_DIM_ALPHA = 0.6f; // the darkening of the window behind a dialog

    inline const juce::Colour ACCENT{juce::Colour(theme::defaultPalette.gold)};
    inline const juce::Colour BG{juce::Colour(theme::defaultPalette.obsidian)};
    inline const juce::Colour BRIGHT{juce::Colour(theme::defaultPalette.warmWhite)};
    inline const juce::Colour GRID{juce::Colour(theme::defaultPalette.charcoalLight)};
    inline const juce::Colour PANEL{juce::Colour(theme::defaultPalette.charcoalDark)};
    inline const juce::Colour TEXT{juce::Colour(theme::defaultPalette.mediumGrey)};
    inline const juce::Colour WAVE{juce::Colour(theme::defaultPalette.cyan)};
    inline const juce::Colour WARNING{juce::Colour(theme::defaultPalette.terracottaLight)}; // something to look at

    inline const juce::Colour STRING_COLOURS[6] = {
        juce::Colour(theme::defaultPalette.gold),
        juce::Colour(theme::defaultPalette.terracottaLight),
        juce::Colour(theme::defaultPalette.terracotta),
        juce::Colour(theme::defaultPalette.sageGreen),
        juce::Colour(theme::defaultPalette.slateBlue),
        juce::Colour(theme::defaultPalette.mauve)};

    inline const juce::Colour GUITAR_COLOURS[8] = {
        juce::Colour(theme::defaultPalette.cyan),
        juce::Colour(theme::defaultPalette.rose),
        juce::Colour(theme::defaultPalette.lime),
        juce::Colour(theme::defaultPalette.amber),
        juce::Colour(theme::defaultPalette.mauve),
        juce::Colour(theme::defaultPalette.teal),
        juce::Colour(theme::defaultPalette.amberWarm),
        juce::Colour(theme::defaultPalette.warmWhite)};

    inline juce::Font font(float size, bool bold = false)
    {
        return juce::Font(juce::FontOptions(size, bold ? juce::Font::bold : juce::Font::plain));
    }

    // the names of a table of the engine (tunings, woods, rooms...), as the choices of a parameter
    template <class Table>
    juce::StringArray namesOf(const Table &table)
    {
        juce::StringArray names;
        for (const auto &entry : table)
            names.add(entry.name);
        return names;
    }

    inline juce::Colour soften(juce::Colour colour)
    {
        return colour.interpolatedWith(juce::Colours::black, 0.25f);
    }
}