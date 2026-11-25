// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// TensionView.h
//
// The strings as their networks solved them: each open string's note and the tension it takes to sound it (the
// tuning network). A string tuned so low that it goes slack (under STRING_SLACK_N) is shown as such, with what it
// does: floppy under the pick, buzzing on the frets.
#pragma once

#include <memory>

#include "../PluginProcessor.h"
#include "../../constants/sizes.hpp"
#include "../../constants/ui.hpp"

class TensionView : public juce::Component
{
public:
    explicit TensionView(GRMGuitarProcessor &processor);
    void paint(juce::Graphics &g) override;

private:
    void refresh();

    GRMGuitarProcessor &processor;
    const grmg::Model *shown = nullptr;
    std::array<int, grmg::NUM_STRINGS> notes{};
    std::array<double, grmg::NUM_STRINGS> tensions{};
    std::unique_ptr<juce::VBlankAttachment> vblank;
    const ui::theme::AppTheme appTheme = ui::theme::AppTheme::fromScale(1.0f);
};
