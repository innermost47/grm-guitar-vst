// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// The guitar roll: six lanes, one per string (string 1 on top, as in a tablature), and a lane of strums.
// A note is a fret on a string; it rings until its end or until its string plays again.
#pragma once

#include <functional>
#include <string>
#include <vector>

#include <juce_gui_basics/juce_gui_basics.h>

#include "../core/song.hpp"

class GRMGuitarProcessor;

constexpr int NUM_FRETS_UI = 20;   // frets 0 .. 19, as the engine (grmg::NUM_FRETS)

namespace ui {

const juce::Colour BG {0xff0d1117}, PANEL {0xff161b22}, GRID {0xff21262d}, TEXT {0xff8b949e}, BRIGHT {0xffc9d1d9},
    WAVE {0xff58a6ff}, ACCENT {0xffd29922};
// one colour per string, index 0 = string 6 (low E)
const juce::Colour STRING_COLOURS[6] = {juce::Colour(0xffd29922), juce::Colour(0xffdb6d28), juce::Colour(0xffe5534b),
                                        juce::Colour(0xff57ab5a), juce::Colour(0xff539bf5), juce::Colour(0xffb083f0)};

}  // namespace ui

class RollView : public juce::Component, private juce::Timer, private juce::ScrollBar::Listener {
public:
    explicit RollView(GRMGuitarProcessor& p);
    ~RollView() override;

    // the tools of the toolbar
    int chord = 7;                 // G
    double snap = 0.25;            // beats
    double newLength = 1.0;        // beats, length of a new note or strum
    int currentBar = 0;            // where Fill writes
    std::function<void()> onPlayToggle, onChanged;

    void reload();                 // the processor's song (after a state load)
    const grmg::Song& getSong() const { return song; }
    void edit(const std::function<void(grmg::Song&)>& change);   // one undoable change
    void undo();
    void redo();
    void zoom(double factor, double aroundBeat);

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed(const juce::KeyPress&) override;

private:
    GRMGuitarProcessor& processor;
    grmg::Song song;
    std::vector<std::string> undoStack, redoStack;
    juce::ScrollBar scroll {false};

    double ppb = 72.0;             // pixels per beat
    double viewBeat = 0.0;         // beat at the left edge
    double lastPlayhead = -1.0;
    int stateVersion = 0;
    int brushFret[6] = {0, 0, 0, 0, 0, 0};

    enum class Drag { none, move, resize };
    struct Selection { bool strum = false; int index = -1; } sel;
    Drag drag = Drag::none;
    bool dragChanged = false;
    double downBeat = 0, origStart = 0, origLength = 0;
    int origString = 0;
    std::string beforeDrag;
    juce::String typed;
    juce::uint32 typedAt = 0;

    static constexpr int GUTTER = 70, RULER = 22, STRUM_LANE = 36, SCROLL = 12;
    juce::Rectangle<int> lanesArea() const;
    float laneTop(int row) const;
    float laneHeight() const;
    int rowAt(float y) const;      // 0..5 string rows (row 0 = string 1), 6 = strum lane, -1 ruler
    static int stringOfRow(int row) { return 5 - row; }
    static int rowOfString(int i) { return 5 - i; }
    float beatToX(double beat) const { return (float) (GUTTER + (beat - viewBeat) * ppb); }
    double xToBeat(float x) const { return viewBeat + (x - GUTTER) / ppb; }
    double snapBeat(double b) const { return std::floor(b / snap + 0.5) * snap; }
    double snapDown(double b) const { return std::floor(b / snap + 1e-9) * snap; }

    juce::Rectangle<float> noteRect(const grmg::RollNote& n) const;
    juce::Rectangle<float> strumRect(const grmg::RollStrum& s) const;
    Selection hitTest(juce::Point<float> p, bool& nearRightEdge) const;
    double nextPluck(int string, double after) const;      // when the string is played again
    void showMenu(const Selection& s);
    void changeSelected(const std::function<void(grmg::RollNote&)>& note, const std::function<void(grmg::RollStrum&)>& strum);
    void deleteSelected();
    void pushUndo();
    void commit();
    void updateScroll();
    void scrollBarMoved(juce::ScrollBar*, double newRangeStart) override;
    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RollView)
};
