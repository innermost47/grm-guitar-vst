// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
#include "Modal.h"

Modal::Modal(const juce::String &titleText)
{
    setWantsKeyboardFocus(true);
    addAndMakeVisible(side);
    title.setText(titleText, juce::dontSendNotification);
    title.setFont(ui::font(appTheme.typography.title, true));
    title.setColour(juce::Label::textColourId, ui::BRIGHT);
    addAndMakeVisible(title);
    message.setFont(ui::font(appTheme.typography.regular));
    message.setColour(juce::Label::textColourId, ui::BRIGHT.withAlpha(ui::theme::defaultOpacity.alpha08));
    message.setJustificationType(juce::Justification::topLeft);
    addAndMakeVisible(message);
}

void Modal::setMessage(const juce::String &text)
{
    message.setText(text, juce::dontSendNotification);
    resized();
}

juce::HyperlinkButton &Modal::addLink(const juce::String &text, const juce::URL &url)
{
    auto &link = addRow(std::make_unique<juce::HyperlinkButton>(text, url));
    link.setFont(ui::font(appTheme.typography.regular), false, juce::Justification::centredLeft);
    link.setTooltip(url.toString(false));
    return link;
}

juce::ToggleButton &Modal::addCheckbox(const juce::String &text)
{
    return addRow(std::make_unique<juce::ToggleButton>(text));
}

juce::TextEditor &Modal::addTextField(const juce::String &placeholder, int maxLength)
{
    auto &field = addRow(std::make_unique<juce::TextEditor>());
    field.setFont(ui::font(appTheme.typography.regular));
    field.setTextToShowWhenEmpty(placeholder, ui::BRIGHT.withAlpha(ui::theme::defaultOpacity.alpha04));
    field.setInputRestrictions(maxLength);
    field.setIndents(appTheme.spacing.base, 0);
    field.setJustification(juce::Justification::centredLeft);
    field.onEscapeKey = [this]
    { dismiss(); };
    return field;
}

juce::TextButton &Modal::addButton(const juce::String &text, std::function<void()> onClick)
{
    auto button = std::make_unique<juce::TextButton>(text);
    button->onClick = [this, onClick = std::move(onClick)]
    {
        if (onClick)
            onClick();
        dismiss();
    };
    auto &added = *button;
    addAndMakeVisible(added);
    buttons.push_back(std::move(button));
    resized();
    return added;
}

void Modal::show(juce::Component &parent)
{
    parent.addAndMakeVisible(this);
    setBounds(parent.getLocalBounds());
    toFront(true);
    // a line to type takes the keyboard, or else the dialog does (Escape)
    for (auto &row : rows)
        if (auto *field = dynamic_cast<juce::TextEditor *>(row.get()))
        {
            field->grabKeyboardFocus();
            return;
        }
    grabKeyboardFocus();
}

void Modal::dismiss()
{
    if (dismissed)
        return;
    dismissed = true;
    setVisible(false);
    if (onDismiss)
        onDismiss();
}

bool Modal::keyPressed(const juce::KeyPress &key)
{
    if (key == juce::KeyPress::escapeKey)
    {
        dismiss();
        return true;
    }
    return true; // a dialog keeps the keys from the window behind it
}

int Modal::messageHeight(int width) const
{
    juce::AttributedString text;
    text.append(message.getText(), message.getFont());
    juce::TextLayout layout;
    layout.createLayout(text, (float)width);
    return (int)std::ceil(layout.getHeight());
}

// as wide as the design system's dialog, as tall as its content needs (title, message, rows, buttons)
juce::Rectangle<int> Modal::panelBounds() const
{
    const auto &d = appTheme.dimensions;
    const int width = std::min(d.baseModalWidth, getWidth());
    const int body = width - d.modalSideWidth - 2 * d.modalPadding;
    const int content = 2 * d.modalPadding + d.modalTitleHeight + messageHeight(body) + appTheme.spacing.lg +
                        (int)rows.size() * d.modalRowHeight + appTheme.spacing.xl + d.modalButtonHeight;
    const int height = std::min(std::clamp(content, d.modalMinHeight, d.baseModalHeight), getHeight());
    return getLocalBounds().withSizeKeepingCentre(width, height);
}

void Modal::paint(juce::Graphics &g)
{
    g.fillAll(ui::BG.withAlpha(ui::MODAL_DIM_ALPHA));
    const auto panel = panelBounds().toFloat();
    const float corner = appTheme.borders.modalCorner;
    g.setColour(ui::PANEL);
    g.fillRoundedRectangle(panel, corner);
    g.setColour(ui::GRID);
    g.drawRoundedRectangle(panel, corner, appTheme.borders.base);
    const float divider = panel.getX() + (float)appTheme.dimensions.modalSideWidth;
    g.drawVerticalLine((int)divider, panel.getY() + (float)appTheme.spacing.xl, panel.getBottom() - (float)appTheme.spacing.xl);
}

void Modal::resized()
{
    const auto &d = appTheme.dimensions;
    auto panel = panelBounds();
    side.setBounds(panel.removeFromLeft(d.modalSideWidth));
    auto body = panel.reduced(d.modalPadding);
    title.setBounds(body.removeFromTop(d.modalTitleHeight));

    auto buttonRow = body.removeFromBottom(d.modalButtonHeight);
    for (auto it = buttons.rbegin(); it != buttons.rend(); ++it)
    {
        (*it)->setBounds(buttonRow.removeFromRight(d.modalButtonWidth));
        buttonRow.removeFromRight(appTheme.spacing.lg);
    }
    body.removeFromBottom(appTheme.spacing.xl);
    for (auto it = rows.rbegin(); it != rows.rend(); ++it)
    {
        auto row = body.removeFromBottom(d.modalRowHeight);
        // a link's text starts where the message's does
        if (dynamic_cast<juce::HyperlinkButton *>(it->get()) != nullptr)
            row.removeFromLeft(getLookAndFeel().getLabelBorderSize(message).getLeft());
        (*it)->setBounds(row);
    }
    body.removeFromBottom(appTheme.spacing.lg);
    message.setBounds(body);
}
