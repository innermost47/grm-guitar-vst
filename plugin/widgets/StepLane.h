// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// StepLane.h
//
// One lane of steps under the roll (a Feel lane: force, timing or palm; later the automation lanes of each string):
// a bar per sixteenth, drawn in notches (FEEL_NOTCHES across the lane; Shift: freely), on the roll's own time axis.
// Draw by dragging across it (Shift: between the notches); Ctrl+drag selects a stretch; Ctrl+A / C / X / V / D, Del work on the
// selection as in the roll; Ctrl+Z / Y undo through the roll. It reads and writes through its owner (the roll):
// its values, its time axis, one undoable change per gesture.
#pragma once

#include <functional>
#include <vector>

#include <JuceHeader.h>

#include "../../constants/ui.hpp"

class StepLane : public juce::Component
{
public:
    struct Host
    {
        std::function<const std::vector<double> &()> values; // one per sixteenth from the pattern's start
        std::function<float(double)> beatToX;
        std::function<double(float)> xToBeat;
        // a change of the lane: `gesture` true for the first change of a gesture (the roll keeps one undo for it)
        std::function<void(const std::function<void(std::vector<double> &)> &, bool gesture)> edit;
        std::function<void()> undo, redo;
    };

    // signedLane: -1 .. 1 around a middle line (force, timing); else 0 .. 1 from the bottom (palm)
    StepLane(juce::String name, juce::Colour colour, bool signedLane, int gutter, Host host);

    // the selected stretch (steps [from, to)); false: none
    bool selection(int &from, int &to) const;

    void paint(juce::Graphics &) override;
    void mouseDown(const juce::MouseEvent &) override;
    void mouseDrag(const juce::MouseEvent &) override;
    void mouseUp(const juce::MouseEvent &) override;
    bool keyPressed(const juce::KeyPress &) override;

private:
    juce::String name;
    juce::Colour colour;
    bool signedLane;
    int gutter;
    Host host;

    int selFrom = -1, selTo = -1; // the selection, steps [selFrom, selTo)
    int anchor = -1;              // where a selecting drag started
    int pasteAt = 0;              // the last step clicked: where Ctrl+V pastes
    int lastStep = -1;            // the last step drawn in this gesture
    double lastValue = 0.0;
    bool selecting = false, gesture = false;

    int stepAt(float x) const;
    double valueAt(float y, bool fine) const;
    float yOf(double v) const;
    double neutral() const { return 0.0; }
    void draw(int step, double value);

    // what Ctrl+C took, for any lane of the same kind
    static std::vector<double> &clipboard(bool signedLane);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StepLane)
};
