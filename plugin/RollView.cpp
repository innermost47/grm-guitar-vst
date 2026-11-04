// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
#include "RollView.h"

#include "PluginProcessor.h"

using namespace grmg;

namespace {
const char* STRING_LABELS[6] = {"6  E", "5  A", "4  D", "3  G", "2  B", "1  e"};
juce::Font font(float size, bool bold = false) {
    return juce::Font(juce::FontOptions(size, bold ? juce::Font::bold : juce::Font::plain));
}
}  // namespace

RollView::RollView(GRMGuitarProcessor& p) : processor(p) {
    stateVersion = p.getStateVersion();
    setWantsKeyboardFocus(true);
    addAndMakeVisible(scroll);
    scroll.addListener(this);
    scroll.setAutoHide(false);
    reload();
    startTimerHz(30);
}

RollView::~RollView() { scroll.removeListener(this); }

void RollView::reload() {
    song = processor.getSong();
    for (int i = 0; i < 6; ++i) brushFret[i] = 0;
    sel = {};
    updateScroll();
    repaint();
}

// ------------------------------------------------------------------ geometry
juce::Rectangle<int> RollView::lanesArea() const {
    return getLocalBounds().withTrimmedLeft(GUTTER).withTrimmedTop(RULER).withTrimmedBottom(STRUM_LANE + SCROLL);
}
float RollView::laneHeight() const { return (float) lanesArea().getHeight() / 6.0f; }
float RollView::laneTop(int row) const {
    if (row == 6) return (float) (getHeight() - SCROLL - STRUM_LANE);
    return (float) RULER + row * laneHeight();
}
int RollView::rowAt(float y) const {
    if (y < RULER) return -1;
    if (y >= getHeight() - SCROLL - STRUM_LANE) return 6;
    return juce::jlimit(0, 5, (int) ((y - RULER) / laneHeight()));
}

juce::Rectangle<float> RollView::noteRect(const RollNote& n) const {
    const float y = laneTop(rowOfString(n.string)) + 3.0f, h = laneHeight() - 6.0f;
    const float x0 = beatToX(n.start), x1 = beatToX(n.start + n.length);
    return {x0, y, std::max(6.0f, x1 - x0), h};
}
juce::Rectangle<float> RollView::strumRect(const RollStrum& s) const {
    const float y = laneTop(6) + 4.0f;
    const float x0 = beatToX(s.start), x1 = beatToX(s.start + s.length);
    return {x0, y, std::max(8.0f, x1 - x0), (float) STRUM_LANE - 8.0f};
}

RollView::Selection RollView::hitTest(juce::Point<float> p, bool& nearRightEdge) const {
    nearRightEdge = false;
    for (int k = (int) song.notes.size() - 1; k >= 0; --k) {
        const auto r = noteRect(song.notes[(size_t) k]);
        if (r.contains(p)) {
            nearRightEdge = p.x > r.getRight() - 7.0f;
            return {false, k};
        }
    }
    for (int k = (int) song.strums.size() - 1; k >= 0; --k) {
        const auto r = strumRect(song.strums[(size_t) k]);
        if (r.contains(p)) {
            nearRightEdge = p.x > r.getRight() - 7.0f;
            return {true, k};
        }
    }
    return {};
}

double RollView::nextPluck(int string, double after) const {
    double next = 1e30;
    for (const auto& n : song.notes)
        if (n.string == string && n.start > after + 1e-9) next = std::min(next, n.start);
    for (const auto& s : song.strums)
        if (s.frets[(size_t) string] >= 0 && s.start > after + 1e-9) next = std::min(next, s.start);
    return next;
}

// ------------------------------------------------------------------ drawing
void RollView::paint(juce::Graphics& g) {
    using namespace ui;
    g.fillAll(BG);
    const float lh = laneHeight();
    const int right = getWidth();
    const double len = song.length_beats();
    // lanes
    for (int row = 0; row < 6; ++row) {
        const float y = laneTop(row);
        g.setColour(row % 2 ? PANEL : BG);
        g.fillRect((float) GUTTER, y, (float) (right - GUTTER), lh);
        const int i = stringOfRow(row);
        g.setColour(STRING_COLOURS[i].withAlpha(0.35f));
        g.drawHorizontalLine((int) (y + lh / 2), (float) GUTTER, (float) right);
    }
    g.setColour(PANEL.brighter(0.05f));
    g.fillRect((float) GUTTER, laneTop(6), (float) (right - GUTTER), (float) STRUM_LANE);
    // beyond the end of the song
    const float xEnd = beatToX(len);
    if (xEnd < right) {
        g.setColour(juce::Colours::black.withAlpha(0.45f));
        g.fillRect(xEnd, (float) RULER, (float) right - xEnd, (float) (getHeight() - RULER - SCROLL));
    }
    // the bar Fill writes to
    {
        const float a = beatToX(currentBar * song.beats_per_bar), b = beatToX((currentBar + 1) * song.beats_per_bar);
        g.setColour(WAVE.withAlpha(0.06f));
        g.fillRect(a, (float) RULER, b - a, (float) (getHeight() - RULER - SCROLL));
    }
    // grid: snap, beats, bars
    const double first = std::max(0.0, std::floor(viewBeat / snap) * snap);
    for (double b = first; beatToX(b) < right; b += snap) {
        const float x = beatToX(b);
        if (x < GUTTER) continue;
        const bool bar = std::abs(std::fmod(b, (double) song.beats_per_bar)) < 1e-6;
        const bool beat = std::abs(b - std::round(b)) < 1e-6;
        g.setColour(bar ? TEXT.withAlpha(0.6f) : beat ? GRID.brighter(0.25f) : GRID);
        g.drawVerticalLine((int) x, (float) RULER, (float) (getHeight() - SCROLL));
    }
    // ruler
    g.setColour(PANEL);
    g.fillRect(0, 0, right, RULER);
    g.setFont(font(11.0f));
    for (int bar = std::max(0, (int) (viewBeat / song.beats_per_bar)); beatToX(bar * song.beats_per_bar) < right; ++bar) {
        const float x = beatToX(bar * song.beats_per_bar);
        if (x < GUTTER - 1) continue;
        g.setColour(bar == currentBar ? WAVE : TEXT);
        g.drawText(juce::String(bar + 1), (int) x + 3, 2, 40, RULER - 4, juce::Justification::centredLeft);
        g.drawVerticalLine((int) x, 0.0f, (float) RULER);
    }
    // strums, and the strings they reach (ghosts in the string lanes)
    for (size_t k = 0; k < song.strums.size(); ++k) {
        const auto& s = song.strums[k];
        const auto r = strumRect(s);
        if (r.getRight() < GUTTER || r.getX() > right) continue;
        const bool selected = sel.strum && sel.index == (int) k;
        g.setColour((s.palm ? ACCENT.darker(0.6f) : WAVE.darker(0.9f)).withAlpha(0.35f + 0.6f * (float) s.velocity));
        g.fillRoundedRectangle(r, 4.0f);
        g.setColour(selected ? juce::Colours::white : WAVE.withAlpha(0.8f));
        g.drawRoundedRectangle(r, 4.0f, selected ? 2.0f : 1.0f);
        g.setColour(BRIGHT);
        g.setFont(font(13.0f, true));
        juce::String label = juce::String::fromUTF8(s.up ? "\xe2\x86\x91" : "\xe2\x86\x93");
        if (r.getWidth() > 40) label << " " << juce::String(s.chord);
        if (s.palm && r.getWidth() > 60) label << "  PM";
        else if (s.palm) label = "x";
        g.drawText(label, r.reduced(4, 0).toNearestInt(), juce::Justification::centredLeft, false);
        g.setFont(font(10.0f));
        for (int i = 0; i < 6; ++i) {
            if (s.frets[(size_t) i] < 0) continue;
            const float y = laneTop(rowOfString(i));
            const float end = std::min((float) beatToX(std::min(s.start + s.length, nextPluck(i, s.start))), r.getRight());
            g.setColour(STRING_COLOURS[i].withAlpha(0.18f));
            g.fillRect(r.getX(), y + lh * 0.5f - 2.0f, std::max(2.0f, end - r.getX()), 4.0f);
            if (r.getWidth() >= 18 && (k == 0 || song.strums[k - 1].chord != s.chord)) {   // the shape when it changes
                g.setColour(STRING_COLOURS[i].withAlpha(0.8f));
                g.drawText(juce::String(s.frets[(size_t) i]), (int) r.getX() + 2, (int) (y + 1), 24, 12, juce::Justification::left);
            }
        }
    }
    // notes
    for (size_t k = 0; k < song.notes.size(); ++k) {
        const auto& n = song.notes[k];
        const auto r = noteRect(n);
        if (r.getRight() < GUTTER || r.getX() > right) continue;
        const auto c = STRING_COLOURS[n.string];
        const bool selected = !sel.strum && sel.index == (int) k;
        // the note rings until its end, or until its string is played again
        const float cut = beatToX(std::min(n.start + n.length, nextPluck(n.string, n.start)));
        auto ringing = r.withRight(std::max(r.getX() + 6.0f, cut));
        g.setColour(c.withAlpha(0.25f));
        g.fillRoundedRectangle(r, 4.0f);
        g.setColour(c.withAlpha(0.45f + 0.5f * (float) n.velocity));
        g.fillRoundedRectangle(ringing, 4.0f);
        if (n.palm) {
            g.setColour(juce::Colours::black.withAlpha(0.35f));
            for (float x = r.getX() - r.getHeight(); x < ringing.getRight(); x += 6.0f)
                g.drawLine(std::max(x, r.getX()), r.getBottom() - std::max(0.0f, r.getX() - x),
                           std::min(x + r.getHeight(), ringing.getRight()),
                           r.getY() + std::max(0.0f, x + r.getHeight() - ringing.getRight()), 1.0f);
        }
        g.setColour(selected ? juce::Colours::white : c.brighter(0.4f));
        g.drawRoundedRectangle(r, 4.0f, selected ? 2.0f : 1.0f);
        g.setColour(juce::Colours::black);
        g.setFont(font(juce::jlimit(10.0f, 15.0f, r.getHeight() * 0.6f), true));
        g.drawText(juce::String(n.fret) + (n.palm ? " PM" : ""), r.reduced(4, 0).toNearestInt(), juce::Justification::centredLeft, true);
    }
    // gutter: the strings
    g.setColour(PANEL);
    g.fillRect(0, RULER, GUTTER, getHeight() - RULER);
    g.setFont(font(13.0f, true));
    for (int row = 0; row < 6; ++row) {
        const int i = stringOfRow(row);
        g.setColour(STRING_COLOURS[i]);
        g.drawText(STRING_LABELS[i], 8, (int) laneTop(row), GUTTER - 10, (int) lh, juce::Justification::centredLeft);
    }
    g.setColour(ui::TEXT);
    g.setFont(font(12.0f, true));
    g.drawText("Strums", 8, (int) laneTop(6), GUTTER - 10, STRUM_LANE, juce::Justification::centredLeft);
    // playhead
    const double ph = processor.getShownBeat();
    if (ph >= 0) {
        const float x = beatToX(ph);
        if (x >= GUTTER && x < right) {
            g.setColour(juce::Colours::white.withAlpha(0.85f));
            g.drawVerticalLine((int) x, 0.0f, (float) (getHeight() - SCROLL));
        }
    }
    if (hasKeyboardFocus(false)) {
        g.setColour(WAVE.withAlpha(0.4f));
        g.drawRect(getLocalBounds().withTrimmedBottom(SCROLL), 1);
    }
}

void RollView::resized() {
    scroll.setBounds(GUTTER, getHeight() - SCROLL, getWidth() - GUTTER, SCROLL);
    updateScroll();
}

void RollView::updateScroll() {
    const double visible = (getWidth() - GUTTER) / ppb;
    const double total = std::max(song.length_beats() + 4.0, visible);
    viewBeat = juce::jlimit(0.0, std::max(0.0, total - visible), viewBeat);
    scroll.setRangeLimits(0.0, total, juce::dontSendNotification);
    scroll.setCurrentRange(viewBeat, visible, juce::dontSendNotification);
}

void RollView::scrollBarMoved(juce::ScrollBar*, double start) {
    viewBeat = start;
    repaint();
}

void RollView::zoom(double factor, double around) {
    const float x = beatToX(around);
    ppb = juce::jlimit(12.0, 400.0, ppb * factor);
    viewBeat = around - (x - GUTTER) / ppb;
    updateScroll();
    repaint();
}

void RollView::timerCallback() {
    if (processor.getStateVersion() != stateVersion) {     // the host loaded a session
        stateVersion = processor.getStateVersion();
        undoStack.clear();
        redoStack.clear();
        reload();
        if (onChanged) onChanged();
    }
    const double ph = processor.getShownBeat();
    if (std::abs(ph - lastPlayhead) > 1e-4) {
        // follow the playhead when it leaves the view
        const double visible = (getWidth() - GUTTER) / ppb;
        if (ph >= 0 && drag == Drag::none && (ph < viewBeat || ph > viewBeat + visible * 0.95)) {
            viewBeat = std::max(0.0, ph - visible * 0.05);
            updateScroll();
        }
        lastPlayhead = ph;
        repaint();
    }
}

// ------------------------------------------------------------------ editing
void RollView::pushUndo() {
    undoStack.push_back(song.to_text());
    if (undoStack.size() > 200) undoStack.erase(undoStack.begin());
    redoStack.clear();
}

void RollView::commit() {
    song.sort();
    processor.setSong(song);
    updateScroll();
    repaint();
    if (onChanged) onChanged();
}

void RollView::edit(const std::function<void(Song&)>& change) {
    pushUndo();
    change(song);
    sel = {};
    commit();
}

void RollView::undo() {
    if (undoStack.empty()) return;
    redoStack.push_back(song.to_text());
    song.from_text(undoStack.back());
    undoStack.pop_back();
    sel = {};
    commit();
}

void RollView::redo() {
    if (redoStack.empty()) return;
    undoStack.push_back(song.to_text());
    song.from_text(redoStack.back());
    redoStack.pop_back();
    sel = {};
    commit();
}

void RollView::changeSelected(const std::function<void(RollNote&)>& note, const std::function<void(RollStrum&)>& strum) {
    if (sel.index < 0) return;
    pushUndo();
    if (sel.strum && sel.index < (int) song.strums.size()) {
        if (strum) strum(song.strums[(size_t) sel.index]);
    } else if (!sel.strum && sel.index < (int) song.notes.size()) {
        if (note) {
            note(song.notes[(size_t) sel.index]);
            auto& n = song.notes[(size_t) sel.index];
            n.fret = juce::jlimit(0, NUM_FRETS_UI - 1, n.fret);
            brushFret[n.string] = n.fret;
        }
    }
    // keep the selection on the same object after sorting
    const auto key = sel.strum ? std::make_pair(song.strums[(size_t) sel.index].start, -1)
                               : std::make_pair(song.notes[(size_t) sel.index].start, song.notes[(size_t) sel.index].string);
    song.sort();
    if (sel.strum) {
        for (size_t k = 0; k < song.strums.size(); ++k)
            if (std::abs(song.strums[k].start - key.first) < 1e-9) sel.index = (int) k;
    } else {
        for (size_t k = 0; k < song.notes.size(); ++k)
            if (std::abs(song.notes[k].start - key.first) < 1e-9 && song.notes[k].string == key.second) sel.index = (int) k;
    }
    commit();
}

void RollView::deleteSelected() {
    if (sel.index < 0) return;
    pushUndo();
    if (sel.strum && sel.index < (int) song.strums.size()) song.strums.erase(song.strums.begin() + sel.index);
    else if (!sel.strum && sel.index < (int) song.notes.size()) song.notes.erase(song.notes.begin() + sel.index);
    sel = {};
    commit();
}

void RollView::showMenu(const Selection& s) {
    juce::PopupMenu menu;
    const double velocities[5] = {0.3, 0.5, 0.7, 0.85, 1.0};
    const char* dyn[5] = {"pp", "p", "mf", "f", "ff"};
    juce::PopupMenu vel;
    if (!s.strum) {
        const auto& n = song.notes[(size_t) s.index];
        juce::PopupMenu frets;
        for (int f = 0; f < NUM_FRETS_UI; ++f) frets.addItem(100 + f, juce::String(f), true, n.fret == f);
        menu.addSubMenu("Fret", frets);
        menu.addItem(2, "Palm mute", true, n.palm);
        for (int k = 0; k < 5; ++k) vel.addItem(200 + k, dyn[k], true, std::abs(n.velocity - velocities[k]) < 0.01);
    } else {
        const auto& t = song.strums[(size_t) s.index];
        menu.addItem(3, juce::String::fromUTF8("Down \xe2\x86\x93"), true, !t.up);
        menu.addItem(4, juce::String::fromUTF8("Up \xe2\x86\x91"), true, t.up);
        menu.addItem(2, "Palm mute", true, t.palm);
        juce::PopupMenu chords;
        for (int c = 0; c < NUM_CHORDS; ++c) chords.addItem(300 + c, CHORD_SHAPES[c].name, true, t.chord == CHORD_SHAPES[c].name);
        menu.addSubMenu("Chord", chords);
        for (int k = 0; k < 5; ++k) vel.addItem(200 + k, dyn[k], true, std::abs(t.velocity - velocities[k]) < 0.01);
    }
    menu.addSubMenu("Dynamics", vel);
    menu.addSeparator();
    menu.addItem(1, "Delete");
    sel = s;
    repaint();
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this).withMousePosition(),
                       [this, s, velocities](int id) {
                           if (id == 0 || s.index != sel.index || s.strum != sel.strum) return;
                           if (id == 1) deleteSelected();
                           else if (id == 2) changeSelected([](RollNote& n) { n.palm = !n.palm; }, [](RollStrum& t) { t.palm = !t.palm; });
                           else if (id == 3 || id == 4) changeSelected(nullptr, [id](RollStrum& t) { t.up = id == 4; });
                           else if (id >= 300) changeSelected(nullptr, [id](RollStrum& t) {
                               const auto& c = CHORD_SHAPES[id - 300];
                               for (int i = 0; i < 6; ++i) t.frets[(size_t) i] = c.frets[i];
                               t.chord = c.name;
                           });
                           else if (id >= 200) changeSelected([&](RollNote& n) { n.velocity = velocities[id - 200]; },
                                                              [&](RollStrum& t) { t.velocity = velocities[id - 200]; });
                           else if (id >= 100) changeSelected([id](RollNote& n) { n.fret = id - 100; }, nullptr);
                       });
}

// ------------------------------------------------------------------ mouse and keys
void RollView::mouseDown(const juce::MouseEvent& e) {
    grabKeyboardFocus();
    const auto p = e.position;
    const int row = rowAt(p.y);
    if (p.x < GUTTER) return;
    if (row < 0) {                              // the ruler: choose the bar, move the playhead there
        const double b = std::max(0.0, xToBeat(p.x));
        currentBar = juce::jlimit(0, song.bars - 1, (int) (b / song.beats_per_bar));
        processor.setInternalPosition(snapDown(b));
        repaint();
        if (onChanged) onChanged();
        return;
    }
    bool edge = false;
    const auto hit = hitTest(p, edge);
    if (e.mods.isPopupMenu()) {
        if (hit.index >= 0) showMenu(hit);
        return;
    }
    beforeDrag = song.to_text();
    dragChanged = false;
    downBeat = xToBeat(p.x);
    currentBar = juce::jlimit(0, song.bars - 1, (int) (std::max(0.0, downBeat) / song.beats_per_bar));
    if (hit.index >= 0) {
        sel = hit;
        drag = edge ? Drag::resize : Drag::move;
        if (hit.strum) {
            origStart = song.strums[(size_t) hit.index].start;
            origLength = song.strums[(size_t) hit.index].length;
        } else {
            const auto& n = song.notes[(size_t) hit.index];
            origStart = n.start;
            origLength = n.length;
            origString = n.string;
            brushFret[n.string] = n.fret;
        }
        repaint();
        return;
    }
    // an empty place: a new note on this string, or a new strum; dragging sets its length
    const double start = snapDown(std::max(0.0, downBeat));
    if (start >= song.length_beats()) return;
    if (row == 6) {
        RollStrum s;
        s.start = start;
        s.length = newLength;
        const auto& c = CHORD_SHAPES[chord];
        for (int i = 0; i < 6; ++i) s.frets[(size_t) i] = c.frets[i];
        s.chord = c.name;
        song.strums.push_back(s);
        sel = {true, (int) song.strums.size() - 1};
    } else {
        RollNote n;
        n.string = stringOfRow(row);
        n.fret = brushFret[n.string];
        n.start = start;
        n.length = newLength;
        song.notes.push_back(n);
        sel = {false, (int) song.notes.size() - 1};
        origString = n.string;
    }
    origStart = start;
    origLength = newLength;
    drag = Drag::resize;
    dragChanged = true;
    downBeat = start + newLength;      // dragging from here changes the length
    repaint();
}

void RollView::mouseDrag(const juce::MouseEvent& e) {
    if (drag == Drag::none || sel.index < 0) return;
    const double b = xToBeat(e.position.x);
    const double len = song.length_beats();
    if (sel.strum) {
        auto& s = song.strums[(size_t) sel.index];
        if (drag == Drag::move) s.start = juce::jlimit(0.0, len - snap, snapBeat(origStart + b - downBeat));
        else s.length = std::max(snap, snapBeat(origStart + origLength + b - downBeat) - s.start);
    } else {
        auto& n = song.notes[(size_t) sel.index];
        if (drag == Drag::move) {
            n.start = juce::jlimit(0.0, len - snap, snapBeat(origStart + b - downBeat));
            const int row = rowAt(e.position.y);
            if (row >= 0 && row < 6) n.string = stringOfRow(row);
        } else {
            n.length = std::max(snap, snapBeat(origStart + origLength + b - downBeat) - n.start);
        }
    }
    dragChanged = true;
    repaint();
}

void RollView::mouseUp(const juce::MouseEvent&) {
    if (drag != Drag::none && dragChanged) {
        if (sel.index >= 0) {
            if (sel.strum) newLength = song.strums[(size_t) sel.index].length;
            else newLength = song.notes[(size_t) sel.index].length;
        }
        undoStack.push_back(beforeDrag);
        redoStack.clear();
        // keep the selection through the sort
        Selection keep = sel;
        double start = 0;
        int string = -1;
        if (keep.strum) start = song.strums[(size_t) keep.index].start;
        else {
            start = song.notes[(size_t) keep.index].start;
            string = song.notes[(size_t) keep.index].string;
        }
        song.sort();
        if (keep.strum) {
            for (size_t k = 0; k < song.strums.size(); ++k)
                if (std::abs(song.strums[k].start - start) < 1e-9) sel.index = (int) k;
        } else {
            for (size_t k = 0; k < song.notes.size(); ++k)
                if (std::abs(song.notes[k].start - start) < 1e-9 && song.notes[k].string == string) sel.index = (int) k;
        }
        commit();
    }
    drag = Drag::none;
}

void RollView::mouseMove(const juce::MouseEvent& e) {
    bool edge = false;
    const auto hit = hitTest(e.position, edge);
    setMouseCursor(hit.index >= 0 ? (edge ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::DraggingHandCursor)
                                  : juce::MouseCursor::NormalCursor);
}

void RollView::mouseDoubleClick(const juce::MouseEvent& e) {
    bool edge = false;
    const auto hit = hitTest(e.position, edge);
    if (hit.index >= 0) {
        sel = hit;
        deleteSelected();
    }
}

void RollView::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w) {
    if (e.mods.isCommandDown() || e.mods.isCtrlDown()) {
        zoom(w.deltaY > 0 ? 1.15 : 1.0 / 1.15, xToBeat(e.position.x));
        return;
    }
    bool edge = false;
    const auto hit = hitTest(e.position, edge);
    if (hit.index >= 0 && !e.mods.isShiftDown()) {
        // the wheel on a note changes its fret; on a strum, its chord
        sel = hit;
        const int step = w.deltaY > 0 ? 1 : -1;
        changeSelected([step](RollNote& n) { n.fret += step; },
                       [step](RollStrum& t) {
                           int c = 0;
                           for (int k = 0; k < NUM_CHORDS; ++k)
                               if (t.chord == CHORD_SHAPES[k].name) c = k;
                           c = (c + step + NUM_CHORDS) % NUM_CHORDS;
                           for (int i = 0; i < 6; ++i) t.frets[(size_t) i] = CHORD_SHAPES[c].frets[i];
                           t.chord = CHORD_SHAPES[c].name;
                       });
        return;
    }
    const float d = std::abs(w.deltaX) > std::abs(w.deltaY) ? w.deltaX : w.deltaY;
    viewBeat = std::max(0.0, viewBeat - d * 400.0 / ppb);
    updateScroll();
    repaint();
}

bool RollView::keyPressed(const juce::KeyPress& k) {
    const auto mods = k.getModifiers();
    const int code = k.getKeyCode();
    if (mods.isCommandDown() && (code == 'Z' || code == 'z')) {
        if (mods.isShiftDown()) redo();
        else undo();
        return true;
    }
    if (mods.isCommandDown() && (code == 'Y' || code == 'y')) {
        redo();
        return true;
    }
    if (code == juce::KeyPress::spaceKey) {
        if (onPlayToggle) onPlayToggle();
        return true;
    }
    if (sel.index < 0) return false;
    if (code == juce::KeyPress::deleteKey || code == juce::KeyPress::backspaceKey) {
        deleteSelected();
        return true;
    }
    if (code == juce::KeyPress::upKey || code == juce::KeyPress::downKey) {
        const int step = code == juce::KeyPress::upKey ? 1 : -1;
        changeSelected([step](RollNote& n) { n.fret += step; }, [step](RollStrum& t) { t.up = step > 0; });
        return true;
    }
    if (code == juce::KeyPress::leftKey || code == juce::KeyPress::rightKey) {
        const double step = (code == juce::KeyPress::rightKey ? 1 : -1) * snap;
        const double len = song.length_beats();
        changeSelected([=](RollNote& n) { n.start = juce::jlimit(0.0, len - snap, n.start + step); },
                       [=](RollStrum& t) { t.start = juce::jlimit(0.0, len - snap, t.start + step); });
        return true;
    }
    const juce::juce_wchar ch = k.getTextCharacter();
    if (ch == 'p' || ch == 'P') {
        changeSelected([](RollNote& n) { n.palm = !n.palm; }, [](RollStrum& t) { t.palm = !t.palm; });
        return true;
    }
    if (ch >= '0' && ch <= '9' && !sel.strum) {
        // type the fret: one or two digits
        const auto now = juce::Time::getMillisecondCounter();
        if (now - typedAt > 900) typed.clear();
        typed << juce::String::charToString(ch);
        typedAt = now;
        int f = typed.getIntValue();
        if (f >= NUM_FRETS_UI) {
            typed = juce::String::charToString(ch);
            f = typed.getIntValue();
        }
        changeSelected([f](RollNote& n) { n.fret = f; }, nullptr);
        return true;
    }
    return false;
}
