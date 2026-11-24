// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
#pragma once

#include "../PluginProcessor.h"
#include "../../constants/ui.hpp"

class NeckView : public juce::Component
{
public:
    explicit NeckView(GRMGuitarProcessor &p) : processor(p)
    {
        vblank = std::make_unique<juce::VBlankAttachment>(this, [this]
                                                          {
				phase += 0.25f;
				repaint(); });
    }
    void paint(juce::Graphics &) override;
    void mouseDown(const juce::MouseEvent &) override;

private:
    GRMGuitarProcessor &processor;
    double phase = 0.0;
    float fretX(int k) const;
    std::unique_ptr<juce::VBlankAttachment> vblank;
};