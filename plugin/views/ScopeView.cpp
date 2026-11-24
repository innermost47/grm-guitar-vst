// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
#include "ScopeView.h"

using namespace grmg;
using namespace ui;

void ScopeView::updateScope()
{
    float tmp[4096];
    const int got = processor.readScope(tmp, 4096);
    if (got > 0)
    {
        for (int k = 0; k < got; ++k)
        {
            history[write] = tmp[k];
            write = (write + 1) % history.size();
        }
        repaint();
    }
}

void ScopeView::paint(juce::Graphics &g)
{
    using namespace ui;
    g.fillAll(PANEL);
    const float w = (float)getWidth(), h = (float)getHeight(), mid = h / 2;
    g.setColour(GRID);
    g.drawHorizontalLine((int)mid, 0, w);

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