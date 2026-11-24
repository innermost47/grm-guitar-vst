// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// Every icon of the interface, read once from the embedded SVG files and kept as a plain outline: drawing one
// is filling a path, in any colour, at any size (no SVG parsing nor component per paint).
#pragma once

#include <array>
#include "../../constants/ui.hpp"

class IconLibrary
{
public:
    static const IconLibrary &getInstance();

    // fits the icon's whole SVG canvas (not its tight bounds) in area, so that all icons share one optical size
    void draw(juce::Graphics &g, ui::IconId id, juce::Rectangle<float> area, juce::Colour colour) const;

    // false when the icon's file is missing from the binary data or holds no shape
    bool isLoaded(ui::IconId id) const;

private:
    IconLibrary();

    struct Icon
    {
        juce::Path outline;
        juce::Rectangle<float> canvas;
    };

    static Icon load(const char *file);
    static juce::Rectangle<float> readCanvas(const juce::XmlElement &svg);
    const Icon &get(ui::IconId id) const;

    std::array<Icon, ui::ICON_COUNT> icons;
};
