// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
#include "MasterStrip.h"

#include "CustomLookAndFeel.h"

using namespace ui;

MasterStrip::MasterStrip(GRMGuitarProcessor &p) : processor(p)
{
    name.setText("MASTER", juce::dontSendNotification);
    name.setJustificationType(juce::Justification::centred);
    name.setFont(font(appTheme.typography.small, true));
    name.setColour(juce::Label::backgroundColourId, MASTER_PLATE);
    name.setColour(juce::Label::textColourId, BRIGHT);
    addAndMakeVisible(name);
    volume.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 54, 16);
    volume.setTooltip("Volume of the plugin's main output");
    volume.getProperties().set(CustomLookAndFeel::getDrawTicksPropertyId(), FADER_TICKS);
    volumeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.params, "volume", volume);
    addAndMakeVisible(volume);
    addAndMakeVisible(meter);
    vblank = std::make_unique<juce::VBlankAttachment>(this, [this]
                                                      {
                                                          const auto [l, r] = processor.takeMasterPeaks();
                                                          meter.setPeaks(l, r); });
}

void MasterStrip::paint(juce::Graphics &g)
{
    g.fillAll(PANEL);
    g.setColour(GRID);
    g.drawVerticalLine(0, (float)appTheme.spacing.lg, (float)(getHeight() - appTheme.spacing.lg));
}

void MasterStrip::resized()
{
    auto a = getLocalBounds().reduced(appTheme.spacing.md);
    name.setBounds(a.removeFromTop(appTheme.dimensions.modalRowHeight));
    a.removeFromTop(appTheme.spacing.md);
    meter.setBounds(a.removeFromRight(appTheme.dimensions.vuMeterWidth));
    a.removeFromRight(appTheme.spacing.base);
    volume.setBounds(a);
}
