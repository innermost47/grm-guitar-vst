// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
#include "IconButton.h"

IconButton::IconButton(ui::IconId iconToShow, const juce::String &tooltip, Behaviour behaviour,
                       std::optional<ui::IconId> iconToShowWhenOn)
    : juce::Button(tooltip), icon(iconToShow), iconWhenOn(iconToShowWhenOn)
{
    jassert(tooltip.isNotEmpty());
    setTooltip(tooltip);
    setClickingTogglesState(behaviour == Behaviour::toggle);
}

ui::IconId IconButton::getShownIcon() const
{
    return getToggleState() && iconWhenOn.has_value() ? *iconWhenOn : icon;
}

void IconButton::paintButton(juce::Graphics &g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    if (auto *methods = dynamic_cast<LookAndFeelMethods *>(&getLookAndFeel()))
        methods->drawIconButton(g, *this, shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);
}
