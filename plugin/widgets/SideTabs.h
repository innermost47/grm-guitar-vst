// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// SideTabs.h
//
// Tabs for a narrow, tall column (the settings' sidebar): their names stacked on the left, one under the other,
// the chosen tab's page beside them. It owns its pages.
#pragma once

#include <memory>
#include <vector>

#include <JuceHeader.h>

#include "../../constants/ui.hpp"

class SideTabs : public juce::Component
{
public:
    SideTabs() = default;
    void addTab(const juce::String &name, std::unique_ptr<juce::Component> page);
    void clearTabs();
    int getCurrentTabIndex() const { return current; }
    void setCurrentTabIndex(int index);
    int getNumTabs() const { return (int)tabs.size(); }

    void paint(juce::Graphics &) override;
    void resized() override;

private:
    struct Tab
    {
        std::unique_ptr<juce::TextButton> button;
        std::unique_ptr<juce::Component> page;
    };
    std::vector<Tab> tabs;
    int current = -1;
    const ui::theme::AppTheme appTheme = ui::theme::AppTheme::fromScale(1.0f);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SideTabs)
};
