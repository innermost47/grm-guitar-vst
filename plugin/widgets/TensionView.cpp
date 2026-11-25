// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
#include "TensionView.h"

using namespace grmg;
using namespace ui;

namespace
{
// a tension as the panel shows it: whole newtons
juce::String newtons(double tension)
{
    return juce::String(juce::roundToInt(tension)) + " N";
}
} // namespace

TensionView::TensionView(GRMGuitarProcessor &p) : processor(p)
{
    refresh(); // the strings as they are now, from the first frame on
    vblank = std::make_unique<juce::VBlankAttachment>(this, [this]
                                                      { refresh(); });
}

void TensionView::refresh()
{
    const auto m = processor.getShownModel();
    if (!m || m.get() == shown)
        return;
    shown = m.get();
    for (int i = 0; i < NUM_STRINGS; ++i)
    {
        notes[(size_t)i] = m->open_midi(i);
        tensions[(size_t)i] = m->tension[(size_t)i];
    }
    repaint();
}

void TensionView::paint(juce::Graphics &g)
{
    if (shown == nullptr)
        return;
    const auto &type = appTheme.typography;
    auto area = getLocalBounds().reduced(appTheme.spacing.padding, 0);
    auto top = area.removeFromTop(area.getHeight() / 2);
    // one cell per string, high string first (as the roll shows them)
    const int cell = top.getWidth() / NUM_STRINGS;
    int slack = -1;
    for (int row = 0; row < NUM_STRINGS; ++row)
    {
        const int i = NUM_STRINGS - 1 - row;
        const bool loose = tensions[(size_t)i] < STRING_SLACK_N;
        if (loose && (slack < 0 || tensions[(size_t)i] < tensions[(size_t)slack]))
            slack = i;
        const auto r = top.withX(top.getX() + row * cell).withWidth(cell).withTrimmedRight(appTheme.spacing.padding);
        g.setFont(font(type.info, true));
        g.setColour(loose ? WARNING : BRIGHT);
        g.drawText(juce::String(NUM_STRINGS - i) + "  " + juce::String(note_name(notes[(size_t)i])),
                   r, juce::Justification::centredLeft, false);
        g.setFont(font(type.small));
        g.setColour(loose ? WARNING : TEXT);
        g.drawText(newtons(tensions[(size_t)i]), r, juce::Justification::centredRight, false);
    }
    g.setFont(font(type.small));
    if (slack >= 0)
    {
        g.setColour(WARNING);
        g.drawText("String " + juce::String(NUM_STRINGS - slack) + " (" + juce::String(note_name(notes[(size_t)slack])) +
                       ") is slack at " + newtons(tensions[(size_t)slack]) +
                       ": floppy under the pick, it buzzes on the frets. A heavier string would hold it.",
                   area, juce::Justification::centredLeft, true);
    }
    else
    {
        g.setColour(TEXT);
        g.drawText("Tension of each open string, as its tuning network solves it.", area, juce::Justification::centredLeft, true);
    }
}
