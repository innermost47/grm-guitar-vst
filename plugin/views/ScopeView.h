// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
#pragma once

#include "../PluginProcessor.h"
#include "../../constants/ui.hpp"

class ScopeView : public juce::Component
{
public:
    explicit ScopeView(GRMGuitarProcessor &p) : processor(p)
    {
        history.assign(4096, 0.0f);
        vblank = std::make_unique<juce::VBlankAttachment>(this, [this]
                                                          { updateScope(); });
    }
    void paint(juce::Graphics &) override;

private:
    GRMGuitarProcessor &processor;
    std::vector<float> history;
    size_t write = 0;
    void updateScope();
    std::unique_ptr<juce::VBlankAttachment> vblank;
};