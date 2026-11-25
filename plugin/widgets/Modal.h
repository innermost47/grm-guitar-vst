// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// A dialog inside the plugin's window, in its style: it darkens the window, shows a panel with the equations of
// the GRM on its left (EquationsPanel), a title, a message, rows of widgets (a checkbox, a link) and buttons.
// Every dialog of the plugin is one of these. Escape or a button closes it (it hides): onDismiss is then called
// once. Its owner frees it later (not from onDismiss: the click that closed it is still running).
#pragma once

#include <functional>
#include <memory>
#include <vector>

#include "../../constants/ui.hpp"
#include "EquationsPanel.h"

class Modal : public juce::Component
{
public:
    explicit Modal(const juce::String &title);

    void setMessage(const juce::String &text);

    // a row under the message (the dialog owns it), as tall as one row of the design system
    template <class Row>
    Row &addRow(std::unique_ptr<Row> row)
    {
        auto &added = *row;
        addAndMakeVisible(added);
        rows.push_back(std::move(row));
        resized();
        return added;
    }

    // the usual rows: a link (in the accent colour, left aligned), a checkbox, a line of text to type (it takes
    // the keyboard when the dialog is shown; Escape closes the dialog from it too)
    juce::HyperlinkButton &addLink(const juce::String &text, const juce::URL &url);
    juce::ToggleButton &addCheckbox(const juce::String &text);
    juce::TextEditor &addTextField(const juce::String &placeholder, int maxLength);

    // a button of the bottom row, from left to right; onClick runs, then the dialog closes
    juce::TextButton &addButton(const juce::String &text, std::function<void()> onClick = nullptr);

    // covers `parent` and takes the keyboard
    void show(juce::Component &parent);
    void dismiss();

    std::function<void()> onDismiss;
    bool isDismissed() const { return dismissed; }

    void paint(juce::Graphics &g) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress &key) override;

    const EquationsPanel &getEquations() const { return side; }
    int getNumButtons() const { return (int)buttons.size(); }

private:
    EquationsPanel side;
    juce::Label title, message;
    std::vector<std::unique_ptr<juce::Component>> rows;
    std::vector<std::unique_ptr<juce::TextButton>> buttons;
    bool dismissed = false;
    const ui::theme::AppTheme appTheme = ui::theme::AppTheme::fromScale(1.0f);

    juce::Rectangle<int> panelBounds() const;
    int messageHeight(int width) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Modal)
};
