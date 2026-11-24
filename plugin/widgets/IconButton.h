// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// A square button showing an icon instead of a text. Its tooltip is required: it is the only place where the
// button says what it does. A toggle can show another icon while it is on (play / stop).
#pragma once

#include <optional>
#include "../../constants/ui.hpp"

class IconButton : public juce::Button
{
public:
    enum class Behaviour
    {
        momentary, // a click runs onClick
        toggle     // a click flips the state (or an attachment does)
    };

    // drawn by the look and feel, like every other widget
    struct LookAndFeelMethods
    {
        virtual ~LookAndFeelMethods() = default;
        virtual void drawIconButton(juce::Graphics &g, IconButton &button, bool shouldDrawButtonAsHighlighted,
                                    bool shouldDrawButtonAsDown) = 0;
    };

    IconButton(ui::IconId icon, const juce::String &tooltip, Behaviour behaviour = Behaviour::momentary,
               std::optional<ui::IconId> iconWhenOn = std::nullopt);

    // the icon for the current state
    ui::IconId getShownIcon() const;

protected:
    void paintButton(juce::Graphics &g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

private:
    ui::IconId icon;
    std::optional<ui::IconId> iconWhenOn;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(IconButton)
};
