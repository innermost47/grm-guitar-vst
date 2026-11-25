// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
#include "NetworksView.h"

using namespace grmg;
using namespace ui;

NetworksView::NetworksView(GRMGuitarProcessor &p) : processor(p)
{
    text.setMultiLine(true, false); // one figure a line: it scrolls sideways in the narrow column
    text.setReadOnly(true);
    text.setScrollbarsShown(true);
    text.setFont(juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 10.0f, juce::Font::plain)));
    text.setColour(juce::TextEditor::backgroundColourId, ui::PANEL);
    text.setColour(juce::TextEditor::textColourId, ui::BRIGHT);
    text.setColour(juce::TextEditor::outlineColourId, ui::PANEL);
    addAndMakeVisible(text);
    updateNetworks(); // shown from the first frame
    vblank = std::make_unique<juce::VBlankAttachment>(this, [this]
                                                      { updateNetworks(); });
}

void NetworksView::updateNetworks()
{
    const auto m = processor.getShownModel();
    if (!m || m.get() == shown)
        return;
    shown = m.get();
    juce::String t;
    auto num = [](double v, int d)
    { return juce::String(v, d); };
    t << "STRINGS (tuning network, then string network at the open string)\n";
    for (int i = 5; i >= 0; --i)
    {
        const auto &p = m->place[(size_t)i][0];
        t << "  " << (6 - i) << "  " << juce::String(note_name(m->open_midi(i))) << "  T = " << num(p.T, 2) << " N   f1 = " << num(p.f1, 2)
          << " Hz   B = " << juce::String(p.B, 7) << "   Zs = " << num(p.Zs, 3) << " kg/s\n";
    }
    t << "\nATTACK at the 3rd fret, set force and pluck point:  release height y0, release time tau\n";
    for (int i = 5; i >= 0; --i)
        t << "  " << (6 - i) << "  y0 = " << num(m->y0[(size_t)i][3] * 1000.0, 3) << " mm   tau = "
          << num(m->tau[(size_t)i][3] * 1e6, 1) << " us\n";
    const auto &b = m->body;
    t << "\nBODY:  Helmholtz " << num(b.fH, 1) << " Hz, top " << num(b.fT, 1) << " Hz -> coupled " << num(b.f_lo, 1)
      << " / " << num(b.f_hi, 1) << " Hz;  radiation corner " << num(b.frad, 0) << " Hz;  bridge corner " << num(b.fb, 0)
      << " Hz\n       top modes:";
    for (int k = 2; k < 7; ++k)
        t << " " << num(b.f[(size_t)k], 0);
    t << " Hz;  modal mass " << num(b.Mm * 1000, 1) << " g;  plate impedance " << num(b.Zp, 1) << " kg/s\n";
    if (m->palm_ready)
    {
        const auto &p = *m->palm[0][0];
        t << "\nPALM on the low E (open): first modes, frequency ratio and added decay (1/s)\n  ";
        for (size_t k = 0; k < std::min<size_t>(6, p.ratio.size()); ++k)
            t << "n=" << (int)k + 1 << ": " << num(p.ratio[k], 3) << " / " << num(p.sigma[k], 0) << "   ";
        t << "\n";
    }
    if (!m->room_ir[0].empty())
    {
        const auto &r = m->room;
        t << "\nROOM: V = " << num(r.V, 1) << " m3, S = " << num(r.S, 1) << " m2, critical distance " << num(r.rc, 2)
          << " m, Schroeder " << num(r.fs, 0) << " Hz\n      RT60:";
        for (int k = 0; k < 6; ++k)
            t << " " << (int)ROOM_BANDS[k] << " Hz " << num(r.RT[(size_t)k], 2) << " s;";
        t << "\n";
    }
    text.setText(t, false);
}