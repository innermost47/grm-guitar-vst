// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// The interface: the neck (click a string to play it), the oscilloscope, the settings of every network,
// and the guitar roll with its tools.
#pragma once

#include <memory>
#include <vector>

#include "CustomLookAndFeel.h"
#include "PluginProcessor.h"
#include "RollView.h"
#include "NeckView.h"
#include "ScopeView.h"

class GRMGuitarEditor : public juce::AudioProcessorEditor
{
public:
	explicit GRMGuitarEditor(GRMGuitarProcessor &);
	~GRMGuitarEditor() override;
	void paint(juce::Graphics &) override;
	void resized() override;

private:
	GRMGuitarProcessor &processor;
	NeckView neck;
	ScopeView scope;
	RollView roll;

	CustomLookAndFeel customLookAndFeel;

	juce::TabbedComponent tabs{juce::TabbedButtonBar::TabsAtTop};

	std::unique_ptr<juce::VBlankAttachment> vblank;

	juce::TextButton playButton{"Play"}, takeButton{"New take"}, exportButton{"Export WAV"};
	std::unique_ptr<juce::FileChooser> chooser;
	juce::Label status, position;
	juce::Slider volume;
	std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> volumeAttachment;

	juce::ComboBox rootBox, qualityBox, strumBox, arpBox, snapBox, barsBox, fillCount;
	juce::ComboBox keyBox, scaleBox;
	juce::Label keyLabel;
	juce::TextButton fillStrums{"Fill strums"}, fillArp{"Fill arpeggio"}, clearBar{"Clear bar"}, undoButton{"Undo"},
		redoButton{"Redo"}, demoButton{"Demo song"};
	juce::Label chordLabel, barLabel, snapLabel, songLabel, help;

	juce::ComboBox patternBox, midiModeBox;
	juce::TextButton newPattern{"New"}, duplicatePattern{"Duplicate"}, deletePatternButton{"Delete"},
		copyPatternButton{"Copy"}, pastePatternButton{"Paste"};

	std::vector<std::unique_ptr<juce::TextButton>> guitarButtons;
	juce::TextButton addGuitarButton{"+ Guitar"}, removeGuitarButton{"Remove"};
	juce::ToggleButton ghostsButton{"Other guitars"};
	std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> ghostsAttachment;
	int shownGuitarVersion = -1;
	void rebuildGuitars();
	juce::ToggleButton followButton{"Follow playhead"};
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
