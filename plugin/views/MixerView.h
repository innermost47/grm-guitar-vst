// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
#include "../PluginProcessor.h"
#include "IconButton.h"

class MixerView : public juce::Component
{
public:
    explicit MixerView(GRMGuitarProcessor &p);
    void resized() override;
    void paint(juce::Graphics &) override;

private:
    GRMGuitarProcessor &processor;
    const ui::theme::AppTheme appTheme = ui::theme::AppTheme::fromScale(1.0f);
    struct Strip
    {
        std::unique_ptr<juce::TextButton> name;
        std::unique_ptr<juce::Slider> gain, level, pan;
        std::unique_ptr<IconButton> mute, solo;
        std::unique_ptr<juce::ComboBox> out;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gainA, levelA, panA;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> muteA, soloA;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> outA;
    };
    std::vector<Strip> strips;
    int shownCount = -1, shownSelected = -1;
    void rebuild();
    void updateMixer();
    std::unique_ptr<juce::VBlankAttachment> vblank;
};