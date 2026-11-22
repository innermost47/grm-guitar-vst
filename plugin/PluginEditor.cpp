// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
#include "PluginEditor.h"

using namespace grmg;

namespace
{
	juce::Font font(float size, bool bold = false)
	{
		return juce::Font(juce::FontOptions(size, bold ? juce::Font::bold : juce::Font::plain));
	}
	const char *NOTE_NAMES[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
} // namespace

// ================================================================== neck
float NeckView::fretX(int k) const
{
	// nut at 40 px, bridge at the right: the frets as on a real neck (12th fret halfway), up to fret 19
	const float x0 = 40.0f, x1 = (float)getWidth() - 20.0f;
	const float scale = (x1 - x0) / (1.0f - std::pow(2.0f, -19.5f / 12.0f));
	return x0 + scale * (1.0f - std::pow(2.0f, -(float)k / 12.0f));
}

void NeckView::paint(juce::Graphics &g)
{
	using namespace ui;
	g.fillAll(PANEL);
	const float h = (float)getHeight(), top = 14.0f, bottom = h - 18.0f;
	g.setColour(juce::Colour(0xff2b2116));
	g.fillRect(40.0f, top - 6, (float)getWidth() - 60.0f, bottom - top + 12);
	g.setFont(font(10.0f));
	for (int k = 1; k < 20; ++k)
	{
		const float x = fretX(k);
		g.setColour(juce::Colour(0xff9c9c9c));
		g.drawLine(x, top - 6, x, bottom + 6, 1.5f);
		if (k == 3 || k == 5 || k == 7 || k == 9 || k == 12 || k == 15 || k == 17)
		{
			g.setColour(TEXT);
			g.drawText(juce::String(k), (int)((fretX(k - 1) + x) / 2 - 10), (int)(h - 16), 20, 14, juce::Justification::centred);
			g.setColour(juce::Colour(0xffe6dccd).withAlpha(0.5f));
			const float cx = (fretX(k - 1) + x) / 2, cy = (top + bottom) / 2;
			if (k == 12)
			{
				g.fillEllipse(cx - 3, cy - (bottom - top) * 0.25f - 3, 6, 6);
				g.fillEllipse(cx - 3, cy + (bottom - top) * 0.25f - 3, 6, 6);
			}
			else
			{
				g.fillEllipse(cx - 3, cy - 3, 6, 6);
			}
		}
	}
	g.setColour(juce::Colour(0xffe6dccd));
	g.fillRect(36.0f, top - 6, 5.0f, bottom - top + 12);
	const float gap = (bottom - top) / 5.0f;
	for (int row = 0; row < 6; ++row)
	{
		const int i = 5 - row; // string 1 on top
		const float y = top + row * gap;
		const float level = processor.getShownLevel(i);
		const int fret = processor.getShownFret(i);
		const float amp = juce::jlimit(0.0f, gap * 0.4f, 4.0f * std::log10(1.0f + 30.0f * level));
		const float xs = fret > 0 ? fretX(fret) - 4.0f : 41.0f;
		juce::Path p;
		p.startNewSubPath(4.0f, y);
		p.lineTo(xs, y);
		const float x1 = (float)getWidth() - 20.0f;
		for (float x = xs; x <= x1; x += 3.0f)
		{
			const float u = (x - xs) / (x1 - xs);
			p.lineTo(x, y + amp * std::sin(juce::MathConstants<float>::pi * u) * (float)std::sin(phase + i * 1.3 + u * 2));
		}
		g.setColour(STRING_COLOURS[i].withAlpha(0.6f + 0.4f * juce::jlimit(0.0f, 1.0f, level)));
		g.strokePath(p, juce::PathStrokeType(i < 3 ? 2.2f : 1.4f));
		g.setColour(TEXT);
		g.setFont(font(10.0f, true));
		const int open = processor.getOpenMidi(i);
		g.drawText(NOTE_NAMES[open % 12], 2, (int)(y - 7), 30, 14, juce::Justification::centredLeft);
		if (level > 0.002f && fret > 0)
		{
			g.setColour(STRING_COLOURS[i]);
			g.fillEllipse((fretX(fret - 1) + fretX(fret)) / 2 - 6, y - 6, 12, 12);
		}
	}
	g.setColour(TEXT);
	g.setFont(font(10.0f));
	g.drawText("click a string to play it", getWidth() - 160, 0, 150, 12, juce::Justification::right);
}

void NeckView::mouseDown(const juce::MouseEvent &e)
{
	const float top = 14.0f, bottom = (float)getHeight() - 18.0f, gap = (bottom - top) / 5.0f;
	const int row = juce::jlimit(0, 5, (int)std::lround((e.position.y - top) / gap));
	int fret = 0;
	for (int k = 1; k < 20; ++k)
		if (e.position.x > fretX(k - 1) && e.position.x <= fretX(k))
			fret = k;
	processor.pluckFromInterface(5 - row, fret, 0.8f);
}

// ================================================================== oscilloscope
void ScopeView::timerCallback()
{
	float tmp[4096];
	const int got = processor.readScope(tmp, 4096);
	for (int k = 0; k < got; ++k)
	{
		history[write] = tmp[k];
		write = (write + 1) % history.size();
	}
	repaint();
}

void ScopeView::paint(juce::Graphics &g)
{
	using namespace ui;
	g.fillAll(PANEL);
	const float w = (float)getWidth(), h = (float)getHeight(), mid = h / 2;
	g.setColour(GRID);
	g.drawHorizontalLine((int)mid, 0, w);
	// a rising zero crossing in the older half: the wave stands still
	const size_t n = history.size(), show = 1024;
	size_t start = (write + n - show - 1024) % n;
	for (size_t k = 0; k < 1024; ++k)
	{
		const size_t a = (write + n - show - 1024 + k) % n, b = (a + 1) % n;
		if (history[a] <= 0.0f && history[b] > 0.0f)
		{
			start = b;
			break;
		}
	}
	float peak = 0.05f;
	for (size_t k = 0; k < show; ++k)
		peak = std::max(peak, std::abs(history[(start + k) % n]));
	juce::Path p;
	for (size_t k = 0; k < show; ++k)
	{
		const float x = w * (float)k / (float)(show - 1);
		const float y = mid - history[(start + k) % n] / peak * (h * 0.42f);
		if (k == 0)
			p.startNewSubPath(x, y);
		else
			p.lineTo(x, y);
	}
	g.setColour(WAVE);
	g.strokePath(p, juce::PathStrokeType(1.3f));
	g.setColour(TEXT);
	g.setFont(font(10.0f));
	g.drawText("output", 6, 2, 60, 12, juce::Justification::left);
}

// ================================================================== parameters
ParamPanel::ParamPanel(juce::AudioProcessorValueTreeState &s, const std::vector<juce::String> &ids)
	: state(s)
{
	for (const auto &id : ids)
	{
		auto *param = state.getParameter(id);
		if (!param)
			continue;
		Row r;
		r.id = id;
		juce::String name = param->getName(64);
		if (name.startsWithChar('G') && name.indexOfChar(' ') > 1 && name.substring(1, name.indexOfChar(' ')).containsOnly("0123456789"))
			name = name.fromFirstOccurrenceOf(" ", false, false);
		r.label = std::make_unique<juce::Label>(juce::String(), name);
		r.label->setFont(font(13.0f));
		r.label->setColour(juce::Label::textColourId, ui::BRIGHT);
		addAndMakeVisible(*r.label);
		if (dynamic_cast<juce::AudioParameterBool *>(param))
		{
			auto b = std::make_unique<juce::ToggleButton>();
			r.button = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(state, id, *b);
			r.control = std::move(b);
		}
		else if (auto *c = dynamic_cast<juce::AudioParameterChoice *>(param))
		{
			auto box = std::make_unique<juce::ComboBox>();
			box->addItemList(c->choices, 1);
			r.combo = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(state, id, *box);
			r.control = std::move(box);
		}
		else
		{
			auto sl = std::make_unique<juce::Slider>(juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
			sl->setTextBoxStyle(juce::Slider::TextBoxRight, false, 56, 20);
			r.slider = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state, id, *sl);
			r.control = std::move(sl);
		}
		addAndMakeVisible(*r.control);
		rows.push_back(std::move(r));
	}

	warningLabel.setText("Please go to the Sensor tab and choose a magnetic pickup to access amplifiers and feedback.", juce::dontSendNotification);
	warningLabel.setFont(font(14.0f, true));
	warningLabel.setColour(juce::Label::textColourId, ui::TEXT);
	warningLabel.setJustificationType(juce::Justification::centred);
	warningLabel.setVisible(false);
	addAndMakeVisible(warningLabel);

	startTimerHz(4);
}

void ParamPanel::resized()
{
	std::vector<Row *> visible;
	visible.reserve(rows.size());
	for (auto &r : rows)
		if (r.control->isVisible())
			visible.push_back(&r);

	if (visible.empty())
	{
		warningLabel.setBounds(getLocalBounds().reduced(40));
		warningLabel.setVisible(true);
		return;
	}
	else
	{
		warningLabel.setVisible(false);
	}

	const int cols = getWidth() > 520 ? 2 : 1;
	const int perCol = (int)(visible.size() + (size_t)cols - 1) / cols;
	const int colW = getWidth() / cols, rowH = 26;
	for (size_t k = 0; k < visible.size(); ++k)
	{
		const int c = (int)k / std::max(perCol, 1), r = (int)k % std::max(perCol, 1);
		auto area = juce::Rectangle<int>(c * colW + 6, 6 + r * (rowH + 2), colW - 12, rowH);
		visible[k]->label->setBounds(area.removeFromLeft(std::min(150, area.getWidth() / 2)));
		visible[k]->control->setBounds(area.reduced(0, 1));
	}
}

void ParamPanel::setSensorId(const juce::String &id)
{
	sensorId = id;
	lastSensor = -1;
	updateForSensor();
}

void ParamPanel::showOnlyForSensor(const juce::String &id, std::initializer_list<int> sensors)
{
	int mask = 0;
	for (int s : sensors)
		mask |= (1 << s);
	for (auto &r : rows)
		if (r.id == id)
			r.sensorMask = mask;
	updateForSensor();
}

void ParamPanel::timerCallback()
{
	updateForSensor();
}

void ParamPanel::updateForSensor()
{
	if (sensorId.isEmpty())
		return;
	auto *v = state.getRawParameterValue(sensorId);
	if (!v)
		return;
	const int sensor = juce::jlimit(0, 3, (int)std::lround(v->load()));
	bool anyChanged = false;
	for (auto &r : rows)
	{
		const bool visible = (r.sensorMask == 0) || (((r.sensorMask >> sensor) & 1) != 0);
		if (r.label->isVisible() != visible)
			anyChanged = true;
		r.label->setVisible(visible);
		r.control->setVisible(visible);
	}
	lastSensor = sensor;
	if (anyChanged)
		resized();
}

// ================================================================== networks
NetworksView::NetworksView(GRMGuitarProcessor &p) : processor(p)
{
	text.setMultiLine(true);
	text.setReadOnly(true);
	text.setScrollbarsShown(true);
	text.setFont(juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 12.0f, juce::Font::plain)));
	text.setColour(juce::TextEditor::backgroundColourId, ui::PANEL);
	text.setColour(juce::TextEditor::textColourId, ui::BRIGHT);
	text.setColour(juce::TextEditor::outlineColourId, ui::PANEL);
	addAndMakeVisible(text);
	startTimerHz(2);
}

void NetworksView::timerCallback()
{
	const auto m = processor.getShownModel();
	if (!m || m.get() == shown)
		return;
	shown = m.get();
	juce::String t;
	auto num = [](double v, int d)
	{ return juce::String(v, d); };
	t << "STRINGS (tuning network, then string network at the open string)\n";
	for (int i = 5; i >= 0; --i)
	{
		const auto &p = m->place[(size_t)i][0];
		t << "  " << (6 - i) << "  " << STRING_NAMES[i] << "  T = " << num(p.T, 2) << " N   f1 = " << num(p.f1, 2)
		  << " Hz   B = " << juce::String(p.B, 7) << "   Zs = " << num(p.Zs, 3) << " kg/s\n";
	}
	t << "\nATTACK at the 3rd fret, set force and pluck point:  release height y0, release time tau\n";
	for (int i = 5; i >= 0; --i)
		t << "  " << (6 - i) << "  y0 = " << num(m->y0[(size_t)i][3] * 1000.0, 3) << " mm   tau = "
		  << num(m->tau[(size_t)i][3] * 1e6, 1) << " us\n";
	const auto &b = m->body;
	t << "\nBODY:  Helmholtz " << num(b.fH, 1) << " Hz, top " << num(b.fT, 1) << " Hz -> coupled " << num(b.f_lo, 1)
	  << " / " << num(b.f_hi, 1) << " Hz;  radiation corner " << num(b.frad, 0) << " Hz;  bridge corner " << num(b.fb, 0)
	  << " Hz\n       top modes:";
	for (int k = 2; k < 7; ++k)
		t << " " << num(b.f[(size_t)k], 0);
	t << " Hz;  modal mass " << num(b.Mm * 1000, 1) << " g;  plate impedance " << num(b.Zp, 1) << " kg/s\n";
	if (m->palm_ready)
	{
		const auto &p = *m->palm[0][0];
		t << "\nPALM on the low E (open): first modes, frequency ratio and added decay (1/s)\n  ";
		for (size_t k = 0; k < std::min<size_t>(6, p.ratio.size()); ++k)
			t << "n=" << (int)k + 1 << ": " << num(p.ratio[k], 3) << " / " << num(p.sigma[k], 0) << "   ";
		t << "\n";
	}
	if (!m->room_ir[0].empty())
	{
		const auto &r = m->room;
		t << "\nROOM: V = " << num(r.V, 1) << " m3, S = " << num(r.S, 1) << " m2, critical distance " << num(r.rc, 2)
		  << " m, Schroeder " << num(r.fs, 0) << " Hz\n      RT60:";
		for (int k = 0; k < 6; ++k)
			t << " " << (int)ROOM_BANDS[k] << " Hz " << num(r.RT[(size_t)k], 2) << " s;";
		t << "\n";
	}
	text.setText(t, false);
}

// ================================================================== editor
GRMGuitarEditor::GRMGuitarEditor(GRMGuitarProcessor &p)
	: AudioProcessorEditor(&p), processor(p), neck(p), scope(p), roll(p)
{
	using namespace ui;
	look.setColourScheme(juce::LookAndFeel_V4::getMidnightColourScheme());
	look.setColour(juce::ResizableWindow::backgroundColourId, BG);
	look.setColour(juce::Slider::thumbColourId, WAVE);
	look.setColour(juce::Slider::trackColourId, WAVE.darker(0.6f));
	look.setColour(juce::TextButton::buttonColourId, PANEL.brighter(0.08f));
	look.setColour(juce::ComboBox::backgroundColourId, PANEL.brighter(0.05f));
	setLookAndFeel(&look);

	addAndMakeVisible(neck);
	addAndMakeVisible(scope);
	tabs.setTabBarDepth(28);
	tabs.setOutline(0);
	addAndMakeVisible(tabs);
	addAndMakeVisible(addGuitarButton);
	addGuitarButton.setTooltip("Another guitar in this plugin (up to 8), with its own settings, patterns and mixer strip");
	addGuitarButton.onClick = [this]
	{ processor.addGuitar(); };
	addAndMakeVisible(removeGuitarButton);
	removeGuitarButton.setTooltip("Remove the selected guitar (the ones after it move down)");
	removeGuitarButton.onClick = [this]
	{ processor.removeGuitar(processor.getSelectedGuitar()); };
	addAndMakeVisible(ghostsButton);
	ghostsButton.setTooltip("Show the notes of the other guitars, faint, behind this guitar's roll");
	ghostsButton.setColour(juce::ToggleButton::textColourId, TEXT);
	ghostsAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.params, "ghosts", ghostsButton);
	rebuildGuitars();

	addAndMakeVisible(playButton);
	playButton.onClick = [this]
	{ togglePlay(); };
	addAndMakeVisible(takeButton);
	takeButton.setTooltip("The player network plays the roll again, with new imprecisions");
	takeButton.onClick = [this]
	{ processor.newTake(); };
	addAndMakeVisible(exportButton);
	exportButton.setTooltip("Render the whole roll with the body and the room to a WAV file");
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

	// ---------------------------------------------------------------- tools of the roll
	for (int r = 0; r < 12; ++r)
		rootBox.addItem(grmg::ROOT_LABELS[r], r + 1);
	rootBox.setSelectedId(roll.chordRoot + 1, juce::dontSendNotification);
	rootBox.onChange = [this]
	{ roll.chordRoot = rootBox.getSelectedId() - 1; };
	for (int q = 0; q < (int)grmg::chord_qualities().size(); ++q)
		qualityBox.addItem(grmg::chord_qualities()[(size_t)q].label, q + 1);
	qualityBox.setSelectedId(roll.chordQuality + 1, juce::dontSendNotification);
	qualityBox.onChange = [this]
	{ roll.chordQuality = qualityBox.getSelectedId() - 1; };
	rootBox.setTooltip("The chord of new strums and fills: its root (sharps and flats)");
	qualityBox.setTooltip("The chord's kind. Its shape is chosen by the left-hand network for this guitar's tuning");
	for (int r = 0; r < 12; ++r)
		keyBox.addItem(grmg::ROOT_LABELS[r], r + 1);
	for (int k = 0; k < (int)grmg::scales().size(); ++k)
		scaleBox.addItem(grmg::scales()[(size_t)k].name, k + 1);
	keyBox.setTooltip("The key of this pattern: harmonizing (right click on notes) follows it");
	scaleBox.setTooltip("The scale of this pattern: harmonizing follows it");
	keyBox.onChange = [this]
	{
		const int k = keyBox.getSelectedId() - 1;
		if (k >= 0 && k != roll.getSong().key)
			roll.edit([k](Song &s)
					  { s.key = k; }, false);
	};
	scaleBox.onChange = [this]
	{
		const int k = scaleBox.getSelectedId() - 1;
		if (k >= 0 && k != roll.getSong().scale)
			roll.edit([k](Song &s)
					  { s.scale = k; }, false);
	};
	keyLabel.setText("Key", juce::dontSendNotification);
	for (int k = 0; k < NUM_STRUM_PATTERNS; ++k)
		strumBox.addItem(STRUM_PATTERNS[k].name, k + 1);
	strumBox.setSelectedId(2, juce::dontSendNotification);
	for (int k = 0; k < NUM_ARPEGGIO_PATTERNS; ++k)
		arpBox.addItem(ARPEGGIO_PATTERNS[k].name, k + 1);
	arpBox.setSelectedId(2, juce::dontSendNotification);
	const std::pair<const char *, double> snaps[] = {{"1/4", 1.0}, {"1/8", 0.5}, {"1/8 triplet", 1.0 / 3}, {"1/16", 0.25}, {"1/16 triplet", 1.0 / 6}, {"1/32", 0.125}};
	for (int k = 0; k < 6; ++k)
		snapBox.addItem(snaps[k].first, k + 1);
	snapBox.setSelectedId(4, juce::dontSendNotification);
	snapBox.onChange = [this, snaps]
	{ roll.snap = snaps[snapBox.getSelectedId() - 1].second; roll.repaint(); };
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
	fillCount.setSelectedId(1, juce::dontSendNotification);
	// Fill: over the selection when there is one (any length), else over the bars from the current one. The chords
	// already written there are kept: the pattern gives the rhythm; elsewhere the chord of the boxes.
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
	// ---------------------------------------------------------------- patterns
	patternLabel.setText("Pattern", juce::dontSendNotification);
	midiLabel.setText("MIDI keys", juce::dontSendNotification);
	patternBox.onChange = [this]
	{
		const int k = patternBox.getSelectedId() - 1;
		if (k >= 0)
			processor.selectPattern(k);
	};
	newPattern.setTooltip("A new empty pattern after this one");
	newPattern.onClick = [this]
	{ processor.addPattern(false); };
	duplicatePattern.setTooltip("A copy of this pattern after it");
	duplicatePattern.onClick = [this]
	{ processor.addPattern(true); };
	deletePatternButton.setTooltip("Delete this pattern");
	deletePatternButton.onClick = [this]
	{ processor.deletePattern(); };
	copyPatternButton.setTooltip("Copy this pattern (paste it into any guitar, or another instance)");
	copyPatternButton.onClick = [this]
	{ processor.copyPattern(); };
	pastePatternButton.setTooltip("Paste the copied pattern as a new pattern after this one");
	pastePatternButton.onClick = [this]
	{ processor.pastePattern(); };
	midiModeBox.addItemList({"Play notes", "Play patterns"}, 1);
	midiModeBox.setTooltip("Play notes: a key plays a note on the guitar. Play patterns: a key plays a whole pattern "
						   "(C1 pattern 1, C#1 pattern 2...), looped while the key is held");
	midiModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.params, "midi_mode", midiModeBox);
	followButton.setTooltip("The roll scrolls with the playhead. Off: it stays where you are editing");
	followAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.params, "follow_playhead", followButton);
	for (auto *c : std::initializer_list<juce::Component *>{&patternLabel, &patternBox, &newPattern, &duplicatePattern,
															&deletePatternButton, &copyPatternButton, &pastePatternButton, &midiLabel, &midiModeBox, &keyHint, &followButton})
		addAndMakeVisible(c);
	for (auto *l : {&patternLabel, &midiLabel, &keyHint})
	{
		l->setColour(juce::Label::textColourId, TEXT);
		l->setFont(font(12.0f));
	}
	followButton.setColour(juce::ToggleButton::textColourId, TEXT);
	refreshPatterns();

	roll.onPlayToggle = [this]
	{ togglePlay(); };
	roll.onChanged = [this]
	{ refreshTools(); };
	addAndMakeVisible(roll);
	refreshTools();

	setResizable(true, true);
	setResizeLimits(1000, 680, 2400, 1600);
	setSize(1280, 820);
	startTimerHz(15);
}

GRMGuitarEditor::~GRMGuitarEditor() { setLookAndFeel(nullptr); }

void GRMGuitarEditor::refreshTools()
{
	keyBox.setSelectedId(roll.getSong().key + 1, juce::dontSendNotification);
	scaleBox.setSelectedId(roll.getSong().scale + 1, juce::dontSendNotification);
	const int bars = roll.getSong().bars;
	if (barsBox.indexOfItemId(bars) < 0)
		barsBox.addItem(juce::String(bars) + " bars", bars); // grown by writing
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
	// the settings tabs of the selected guitar (the plugin's own settings stay in Playback)
	const int shownTab = juce::jmax(0, tabs.getCurrentTabIndex());
	tabs.clearTabs();
	auto addTab = [&](const char *name, std::vector<const char *> ids, bool ownGuitar = true,
					  std::function<void(ParamPanel &)> setup = nullptr)
	{
		std::vector<juce::String> full;
		for (auto *id : ids)
			full.push_back(ownGuitar ? processor.paramId(id) : juce::String(id));
		auto *panel = new ParamPanel(processor.params, full);
		if (setup)
			setup(*panel);
		tabs.addTab(name, PANEL, panel, true);
	};
	addTab("Strings", {"tuning", "string_age", "brightness", "core", "stiff"});
	addTab("Attack & palm", {"attack", "pluck", "force", "pick", "palm_mm", "palm_intensity", "live_palm"});
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
			   // sensor 1: the one magnetic pickup
			   panel.showOnlyForSensor(processor.paramId("pickup_mm"), {1});
			   panel.showOnlyForSensor(processor.paramId("pickup_type"), {1});
			   // sensor 1 and sensor 3: the cable is common to both
			   panel.showOnlyForSensor(processor.paramId("cable_pf"), {1, 3});
			   // sensor 3: the three pickups and their switches
			   panel.showOnlyForSensor(processor.paramId("pickup_bridge_on"), {3});
			   panel.showOnlyForSensor(processor.paramId("pickup_middle_on"), {3});
			   panel.showOnlyForSensor(processor.paramId("pickup_neck_on"), {3});
			   panel.showOnlyForSensor(processor.paramId("pickup_bridge_split"), {3});
			   panel.showOnlyForSensor(processor.paramId("pickup_bridge_type"), {3});
			   panel.showOnlyForSensor(processor.paramId("pickup_middle_type"), {3});
			   panel.showOnlyForSensor(processor.paramId("pickup_neck_type"), {3});
		   });
	addTab("Amp & Feedback",
		   {"amp_on", "amp_model", "amp_gain", "amp_master",
			"amp_bass", "amp_middle", "amp_treble", "amp_presence", "amp_tight",
			"feedback_amount", "feedback_distance", "feedback_target"},
		   true,
		   [this](ParamPanel &panel)
		   {
			   panel.setSensorId(processor.paramId("sensor"));
			   for (auto *id : {"amp_on", "amp_model", "amp_gain", "amp_master",
								"amp_bass", "amp_middle", "amp_treble", "amp_presence", "amp_tight",
								"feedback_amount", "feedback_distance", "feedback_target"})
			   {
				   panel.showOnlyForSensor(processor.paramId(id), {1, 3});
			   }
		   });
	addTab("Room", {"room_on", "room_size", "room_material", "room_distance"});
	addTab("Player", {"human", "human_amount", "swing_cm", "finger_noises", "finger_force"});
	tabs.addTab("Mixer", PANEL, new MixerView(processor), true);
	addTab("Playback", {"follow_host", "loop", "tempo", "midi_strings", "midi_route", "audition"}, false);
	tabs.addTab("Networks", PANEL, new NetworksView(processor), true);
	tabs.setCurrentTabIndex(juce::jmin(shownTab, tabs.getNumTabs() - 1), false);
	resized();
}

// ================================================================== mixer
MixerView::MixerView(GRMGuitarProcessor &p) : processor(p)
{
	rebuild();
	startTimerHz(10);
}

void MixerView::timerCallback()
{
	if (processor.getGuitarCount() != shownCount || processor.getSelectedGuitar() != shownSelected)
		rebuild();
}

void MixerView::rebuild()
{
	using Att = juce::AudioProcessorValueTreeState;
	strips.clear();
	shownCount = processor.getGuitarCount();
	shownSelected = processor.getSelectedGuitar();
	for (int g = 0; g < shownCount; ++g)
	{
		Strip s;
		auto id = [g](const char *k)
		{ return GuitarTrack::paramId(g, k); };
		s.name = std::make_unique<juce::TextButton>(processor.getGuitarName(g));
		s.name->setToggleState(g == shownSelected, juce::dontSendNotification);
		s.name->setColour(juce::TextButton::buttonOnColourId, ui::WAVE.darker(0.4f));
		s.name->onClick = [this, g]
		{ processor.selectGuitar(g); };
		s.gain = std::make_unique<juce::Slider>(juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow);
		s.level = std::make_unique<juce::Slider>(juce::Slider::LinearVertical, juce::Slider::TextBoxBelow);
		s.pan = std::make_unique<juce::Slider>(juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow);
		for (auto *sl : {s.gain.get(), s.level.get(), s.pan.get()})
			sl->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 54, 16);
		s.gain->setTooltip("Gain (dB)");
		s.pan->setTooltip("Pan");
		s.level->setTooltip("Level");
		s.mute = std::make_unique<juce::ToggleButton>("M");
		s.solo = std::make_unique<juce::ToggleButton>("S");
		s.out = std::make_unique<juce::ComboBox>();
		s.out->addItemList({"Main", "Own output"}, 1);
		s.out->setTooltip("Main: the plugin's main output. Own output: this guitar's output (enable it in the host)");
		s.gainA = std::make_unique<Att::SliderAttachment>(processor.params, id("mix_gain"), *s.gain);
		s.levelA = std::make_unique<Att::SliderAttachment>(processor.params, id("mix_level"), *s.level);
		s.panA = std::make_unique<Att::SliderAttachment>(processor.params, id("mix_pan"), *s.pan);
		s.muteA = std::make_unique<Att::ButtonAttachment>(processor.params, id("mix_mute"), *s.mute);
		s.soloA = std::make_unique<Att::ButtonAttachment>(processor.params, id("mix_solo"), *s.solo);
		s.outA = std::make_unique<Att::ComboBoxAttachment>(processor.params, id("mix_out"), *s.out);
		for (juce::Component *c : std::initializer_list<juce::Component *>{s.name.get(), s.gain.get(), s.level.get(), s.pan.get(),
																		   s.mute.get(), s.solo.get(), s.out.get()})
			addAndMakeVisible(c);
		strips.push_back(std::move(s));
	}
	resized();
	repaint();
}

void MixerView::paint(juce::Graphics &g)
{
	g.fillAll(ui::PANEL);
	g.setColour(ui::GRID);
	const int w = 96;
	for (size_t k = 1; k < strips.size(); ++k)
		g.drawVerticalLine((int)k * w, 4.0f, (float)getHeight() - 4.0f);
	g.setColour(ui::TEXT);
	g.setFont(juce::Font(juce::FontOptions(10.0f)));
	for (const auto &s : strips)
	{
		g.drawText("gain dB", s.gain->getBounds().withHeight(11).translated(0, -2), juce::Justification::centredLeft);
		g.drawText("pan", s.pan->getBounds().withHeight(11).translated(0, -2), juce::Justification::centredLeft);
		g.drawText("level", s.level->getBounds().withHeight(11).translated(0, -2), juce::Justification::centredRight);
	}
}

void MixerView::resized()
{
	const int w = 96;
	for (size_t k = 0; k < strips.size(); ++k)
	{
		auto a = juce::Rectangle<int>((int)k * w, 0, w, getHeight()).reduced(4);
		auto &s = strips[k];
		s.name->setBounds(a.removeFromTop(22));
		a.removeFromTop(10); // room for the labels
		s.out->setBounds(a.removeFromBottom(22));
		auto ms = a.removeFromBottom(22);
		s.mute->setBounds(ms.removeFromLeft(ms.getWidth() / 2));
		s.solo->setBounds(ms);
		auto knobs = a.removeFromLeft(a.getWidth() / 2);
		s.gain->setBounds(knobs.removeFromTop(knobs.getHeight() / 2));
		s.pan->setBounds(knobs);
		s.level->setBounds(a);
	}
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

void GRMGuitarEditor::timerCallback()
{
	status.setText(processor.getStatus(), juce::dontSendNotification);
	if (processor.getGuitarVersion() != shownGuitarVersion)
		rebuildGuitars();
	refreshPatterns();
	pastePatternButton.setEnabled(processor.hasCopiedPattern() || juce::SystemClipboard::getTextFromClipboard().startsWith("grm-guitar-roll"));
	const bool host = processor.isHostPlaying();
	playButton.setButtonText(processor.isInternalPlaying() ? "Stop" : "Play");
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
		// and where the whole song is (the patterns may have other lengths): bar.beat from its start
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
	g.drawText("every string, attack, palm, finger, body and room: a network solved by the GRM core", 150, 6, subtitleWidth, 26,
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

	if (isStandalone)
	{
		playButton.setBounds(header.removeFromRight(70));
		header.removeFromRight(6);
	}
	else
	{
		playButton.setBounds({});
	}

	takeButton.setBounds(header.removeFromRight(84));
	header.removeFromRight(6);
	exportButton.setBounds(header.removeFromRight(90));
	header.removeFromRight(10);
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
		bar.removeFromLeft(6);
		addGuitarButton.setBounds(bar.removeFromLeft(80));
		bar.removeFromLeft(4);
		removeGuitarButton.setBounds(bar.removeFromLeft(70));
	}
	auto top = area.removeFromTop(juce::jmax(250, area.getHeight() * 36 / 100)).reduced(8, 4);
	auto right = top.removeFromRight(top.getWidth() * 46 / 100);
	neck.setBounds(right.removeFromTop(right.getHeight() * 64 / 100));
	right.removeFromTop(6);
	scope.setBounds(right);
	top.removeFromRight(8);
	tabs.setBounds(top);

	auto tools = area.removeFromTop(32).reduced(8, 3);
	songLabel.setBounds(tools.removeFromLeft(38));
	barsBox.setBounds(tools.removeFromLeft(88));
	tools.removeFromLeft(8);
	snapLabel.setBounds(tools.removeFromLeft(36));
	snapBox.setBounds(tools.removeFromLeft(84));
	tools.removeFromLeft(14);
	chordLabel.setBounds(tools.removeFromLeft(42));
	rootBox.setBounds(tools.removeFromLeft(76));
	qualityBox.setBounds(tools.removeFromLeft(78));
	tools.removeFromLeft(4);
	strumBox.setBounds(tools.removeFromLeft(132));
	fillStrums.setBounds(tools.removeFromLeft(84));
	tools.removeFromLeft(4);
	arpBox.setBounds(tools.removeFromLeft(150));
	fillArp.setBounds(tools.removeFromLeft(96));
	tools.removeFromLeft(4);
	fillCount.setBounds(tools.removeFromLeft(54));
	tools.removeFromLeft(10);
	clearBar.setBounds(tools.removeFromLeft(74));
	tools.removeFromLeft(10);
	redoButton.setBounds(tools.removeFromRight(52));
	undoButton.setBounds(tools.removeFromRight(52));
	tools.removeFromRight(6);
	demoButton.setBounds(tools.removeFromRight(84));

	auto row = area.removeFromTop(30).reduced(8, 3);
	patternLabel.setBounds(row.removeFromLeft(50));
	patternBox.setBounds(row.removeFromLeft(150));
	row.removeFromLeft(4);
	newPattern.setBounds(row.removeFromLeft(48));
	row.removeFromLeft(4);
	duplicatePattern.setBounds(row.removeFromLeft(74));
	row.removeFromLeft(4);
	deletePatternButton.setBounds(row.removeFromLeft(56));
	row.removeFromLeft(4);
	copyPatternButton.setBounds(row.removeFromLeft(48));
	row.removeFromLeft(4);
	pastePatternButton.setBounds(row.removeFromLeft(50));
	row.removeFromLeft(12);
	keyLabel.setBounds(row.removeFromLeft(28));
	keyBox.setBounds(row.removeFromLeft(74));
	scaleBox.setBounds(row.removeFromLeft(120));
	row.removeFromLeft(12);
	midiLabel.setBounds(row.removeFromLeft(66));
	midiModeBox.setBounds(row.removeFromLeft(120));
	row.removeFromLeft(8);
	followButton.setBounds(row.removeFromRight(130));
	ghostsButton.setBounds(row.removeFromRight(120));
	keyHint.setBounds(row);

	help.setBounds(area.removeFromBottom(20).reduced(10, 0));
	roll.setBounds(area.reduced(8, 2));
}
