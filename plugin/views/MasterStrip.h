// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// MasterStrip.h
//
// The mixer's master strip, beside the guitars' (it stays when they scroll): the plugin's volume on a graduated
// fader, the main output's meter.
#pragma once

#include "../PluginProcessor.h"
#include "../../constants/ui.hpp"
#include "VuMeter.h"

class MasterStrip : public juce::Component
{
public:
    explicit MasterStrip(GRMGuitarProcessor &p);
    void paint(juce::Graphics &) override;
    void resized() override;

private:
    GRMGuitarProcessor &processor;
    const ui::theme::AppTheme appTheme = ui::theme::AppTheme::fromScale(1.0f);
    juce::Label name;
    juce::Slider volume{juce::Slider::LinearVertical, juce::Slider::TextBoxBelow};
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> volumeAttachment;
    VuMeter meter;
    std::unique_ptr<juce::VBlankAttachment> vblank;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MasterStrip)
};
