// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// TransportBar.h
//
// The transport, in the middle of the top line: play / stop (standalone: a host plays its own), where the roll is
// (bar.beat, and minutes:seconds), the tempo (standalone), the pattern's signature and how its beats are grouped,
// the loop, the roll following the playhead, the full screen (standalone).
#pragma once

#include "../PluginProcessor.h"
#include "../../constants/ui.hpp"
#include "IconButton.h"
#include "RollView.h"

class TransportBar : public juce::Component
{
public:
    TransportBar(GRMGuitarProcessor &p, RollView &roll);
    void paint(juce::Graphics &) override;
    void resized() override;
    int preferredWidth() const;

private:
    GRMGuitarProcessor &processor;
    RollView &roll;
    const ui::theme::AppTheme appTheme = ui::theme::AppTheme::fromScale(1.0f);
    using Icon = ui::IconId;
    using Behaviour = IconButton::Behaviour;

    IconButton play{Icon::play, "Play / stop the roll (Space)", Behaviour::momentary, Icon::stop};
    IconButton loop{Icon::loop, "Loop the roll", Behaviour::toggle};
    IconButton follow{Icon::crosshair, "Follow playhead: the roll scrolls with the playhead. Off: it stays where you are editing", Behaviour::toggle};
    IconButton fullScreen{Icon::cornersOut, "Full screen"};
    juce::Label where, clock;
    juce::Slider tempo{juce::Slider::LinearBar, juce::Slider::TextBoxLeft};
    juce::ComboBox signature, grouping;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> loopAttachment, followAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> tempoAttachment;
    std::vector<std::vector<int>> groupings; // the grouping menu's entries, for the signature shown
    int shownBeats = -1;
    std::vector<int> shownGrouping;
    std::unique_ptr<juce::VBlankAttachment> vblank;

    static constexpr int WHERE_W = 150, CLOCK_W = 96, TEMPO_W = 96, SIGNATURE_W = 64, GROUPING_W = 72;
    bool standalone() const { return processor.wrapperType == juce::AudioProcessor::wrapperType_Standalone; }
    void update();
    void refreshSignature();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TransportBar)
};
