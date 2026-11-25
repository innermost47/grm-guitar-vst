// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
#include "IconLibrary.h"
#include "Assets.h"

using ui::IconId;

const IconLibrary &IconLibrary::getInstance()
{
    static const IconLibrary instance;
    return instance;
}

IconLibrary::IconLibrary()
{
    for (const auto &entry : ui::ICON_FILES)
        icons[static_cast<std::size_t>(entry.id)] = load(entry.file);
}

IconLibrary::Icon IconLibrary::load(const char *file)
{
    const auto svg = Assets::svg(file);
    if (svg == nullptr)
        return {};
    // the outline is taken in the canvas' own units: the SVG is made exactly as large as its viewBox
    Icon icon;
    icon.canvas = readCanvas(*svg);
    svg->setAttribute("width", icon.canvas.getWidth());
    svg->setAttribute("height", icon.canvas.getHeight());
    if (const auto drawable = juce::Drawable::createFromSVG(*svg))
        icon.outline = drawable->getOutlineAsPath();
    return icon;
}

juce::Rectangle<float> IconLibrary::readCanvas(const juce::XmlElement &svg)
{
    const auto viewBox = juce::StringArray::fromTokens(svg.getStringAttribute("viewBox"), " ,", "");
    if (viewBox.size() == 4)
        return {0.0f, 0.0f, viewBox[2].getFloatValue(), viewBox[3].getFloatValue()};
    return {0.0f, 0.0f, (float)svg.getDoubleAttribute("width"), (float)svg.getDoubleAttribute("height")};
}

const IconLibrary::Icon &IconLibrary::get(IconId id) const
{
    return icons[static_cast<std::size_t>(id)];
}

bool IconLibrary::isLoaded(IconId id) const
{
    const auto &icon = get(id);
    return !icon.outline.isEmpty() && !icon.canvas.isEmpty();
}

void IconLibrary::draw(juce::Graphics &g, IconId id, juce::Rectangle<float> area, juce::Colour colour) const
{
    const auto &icon = get(id);
    if (icon.outline.isEmpty() || icon.canvas.isEmpty() || area.isEmpty())
        return;

    const auto fit = juce::RectanglePlacement(juce::RectanglePlacement::centred).getTransformToFit(icon.canvas, area);
    g.setColour(colour);
    g.fillPath(icon.outline, fit);
}
