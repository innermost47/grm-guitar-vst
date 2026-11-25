// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
#pragma once

#include "../PluginProcessor.h"
#include "../../constants/ui.hpp"

class ParamPanel : public juce::Component
{
public:
    ParamPanel(juce::AudioProcessorValueTreeState &state, const std::vector<juce::String> &ids);
    void resized() override;
    // the row `id` shows only while the parameter `switchId` (a choice or an index) takes one of `values`
    void showOnlyWhen(const juce::String &id, const juce::String &switchId, std::initializer_list<int> values);
    // a component under the rows (what the parameters above it give), `height` high
    void setFooter(std::unique_ptr<juce::Component> component, int height);

private:
    struct Row
    {
        juce::String id;
        const std::atomic<float> *switchValue = nullptr; // the parameter it depends on (none: always shown)
        int switchMask = 0;                              // the values of it that show the row
        std::unique_ptr<juce::Label> label;
        std::unique_ptr<juce::Component> control;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> slider;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> button;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> combo;
    };

    void updateVisibility();
    void layoutContent();

    juce::AudioProcessorValueTreeState &state;
    std::vector<Row> rows;
    juce::Label warningLabel;
    std::unique_ptr<juce::Component> footer;
    int footerHeight = 0;

    juce::Viewport viewport;
    juce::Component content;

    std::unique_ptr<juce::VBlankAttachment> vblank;
};