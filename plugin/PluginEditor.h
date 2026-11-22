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
class NeckView : public juce::Component, private juce::Timer
{
public:
	explicit NeckView(GRMGuitarProcessor &p) : processor(p) { startTimerHz(30); }
	void paint(juce::Graphics &) override;
	void mouseDown(const juce::MouseEvent &) override;

private:
	GRMGuitarProcessor &processor;
	double phase = 0.0;
	float fretX(int k) const;
	void timerCallback() override
	{
		phase += 0.9;
		repaint();
	}
};

class ScopeView : public juce::Component, private juce::Timer
{
public:
	explicit ScopeView(GRMGuitarProcessor &p) : processor(p)
	{
		history.assign(4096, 0.0f);
		startTimerHz(30);
	}
	void paint(juce::Graphics &) override;

private:
	GRMGuitarProcessor &processor;
	std::vector<float> history;
	size_t write = 0;
	void timerCallback() override;
};

// the parameters of a group of networks, as rows of sliders, switches and lists
class ParamPanel : public juce::Component, private juce::Timer
{
public:
	ParamPanel(juce::AudioProcessorValueTreeState &state, const std::vector<juce::String> &ids);
	void resized() override;

	// sensor-aware visibility: rows flagged with showOnlyForSensor() are shown only for those sensor values.
	// sensor values: 0 mic, 1 single pickup, 2 piezo, 3 HSS.
	void setSensorId(const juce::String &sensorParameterId);
	void showOnlyForSensor(const juce::String &id, std::initializer_list<int> sensors);

private:
	struct Row
	{
		juce::String id;
		int sensorMask = 0; // 0 = always visible, otherwise bitmask over sensor values
		std::unique_ptr<juce::Label> label;
		std::unique_ptr<juce::Component> control;
		std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> slider;
		std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> button;
		std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> combo;
	};

	void timerCallback() override;
	void updateForSensor();

	juce::AudioProcessorValueTreeState &state;
	std::vector<Row> rows;
	juce::String sensorId;
	juce::Label warningLabel;
	int lastSensor = -1;
};

// what the networks found, as numbers (read only)
class NetworksView : public juce::Component, private juce::Timer
{
public:
	explicit NetworksView(GRMGuitarProcessor &p);
	void resized() override { text.setBounds(getLocalBounds().reduced(4)); }

private:
	GRMGuitarProcessor &processor;
	juce::TextEditor text;
	const void *shown = nullptr;
	void timerCallback() override;
};

// the mixer: one strip per guitar (gain, level, pan, mute, solo, output); a click on a name selects that guitar
class MixerView : public juce::Component, private juce::Timer
{
public:
	explicit MixerView(GRMGuitarProcessor &p);
	void resized() override;
	void paint(juce::Graphics &) override;

private:
	GRMGuitarProcessor &processor;
	struct Strip
	{
		std::unique_ptr<juce::TextButton> name;
		std::unique_ptr<juce::Slider> gain, level, pan;
		std::unique_ptr<juce::ToggleButton> mute, solo;
		std::unique_ptr<juce::ComboBox> out;
		std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gainA, levelA, panA;
		std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> muteA, soloA;
		std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> outA;
	};
	std::vector<Strip> strips;
	int shownCount = -1, shownSelected = -1;
	void rebuild();
	void timerCallback() override;
};

class GRMGuitarEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
	explicit GRMGuitarEditor(GRMGuitarProcessor &);
	~GRMGuitarEditor() override;
	void paint(juce::Graphics &) override;
	void resized() override;

private:
	GRMGuitarProcessor &processor;
	juce::LookAndFeel_V4 look;
	NeckView neck;
	ScopeView scope;
	juce::TabbedComponent tabs{juce::TabbedButtonBar::TabsAtTop};
	RollView roll;

	// header
	juce::TextButton playButton{"Play"}, takeButton{"New take"}, exportButton{"Export WAV"};
	std::unique_ptr<juce::FileChooser> chooser;
	juce::Label status, position;
	juce::Slider volume;
	std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> volumeAttachment;

	// tools of the roll
	juce::ComboBox rootBox, qualityBox, strumBox, arpBox, snapBox, barsBox, fillCount;
	juce::ComboBox keyBox, scaleBox; // the key and scale of the pattern (harmonizing)
	juce::Label keyLabel;
	juce::TextButton fillStrums{"Fill strums"}, fillArp{"Fill arpeggio"}, clearBar{"Clear bar"}, undoButton{"Undo"},
		redoButton{"Redo"}, demoButton{"Demo song"};
	juce::Label chordLabel, barLabel, snapLabel, songLabel, help;

	// patterns: which one is edited, and how MIDI keys play them
	juce::ComboBox patternBox, midiModeBox;
	juce::TextButton newPattern{"New"}, duplicatePattern{"Duplicate"}, deletePatternButton{"Delete"},
		copyPatternButton{"Copy"}, pastePatternButton{"Paste"};

	// the guitars: one button each, add, remove
	std::vector<std::unique_ptr<juce::TextButton>> guitarButtons;
	juce::TextButton addGuitarButton{"+ Guitar"}, removeGuitarButton{"Remove"};
	juce::ToggleButton ghostsButton{"Other guitars"};
	std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> ghostsAttachment;
	int shownGuitarVersion = -1;
	void rebuildGuitars(); // the guitar buttons and the settings tabs of the selected guitar
	juce::ToggleButton followButton{"Follow playhead"};
	juce::Label patternLabel, midiLabel, keyHint;
	std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> midiModeAttachment;
	std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> followAttachment;
	int shownPatterns = -1, shownEditing = -1;
	void refreshPatterns();
	int subtitleWidth = 420;

	void timerCallback() override;
	void togglePlay();
	void refreshTools();

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GRMGuitarEditor)
};
