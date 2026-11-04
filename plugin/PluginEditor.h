// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// The interface: the neck (click a string to play it), the oscilloscope, the settings of every network,
// and the guitar roll with its tools.
#pragma once

#include <memory>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"
#include "RollView.h"

// the neck seen from the player: string 1 on top, nut on the left; a click plays that string at that fret
class NeckView : public juce::Component, private juce::Timer {
public:
    explicit NeckView(GRMGuitarProcessor& p) : processor(p) { startTimerHz(30); }
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;

private:
    GRMGuitarProcessor& processor;
    double phase = 0.0;
    float fretX(int k) const;
    void timerCallback() override {
        phase += 0.9;
        repaint();
    }
};

class ScopeView : public juce::Component, private juce::Timer {
public:
    explicit ScopeView(GRMGuitarProcessor& p) : processor(p) {
        history.assign(4096, 0.0f);
        startTimerHz(30);
    }
    void paint(juce::Graphics&) override;

private:
    GRMGuitarProcessor& processor;
    std::vector<float> history;
    size_t write = 0;
    void timerCallback() override;
};

// the parameters of a group of networks, as rows of sliders, switches and lists
class ParamPanel : public juce::Component {
public:
    ParamPanel(juce::AudioProcessorValueTreeState& state, const std::vector<const char*>& ids);
    void resized() override;

private:
    struct Row {
        std::unique_ptr<juce::Label> label;
        std::unique_ptr<juce::Component> control;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> slider;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> button;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> combo;
    };
    std::vector<Row> rows;
};

// what the networks found, as numbers (read only)
class NetworksView : public juce::Component, private juce::Timer {
public:
    explicit NetworksView(GRMGuitarProcessor& p);
    void resized() override { text.setBounds(getLocalBounds().reduced(4)); }

private:
    GRMGuitarProcessor& processor;
    juce::TextEditor text;
    const void* shown = nullptr;
    void timerCallback() override;
};

class GRMGuitarEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit GRMGuitarEditor(GRMGuitarProcessor&);
    ~GRMGuitarEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    GRMGuitarProcessor& processor;
    juce::LookAndFeel_V4 look;
    NeckView neck;
    ScopeView scope;
    juce::TabbedComponent tabs {juce::TabbedButtonBar::TabsAtTop};
    RollView roll;

    // header
    juce::TextButton playButton {"Play"}, takeButton {"New take"}, exportButton {"Export WAV"};
    std::unique_ptr<juce::FileChooser> chooser;
    juce::Label status, position;
    juce::Slider volume;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> volumeAttachment;

    // tools of the roll
    juce::ComboBox chordBox, strumBox, arpBox, snapBox, barsBox, fillCount;
    juce::TextButton fillStrums {"Fill strums"}, fillArp {"Fill arpeggio"}, clearBar {"Clear bar"}, undoButton {"Undo"},
        redoButton {"Redo"}, demoButton {"Demo song"};
    juce::Label chordLabel, barLabel, snapLabel, songLabel, help;
    int subtitleWidth = 420;

    void timerCallback() override;
    void togglePlay();
    void refreshTools();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GRMGuitarEditor)
};
