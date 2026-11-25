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
#include "IconButton.h"
#include "Modal.h"
#include "PresetBar.h"
#include "UpdateChecker.h"

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
	const ui::theme::AppTheme appTheme = ui::theme::AppTheme::fromScale(1.0f);

	juce::SharedResourcePointer<juce::TooltipWindow> tooltipWindow;

	juce::TabbedComponent tabs{juce::TabbedButtonBar::TabsAtTop};

	std::unique_ptr<juce::VBlankAttachment> vblank;

	using Icon = ui::IconId;
	using Behaviour = IconButton::Behaviour;

	IconButton playButton{Icon::play, "Play / stop the roll (Space)", Behaviour::momentary, Icon::stop};
	IconButton takeButton{Icon::arrowsClockwise, "New take: the player network plays the roll again, with new imprecisions"};
	IconButton exportButton{Icon::exportFile, "Export WAV: render the whole roll with the body and the room to a WAV file"};
	std::unique_ptr<juce::FileChooser> chooser;
	juce::Label status, position;
	juce::Slider volume;
	std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> volumeAttachment;

	juce::ComboBox rootBox, qualityBox, strumBox, arpBox, snapBox, barsBox, fillCount;
	juce::ComboBox keyBox, scaleBox;
	juce::Label keyLabel;
	IconButton fillStrums{Icon::strum, "Fill strums: the selection (or the bars from the current one) strummed with the chord"};
	IconButton fillArp{Icon::arpeggio, "Fill arpeggio: the selection (or the bars from the current one) picked string by string with the chord"};
	IconButton clearBar{Icon::eraser, "Clear bar: remove every note of the current bar"};
	IconButton undoButton{Icon::arrowCounterClockwise, "Undo (Ctrl+Z)"};
	IconButton redoButton{Icon::arrowClockwise, "Redo (Ctrl+Y)"};
	IconButton demoButton{Icon::musicNotes, "Demo song: replace the roll with the demo song (undo brings it back)"};
	juce::Label chordLabel, barLabel, snapLabel, songLabel, help;

	juce::ComboBox patternBox, midiModeBox;
	IconButton newPattern{Icon::filePlus, "New pattern: an empty pattern after this one"};
	IconButton duplicatePattern{Icon::copySimple, "Duplicate pattern: a copy of this pattern after it"};
	IconButton deletePatternButton{Icon::trash, "Delete this pattern"};
	IconButton copyPatternButton{Icon::copy, "Copy this pattern (paste it into any guitar, or another instance)"};
	IconButton pastePatternButton{Icon::clipboard, "Paste the copied pattern as a new pattern after this one"};

	std::vector<std::unique_ptr<juce::TextButton>> guitarButtons;
	IconButton addGuitarButton{Icon::plus, "Add a guitar to this plugin (up to 8), with its own settings, patterns and mixer strip"};
	IconButton removeGuitarButton{Icon::minus, "Remove the selected guitar (the ones after it move down)"};
	IconButton ghostsButton{Icon::ghost, "Other guitars: show their notes, faint, behind this guitar's roll", Behaviour::toggle};
	std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> ghostsAttachment;
	int shownGuitarVersion = -1;
	void rebuildGuitars();
	PresetBar presets; // the selected guitar's
	IconButton followButton{Icon::crosshair, "Follow playhead: the roll scrolls with the playhead. Off: it stays where you are editing", Behaviour::toggle};
	juce::Label patternLabel, midiLabel, keyHint;
	std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> midiModeAttachment;
	std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> followAttachment;
	int shownPatterns = -1, shownEditing = -1;
	void refreshPatterns();
	int subtitleWidth = 420;

	// the latest release, asked once per host process; shown in a dialog when it is newer
	juce::SharedResourcePointer<UpdateChecker> updates;
	// the dialog shown (one at a time); one replaced by another from its own button waits for the next frame to go
	std::unique_ptr<Modal> modal, retiredModal;
	void openDialog(std::unique_ptr<Modal> dialog);
	void showUpdate(const grmg::models::Release &release);

	void updateEditor();
	void togglePlay();
	void refreshTools();

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GRMGuitarEditor)
};
