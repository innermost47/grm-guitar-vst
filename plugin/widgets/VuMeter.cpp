// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
#include "VuMeter.h"

#include <cmath>

using namespace ui;

VuMeter::VuMeter()
{
    setInterceptsMouseClicks(false, false);
}

void VuMeter::Side::take(float peak)
{
    const float db = peak > 1e-5f ? 20.0f * std::log10(peak) : -100.0f;
    const float norm = juce::jlimit(0.0f, 1.0f, (db - vu::FLOOR_DB) / -vu::FLOOR_DB);
    level = norm > level ? norm : level * vu::RELEASE + norm * (1.0f - vu::RELEASE);
    if (level >= held)
    {
        held = level;
        hold = vu::PEAK_HOLD_FRAMES;
    }
    else if (hold > 0)
        --hold;
    else
        held *= vu::PEAK_FALL;
}

void VuMeter::setPeaks(float l, float r)
{
    left.take(l);
    right.take(r);
    repaint();
}

void VuMeter::paintSide(juce::Graphics &g, juce::Rectangle<float> area, const Side &side) const
{
    g.setColour(VU_CASE);
    g.fillRoundedRectangle(area, 2.0f);
    g.setColour(GRID);
    g.drawRoundedRectangle(area, 2.0f, 0.5f);
    auto inner = area.reduced(1.0f, 2.0f);
    // the clip light on top
    const auto clip = inner.removeFromTop(4.0f);
    g.setColour(side.level >= vu::CLIP_FROM ? VU_RED : VU_RED.withAlpha(vu::UNLIT_ALPHA));
    g.fillRoundedRectangle(clip, 1.0f);
    inner.removeFromTop(2.0f);
    const float segment = inner.getHeight() / (float)vu::SEGMENTS;
    for (int i = 0; i < vu::SEGMENTS; ++i)
    {
        const float from = (float)i / (float)vu::SEGMENTS;
        const auto colour = from < vu::ORANGE_FROM ? VU_GREEN : (from < vu::RED_FROM ? VU_ORANGE : VU_RED);
        const juce::Rectangle<float> r(inner.getX(), inner.getBottom() - (float)(i + 1) * segment, inner.getWidth(), segment - 1.0f);
        g.setColour(side.level >= from ? colour : colour.withAlpha(vu::UNLIT_ALPHA));
        g.fillRoundedRectangle(r, 1.0f);
    }
    if (side.held > 0.0f)
    {
        const int at = juce::jmin(vu::SEGMENTS - 1, (int)(side.held * (float)vu::SEGMENTS));
        g.setColour(VU_PEAK);
        g.fillRect(inner.getX(), inner.getBottom() - (float)(at + 1) * segment, inner.getWidth(), 2.0f);
    }
}

void VuMeter::paint(juce::Graphics &g)
{
    auto area = getLocalBounds().toFloat();
    const float gap = 2.0f, w = (area.getWidth() - gap) / 2.0f;
    paintSide(g, area.removeFromLeft(w), left);
    area.removeFromLeft(gap);
    paintSide(g, area, right);
}
