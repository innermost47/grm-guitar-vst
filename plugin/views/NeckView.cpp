// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
#include "NeckView.h"

#include "../../core/chords.hpp"

using namespace grmg;
using namespace ui;

float NeckView::fretX(int k) const
{
    const float x0 = 40.0f, x1 = (float)getWidth() - 20.0f;
    const float scale = (x1 - x0) / (1.0f - std::pow(2.0f, -19.5f / 12.0f));
    return x0 + scale * (1.0f - std::pow(2.0f, -(float)k / 12.0f));
}

void NeckView::paint(juce::Graphics &g)
{
    using namespace ui;
    g.fillAll(PANEL);
    const float h = (float)getHeight(), top = 14.0f, bottom = h - 18.0f;
    g.setColour(juce::Colour(0xff2b2116));
    g.fillRect(40.0f, top - 6, (float)getWidth() - 60.0f, bottom - top + 12);
    g.setFont(font(10.0f));

    const auto curSong = processor.getSong();
    const int songKey = curSong.key;
    const int songScale = curSong.scale;

    for (int k = 1; k < 20; ++k)
    {
        const float x = fretX(k);
        g.setColour(juce::Colour(0xff9c9c9c));
        g.drawLine(x, top - 6, x, bottom + 6, 1.5f);
        if (k == 3 || k == 5 || k == 7 || k == 9 || k == 12 || k == 15 || k == 17)
        {
            g.setColour(TEXT);
            g.drawText(juce::String(k), (int)((fretX(k - 1) + x) / 2 - 10), (int)(h - 16), 20, 14, juce::Justification::centred);
            g.setColour(juce::Colour(0xffe6dccd).withAlpha(0.5f));
            const float cx = (fretX(k - 1) + x) / 2, cy = (top + bottom) / 2;
            if (k == 12)
            {
                g.fillEllipse(cx - 3, cy - (bottom - top) * 0.25f - 3, 6, 6);
                g.fillEllipse(cx - 3, cy + (bottom - top) * 0.25f - 3, 6, 6);
            }
            else
            {
                g.fillEllipse(cx - 3, cy - 3, 6, 6);
            }
        }
    }
    g.setColour(juce::Colour(0xffe6dccd));
    g.fillRect(36.0f, top - 6, 5.0f, bottom - top + 12);
    const float gap = (bottom - top) / 5.0f;

    for (int row = 0; row < 6; ++row)
    {
        const int i = 5 - row;
        const float y = top + row * gap;
        const int open = processor.getOpenMidi(i);

        for (int k = 1; k < 20; ++k)
        {
            const int pitch = open + k;
            const bool isRoot = (((pitch % 12) + 12) % 12 == ((songKey % 12) + 12) % 12);
            const bool inScale = isRoot || (grmg::scale_step(pitch, songKey, songScale, 0) == pitch);

            if (inScale)
            {
                const float cx = (fretX(k - 1) + fretX(k)) * 0.5f;
                if (isRoot)
                {
                    g.setColour(ACCENT.withAlpha(0.9f));
                    g.fillEllipse(cx - 4.5f, y - 4.5f, 9.0f, 9.0f);
                    g.setColour(juce::Colours::white);
                    g.drawEllipse(cx - 4.5f, y - 4.5f, 9.0f, 9.0f, 1.2f);
                }
                else
                {
                    g.setColour(STRING_COLOURS[i].withAlpha(0.38f));
                    g.fillEllipse(cx - 3.0f, y - 3.0f, 6.0f, 6.0f);
                }
            }
        }
    }

    for (int row = 0; row < 6; ++row)
    {
        const int i = 5 - row;
        const float y = top + row * gap;
        const float level = processor.getShownLevel(i);
        const int fret = processor.getShownFret(i);
        const float amp = juce::jlimit(0.0f, gap * 0.4f, 4.0f * std::log10(1.0f + 30.0f * level));
        const float xs = fret > 0 ? fretX(fret) - 4.0f : 41.0f;
        juce::Path p;
        p.startNewSubPath(4.0f, y);
        p.lineTo(xs, y);
        const float x1 = (float)getWidth() - 20.0f;
        for (float x = xs; x <= x1; x += 3.0f)
        {
            const float u = (x - xs) / (x1 - xs);
            p.lineTo(x, y + amp * std::sin(juce::MathConstants<float>::pi * u) * (float)std::sin(phase + i * 1.3 + u * 2));
        }
        g.setColour(STRING_COLOURS[i].withAlpha(0.6f + 0.4f * juce::jlimit(0.0f, 1.0f, level)));
        g.strokePath(p, juce::PathStrokeType(i < 3 ? 2.2f : 1.4f));

        const int open = processor.getOpenMidi(i);
        const bool isRoot = (((open % 12) + 12) % 12 == ((songKey % 12) + 12) % 12);
        const bool inScale = isRoot || (grmg::scale_step(open, songKey, songScale, 0) == open);

        if (isRoot)
            g.setColour(ACCENT);
        else if (inScale)
            g.setColour(BRIGHT);
        else
            g.setColour(TEXT.withAlpha(0.40f));

        g.setFont(font(10.0f, isRoot || inScale));
        g.drawText(NOTE_NAMES[open % 12], 2, (int)(y - 7), 30, 14, juce::Justification::centredLeft);

        if (level > 0.002f && fret > 0)
        {
            g.setColour(STRING_COLOURS[i]);
            g.fillEllipse((fretX(fret - 1) + fretX(fret)) / 2 - 6, y - 6, 12, 12);
            g.setColour(juce::Colours::white);
            g.drawEllipse((fretX(fret - 1) + fretX(fret)) / 2 - 6, y - 6, 12, 12, 1.5f);
        }
    }
    g.setColour(TEXT);
    g.setFont(font(10.0f));
    g.drawText("click a string to play it", getWidth() - 160, 0, 150, 12, juce::Justification::right);
}

void NeckView::mouseDown(const juce::MouseEvent &e)
{
    const float top = 14.0f, bottom = (float)getHeight() - 18.0f, gap = (bottom - top) / 5.0f;
    const int row = juce::jlimit(0, 5, (int)std::lround((e.position.y - top) / gap));
    int fret = 0;
    for (int k = 1; k < 20; ++k)
        if (e.position.x > fretX(k - 1) && e.position.x <= fretX(k))
            fret = k;
    processor.pluckFromInterface(5 - row, fret, 0.8f);
}