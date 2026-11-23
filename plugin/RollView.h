// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// The guitar roll: six lanes, one per string (string 1 on top, as in a tablature), and a lane of strums.
// A note is a fret on a string; it rings until its end or until its string plays again.
#pragma once

#include <functional>
#include <map>
#include <string>
#include <vector>

#include <juce_gui_basics/juce_gui_basics.h>

#include "../core/chords.hpp"
#include "../core/playability.hpp"
#include "../core/ringing.hpp"
#include "../core/song.hpp"

class GRMGuitarProcessor;

constexpr int NUM_FRETS_UI = 20; // frets 0 .. 19, as the engine (grmg::NUM_FRETS)

namespace ui
{

	const juce::Colour BG{ 0xff0d1117 }, PANEL{ 0xff161b22 }, GRID{ 0xff21262d }, TEXT{ 0xff8b949e }, BRIGHT{ 0xffc9d1d9 },
		WAVE{ 0xff58a6ff }, ACCENT{ 0xffd29922 };
	// one colour per string, index 0 = string 6 (low E)
	const juce::Colour STRING_COLOURS[6] = { juce::Colour(0xffd29922), juce::Colour(0xffdb6d28), juce::Colour(0xffe5534b),
											juce::Colour(0xff57ab5a), juce::Colour(0xff539bf5), juce::Colour(0xffb083f0) };

} // namespace ui

class RollView : public juce::Component, private juce::ScrollBar::Listener
{
public:
	explicit RollView(GRMGuitarProcessor& p);
	~RollView() override;

	// the tools of the toolbar
	int chordRoot = 7, chordQuality = 0; // the chord of new strums and fills (G major)
	grmg::Chord currentChord() const;    // its shape on this guitar's tuning (chords.hpp: the hand chooses it)
	int tuning() const;
	// the span of the selection (false: nothing selected)
	bool selectionRange(double& from, double& to) const;
	void transposeSelected(int semitones); // notes a fret up or down, strums a semitone (their chord follows)
	void harmonizeSelected(int degrees);   // a note `degrees` scale steps from each selected note, where the hand can
	void copySelectedToGuitar(int guitar); // -1: every other guitar
	double snap = 0.25;                    // beats
	double newLength = 1.0;                // beats, length of a new note or strum
	int currentBar = 0;                    // where Fill writes
	std::function<void()> onPlayToggle, onChanged;

	void reload(); // the processor's song (after a state load)
	const grmg::Song& getSong() const { return song; }
	// one undoable change; fit: the song grows to hold what was written (not when its length is chosen)
	void edit(const std::function<void(grmg::Song&)>& change, bool fit = true);
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
	juce::ScrollBar scroll{ false };

	double ppb = 72.0;     // pixels per beat
	double viewBeat = 0.0; // beat at the left edge
	double lastPlayhead = -1.0;
	int stateVersion = 0, patternVersion = 0, pattern = 0, patternCount = 1;
	// each pattern keeps its own undo history
	std::map<int, std::pair<std::vector<std::string>, std::vector<std::string>>> histories;
	double playhead() const; // the playhead in the pattern shown, -1 if another one plays
	int historyKey() const;  // the guitar and pattern being edited

	// the other guitars' notes, faint behind this one's (to write in harmony with them)
	struct Ghost
	{
		int guitar;
		int tuning;
		grmg::Song song;
	};
	std::vector<Ghost> ghosts;
	long ghostKey = -1;
	void refreshGhosts();

	// where the hands cannot reach: the places a note of the brush's fret would be refused (computed for the
	// visible part, again when the song, the brush or the view change)
	std::vector<juce::Rectangle<float>> blocked;
	std::string blockedKey;
	void refreshBlocked();
	double fittedLength(int string, double start) const;
	grmg::Ringing ringCache;
	std::string ringKey; // a new note: no longer than the room before the next one
	int brushFret[6] = { 0, 0, 0, 0, 0, 0 };

	enum class Drag
	{
		none,
		move,
		resize,
		resizeLeft,
		stack,
		rect,
		loopStart,
		loopEnd
	};
	struct Selection
	{
		bool strum = false;
		int index = -1;
	} sel;
	Drag drag = Drag::none;
	bool dragChanged = false;
	double downBeat = 0, origStart = 0, origLength = 0;
	int origString = 0;
	std::string beforeDrag;
	grmg::Song dragBase; // the song when the drag started (every selected object moves from there)
	bool copyDrag = false, copied = false, toggleOnUp = false;
	juce::Point<float> rectFrom, rectTo;
	double pasteBeat = 0.0; // where Ctrl+V pastes: the last place clicked
	juce::String typed;
	juce::uint32 typedAt = 0;

	static constexpr int GUTTER = 70, RULER = 22, STRUM_LANE = 36, SCROLL = 12;
	juce::Rectangle<int> lanesArea() const;
	float laneTop(int row) const;
	float laneHeight() const;
	int rowAt(float y) const; // 0..5 string rows (row 0 = string 1), 6 = strum lane, -1 ruler
	static int stringOfRow(int row) { return 5 - row; }
	static int rowOfString(int i) { return 5 - i; }
	float beatToX(double beat) const { return (float)(GUTTER + (beat - viewBeat) * ppb); }
	double xToBeat(float x) const { return viewBeat + (x - GUTTER) / ppb; }
	double snapBeat(double b) const { return std::floor(b / snap + 0.5) * snap; }
	double snapDown(double b) const { return std::floor(b / snap + 1e-9) * snap; }

	juce::Rectangle<float> noteRect(const grmg::RollNote& n) const;
	juce::Rectangle<float> strumRect(const grmg::RollStrum& s) const;
	Selection hitTest(juce::Point<float> p, bool& nearRightEdge) const;
	mutable bool hitLeftEdge = false;                 // the last hitTest was on an object's left edge
	mutable int hitStackEdge = 0;                     // ... on a note's top (+1) or bottom (-1) edge: drag it over more strings
	void toggleStop(double beat);                     // a hand stop there (or none if there was one)
	double stopBeat(float x) const;                   // the beat a hand stop goes to: on the grid's steps and between them
	double stopAt = 0.0;                              // where the last right click was (the menu's hand stop)
	double nextPluck(int string, double after) const; // when the string is played again
	void showMenu(const Selection& s);
	void changeSelected(const std::function<void(grmg::RollNote&)>& note, const std::function<void(grmg::RollStrum&)>& strum);
	void deleteSelected();
	// the selection lives in the song's objects (their `selected` flag), so it follows them through sorts
	bool anySelected() const;
	void clearSelection();
	void selectOnly(const Selection& s);
	bool isSelected(const Selection& s) const;
	void setSelected(const Selection& s, bool on);
	void primaryFromFlags(); // `sel` = the first selected object (menus, the fret typed)
	void selectAll();
	std::string selectionText() const; // the selected objects, from their earliest start, as a song text
	void copySelection();
	void paste(double at);
	void duplicateSelection();
	void showEmptyMenu();
	void pushUndo();
	void commit(bool checkPlayable = true, bool fit = true); // checkPlayable: refuse a change that adds an impossible moment
	int loopFlagAt(juce::Point<float> p) const;              // 0 start flag, 1 end flag, -1 none (in the ruler)
	void findProblems();

	// what one guitarist cannot play (the hands' constraints, core/playability.hpp)
	std::vector<grmg::Problem> problems;
	bool problemsPick = true;
	juce::String refusal; // why the last change was refused (shown for a few seconds)
	juce::uint32 refusalAt = 0;
	void updateScroll();
	void scrollBarMoved(juce::ScrollBar*, double newRangeStart) override;
	void updateView();

	std::unique_ptr<juce::VBlankAttachment> vblank;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RollView)
};
