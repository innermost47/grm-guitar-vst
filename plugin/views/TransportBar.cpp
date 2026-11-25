// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
#include "TransportBar.h"

#include <cmath>

using namespace grmg;
using namespace ui;

TransportBar::TransportBar(GRMGuitarProcessor &p, RollView &r) : processor(p), roll(r)
{
    play.onClick = [this]
    { processor.setInternalPlaying(!processor.isInternalPlaying()); };
    fullScreen.onClick = [this]
    {
        if (auto *window = findParentComponentOfClass<juce::DocumentWindow>())
            window->setFullScreen(!window->isFullScreen());
    };
    loopAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.params, "loop", loop);
    followAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.params, "follow_playhead", follow);
    tempo.setTooltip("Tempo (standalone; in a host, the host's)");
    tempo.setTextValueSuffix(" bpm");
    tempoAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.params, "tempo", tempo);
    for (int b = 1; b <= 12; ++b)
        signature.addItem(juce::String(b) + "/4", b);
    signature.setTooltip("The pattern's signature (beats of a quarter note)");
    signature.onChange = [this]
    {
        const int b = signature.getSelectedId();
        if (b > 0 && b != roll.getSong().beats_per_bar)
            roll.edit([b](Song &s)
                      { s.beats_per_bar = b; s.grouping.clear(); }, false);
    };
    grouping.setTooltip("How the bar's beats are grouped: where its accents fall (the Feel follows)");
    grouping.onChange = [this]
    {
        const int k = grouping.getSelectedId() - 1;
        if (k < 0 || k >= (int)groupings.size())
            return;
        const auto g = k == 0 ? std::vector<int>{} : groupings[(size_t)k];
        if (g != roll.getSong().grouping)
            roll.edit([g](Song &s)
                      { s.grouping = g; }, false);
    };
    for (auto *l : {&where, &clock})
    {
        l->setJustificationType(juce::Justification::centred);
        l->setColour(juce::Label::textColourId, BRIGHT);
        l->setFont(font(appTheme.typography.subtitle, true));
    }
    for (auto *c : std::initializer_list<juce::Component *>{&play, &loop, &follow, &fullScreen, &where, &clock, &tempo, &signature, &grouping})
        addAndMakeVisible(c);
    play.setVisible(standalone());
    fullScreen.setVisible(standalone());
    tempo.setVisible(standalone());
    update(); // shown from the first frame
    vblank = std::make_unique<juce::VBlankAttachment>(this, [this]
                                                      { update(); });
}

void TransportBar::refreshSignature()
{
    const auto &s = roll.getSong();
    if (s.beats_per_bar == shownBeats && s.grouping == shownGrouping)
        return;
    shownBeats = s.beats_per_bar;
    shownGrouping = s.grouping;
    signature.setSelectedId(s.beats_per_bar, juce::dontSendNotification);
    groupings = Meter::groupings(s.beats_per_bar);
    grouping.clear(juce::dontSendNotification);
    for (size_t k = 0; k < groupings.size(); ++k)
    {
        juce::StringArray parts;
        for (int g : groupings[k])
            parts.add(juce::String(g));
        grouping.addItem(parts.joinIntoString("+"), (int)k + 1);
    }
    const auto current = s.meter().groups();
    for (size_t k = 0; k < groupings.size(); ++k)
        if (groupings[k] == current)
            grouping.setSelectedId((int)k + 1, juce::dontSendNotification);
    grouping.setEnabled(groupings.size() > 1);
}

void TransportBar::update()
{
    refreshSignature();
    const bool host = processor.isHostPlaying();
    play.setToggleState(processor.isInternalPlaying(), juce::dontSendNotification);
    play.setEnabled(!host);
    const double b = processor.getShownBeat();
    const auto &s = roll.getSong();
    const int playing = processor.getPlayingPattern();
    if (b < 0 || playing < 0)
    {
        where.setText("-", juce::dontSendNotification);
        clock.setText("0:00.000", juce::dontSendNotification);
        return;
    }
    const int bar = (int)(b / s.beats_per_bar);
    const double beat = b - bar * s.beats_per_bar;
    juce::String text = playing == processor.getEditingPattern() ? juce::String(bar + 1) + "." + juce::String((int)beat + 1)
                                                                  : juce::String("P") + juce::String(playing + 1);
    const double g = processor.getSongBeat();
    if (g >= 0)
        text << "   song " << (int)(g / s.beats_per_bar) + 1 << "." << (int)std::fmod(g, s.beats_per_bar) + 1;
    where.setText(text + (host ? "  (host)" : ""), juce::dontSendNotification);
    const double seconds = (g >= 0 ? g : b) * 60.0 / std::max(processor.getTempo(), 1.0);
    const int minutes = (int)(seconds / 60.0);
    clock.setText(juce::String(minutes) + ":" + juce::String(seconds - 60.0 * minutes, 3).paddedLeft('0', 6), juce::dontSendNotification);
}

int TransportBar::preferredWidth() const
{
    const int icon = appTheme.dimensions.iconButton, gap = appTheme.spacing.md;
    const int icons = 2 + (standalone() ? 2 : 0);
    const int fields = WHERE_W + CLOCK_W + SIGNATURE_W + GROUPING_W + (standalone() ? TEMPO_W : 0);
    const int items = icons + 4 + (standalone() ? 1 : 0);
    return 2 * appTheme.spacing.lg + icons * icon + fields + items * gap;
}

void TransportBar::paint(juce::Graphics &g)
{
    g.setColour(VU_CASE);
    g.fillRoundedRectangle(getLocalBounds().toFloat(), appTheme.borders.corner * 2);
    g.setColour(GRID);
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), appTheme.borders.corner * 2, 1.0f);
}

void TransportBar::resized()
{
    const int icon = appTheme.dimensions.iconButton, gap = appTheme.spacing.md;
    auto a = getLocalBounds().reduced(appTheme.spacing.lg, appTheme.spacing.base);
    auto place = [&](juce::Component &c, int width)
    {
        c.setBounds(a.removeFromLeft(width).withSizeKeepingCentre(width, juce::jmin(icon, a.getHeight())));
        a.removeFromLeft(gap);
    };
    if (play.isVisible())
        place(play, icon);
    place(where, WHERE_W);
    place(clock, CLOCK_W);
    if (tempo.isVisible())
        place(tempo, TEMPO_W);
    place(signature, SIGNATURE_W);
    place(grouping, GROUPING_W);
    place(loop, icon);
    place(follow, icon);
    if (fullScreen.isVisible())
        place(fullScreen, icon);
}
