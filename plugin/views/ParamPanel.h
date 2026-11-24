// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
#include "../PluginProcessor.h"

class ParamPanel : public juce::Component
{
public:
    ParamPanel(juce::AudioProcessorValueTreeState &state, const std::vector<juce::String> &ids);
    void resized() override;
    void setSensorId(const juce::String &sensorParameterId);
    void showOnlyForSensor(const juce::String &id, std::initializer_list<int> sensors);

private:
    struct Row
    {
        juce::String id;
        int sensorMask = 0;
        std::unique_ptr<juce::Label> label;
        std::unique_ptr<juce::Component> control;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> slider;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> button;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> combo;
    };

    void updateForSensor();
    void layoutContent();

    juce::AudioProcessorValueTreeState &state;
    std::vector<Row> rows;
    juce::String sensorId;
    juce::Label warningLabel;
    int lastSensor = -1;

    juce::Viewport viewport;
    juce::Component content;

    std::unique_ptr<juce::VBlankAttachment> vblank;
};