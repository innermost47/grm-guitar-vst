// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
#include "TimeGrid.h"

#include <cmath>

#include "../../constants/ui.hpp"

void TimeGrid::paint(juce::Graphics &g, const std::function<float(double)> &beatToX,
                     const std::function<double(float)> &xToBeat, double step, int beatsPerBar, float from, float to,
                     float top, float bottom)
{
    const double first = std::max(0.0, std::floor(xToBeat(from) / step) * step);
    for (double b = first; beatToX(b) < to; b += step)
    {
        const float x = beatToX(b);
        if (x < from)
            continue;
        const bool bar = std::abs(std::fmod(b, (double)beatsPerBar)) < 1e-6;
        const bool beat = std::abs(b - std::round(b)) < 1e-6;
        g.setColour(bar ? ui::GRID_BAR : beat ? ui::GRID_BEAT : ui::GRID);
        g.drawVerticalLine((int)x, top, bottom);
    }
}

void TimeGrid::paintPasteMarker(juce::Graphics &g, float x, float top, float bottom)
{
    const float half = 0.5f * ui::PASTE_MARKER_SIZE;
    juce::Path triangle;
    triangle.addTriangle(x - half, top, x + half, top, x, top + half);
    g.setColour(ui::PASTE_MARKER);
    g.fillPath(triangle);
    g.drawVerticalLine((int)x, top, bottom);
}
