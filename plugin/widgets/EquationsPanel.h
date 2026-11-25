// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// The left column of every dialog: discreet, the chain of seven relations through which a GRM network of known
// physics brought up S * E_quant = E_stock (the entropy of the horizon times its quantum of energy equals the
// energy it holds), and under it the link to grm-core, where that story is told.
#pragma once

#include <memory>
#include "../../constants/ui.hpp"

class EquationsPanel : public juce::Component
{
public:
    EquationsPanel();

    void paint(juce::Graphics &g) override;
    void resized() override;

    // false when the equations are missing from the binary data
    bool hasEquations() const { return equations != nullptr; }
    const juce::HyperlinkButton &getLink() const { return link; }

private:
    std::unique_ptr<juce::Drawable> equations;
    juce::HyperlinkButton link;
    const ui::theme::AppTheme appTheme = ui::theme::AppTheme::fromScale(1.0f);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EquationsPanel)
};
