// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
#include "PluginEditor.h"
#include "ParamPanel.h"
#include "NetworksView.h"
#include "MixerView.h"

using namespace grmg;
using namespace ui;

GRMGuitarEditor::GRMGuitarEditor(GRMGuitarProcessor &p)
	: AudioProcessorEditor(&p), processor(p), neck(p), scope(p), roll(p)
{
	juce::LookAndFeel::setDefaultLookAndFeel(&CustomLookAndFeel::getInstance());

	addAndMakeVisible(neck);
	addAndMakeVisible(scope);
	tabs.setTabBarDepth(28);
	tabs.setOutline(0);
	addAndMakeVisible(tabs);
	addAndMakeVisible(addGuitarButton);
	addGuitarButton.onClick = [this]
	{ processor.addGuitar(); };
	addAndMakeVisible(removeGuitarButton);
	removeGuitarButton.onClick = [this]
	{ processor.removeGuitar(processor.getSelectedGuitar()); };
	addAndMakeVisible(ghostsButton);
	ghostsAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.params, "ghosts", ghostsButton);
	rebuildGuitars();

	addAndMakeVisible(playButton);
	playButton.onClick = [this]
	{ togglePlay(); };
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
	for (auto *l : {&status, &position})
	{
		l->setColour(juce::Label::textColourId, TEXT);
		l->setFont(font(12.0f));
		addAndMakeVisible(*l);
	}
	position.setFont(font(15.0f, true));
	position.setColour(juce::Label::textColourId, BRIGHT);
	volume.setSliderStyle(juce::Slider::LinearHorizontal);
	volume.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
	volumeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.params, "volume", volume);
	addAndMakeVisible(volume);

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
	followAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.params, "follow_playhead", followButton);
	for (auto *c : std::initializer_list<juce::Component *>{&patternLabel, &patternBox, &newPattern, &duplicatePattern,
															&deletePatternButton, &copyPatternButton, &pastePatternButton, &midiLabel, &midiModeBox, &keyHint, &followButton})
		addAndMakeVisible(c);
	for (auto *l : {&patternLabel, &midiLabel, &keyHint})
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
	setResizeLimits(1000, 680, 2400, 1600);
	setSize(1420, 820);
	vblank = std::make_unique<juce::VBlankAttachment>(this, [this]
													  { updateEditor(); });
}

GRMGuitarEditor::~GRMGuitarEditor() { setLookAndFeel(nullptr); }

void GRMGuitarEditor::refreshTools()
{
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
	const int count = processor.getGuitarCount(), sel = processor.getSelectedGuitar();
	guitarButtons.clear();
	for (int g = 0; g < count; ++g)
	{
		auto b = std::make_unique<juce::TextButton>(processor.getGuitarName(g));
		b->setClickingTogglesState(false);
		b->setToggleState(g == sel, juce::dontSendNotification);
		b->setColour(juce::TextButton::buttonOnColourId, WAVE.darker(0.4f));
		b->onClick = [this, g]
		{ processor.selectGuitar(g); };
		addAndMakeVisible(*b);
		guitarButtons.push_back(std::move(b));
	}
	addGuitarButton.setEnabled(count < GRMGuitarProcessor::MAX_GUITARS);
	removeGuitarButton.setEnabled(count > 1);

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
		auto *panel = new ParamPanel(processor.params, full);
		if (setup)
			setup(*panel);
		tabs.addTab(name, PANEL, panel, true);
	};
	addTab("Strings", {"tuning", "string_age", "brightness", "core", "stiff"});
	addTab("Attack & palm", withTunables({"attack", "pluck", "force", "pick", "palm_mm", "palm_intensity", "live_palm"}, VOICE_TUNABLES));
	addTab("Body", {"hollow", "wood", "density_pct", "body_size", "top_mm", "braces_g", "body_q", "coupling", "body_amount", "sympathy"});
	addTab("Sensor",
		   {"sensor", "pickup_mm", "pickup_type", "cable_pf",
			"pickup_bridge_on", "pickup_middle_on", "pickup_neck_on",
			"pickup_bridge_split",
			"pickup_bridge_type", "pickup_middle_type", "pickup_neck_type"},
		   true,
		   [this](ParamPanel &panel)
		   {
			   panel.setSensorId(processor.paramId("sensor"));
			   panel.showOnlyForSensor(processor.paramId("pickup_mm"), {1});
			   panel.showOnlyForSensor(processor.paramId("pickup_type"), {1});
			   panel.showOnlyForSensor(processor.paramId("cable_pf"), {1, 3});
			   panel.showOnlyForSensor(processor.paramId("pickup_bridge_on"), {3});
			   panel.showOnlyForSensor(processor.paramId("pickup_middle_on"), {3});
			   panel.showOnlyForSensor(processor.paramId("pickup_neck_on"), {3});
			   panel.showOnlyForSensor(processor.paramId("pickup_bridge_split"), {3});
			   panel.showOnlyForSensor(processor.paramId("pickup_bridge_type"), {3});
			   panel.showOnlyForSensor(processor.paramId("pickup_middle_type"), {3});
			   panel.showOnlyForSensor(processor.paramId("pickup_neck_type"), {3});
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
			   panel.setSensorId(processor.paramId("sensor"));
			   for (const auto &id : amp)
				   panel.showOnlyForSensor(processor.paramId(id), {1, 3});
		   });
	addTab("Room", {"room_on", "room_size", "room_material", "room_distance"});
	addTab("Player", withTunables({"human", "human_amount", "swing_cm", "finger_noises", "finger_force"}, HAND_TUNABLES));
	tabs.addTab("Mixer", PANEL, new MixerView(processor), true);
	addTab("Playback", {"follow_host", "loop", "tempo", "midi_strings", "midi_route", "audition"}, false);
	tabs.addTab("Networks", PANEL, new NetworksView(processor), true);
	tabs.setCurrentTabIndex(juce::jmin(shownTab, tabs.getNumTabs() - 1), false);
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

void GRMGuitarEditor::updateEditor()
{
	status.setText(processor.getStatus(), juce::dontSendNotification);
	if (processor.getGuitarVersion() != shownGuitarVersion)
		rebuildGuitars();
	refreshPatterns();
	pastePatternButton.setEnabled(processor.hasCopiedPattern() || juce::SystemClipboard::getTextFromClipboard().startsWith("grm-guitar-roll"));
	const bool host = processor.isHostPlaying();
	playButton.setToggleState(processor.isInternalPlaying(), juce::dontSendNotification);
	playButton.setEnabled(!host);
	exportButton.setEnabled(!processor.isExporting());
	const double b = processor.getShownBeat();
	const auto &s = roll.getSong();
	const int playing = processor.getPlayingPattern();
	if (b >= 0 && playing >= 0)
	{
		const int bar = (int)(b / s.beats_per_bar);
		const double beat = b - bar * s.beats_per_bar;
		const juce::String where = playing == processor.getEditingPattern() ? juce::String(bar + 1) + "." + juce::String((int)beat + 1)
																			: juce::String("P") + juce::String(playing + 1);

		juce::String song;
		const double g = processor.getSongBeat();
		if (g >= 0)
			song = "   song " + juce::String((int)(g / s.beats_per_bar) + 1) + "." + juce::String((int)std::fmod(g, s.beats_per_bar) + 1);
		position.setText(where + song + (host ? "  (host)" : ""), juce::dontSendNotification);
	}
	else
	{
		position.setText("-", juce::dontSendNotification);
	}
}

void GRMGuitarEditor::paint(juce::Graphics &g)
{
	using namespace ui;
	g.fillAll(BG);
	g.setColour(BRIGHT);
	g.setFont(font(19.0f, true));
	g.drawText("GRM GUITAR", 12, 6, 160, 26, juce::Justification::centredLeft);
	g.setColour(TEXT);
	g.setFont(font(11.0f));
	g.drawText("from string to amp and room: all solved by GRM", 110, 6, subtitleWidth, 26,
			   juce::Justification::centredLeft);
	g.drawText("Volume", getWidth() - 256, 8, 52, 22, juce::Justification::centredRight);
}

void GRMGuitarEditor::resized()
{
	auto area = getLocalBounds();
	auto header = area.removeFromTop(38).reduced(8, 6);

	volume.setBounds(header.removeFromRight(190));
	header.removeFromRight(60);

	const bool isStandalone = processor.wrapperType == juce::AudioProcessor::wrapperType_Standalone;
	playButton.setVisible(isStandalone);

	const int iconSize = appTheme.dimensions.iconButton;
	const int iconGap = appTheme.spacing.md;
	auto placeIcon = [iconSize](juce::Component &button, juce::Rectangle<int> slot)
	{ button.setBounds(slot.withSizeKeepingCentre(iconSize, iconSize)); };
	auto iconItem = [iconSize](juce::Component &button, juce::FlexItem::Margin margin)
	{ return juce::FlexItem(button).withWidth((float)iconSize).withHeight((float)iconSize).withMargin(margin); };

	if (isStandalone)
	{
		placeIcon(playButton, header.removeFromRight(iconSize));
		header.removeFromRight(iconGap);
	}
	else
	{
		playButton.setBounds({});
	}

	placeIcon(takeButton, header.removeFromRight(iconSize));
	header.removeFromRight(iconGap);
	placeIcon(exportButton, header.removeFromRight(iconSize));
	header.removeFromRight(appTheme.spacing.xl);
	position.setBounds(header.removeFromRight(200));
	header.removeFromRight(10);
	status.setBounds(header.removeFromRight(juce::jmin(260, juce::jmax(0, header.getWidth() - 150))));
	subtitleWidth = juce::jmax(0, header.getWidth() - 150);

	{
		auto bar = area.removeFromTop(28).reduced(8, 2);
		for (auto &b : guitarButtons)
		{
			b->setBounds(bar.removeFromLeft(84));
			bar.removeFromLeft(4);
		}
		bar.removeFromLeft(iconGap);
		placeIcon(addGuitarButton, bar.removeFromLeft(iconSize));
		bar.removeFromLeft(appTheme.spacing.base);
		placeIcon(removeGuitarButton, bar.removeFromLeft(iconSize));
	}
	auto top = area.removeFromTop(juce::jmax(250, area.getHeight() * 36 / 100)).reduced(8, 4);
	auto right = top.removeFromRight(top.getWidth() * 46 / 100);
	neck.setBounds(right.removeFromTop(right.getHeight() * 64 / 100));
	right.removeFromTop(6);
	scope.setBounds(right);
	top.removeFromRight(8);
	tabs.setBounds(top);

	{
		auto toolsBounds = area.removeFromTop(34).reduced(8, 2);
		juce::FlexBox toolsBox;
		toolsBox.flexDirection = juce::FlexBox::Direction::row;
		toolsBox.alignItems = juce::FlexBox::AlignItems::center;
		toolsBox.justifyContent = juce::FlexBox::JustifyContent::flexStart;

		constexpr float labelWidth = 56.0f;
		constexpr float itemHeight = 24.0f;
		const juce::FlexItem::Margin itemGap(0.0f, 6.0f, 0.0f, 0.0f);
		const juce::FlexItem::Margin groupGap(0.0f, 12.0f, 0.0f, 0.0f);
		const juce::FlexItem::Margin noMargin(0.0f);

		toolsBox.items.add(juce::FlexItem(songLabel).withWidth(labelWidth).withHeight(itemHeight).withMargin(itemGap));
		toolsBox.items.add(juce::FlexItem(barsBox).withWidth(86.0f).withHeight(itemHeight).withMargin(groupGap));

		toolsBox.items.add(juce::FlexItem(snapLabel).withWidth(labelWidth).withHeight(itemHeight).withMargin(itemGap));
		toolsBox.items.add(juce::FlexItem(snapBox).withWidth(82.0f).withHeight(itemHeight).withMargin(groupGap));

		toolsBox.items.add(juce::FlexItem(chordLabel).withWidth(labelWidth).withHeight(itemHeight).withMargin(itemGap));
		toolsBox.items.add(juce::FlexItem(rootBox).withWidth(72.0f).withHeight(itemHeight).withMargin(itemGap));
		toolsBox.items.add(juce::FlexItem(qualityBox).withWidth(76.0f).withHeight(itemHeight).withMargin(itemGap));
		toolsBox.items.add(juce::FlexItem(strumBox).withWidth(122.0f).withHeight(itemHeight).withMargin(itemGap));
		toolsBox.items.add(iconItem(fillStrums, groupGap));

		toolsBox.items.add(juce::FlexItem(arpBox).withWidth(138.0f).withHeight(itemHeight).withMargin(itemGap));
		toolsBox.items.add(iconItem(fillArp, itemGap));
		toolsBox.items.add(juce::FlexItem(fillCount).withWidth(54.0f).withHeight(itemHeight).withMargin(itemGap));
		toolsBox.items.add(iconItem(clearBar, noMargin));

		toolsBox.items.add(juce::FlexItem().withFlex(1.0f).withHeight(itemHeight));
		toolsBox.items.add(iconItem(demoButton, itemGap));
		toolsBox.items.add(iconItem(undoButton, itemGap));
		toolsBox.items.add(iconItem(redoButton, noMargin));

		toolsBox.performLayout(toolsBounds.toFloat());
	}

	{
		auto rowBounds = area.removeFromTop(32).reduced(8, 2);
		juce::FlexBox rowBox;
		rowBox.flexDirection = juce::FlexBox::Direction::row;
		rowBox.alignItems = juce::FlexBox::AlignItems::center;
		rowBox.justifyContent = juce::FlexBox::JustifyContent::flexStart;

		constexpr float labelWidth = 56.0f;
		constexpr float itemHeight = 24.0f;
		const juce::FlexItem::Margin itemGap(0.0f, 6.0f, 0.0f, 0.0f);
		const juce::FlexItem::Margin groupGap(0.0f, 12.0f, 0.0f, 0.0f);
		const juce::FlexItem::Margin noMargin(0.0f);

		rowBox.items.add(juce::FlexItem(patternLabel).withWidth(labelWidth).withHeight(itemHeight).withMargin(itemGap));
		rowBox.items.add(juce::FlexItem(patternBox).withWidth(144.0f).withHeight(itemHeight).withMargin(itemGap));
		rowBox.items.add(iconItem(newPattern, itemGap));
		rowBox.items.add(iconItem(duplicatePattern, itemGap));
		rowBox.items.add(iconItem(deletePatternButton, itemGap));
		rowBox.items.add(iconItem(copyPatternButton, itemGap));
		rowBox.items.add(iconItem(pastePatternButton, groupGap));

		rowBox.items.add(juce::FlexItem(keyLabel).withWidth(labelWidth).withHeight(itemHeight).withMargin(itemGap));
		rowBox.items.add(juce::FlexItem(keyBox).withWidth(72.0f).withHeight(itemHeight).withMargin(itemGap));
		rowBox.items.add(juce::FlexItem(scaleBox).withWidth(114.0f).withHeight(itemHeight).withMargin(groupGap));

		rowBox.items.add(juce::FlexItem(midiLabel).withWidth(labelWidth).withHeight(itemHeight).withMargin(itemGap));
		rowBox.items.add(juce::FlexItem(midiModeBox).withWidth(114.0f).withHeight(itemHeight).withMargin(groupGap));

		rowBox.items.add(juce::FlexItem(keyHint).withFlex(1.0f).withMinWidth(80.0f).withHeight(itemHeight).withMargin(itemGap));
		rowBox.items.add(iconItem(ghostsButton, itemGap));
		rowBox.items.add(iconItem(followButton, noMargin));

		rowBox.performLayout(rowBounds.toFloat());
	}

	help.setBounds(area.removeFromBottom(20).reduced(10, 0));
	roll.setBounds(area.reduced(8, 2));
}
