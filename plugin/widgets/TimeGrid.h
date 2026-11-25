// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// TimeGrid.h
//
// The vertical lines of the roll's time axis, shared by the roll and the lanes under it: a line every step, brighter
// on the beats, brightest on the bars.
#pragma once

#include <functional>

#include <JuceHeader.h>

class TimeGrid
{
public:
    // step: the beats between two lines; from, to: the x range drawn; top, bottom: the lines' extent
    static void paint(juce::Graphics &g, const std::function<float(double)> &beatToX,
                      const std::function<double(float)> &xToBeat, double step, int beatsPerBar, float from, float to,
                      float top, float bottom);

    // where Ctrl+V pastes: a small triangle pointing down at `top` and a line down to `bottom`
    static void paintPasteMarker(juce::Graphics &g, float x, float top, float bottom);
};
