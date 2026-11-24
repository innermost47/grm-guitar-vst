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
    }};

    inline const juce::Colour ACCENT{juce::Colour(theme::defaultPalette.gold)};
    inline const juce::Colour BG{juce::Colour(theme::defaultPalette.obsidian)};
    inline const juce::Colour BRIGHT{juce::Colour(theme::defaultPalette.warmWhite)};
    inline const juce::Colour GRID{juce::Colour(theme::defaultPalette.charcoalLight)};
    inline const juce::Colour PANEL{juce::Colour(theme::defaultPalette.charcoalDark)};
    inline const juce::Colour TEXT{juce::Colour(theme::defaultPalette.mediumGrey)};
    inline const juce::Colour WAVE{juce::Colour(theme::defaultPalette.cyan)};

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

    inline juce::Colour soften(juce::Colour colour)
    {
        return colour.interpolatedWith(juce::Colours::black, 0.25f);
    }
}