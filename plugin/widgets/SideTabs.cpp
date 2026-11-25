// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
#include "SideTabs.h"

using namespace ui;

void SideTabs::addTab(const juce::String &name, std::unique_ptr<juce::Component> page)
{
    Tab t;
    t.button = std::make_unique<juce::TextButton>(name);
    t.button->setClickingTogglesState(false);
    t.button->setColour(juce::TextButton::buttonOnColourId, WAVE.darker(0.4f));
    const int index = (int)tabs.size();
    t.button->onClick = [this, index]
    { setCurrentTabIndex(index); };
    addAndMakeVisible(*t.button);
    t.page = std::move(page);
    addChildComponent(*t.page);
    tabs.push_back(std::move(t));
    if (current < 0)
        setCurrentTabIndex(0);
    resized();
}

void SideTabs::clearTabs()
{
    tabs.clear();
    current = -1;
}

void SideTabs::setCurrentTabIndex(int index)
{
    if (tabs.empty())
        return;
    current = juce::jlimit(0, (int)tabs.size() - 1, index);
    for (int k = 0; k < (int)tabs.size(); ++k)
    {
        tabs[(size_t)k].button->setToggleState(k == current, juce::dontSendNotification);
        tabs[(size_t)k].page->setVisible(k == current);
    }
    resized();
}

void SideTabs::paint(juce::Graphics &g)
{
    g.fillAll(PANEL);
    g.setColour(GRID);
    g.drawVerticalLine(appTheme.dimensions.sideTabWidth, 0.0f, (float)getHeight());
}

void SideTabs::resized()
{
    auto area = getLocalBounds();
    auto column = area.removeFromLeft(appTheme.dimensions.sideTabWidth).reduced(appTheme.spacing.sm, appTheme.spacing.sm);
    for (auto &t : tabs)
    {
        t.button->setBounds(column.removeFromTop(appTheme.dimensions.sideTabHeight));
        column.removeFromTop(appTheme.spacing.xs);
    }
    area.removeFromLeft(1);
    for (auto &t : tabs)
        t.page->setBounds(area);
}
