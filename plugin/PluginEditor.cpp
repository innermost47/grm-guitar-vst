// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
#include "PluginEditor.h"

using namespace grmg;

namespace {
juce::Font font(float size, bool bold = false) {
    return juce::Font(juce::FontOptions(size, bold ? juce::Font::bold : juce::Font::plain));
}
const char* NOTE_NAMES[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
}  // namespace

// ================================================================== neck
float NeckView::fretX(int k) const {
    // nut at 40 px, bridge at the right: the frets as on a real neck (12th fret halfway), up to fret 19
    const float x0 = 40.0f, x1 = (float) getWidth() - 20.0f;
    const float scale = (x1 - x0) / (1.0f - std::pow(2.0f, -19.5f / 12.0f));
    return x0 + scale * (1.0f - std::pow(2.0f, -(float) k / 12.0f));
}

void NeckView::paint(juce::Graphics& g) {
    using namespace ui;
    g.fillAll(PANEL);
    const float h = (float) getHeight(), top = 14.0f, bottom = h - 18.0f;
    g.setColour(juce::Colour(0xff2b2116));
    g.fillRect(40.0f, top - 6, (float) getWidth() - 60.0f, bottom - top + 12);
    g.setFont(font(10.0f));
    for (int k = 1; k < 20; ++k) {
        const float x = fretX(k);
        g.setColour(juce::Colour(0xff9c9c9c));
        g.drawLine(x, top - 6, x, bottom + 6, 1.5f);
        if (k == 3 || k == 5 || k == 7 || k == 9 || k == 12 || k == 15 || k == 17) {
            g.setColour(TEXT);
            g.drawText(juce::String(k), (int) ((fretX(k - 1) + x) / 2 - 10), (int) (h - 16), 20, 14, juce::Justification::centred);
            g.setColour(juce::Colour(0xffe6dccd).withAlpha(0.5f));
            const float cx = (fretX(k - 1) + x) / 2, cy = (top + bottom) / 2;
            if (k == 12) {
                g.fillEllipse(cx - 3, cy - (bottom - top) * 0.25f - 3, 6, 6);
                g.fillEllipse(cx - 3, cy + (bottom - top) * 0.25f - 3, 6, 6);
            } else {
                g.fillEllipse(cx - 3, cy - 3, 6, 6);
            }
        }
    }
    g.setColour(juce::Colour(0xffe6dccd));
    g.fillRect(36.0f, top - 6, 5.0f, bottom - top + 12);
    const float gap = (bottom - top) / 5.0f;
    for (int row = 0; row < 6; ++row) {
        const int i = 5 - row;                      // string 1 on top
        const float y = top + row * gap;
        const float level = processor.getShownLevel(i);
        const int fret = processor.getShownFret(i);
        const float amp = juce::jlimit(0.0f, gap * 0.4f, 4.0f * std::log10(1.0f + 30.0f * level));
        const float xs = fret > 0 ? fretX(fret) - 4.0f : 41.0f;
        juce::Path p;
        p.startNewSubPath(4.0f, y);
        p.lineTo(xs, y);
        const float x1 = (float) getWidth() - 20.0f;
        for (float x = xs; x <= x1; x += 3.0f) {
            const float u = (x - xs) / (x1 - xs);
            p.lineTo(x, y + amp * std::sin(juce::MathConstants<float>::pi * u) * (float) std::sin(phase + i * 1.3 + u * 2));
        }
        g.setColour(STRING_COLOURS[i].withAlpha(0.6f + 0.4f * juce::jlimit(0.0f, 1.0f, level)));
        g.strokePath(p, juce::PathStrokeType(i < 3 ? 2.2f : 1.4f));
        g.setColour(TEXT);
        g.setFont(font(10.0f, true));
        const int open = processor.getOpenMidi(i);
        g.drawText(NOTE_NAMES[open % 12], 2, (int) (y - 7), 30, 14, juce::Justification::centredLeft);
        if (level > 0.002f && fret > 0) {
            g.setColour(STRING_COLOURS[i]);
            g.fillEllipse((fretX(fret - 1) + fretX(fret)) / 2 - 6, y - 6, 12, 12);
        }
    }
    g.setColour(TEXT);
    g.setFont(font(10.0f));
    g.drawText("click a string to play it", getWidth() - 160, 0, 150, 12, juce::Justification::right);
}

void NeckView::mouseDown(const juce::MouseEvent& e) {
    const float top = 14.0f, bottom = (float) getHeight() - 18.0f, gap = (bottom - top) / 5.0f;
    const int row = juce::jlimit(0, 5, (int) std::lround((e.position.y - top) / gap));
    int fret = 0;
    for (int k = 1; k < 20; ++k)
        if (e.position.x > fretX(k - 1) && e.position.x <= fretX(k)) fret = k;
    processor.pluckFromInterface(5 - row, fret, 0.8f);
}

// ================================================================== oscilloscope
void ScopeView::timerCallback() {
    float tmp[4096];
    const int got = processor.readScope(tmp, 4096);
    for (int k = 0; k < got; ++k) {
        history[write] = tmp[k];
        write = (write + 1) % history.size();
    }
    repaint();
}

void ScopeView::paint(juce::Graphics& g) {
    using namespace ui;
    g.fillAll(PANEL);
    const float w = (float) getWidth(), h = (float) getHeight(), mid = h / 2;
    g.setColour(GRID);
    g.drawHorizontalLine((int) mid, 0, w);
    // a rising zero crossing in the older half: the wave stands still
    const size_t n = history.size(), show = 1024;
    size_t start = (write + n - show - 1024) % n;
    for (size_t k = 0; k < 1024; ++k) {
        const size_t a = (write + n - show - 1024 + k) % n, b = (a + 1) % n;
        if (history[a] <= 0.0f && history[b] > 0.0f) {
            start = b;
            break;
        }
    }
    float peak = 0.05f;
    for (size_t k = 0; k < show; ++k) peak = std::max(peak, std::abs(history[(start + k) % n]));
    juce::Path p;
    for (size_t k = 0; k < show; ++k) {
        const float x = w * (float) k / (float) (show - 1);
        const float y = mid - history[(start + k) % n] / peak * (h * 0.42f);
        if (k == 0) p.startNewSubPath(x, y);
        else p.lineTo(x, y);
    }
    g.setColour(WAVE);
    g.strokePath(p, juce::PathStrokeType(1.3f));
    g.setColour(TEXT);
    g.setFont(font(10.0f));
    g.drawText("output", 6, 2, 60, 12, juce::Justification::left);
}

// ================================================================== parameters
ParamPanel::ParamPanel(juce::AudioProcessorValueTreeState& state, const std::vector<const char*>& ids) {
    for (const char* id : ids) {
        auto* param = state.getParameter(id);
        if (!param) continue;
        Row r;
        r.label = std::make_unique<juce::Label>(juce::String(), param->getName(64));
        r.label->setFont(font(13.0f));
        r.label->setColour(juce::Label::textColourId, ui::BRIGHT);
        addAndMakeVisible(*r.label);
        if (dynamic_cast<juce::AudioParameterBool*>(param)) {
            auto b = std::make_unique<juce::ToggleButton>();
            r.button = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(state, id, *b);
            r.control = std::move(b);
        } else if (auto* c = dynamic_cast<juce::AudioParameterChoice*>(param)) {
            auto box = std::make_unique<juce::ComboBox>();
            box->addItemList(c->choices, 1);
            r.combo = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(state, id, *box);
            r.control = std::move(box);
        } else {
            auto s = std::make_unique<juce::Slider>(juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
            s->setTextBoxStyle(juce::Slider::TextBoxRight, false, 56, 20);
            r.slider = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state, id, *s);
            r.control = std::move(s);
        }
        addAndMakeVisible(*r.control);
        rows.push_back(std::move(r));
    }
}

void ParamPanel::resized() {
    // two columns of rows
    const int cols = getWidth() > 520 ? 2 : 1;
    const int perCol = (int) (rows.size() + (size_t) cols - 1) / cols;
    const int colW = getWidth() / cols, rowH = 26;
    for (size_t k = 0; k < rows.size(); ++k) {
        const int c = (int) k / std::max(perCol, 1), r = (int) k % std::max(perCol, 1);
        auto area = juce::Rectangle<int>(c * colW + 6, 6 + r * (rowH + 2), colW - 12, rowH);
        rows[k].label->setBounds(area.removeFromLeft(std::min(150, area.getWidth() / 2)));
        rows[k].control->setBounds(area.reduced(0, 1));
    }
}

// ================================================================== networks
NetworksView::NetworksView(GRMGuitarProcessor& p) : processor(p) {
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

void NetworksView::timerCallback() {
    const auto m = processor.getShownModel();
    if (!m || m.get() == shown) return;
    shown = m.get();
    juce::String t;
    auto num = [](double v, int d) { return juce::String(v, d); };
    t << "STRINGS (tuning network, then string network at the open string)\n";
    for (int i = 5; i >= 0; --i) {
        const auto& p = m->place[(size_t) i][0];
        t << "  " << (6 - i) << "  " << STRING_NAMES[i] << "  T = " << num(p.T, 2) << " N   f1 = " << num(p.f1, 2)
          << " Hz   B = " << juce::String(p.B, 7) << "   Zs = " << num(p.Zs, 3) << " kg/s\n";
    }
    t << "\nATTACK at the 3rd fret, set force and pluck point:  release height y0, release time tau\n";
    for (int i = 5; i >= 0; --i)
        t << "  " << (6 - i) << "  y0 = " << num(m->y0[(size_t) i][3] * 1000.0, 3) << " mm   tau = "
          << num(m->tau[(size_t) i][3] * 1e6, 1) << " us\n";
    const auto& b = m->body;
    t << "\nBODY:  Helmholtz " << num(b.fH, 1) << " Hz, top " << num(b.fT, 1) << " Hz -> coupled " << num(b.f_lo, 1)
      << " / " << num(b.f_hi, 1) << " Hz;  radiation corner " << num(b.frad, 0) << " Hz;  bridge corner " << num(b.fb, 0)
      << " Hz\n       top modes:";
    for (int k = 2; k < 7; ++k) t << " " << num(b.f[(size_t) k], 0);
    t << " Hz;  modal mass " << num(b.Mm * 1000, 1) << " g;  plate impedance " << num(b.Zp, 1) << " kg/s\n";
    if (m->palm_ready) {
        const auto& p = *m->palm[0][0];
        t << "\nPALM on the low E (open): first modes, frequency ratio and added decay (1/s)\n  ";
        for (size_t k = 0; k < std::min<size_t>(6, p.ratio.size()); ++k)
            t << "n=" << (int) k + 1 << ": " << num(p.ratio[k], 3) << " / " << num(p.sigma[k], 0) << "   ";
        t << "\n";
    }
    if (!m->room_ir[0].empty()) {
        const auto& r = m->room;
        t << "\nROOM: V = " << num(r.V, 1) << " m3, S = " << num(r.S, 1) << " m2, critical distance " << num(r.rc, 2)
          << " m, Schroeder " << num(r.fs, 0) << " Hz\n      RT60:";
        for (int k = 0; k < 6; ++k) t << " " << (int) ROOM_BANDS[k] << " Hz " << num(r.RT[(size_t) k], 2) << " s;";
        t << "\n";
    }
    text.setText(t, false);
}

// ================================================================== editor
GRMGuitarEditor::GRMGuitarEditor(GRMGuitarProcessor& p)
    : AudioProcessorEditor(&p), processor(p), neck(p), scope(p), roll(p) {
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
    auto addTab = [&](const char* name, std::vector<const char*> ids) {
        tabs.addTab(name, PANEL, new ParamPanel(processor.params, ids), true);
    };
    addTab("Strings", {"tuning", "resonance", "brightness", "core", "stiff"});
    addTab("Attack & palm", {"attack", "pluck", "force", "pick", "palm_mm", "palm_intensity", "live_palm"});
    addTab("Body", {"body_size", "top_mm", "coupling", "body_amount", "sympathy"});
    addTab("Room", {"room_on", "room_size", "room_material", "room_distance"});
    addTab("Player", {"human", "swing_cm", "finger_noises", "finger_force"});
    addTab("Playback", {"follow_host", "loop", "tempo", "midi_strings"});
    tabs.addTab("Networks", ui::PANEL, new NetworksView(processor), true);
    tabs.setTabBarDepth(28);
    tabs.setOutline(0);
    addAndMakeVisible(tabs);

    addAndMakeVisible(playButton);
    playButton.onClick = [this] { togglePlay(); };
    addAndMakeVisible(takeButton);
    takeButton.setTooltip("The player network plays the roll again, with new imprecisions");
    takeButton.onClick = [this] { processor.newTake(); };
    addAndMakeVisible(exportButton);
    exportButton.setTooltip("Render the whole roll with the body and the room to a WAV file");
    exportButton.onClick = [this] {
        chooser = std::make_unique<juce::FileChooser>("Export the roll", juce::File::getSpecialLocation(juce::File::userMusicDirectory)
                                                                             .getChildFile("grm_guitar.wav"), "*.wav");
        chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                                 | juce::FileBrowserComponent::warnAboutOverwriting,
                             [this](const juce::FileChooser& fc) {
                                 const auto f = fc.getResult();
                                 if (f != juce::File()) processor.exportSong(f.withFileExtension("wav"));
                             });
    };
    for (auto* l : {&status, &position}) {
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
    for (int c = 0; c < NUM_CHORDS; ++c) chordBox.addItem(CHORD_SHAPES[c].name, c + 1);
    chordBox.setSelectedId(roll.chord + 1, juce::dontSendNotification);
    chordBox.onChange = [this] { roll.chord = chordBox.getSelectedId() - 1; };
    for (int k = 0; k < NUM_STRUM_PATTERNS; ++k) strumBox.addItem(STRUM_PATTERNS[k].name, k + 1);
    strumBox.setSelectedId(2, juce::dontSendNotification);
    for (int k = 0; k < NUM_ARPEGGIO_PATTERNS; ++k) arpBox.addItem(ARPEGGIO_PATTERNS[k].name, k + 1);
    arpBox.setSelectedId(2, juce::dontSendNotification);
    const std::pair<const char*, double> snaps[] = {{"1/4", 1.0}, {"1/8", 0.5}, {"1/8 triplet", 1.0 / 3}, {"1/16", 0.25},
                                                    {"1/16 triplet", 1.0 / 6}, {"1/32", 0.125}};
    for (int k = 0; k < 6; ++k) snapBox.addItem(snaps[k].first, k + 1);
    snapBox.setSelectedId(4, juce::dontSendNotification);
    snapBox.onChange = [this, snaps] { roll.snap = snaps[snapBox.getSelectedId() - 1].second; roll.repaint(); };
    for (int b : {1, 2, 4, 8, 12, 16, 24, 32, 48, 64}) barsBox.addItem(juce::String(b) + " bars", b);
    barsBox.onChange = [this] {
        const int b = barsBox.getSelectedId();
        if (b > 0 && b != roll.getSong().bars) roll.edit([b](Song& s) { s.bars = b; });
    };
    for (int b : {1, 2, 4, 8}) fillCount.addItem("x " + juce::String(b), b);
    fillCount.setSelectedId(1, juce::dontSendNotification);
    fillStrums.onClick = [this] {
        const int bar = roll.currentBar, count = std::max(1, fillCount.getSelectedId()), pattern = strumBox.getSelectedId() - 1;
        const int chord = roll.chord;
        roll.edit([=](Song& s) { s.fill_strums(bar, std::min(count, s.bars - bar), pattern, chord, 2.0); });
    };
    fillArp.onClick = [this] {
        const int bar = roll.currentBar, count = std::max(1, fillCount.getSelectedId()), pattern = arpBox.getSelectedId() - 1;
        const int chord = roll.chord;
        roll.edit([=](Song& s) { s.fill_arpeggio(bar, std::min(count, s.bars - bar), pattern, chord, 2.0); });
    };
    clearBar.onClick = [this] {
        const int bar = roll.currentBar;
        roll.edit([=](Song& s) { s.remove_range(bar * s.beats_per_bar, (bar + 1) * s.beats_per_bar); });
    };
    undoButton.onClick = [this] { roll.undo(); };
    redoButton.onClick = [this] { roll.redo(); };
    demoButton.onClick = [this] { roll.edit([](Song& s) { s = Song::demo(); }); };
    chordLabel.setText("Chord", juce::dontSendNotification);
    snapLabel.setText("Snap", juce::dontSendNotification);
    songLabel.setText("Song", juce::dontSendNotification);
    for (auto* c : std::initializer_list<juce::Component*> {&chordBox, &strumBox, &arpBox, &snapBox, &barsBox, &fillCount,
                                                             &fillStrums, &fillArp, &clearBar, &undoButton, &redoButton,
                                                             &demoButton, &chordLabel, &barLabel, &snapLabel, &songLabel, &help})
        addAndMakeVisible(c);
    for (auto* l : {&chordLabel, &barLabel, &snapLabel, &songLabel, &help}) {
        l->setColour(juce::Label::textColourId, TEXT);
        l->setFont(font(12.0f));
    }
    help.setText("Click a lane: new note (drag: length).  Wheel or digits: fret.  P: palm mute.  Right click: menu.  "
                 "Double click / Del: delete.  Ctrl+wheel: zoom.  Space: play.",
                 juce::dontSendNotification);
    help.setFont(font(11.0f));
    roll.onPlayToggle = [this] { togglePlay(); };
    roll.onChanged = [this] { refreshTools(); };
    addAndMakeVisible(roll);
    refreshTools();

    setResizable(true, true);
    setResizeLimits(1000, 680, 2400, 1600);
    setSize(1280, 820);
    startTimerHz(15);
}

GRMGuitarEditor::~GRMGuitarEditor() { setLookAndFeel(nullptr); }

void GRMGuitarEditor::refreshTools() {
    barsBox.setSelectedId(roll.getSong().bars, juce::dontSendNotification);
    barLabel.setText("Bar " + juce::String(roll.currentBar + 1), juce::dontSendNotification);
}

void GRMGuitarEditor::togglePlay() {
    processor.setInternalPlaying(!processor.isInternalPlaying());
}

void GRMGuitarEditor::timerCallback() {
    status.setText(processor.getStatus(), juce::dontSendNotification);
    const bool host = processor.isHostPlaying();
    playButton.setButtonText(processor.isInternalPlaying() ? "Stop" : "Play");
    playButton.setEnabled(!host);
    exportButton.setEnabled(!processor.isExporting());
    const double b = processor.getShownBeat();
    const auto& s = roll.getSong();
    if (b >= 0) {
        const int bar = (int) (b / s.beats_per_bar);
        const double beat = b - bar * s.beats_per_bar;
        position.setText(juce::String(bar + 1) + "." + juce::String((int) beat + 1) + (host ? "  (host)" : ""),
                         juce::dontSendNotification);
    } else {
        position.setText("-", juce::dontSendNotification);
    }
}

void GRMGuitarEditor::paint(juce::Graphics& g) {
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

void GRMGuitarEditor::resized() {
    auto area = getLocalBounds();
    auto header = area.removeFromTop(38).reduced(8, 6);
    volume.setBounds(header.removeFromRight(190));
    header.removeFromRight(60);
    playButton.setBounds(header.removeFromRight(70));
    header.removeFromRight(6);
    takeButton.setBounds(header.removeFromRight(84));
    header.removeFromRight(6);
    exportButton.setBounds(header.removeFromRight(90));
    header.removeFromRight(10);
    position.setBounds(header.removeFromRight(110));
    header.removeFromRight(10);
    status.setBounds(header.removeFromRight(juce::jmin(260, juce::jmax(0, header.getWidth() - 150))));
    subtitleWidth = juce::jmax(0, header.getWidth() - 150);

    auto top = area.removeFromTop(juce::jmax(260, area.getHeight() * 38 / 100)).reduced(8, 4);
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
    barLabel.setBounds(tools.removeFromLeft(52));
    chordLabel.setBounds(tools.removeFromLeft(42));
    chordBox.setBounds(tools.removeFromLeft(80));
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

    help.setBounds(area.removeFromBottom(20).reduced(10, 0));
    roll.setBounds(area.reduced(8, 2));
}
