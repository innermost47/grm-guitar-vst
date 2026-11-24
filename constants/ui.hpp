// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
#pragma once
#include <JuceHeader.h>
#include "theme.hpp"
#include "fonts.hpp"
#include "sizes.hpp"

namespace ui
{
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