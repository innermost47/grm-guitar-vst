// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
#include "PluginEditor.h"
#include "ParamPanel.h"
#include "NetworksView.h"
#include "MixerView.h"
#include "TensionView.h"

using namespace grmg;
using namespace ui;

GRMGuitarEditor::GRMGuitarEditor(GRMGuitarProcessor &p)
	: AudioProcessorEditor(&p), processor(p), neck(p), mixer(p), roll(p), transport(p, roll), master(p), networks(p)
{
	juce::LookAndFeel::setDefaultLookAndFeel(&CustomLookAndFeel::getInstance());

	addAndMakeVisible(neck);
	mixerScroll.setViewedComponent(&mixer, false);
	mixerScroll.setScrollBarsShown(false, true);
	for (auto *c : std::initializer_list<juce::Component *>{&mixerScroll, &master, &transport, &networks, &tabs})
		addAndMakeVisible(c);
	addAndMakeVisible(addGuitarButton);
	addGuitarButton.onClick = [this]
	{ processor.addGuitar(); };
	addAndMakeVisible(removeGuitarButton);
	removeGuitarButton.onClick = [this]
	{ processor.removeGuitar(processor.getSelectedGuitar()); };
	addAndMakeVisible(ghostsButton);
	ghostsAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.params, "ghosts", ghostsButton);
	presets.capture = [this](const std::string &name)
	{ return processor.capturePreset(processor.getSelectedGuitar(), name); };
	presets.apply = [this](const grmg::models::Preset &preset)
	{ processor.applyPreset(processor.getSelectedGuitar(), preset); };
	presets.currentName = [this]
	{ return processor.getPresetName(processor.getSelectedGuitar()); };
	presets.showDialog = [this](std::unique_ptr<Modal> dialog)
	{ openDialog(std::move(dialog)); };
	addAndMakeVisible(presets);
	rebuildGuitars();

	addAndMakeVisible(takeButton);
	takeButton.onClick = [this]
	{ processor.newTake(); };
	addAndMakeVisible(exportButton);
	exportButton.onClick = [this]
	{
		chooser = std::make_unique<juce::FileChooser>("Export the roll", juce::File::getSpecialLocation(juce::File::userMusicDirectory).getChildFile("grm_guitar.wav"), "*.wav");
		chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
							 [this](const juce::FileChooser &fc)
							 {
								 const auto f = fc.getResult();
								 if (f != juce::File())
									 processor.exportSong(f.withFileExtension("wav"));
							 });
	};
	status.setColour(juce::Label::textColourId, TEXT);
	status.setFont(font(12.0f));
	status.setJustificationType(juce::Justification::centredRight);
	addAndMakeVisible(status);

	roll.chordRoot = processor.getPersistentChordRoot();
	roll.chordQuality = processor.getPersistentChordQuality();

	for (int r = 0; r < 12; ++r)
		rootBox.addItem(grmg::ROOT_LABELS[r], r + 1);
	rootBox.setSelectedId(roll.chordRoot + 1, juce::dontSendNotification);
	rootBox.onChange = [this]
	{
		roll.chordRoot = rootBox.getSelectedId() - 1;
		processor.setPersistentChord(roll.chordRoot, roll.chordQuality);
	};

	for (int q = 0; q < (int)grmg::chord_qualities().size(); ++q)
		qualityBox.addItem(grmg::chord_qualities()[(size_t)q].label, q + 1);
	qualityBox.setSelectedId(roll.chordQuality + 1, juce::dontSendNotification);
	qualityBox.onChange = [this]
	{
		roll.chordQuality = qualityBox.getSelectedId() - 1;
		processor.setPersistentChord(roll.chordRoot, roll.chordQuality);
	};
	rootBox.setTooltip("The chord of new strums and fills: its root (sharps and flats)");
	qualityBox.setTooltip("The chord's kind. Its shape is chosen by the left-hand network for this guitar's tuning");

	for (int r = 0; r < 12; ++r)
		keyBox.addItem(grmg::ROOT_LABELS[r], r + 1);
	for (int k = 0; k < (int)grmg::scales().size(); ++k)
		scaleBox.addItem(grmg::scales()[(size_t)k].name, k + 1);
	keyBox.setTooltip("The key of this pattern: harmonizing (right click on notes) follows it");
	scaleBox.setTooltip("The scale of this pattern: harmonizing follows it");

	if (roll.getSong().key == 0 && processor.getPersistentSongKey() != 0)
		roll.edit([this](Song &s)
				  { s.key = processor.getPersistentSongKey(); }, false);
	if (roll.getSong().scale == 0 && processor.getPersistentSongScale() != 0)
		roll.edit([this](Song &s)
				  { s.scale = processor.getPersistentSongScale(); }, false);

	keyBox.setSelectedId(roll.getSong().key + 1, juce::dontSendNotification);
	scaleBox.setSelectedId(roll.getSong().scale + 1, juce::dontSendNotification);

	keyBox.onChange = [this]
	{
		const int k = keyBox.getSelectedId() - 1;
		if (k >= 0)
		{
			processor.setPersistentSongKey(k);
			if (k != roll.getSong().key)
				roll.edit([k](Song &s)
						  { s.key = k; }, false);
			neck.repaint();
			roll.repaint();
		}
	};

	scaleBox.onChange = [this]
	{
		const int k = scaleBox.getSelectedId() - 1;
		if (k >= 0)
		{
			processor.setPersistentSongScale(k);
			if (k != roll.getSong().scale)
				roll.edit([k](Song &s)
						  { s.scale = k; }, false);
			neck.repaint();
			roll.repaint();
		}
	};

	keyLabel.setText("Key", juce::dontSendNotification);

	for (int k = 0; k < NUM_STRUM_PATTERNS; ++k)
		strumBox.addItem(STRUM_PATTERNS[k].name, k + 1);
	strumBox.setSelectedId(processor.persistentStrumPattern.load(), juce::dontSendNotification);
	strumBox.onChange = [this]
	{ processor.persistentStrumPattern.store(strumBox.getSelectedId()); };

	for (int k = 0; k < NUM_ARPEGGIO_PATTERNS; ++k)
		arpBox.addItem(ARPEGGIO_PATTERNS[k].name, k + 1);
	arpBox.setSelectedId(processor.persistentArpPattern.load(), juce::dontSendNotification);
	arpBox.onChange = [this]
	{ processor.persistentArpPattern.store(arpBox.getSelectedId()); };

	const std::pair<const char *, double> snaps[] = {{"1/4", 1.0}, {"1/8", 0.5}, {"1/8 triplet", 1.0 / 3}, {"1/16", 0.25}, {"1/16 triplet", 1.0 / 6}, {"1/32", 0.125}};
	for (int k = 0; k < 6; ++k)
		snapBox.addItem(snaps[k].first, k + 1);
	snapBox.setSelectedId(processor.persistentSnapIndex.load(), juce::dontSendNotification);
	roll.snap = snaps[processor.persistentSnapIndex.load() - 1].second;
	snapBox.onChange = [this, snaps]
	{
		const int idx = snapBox.getSelectedId();
		processor.persistentSnapIndex.store(idx);
		roll.snap = snaps[idx - 1].second;
		roll.repaint();
	};

	for (int b : {1, 2, 4, 8, 12, 16, 24, 32, 48, 64})
		barsBox.addItem(juce::String(b) + " bars", b);
	barsBox.onChange = [this]
	{
		const int b = barsBox.getSelectedId();
		if (b > 0 && b != roll.getSong().bars)
			roll.edit([b](Song &s)
					  { s.bars = b; }, false);
	};

	for (int b : {1, 2, 4, 8})
		fillCount.addItem("x " + juce::String(b), b);
	fillCount.setSelectedId(processor.persistentFillCount.load(), juce::dontSendNotification);
	fillCount.onChange = [this]
	{ processor.persistentFillCount.store(fillCount.getSelectedId()); };

	auto fillRange = [this](double &from, double &to)
	{
		if (roll.selectionRange(from, to))
			return;
		const auto &s = roll.getSong();
		const int bar = roll.currentBar, count = std::max(1, fillCount.getSelectedId());
		from = bar * s.beats_per_bar;
		to = std::min(grmg::Song::MAX_BARS, bar + count) * s.beats_per_bar;
	};
	fillStrums.onClick = [this, fillRange]
	{
		double from, to;
		fillRange(from, to);
		const int pattern = strumBox.getSelectedId() - 1;
		const auto c = roll.currentChord();
		roll.edit([=](Song &s)
				  { s.fill_strums_range(from, to, pattern, c.frets, c.name, 2.0); });
	};
	fillArp.onClick = [this, fillRange]
	{
		double from, to;
		fillRange(from, to);
		const int pattern = arpBox.getSelectedId() - 1;
		const auto c = roll.currentChord();
		roll.edit([=](Song &s)
				  { s.fill_arpeggio_range(from, to, pattern, c.frets, 2.0); });
	};
	clearBar.onClick = [this]
	{
		const int bar = roll.currentBar;
		roll.edit([=](Song &s)
				  { s.remove_range(bar * s.beats_per_bar, (bar + 1) * s.beats_per_bar); });
	};
	undoButton.onClick = [this]
	{ roll.undo(); };
	redoButton.onClick = [this]
	{ roll.redo(); };
	demoButton.onClick = [this]
	{ roll.edit([](Song &s)
				{ s = Song::demo(); }); };
	chordLabel.setText("Chord", juce::dontSendNotification);
	snapLabel.setText("Snap", juce::dontSendNotification);
	songLabel.setText("Length", juce::dontSendNotification);
	for (auto *c : std::initializer_list<juce::Component *>{&rootBox, &qualityBox, &keyBox, &scaleBox, &keyLabel, &strumBox, &arpBox, &snapBox, &barsBox, &fillCount,
															&fillStrums, &fillArp, &clearBar, &undoButton, &redoButton,
															&demoButton, &chordLabel, &barLabel, &snapLabel, &songLabel, &help})
		addAndMakeVisible(c);
	for (auto *l : {&chordLabel, &barLabel, &snapLabel, &songLabel, &help})
	{
		l->setColour(juce::Label::textColourId, TEXT);
		l->setFont(font(12.0f));
	}
	help.setText("Click: new note / select.  Ctrl+click: add to selection.  Ctrl/Shift+drag: rectangle.  Ctrl+drag a note: "
				 "copy.  Ctrl+A/C/X/V/D: all, copy, cut, paste, duplicate.  Arrows: move (Shift: bar, Shift+up/down: string).  "
				 "Wheel or digits: fret.  +/-: transpose.  P: palm.  X: dead note.  Alt+click: hand stop.  Drag a note's top or bottom "
				 "edge: more strings.  Del: delete.  Ctrl+wheel: zoom.  Space: play.",
				 juce::dontSendNotification);
	help.setFont(font(11.0f));

	feelLabel.setText("Feel", juce::dontSendNotification);
	for (int k = 0; k < NUM_FEEL_PRESETS; ++k)
		feelBox.addItem(FEEL_PRESET_NAMES[k], k + 1);
	feelBox.setTooltip("The Feel of this pattern: its lanes (force, timing, palm under the roll) drawn again from this "
					   "preset and its meter (undo brings them back)");
	feelBox.onChange = [this]
	{
		const int preset = feelBox.getSelectedId() - 1;
		if (preset >= 0 && preset != roll.getSong().feel_preset)
			roll.edit([preset](Song &s)
					  { s.feel_preset = preset; s.feel = {}; }, false);
	};
	feelDepth.setSliderStyle(juce::Slider::LinearHorizontal);
	feelDepth.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
	feelDepth.setTooltip("How much the Feel weighs (the accents and the timing of its lanes)");
	cloneSteps.onClick = [this]
	{
		int from, to;
		feelRange(from, to);
		roll.edit([=](Song &s)
				  { FeelGenerator::clone(s, from, to - from); }, false);
	};
	varySteps.onClick = [this]
	{
		int from, to;
		feelRange(from, to);
		const unsigned seed = (unsigned)juce::Random::getSystemRandom().nextInt();
		roll.edit([=](Song &s)
				  { FeelGenerator::vary(s, from, to - from, seed); }, false);
	};
	grooveSteps.onClick = [this]
	{
		int from = 0, to = roll.getSong().steps();
		roll.feelSelection(from, to);
		const unsigned seed = (unsigned)juce::Random::getSystemRandom().nextInt();
		roll.edit([=](Song &s)
				  { FeelGenerator::groove(s, from, to - from, seed); }, false);
	};
	for (auto *c : std::initializer_list<juce::Component *>{&feelLabel, &feelBox, &feelDepth, &cloneSteps, &varySteps, &grooveSteps})
		addAndMakeVisible(c);
	feelLabel.setColour(juce::Label::textColourId, TEXT);
	feelLabel.setFont(font(12.0f));

	patternLabel.setText("Pattern", juce::dontSendNotification);
	midiLabel.setText("MIDI keys", juce::dontSendNotification);
	patternBox.onChange = [this]
	{
		const int k = patternBox.getSelectedId() - 1;
		if (k >= 0)
			processor.selectPattern(k);
	};
	newPattern.onClick = [this]
	{ processor.addPattern(false); };
	duplicatePattern.onClick = [this]
	{ processor.addPattern(true); };
	deletePatternButton.onClick = [this]
	{ processor.deletePattern(); };
	copyPatternButton.onClick = [this]
	{ processor.copyPattern(); };
	pastePatternButton.onClick = [this]
	{ processor.pastePattern(); };
	midiModeBox.addItemList({"Play notes", "Play patterns"}, 1);
	midiModeBox.setTooltip("Play notes: a key plays a note on the guitar. Play patterns: a key plays a whole pattern "
						   "(C1 pattern 1, C#1 pattern 2...), looped while the key is held");
	midiModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.params, "midi_mode", midiModeBox);
	for (auto *c : std::initializer_list<juce::Component *>{&patternLabel, &patternBox, &newPattern, &duplicatePattern,
															&deletePatternButton, &copyPatternButton, &pastePatternButton, &midiLabel, &midiModeBox, &keyHint})
		addAndMakeVisible(c);
	for (auto *l : {&patternLabel, &midiLabel, &keyHint, &keyLabel})
	{
		l->setColour(juce::Label::textColourId, TEXT);
		l->setFont(font(12.0f));
	}
	refreshPatterns();

	roll.onPlayToggle = [this]
	{ togglePlay(); };
	roll.onChanged = [this]
	{ refreshTools(); };
	addAndMakeVisible(roll);
	refreshTools();

	setResizable(true, true);
	setResizeLimits(appTheme.dimensions.editorMinWidth, appTheme.dimensions.editorMinHeight, 2800, 1800);
	setSize(appTheme.dimensions.editorWidth, appTheme.dimensions.editorHeight);
	vblank = std::make_unique<juce::VBlankAttachment>(this, [this]
													  { updateEditor(); });
}

GRMGuitarEditor::~GRMGuitarEditor() { setLookAndFeel(nullptr); }

void GRMGuitarEditor::feelRange(int &from, int &to) const
{
	if (roll.feelSelection(from, to))
		return;
	const int perBar = roll.getSong().beats_per_bar * 4;
	from = roll.currentBar * perBar;
	to = from + perBar;
}

void GRMGuitarEditor::refreshTools()
{
	feelBox.setSelectedId(roll.getSong().feel_preset + 1, juce::dontSendNotification);
	keyBox.setSelectedId(roll.getSong().key + 1, juce::dontSendNotification);
	scaleBox.setSelectedId(roll.getSong().scale + 1, juce::dontSendNotification);
	const int bars = roll.getSong().bars;
	if (barsBox.indexOfItemId(bars) < 0)
		barsBox.addItem(juce::String(bars) + " bars", bars);
	barsBox.setSelectedId(bars, juce::dontSendNotification);
	barLabel.setText("Bar " + juce::String(roll.currentBar + 1), juce::dontSendNotification);
}

void GRMGuitarEditor::rebuildGuitars()
{
	using namespace ui;
	shownGuitarVersion = processor.getGuitarVersion();
	const int count = processor.getGuitarCount();
	feelDepthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.params, processor.paramId("feel_depth"), feelDepth);
	addGuitarButton.setEnabled(count < GRMGuitarProcessor::MAX_GUITARS);
	removeGuitarButton.setEnabled(count > 1);
	presets.refresh();

	const int shownTab = juce::jmax(0, tabs.getCurrentTabIndex());
	tabs.clearTabs();
	// the ids of a tab followed by the sliders of a table of tunables
	auto withTunables = [](std::vector<juce::String> ids, const auto &tunables)
	{
		for (const auto &t : tunables)
			ids.push_back(t.id);
		return ids;
	};
	auto addTab = [&](const char *name, std::vector<juce::String> ids, bool ownGuitar = true,
					  std::function<void(ParamPanel &)> setup = nullptr)
	{
		std::vector<juce::String> full;
		for (const auto &id : ids)
			full.push_back(ownGuitar ? processor.paramId(id) : id);
		auto panel = std::make_unique<ParamPanel>(processor.params, full);
		if (setup)
			setup(*panel);
		tabs.addTab(name, std::move(panel));
	};
	// the strings: the tuning, each open string's note in a Custom tuning (high string first, as the roll shows them)
	std::vector<juce::String> strings = {"tuning"};
	for (int i = NUM_STRINGS - 1; i >= 0; --i)
		strings.push_back(OPEN_NOTE_PARAMS[i]);
	for (const char *id : {"string_set", "scale_mm", "string_age", "brightness", "core", "stiff"})
		strings.push_back(id);
	addTab("Strings", strings, true,
		   [this](ParamPanel &panel)
		   {
			   for (const char *id : OPEN_NOTE_PARAMS)
				   panel.showOnlyWhen(processor.paramId(id), processor.paramId("tuning"), {CUSTOM_TUNING});
			   // the wound-core setting: the sets whose wound strings have a steel core (Automatic's always do)
			   std::vector<int> steelCores = {STRING_SET_AUTOMATIC};
			   for (int k = 0; k < NUM_STRING_SETS; ++k)
				   if (STRING_MATERIALS[STRING_SETS[k].wound_material].core_ratio <= 0.0)
					   steelCores.push_back(k + 1);
			   panel.showOnlyWhen(processor.paramId("core"), processor.paramId("string_set"), steelCores);
			   panel.setFooter(std::make_unique<TensionView>(processor), appTheme.dimensions.tensionViewHeight);
		   });
	addTab("Attack & palm", withTunables({"attack", "pluck", "force", "pick", "palm_mm", "palm_intensity", "live_palm"}, VOICE_TUNABLES));
	addTab("Body", {"hollow", "wood", "density_pct", "body_size", "top_mm", "braces_g", "body_q", "coupling", "body_amount", "sympathy"});
	addTab("Sensor",
		   {"sensor", "pickup_mm", "pickup_height_mm", "pickup_type", "cable_pf",
			"pickup_bridge_on", "pickup_middle_on", "pickup_neck_on",
			"pickup_bridge_split",
			"pickup_bridge_type", "pickup_middle_type", "pickup_neck_type"},
		   true,
		   [this](ParamPanel &panel)
		   {
			   panel.showOnlyWhen(processor.paramId("pickup_mm"), processor.paramId("sensor"), {1});
			   panel.showOnlyWhen(processor.paramId("pickup_type"), processor.paramId("sensor"), {1});
			   panel.showOnlyWhen(processor.paramId("pickup_height_mm"), processor.paramId("sensor"), {1, 3});
			   panel.showOnlyWhen(processor.paramId("cable_pf"), processor.paramId("sensor"), {1, 3});
			   panel.showOnlyWhen(processor.paramId("pickup_bridge_on"), processor.paramId("sensor"), {3});
			   panel.showOnlyWhen(processor.paramId("pickup_middle_on"), processor.paramId("sensor"), {3});
			   panel.showOnlyWhen(processor.paramId("pickup_neck_on"), processor.paramId("sensor"), {3});
			   panel.showOnlyWhen(processor.paramId("pickup_bridge_split"), processor.paramId("sensor"), {3});
			   panel.showOnlyWhen(processor.paramId("pickup_bridge_type"), processor.paramId("sensor"), {3});
			   panel.showOnlyWhen(processor.paramId("pickup_middle_type"), processor.paramId("sensor"), {3});
			   panel.showOnlyWhen(processor.paramId("pickup_neck_type"), processor.paramId("sensor"), {3});
		   });
	// the amplifier: the rig's own knobs, each channel (voicing and knobs), the pedal, the cabinet; then the larsen.
	// All of it for a magnetic pickup only.
	auto amp = withTunables({"amp_on"}, AMP_TUNABLES);
	for (const auto &channel : AMP_CHANNELS)
	{
		amp.push_back(channel.voicing);
		for (const auto &t : AMP_CHANNEL_TUNABLES)
			amp.push_back(juce::String(amp_channel_id(channel, t.id)));
	}
	amp.push_back("amp_od_on");
	amp = withTunables(amp, OVERDRIVE_TUNABLES);
	amp.push_back("amp_cab");
	amp = withTunables(amp, CABINET_TUNABLES);
	for (const char *id : {"feedback_amount", "feedback_distance", "feedback_target"})
		amp.push_back(id);
	addTab("Amp & Feedback", amp, true,
		   [this, amp](ParamPanel &panel)
		   {
			   for (const auto &id : amp)
				   panel.showOnlyWhen(processor.paramId(id), processor.paramId("sensor"), {1, 3});
		   });
	addTab("Room", {"room_on", "room_size", "room_material", "room_distance"});
	addTab("Player", withTunables({"human", "human_amount", "swing_cm", "feel_depth", "imperfection", "palm_coupling", "finger_noises", "finger_force"}, HAND_TUNABLES));
	addTab("Playback", {"follow_host", "loop", "tempo", "midi_strings", "midi_route", "audition"}, false);
	tabs.setCurrentTabIndex(juce::jmin(shownTab, tabs.getNumTabs() - 1));
	resized();
}

void GRMGuitarEditor::refreshPatterns()
{
	const int count = processor.getPatternCount(), k = processor.getEditingPattern();
	if (count != shownPatterns)
	{
		patternBox.clear(juce::dontSendNotification);
		for (int i = 0; i < count; ++i)
			patternBox.addItem(juce::String(i + 1) + "   (key " + GRMGuitarProcessor::patternKeyName(i) + ")", i + 1);
		shownPatterns = count;
		shownEditing = -1;
	}
	if (k != shownEditing)
	{
		patternBox.setSelectedId(k + 1, juce::dontSendNotification);
		shownEditing = k;
	}
	deletePatternButton.setEnabled(count > 1);
	newPattern.setEnabled(count < GRMGuitarProcessor::MAX_PATTERNS);
	duplicatePattern.setEnabled(count < GRMGuitarProcessor::MAX_PATTERNS);
	const bool patterns = processor.params.getRawParameterValue("midi_mode")->load() > 0.5f;
	keyHint.setText(patterns ? "key " + GRMGuitarProcessor::patternKeyName(0) + " = pattern 1, " + GRMGuitarProcessor::patternKeyName(1) + " = pattern 2..."
							 : "keys play notes on the guitar",
					juce::dontSendNotification);
}

void GRMGuitarEditor::togglePlay()
{
	processor.setInternalPlaying(!processor.isInternalPlaying());
}

void GRMGuitarEditor::showUpdate(const grmg::models::Release &release)
{
	const juce::String version(release.version);
	auto dialog = std::make_unique<Modal>("A new GRM Guitar is out");
	dialog->setMessage("GRM Guitar " + version + " is available (this one is " + juce::String(UpdateChecker::currentVersion()) +
					  "). Its release page tells what is new and has the downloads for every system.");
	dialog->addLink("Get GRM Guitar " + version, juce::URL(release.url));
	auto &again = dialog->addCheckbox("Don't ask me again for this version");
	dialog->addButton("Close");
	dialog->onDismiss = [this, &again, version]
	{
		if (again.getToggleState())
			updates->skip(version.toStdString());
	};
	openDialog(std::move(dialog));
}

void GRMGuitarEditor::openDialog(std::unique_ptr<Modal> dialog)
{
	retiredModal = std::move(modal); // its button may still be running
	if (retiredModal != nullptr)
		retiredModal->dismiss();
	modal = std::move(dialog);
	modal->show(*this);
}

void GRMGuitarEditor::updateEditor()
{
	retiredModal.reset();
	if (modal != nullptr && modal->isDismissed())
		modal.reset(); // closed during the last frame
	if (modal == nullptr)
		if (const auto release = updates->take())
			showUpdate(*release);
	status.setText(processor.getStatus(), juce::dontSendNotification);
	if (processor.getGuitarVersion() != shownGuitarVersion)
		rebuildGuitars();
	refreshPatterns();
	pastePatternButton.setEnabled(processor.hasCopiedPattern() || juce::SystemClipboard::getTextFromClipboard().startsWith("grm-guitar-roll"));
	exportButton.setEnabled(!processor.isExporting());
}

// the sidebar's column, from the top of the window to its bottom
juce::Rectangle<int> GRMGuitarEditor::sidebar() const
{
	return getLocalBounds().removeFromLeft(getWidth() * appTheme.dimensions.sidebarPercent / 100);
}

void GRMGuitarEditor::paint(juce::Graphics &g)
{
	using namespace ui;
	g.fillAll(BG);
	const auto side = sidebar();
	g.setColour(PANEL);
	g.fillRect(side);
	g.setColour(GRID);
	g.drawVerticalLine(side.getRight() - 1, 0.0f, (float)getHeight());
	// the title, on the right of the top line
	const auto title = getLocalBounds().removeFromTop(appTheme.dimensions.topLineHeight).removeFromRight(appTheme.dimensions.titleWidth).reduced(appTheme.spacing.lg, 0);
	g.setColour(BRIGHT);
	g.setFont(font(19.0f, true));
	g.drawText("GRM GUITAR", title.withTrimmedBottom(appTheme.dimensions.topLineHeight / 2 - 4), juce::Justification::bottomRight);
	g.setColour(TEXT);
	g.setFont(font(11.0f));
	g.drawText("from string to amp and room: all solved by GRM", title.withTrimmedTop(appTheme.dimensions.topLineHeight / 2 + 2), juce::Justification::topRight);
}

void GRMGuitarEditor::resized()
{
	if (modal != nullptr)
		modal->setBounds(getLocalBounds());
	const int iconSize = appTheme.dimensions.iconButton;
	const int iconGap = appTheme.spacing.md;
	auto placeIcon = [iconSize](juce::Component &button, juce::Rectangle<int> slot)
	{ button.setBounds(slot.withSizeKeepingCentre(iconSize, iconSize)); };
	// an icon keeps its size in a crowded bar (the menus give way)
	auto iconItem = [iconSize](juce::Component &button, juce::FlexItem::Margin margin)
	{ return juce::FlexItem(button).withWidth((float)iconSize).withHeight((float)iconSize).withMargin(margin).withFlex(0.0f, 0.0f); };

	// the sidebar: on top the take, the export and the presets; the settings by tabs; the networks' figures under them
	auto side = sidebar().withTrimmedRight(1).reduced(appTheme.spacing.lg, appTheme.spacing.base);
	{
		auto line = side.removeFromTop(appTheme.dimensions.topLineHeight - 2 * appTheme.spacing.base);
		placeIcon(takeButton, line.removeFromLeft(iconSize));
		line.removeFromLeft(iconGap);
		placeIcon(exportButton, line.removeFromLeft(iconSize));
		line.removeFromLeft(appTheme.spacing.xl);
		presets.setBounds(line.withSizeKeepingCentre(line.getWidth(), iconSize));
	}
	side.removeFromTop(appTheme.spacing.base);
	networks.setBounds(side.removeFromBottom(appTheme.dimensions.networksHeight));
	side.removeFromBottom(appTheme.spacing.base);
	tabs.setBounds(side);

	auto area = getLocalBounds().withTrimmedLeft(sidebar().getWidth());
	// the top line: the transport in the middle, the guitars' + and - beside it, the status, the title on the right
	{
		auto line = area.removeFromTop(appTheme.dimensions.topLineHeight).reduced(appTheme.spacing.lg, appTheme.spacing.base);
		line.removeFromRight(appTheme.dimensions.titleWidth);
		const int width = juce::jmin(transport.preferredWidth(), line.getWidth() - 3 * iconSize);
		transport.setBounds(line.removeFromLeft(width));
		line.removeFromLeft(appTheme.spacing.xl);
		placeIcon(addGuitarButton, line.removeFromLeft(iconSize));
		line.removeFromLeft(appTheme.spacing.base);
		placeIcon(removeGuitarButton, line.removeFromLeft(iconSize));
		line.removeFromLeft(appTheme.spacing.xl);
		status.setBounds(line);
	}
	// above the roll: the guitars' strips (they scroll), the master, the neck (the room under it kept free)
	auto top = area.removeFromTop(area.getHeight() * appTheme.dimensions.mixerPercent / 100).reduced(appTheme.spacing.lg, appTheme.spacing.base);
	auto neckArea = top.removeFromRight(top.getWidth() * appTheme.dimensions.neckPercent / 100);
	neck.setBounds(neckArea.removeFromTop(juce::jmin(neckArea.getHeight(), appTheme.dimensions.neckHeight)));
	top.removeFromRight(appTheme.spacing.lg);
	master.setBounds(top.removeFromRight(appTheme.dimensions.mixerStripWidth));
	mixerScroll.setBounds(top);
	mixer.setSize(mixer.preferredWidth(), top.getHeight() - (mixer.preferredWidth() > top.getWidth() ? mixerScroll.getScrollBarThickness() : 0));

	constexpr float labelWidth = 56.0f;
	constexpr float itemHeight = 24.0f;
	const juce::FlexItem::Margin itemGap(0.0f, 6.0f, 0.0f, 0.0f);
	const juce::FlexItem::Margin groupGap(0.0f, 12.0f, 0.0f, 0.0f);
	const juce::FlexItem::Margin noMargin(0.0f);
	auto rowOf = [](juce::Rectangle<int> bounds, juce::Array<juce::FlexItem> items)
	{
		juce::FlexBox box;
		box.flexDirection = juce::FlexBox::Direction::row;
		box.alignItems = juce::FlexBox::AlignItems::center;
		box.justifyContent = juce::FlexBox::JustifyContent::flexStart;
		box.items = std::move(items);
		box.performLayout(bounds.toFloat());
	};
	// below: the roll's two bars (the pattern, then the tools of writing), the roll, its help
	rowOf(area.removeFromTop(32).reduced(8, 2),
		  {juce::FlexItem(patternLabel).withWidth(labelWidth).withHeight(itemHeight).withMargin(itemGap),
		   juce::FlexItem(patternBox).withWidth(144.0f).withHeight(itemHeight).withMargin(itemGap),
		   iconItem(newPattern, itemGap), iconItem(duplicatePattern, itemGap), iconItem(deletePatternButton, itemGap),
		   iconItem(copyPatternButton, itemGap), iconItem(pastePatternButton, groupGap),
		   juce::FlexItem(songLabel).withWidth(labelWidth).withHeight(itemHeight).withMargin(itemGap),
		   juce::FlexItem(barsBox).withWidth(86.0f).withHeight(itemHeight).withMargin(groupGap),
		   juce::FlexItem(feelLabel).withWidth(40.0f).withHeight(itemHeight).withMargin(itemGap),
		   juce::FlexItem(feelBox).withWidth(104.0f).withHeight(itemHeight).withMargin(itemGap),
		   juce::FlexItem(feelDepth).withWidth(80.0f).withHeight(itemHeight).withMargin(itemGap),
		   iconItem(cloneSteps, itemGap), iconItem(varySteps, itemGap), iconItem(grooveSteps, groupGap),
		   juce::FlexItem(midiLabel).withWidth(labelWidth).withHeight(itemHeight).withMargin(itemGap),
		   juce::FlexItem(midiModeBox).withWidth(114.0f).withHeight(itemHeight).withMargin(groupGap),
		   juce::FlexItem(keyHint).withFlex(1.0f).withMinWidth(0.0f).withHeight(itemHeight).withMargin(itemGap),
		   iconItem(ghostsButton, noMargin)});
	rowOf(area.removeFromTop(32).reduced(8, 2),
		  {juce::FlexItem(snapLabel).withWidth(labelWidth).withHeight(itemHeight).withMargin(itemGap),
		   juce::FlexItem(snapBox).withWidth(82.0f).withHeight(itemHeight).withMargin(groupGap),
		   juce::FlexItem(chordLabel).withWidth(labelWidth).withHeight(itemHeight).withMargin(itemGap),
		   juce::FlexItem(rootBox).withWidth(72.0f).withHeight(itemHeight).withMargin(itemGap),
		   juce::FlexItem(qualityBox).withWidth(76.0f).withHeight(itemHeight).withMargin(itemGap),
		   juce::FlexItem(strumBox).withWidth(122.0f).withHeight(itemHeight).withMargin(itemGap), iconItem(fillStrums, groupGap),
		   juce::FlexItem(arpBox).withWidth(138.0f).withHeight(itemHeight).withMargin(itemGap), iconItem(fillArp, itemGap),
		   juce::FlexItem(fillCount).withWidth(54.0f).withHeight(itemHeight).withMargin(itemGap), iconItem(clearBar, groupGap),
		   juce::FlexItem(keyLabel).withWidth(32.0f).withHeight(itemHeight).withMargin(itemGap),
		   juce::FlexItem(keyBox).withWidth(64.0f).withHeight(itemHeight).withMargin(itemGap),
		   juce::FlexItem(scaleBox).withWidth(114.0f).withHeight(itemHeight).withMargin(groupGap),
		   juce::FlexItem().withFlex(1.0f).withHeight(itemHeight), iconItem(demoButton, itemGap), iconItem(undoButton, itemGap),
		   iconItem(redoButton, noMargin)});

	help.setBounds(area.removeFromBottom(20).reduced(10, 0));
	roll.setBounds(area.reduced(8, 2));
}
