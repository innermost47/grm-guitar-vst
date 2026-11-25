// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
#include "StepLane.h"

#include <cmath>

#include "../../constants/core.hpp"

using namespace ui;

StepLane::StepLane(juce::String laneName, juce::Colour laneColour, bool isSigned, int gutterWidth, Host laneHost)
    : name(std::move(laneName)), colour(laneColour), signedLane(isSigned), gutter(gutterWidth), host(std::move(laneHost))
{
    setWantsKeyboardFocus(true);
}

std::vector<double> &StepLane::clipboard(bool isSigned)
{
    static std::vector<double> signedSteps, plainSteps;
    return isSigned ? signedSteps : plainSteps;
}

bool StepLane::selection(int &from, int &to) const
{
    if (selFrom < 0 || selTo <= selFrom)
        return false;
    from = selFrom;
    to = selTo;
    return true;
}

int StepLane::stepAt(float x) const { return (int)std::floor(host.xToBeat(x) * 4.0); }

float StepLane::yOf(double v) const
{
    const float top = 2.0f, bottom = (float)getHeight() - 2.0f;
    return signedLane ? (top + bottom) * 0.5f - (float)v * (bottom - top) * 0.5f : bottom - (float)v * (bottom - top);
}

// a value on the lane's notches (Shift: anywhere between them)
double StepLane::valueAt(float y, bool fine) const
{
    const float top = 2.0f, bottom = (float)getHeight() - 2.0f;
    double v = signedLane ? ((top + bottom) * 0.5f - y) / ((bottom - top) * 0.5f) : (bottom - y) / (bottom - top);
    const double lo = signedLane ? -1.0 : 0.0;
    v = juce::jlimit(lo, 1.0, v);
    if (!fine)
        v = lo + std::round((v - lo) / (1.0 - lo) * grmg::FEEL_NOTCHES) / grmg::FEEL_NOTCHES * (1.0 - lo);
    return v;
}

void StepLane::draw(int step, double value)
{
    if (step < 0)
        return;
    // from the last step drawn to this one, every step between gets its share (a fast drag leaves no hole)
    const int from = lastStep < 0 ? step : lastStep;
    const double v0 = lastStep < 0 ? value : lastValue;
    host.edit(
        [=](std::vector<double> &lane)
        {
            const int a = std::min(from, step), b = std::max(from, step);
            for (int s = a; s <= b && s < (int)lane.size(); ++s)
            {
                const double u = b == a ? 1.0 : (double)(s - from) / (double)(step - from);
                lane[(size_t)s] = v0 + u * (value - v0);
            }
        },
        !gesture);
    gesture = true;
    lastStep = step;
    lastValue = value;
}

void StepLane::paint(juce::Graphics &g)
{
    g.fillAll(PANEL);
    g.setColour(TEXT);
    g.setFont(font(11.0f));
    g.drawText(name, 8, 0, gutter - 10, getHeight(), juce::Justification::centredLeft);
    g.reduceClipRegion(gutter, 0, getWidth() - gutter, getHeight());
    // the middle line (or the floor), and a faint line every quarter of the lane
    g.setColour(GRID);
    for (double v : signedLane ? std::initializer_list<double>{-0.5, 0.0, 0.5} : std::initializer_list<double>{0.25, 0.5, 0.75})
        g.drawHorizontalLine((int)yOf(v), (float)gutter, (float)getWidth());
    const auto &lane = host.values();
    const int first = std::max(0, stepAt((float)gutter)), last = std::min((int)lane.size() - 1, stepAt((float)getWidth()));
    const float base = yOf(signedLane ? 0.0 : 0.0);
    for (int s = first; s <= last; ++s)
    {
        const float x0 = host.beatToX(0.25 * s) + 1.0f, x1 = host.beatToX(0.25 * (s + 1)) - 1.0f;
        const float y = yOf(lane[(size_t)s]);
        const bool selected = s >= selFrom && s < selTo;
        if (selected)
        {
            g.setColour(colour.withAlpha(0.12f));
            g.fillRect(x0 - 1.0f, 0.0f, x1 - x0 + 2.0f, (float)getHeight());
        }
        g.setColour(selected ? colour.brighter(0.4f) : colour.withAlpha(0.85f));
        g.fillRect(juce::Rectangle<float>(x0, std::min(y, base), std::max(1.0f, x1 - x0), std::max(1.0f, std::abs(base - y))));
    }
    g.setColour(GRID);
    g.drawHorizontalLine(0, (float)gutter, (float)getWidth());
}

void StepLane::mouseDown(const juce::MouseEvent &e)
{
    grabKeyboardFocus();
    const int step = stepAt(e.position.x);
    pasteAt = std::max(0, step);
    gesture = false;
    lastStep = -1;
    selecting = e.mods.isCommandDown();
    if (selecting)
    {
        anchor = step;
        selFrom = step;
        selTo = step + 1;
        repaint();
        return;
    }
    selFrom = selTo = -1;
    draw(step, valueAt(e.position.y, e.mods.isShiftDown()));
}

void StepLane::mouseDrag(const juce::MouseEvent &e)
{
    const int step = stepAt(e.position.x);
    if (selecting)
    {
        selFrom = std::max(0, std::min(anchor, step));
        selTo = std::max(anchor, step) + 1;
        repaint();
        return;
    }
    draw(step, valueAt(e.position.y, e.mods.isShiftDown()));
}

void StepLane::mouseUp(const juce::MouseEvent &)
{
    selecting = false;
    gesture = false;
    lastStep = -1;
}

bool StepLane::keyPressed(const juce::KeyPress &key)
{
    const auto &lane = host.values();
    const int n = (int)lane.size();
    const bool ctrl = key.getModifiers().isCommandDown();
    const int code = key.getKeyCode();
    auto set_range = [&](int from, int to, const std::function<double(int)> &value)
    {
        host.edit(
            [=](std::vector<double> &l)
            {
                for (int s = std::max(0, from); s < std::min(to, (int)l.size()); ++s)
                    l[(size_t)s] = value(s);
            },
            true);
    };
    int from = 0, to = 0;
    const bool any = selection(from, to);
    if (ctrl && (code == 'A' || code == 'a'))
    {
        selFrom = 0;
        selTo = n;
        repaint();
        return true;
    }
    if (ctrl && (code == 'Z' || code == 'z'))
    {
        host.undo();
        return true;
    }
    if (ctrl && (code == 'Y' || code == 'y'))
    {
        host.redo();
        return true;
    }
    if (ctrl && (code == 'C' || code == 'c' || code == 'X' || code == 'x') && any)
    {
        clipboard(signedLane).assign(lane.begin() + from, lane.begin() + std::min(to, n));
        if (code == 'X' || code == 'x')
            set_range(from, to, [this](int) { return neutral(); });
        return true;
    }
    if (ctrl && (code == 'V' || code == 'v'))
    {
        const auto copied = clipboard(signedLane);
        if (copied.empty())
            return true;
        const int at = pasteAt;
        set_range(at, at + (int)copied.size(), [copied, at](int s) { return copied[(size_t)(s - at)]; });
        selFrom = at;
        selTo = std::min(n, at + (int)copied.size());
        return true;
    }
    if (ctrl && (code == 'D' || code == 'd') && any)
    {
        const std::vector<double> part(lane.begin() + from, lane.begin() + std::min(to, n));
        const int at = to;
        set_range(at, at + (int)part.size(), [part, at](int s) { return part[(size_t)(s - at)]; });
        selFrom = at;
        selTo = std::min(n, at + (int)part.size());
        return true;
    }
    if ((code == juce::KeyPress::deleteKey || code == juce::KeyPress::backspaceKey) && any)
    {
        set_range(from, to, [this](int) { return neutral(); });
        return true;
    }
    return false;
}
