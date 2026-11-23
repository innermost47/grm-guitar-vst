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

class NeckView : public juce::Component
{
public:
	explicit NeckView(GRMGuitarProcessor& p) : processor(p) {
		vblank = std::make_unique<juce::VBlankAttachment>(this, [this]
			{
				phase += 0.25f;
				repaint();
			});
	}
	void paint(juce::Graphics&) override;
	void mouseDown(const juce::MouseEvent&) override;

private:
	GRMGuitarProcessor& processor;
	double phase = 0.0;
	float fretX(int k) const;
	std::unique_ptr<juce::VBlankAttachment> vblank;
};

class ScopeView : public juce::Component
{
public:
	explicit ScopeView(GRMGuitarProcessor& p) : processor(p)
	{
		history.assign(4096, 0.0f);
		vblank = std::make_unique<juce::VBlankAttachment>(this, [this] { updateScope(); });
	}
	void paint(juce::Graphics&) override;

private:
	GRMGuitarProcessor& processor;
	std::vector<float> history;
	size_t write = 0;
	void updateScope();
	std::unique_ptr<juce::VBlankAttachment> vblank;
};

class ParamPanel : public juce::Component
{
public:
	ParamPanel(juce::AudioProcessorValueTreeState& state, const std::vector<juce::String>& ids);
	void resized() override;
	void setSensorId(const juce::String& sensorParameterId);
	void showOnlyForSensor(const juce::String& id, std::initializer_list<int> sensors);

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

	juce::AudioProcessorValueTreeState& state;
	std::vector<Row> rows;
	juce::String sensorId;
	juce::Label warningLabel;
	int lastSensor = -1;
	std::unique_ptr<juce::VBlankAttachment> vblank;
};

class NetworksView : public juce::Component
{
public:
	explicit NetworksView(GRMGuitarProcessor& p);
	void resized() override { text.setBounds(getLocalBounds().reduced(4)); }

private:
	GRMGuitarProcessor& processor;
	juce::TextEditor text;
	const void* shown = nullptr;
	void updateNetworks();
	std::unique_ptr<juce::VBlankAttachment> vblank;
};

class MixerView : public juce::Component
{
public:
	explicit MixerView(GRMGuitarProcessor& p);
	void resized() override;
	void paint(juce::Graphics&) override;

private:
	GRMGuitarProcessor& processor;
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
	void updateMixer();
	std::unique_ptr<juce::VBlankAttachment> vblank;
};

class GRMGuitarEditor : public juce::AudioProcessorEditor
{
public:
	explicit GRMGuitarEditor(GRMGuitarProcessor&);
	~GRMGuitarEditor() override;
	void paint(juce::Graphics&) override;
	void resized() override;

private:
	GRMGuitarProcessor& processor;
	NeckView neck;
	ScopeView scope;
	RollView roll;

	juce::LookAndFeel_V4 look;

	juce::TabbedComponent tabs{ juce::TabbedButtonBar::TabsAtTop };

	std::unique_ptr<juce::VBlankAttachment> vblank;

	juce::TextButton playButton{ "Play" }, takeButton{ "New take" }, exportButton{ "Export WAV" };
	std::unique_ptr<juce::FileChooser> chooser;
	juce::Label status, position;
	juce::Slider volume;
	std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> volumeAttachment;

	juce::ComboBox rootBox, qualityBox, strumBox, arpBox, snapBox, barsBox, fillCount;
	juce::ComboBox keyBox, scaleBox;
	juce::Label keyLabel;
	juce::TextButton fillStrums{ "Fill strums" }, fillArp{ "Fill arpeggio" }, clearBar{ "Clear bar" }, undoButton{ "Undo" },
		redoButton{ "Redo" }, demoButton{ "Demo song" };
	juce::Label chordLabel, barLabel, snapLabel, songLabel, help;

	juce::ComboBox patternBox, midiModeBox;
	juce::TextButton newPattern{ "New" }, duplicatePattern{ "Duplicate" }, deletePatternButton{ "Delete" },
		copyPatternButton{ "Copy" }, pastePatternButton{ "Paste" };

	std::vector<std::unique_ptr<juce::TextButton>> guitarButtons;
	juce::TextButton addGuitarButton{ "+ Guitar" }, removeGuitarButton{ "Remove" };
	juce::ToggleButton ghostsButton{ "Other guitars" };
	std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> ghostsAttachment;
	int shownGuitarVersion = -1;
	void rebuildGuitars();
	juce::ToggleButton followButton{ "Follow playhead" };
	juce::Label patternLabel, midiLabel, keyHint;
	std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> midiModeAttachment;
	std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> followAttachment;
	int shownPatterns = -1, shownEditing = -1;
	void refreshPatterns();
	int subtitleWidth = 420;

	void updateEditor();
	void togglePlay();
	void refreshTools();

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GRMGuitarEditor)
};
