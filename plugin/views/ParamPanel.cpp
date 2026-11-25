// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
#include "ParamPanel.h"

using namespace grmg;
using namespace ui;

ParamPanel::ParamPanel(juce::AudioProcessorValueTreeState &s, const std::vector<juce::String> &ids)
    : state(s)
{
    viewport.setViewedComponent(&content, false);
    viewport.setScrollBarsShown(true, false);
    viewport.setScrollBarThickness(8);
    addAndMakeVisible(viewport);

    for (const auto &id : ids)
    {
        auto *param = state.getParameter(id);
        if (!param)
            continue;
        Row r;
        r.id = id;
        juce::String name = param->getName(64);
        if (name.startsWithChar('G') && name.indexOfChar(' ') > 1 && name.substring(1, name.indexOfChar(' ')).containsOnly("0123456789"))
            name = name.fromFirstOccurrenceOf(" ", false, false);
        r.label = std::make_unique<juce::Label>(juce::String(), name);
        r.label->setFont(font(13.0f));
        r.label->setColour(juce::Label::textColourId, ui::BRIGHT);
        content.addAndMakeVisible(*r.label);

        if (dynamic_cast<juce::AudioParameterBool *>(param))
        {
            auto b = std::make_unique<juce::ToggleButton>();
            r.button = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(state, id, *b);
            r.control = std::move(b);
        }
        else if (auto *c = dynamic_cast<juce::AudioParameterChoice *>(param))
        {
            auto box = std::make_unique<juce::ComboBox>();
            box->addItemList(c->choices, 1);
            r.combo = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(state, id, *box);
            r.control = std::move(box);
        }
        else
        {
            auto sl = std::make_unique<juce::Slider>(juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
            sl->setTextBoxStyle(juce::Slider::TextBoxRight, false, 56, 20);
            sl->setScrollWheelEnabled(false);
            r.slider = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state, id, *sl);
            r.control = std::move(sl);
        }
        content.addAndMakeVisible(*r.control);
        rows.push_back(std::move(r));
    }

    warningLabel.setText("Please go to the Sensor tab and choose a magnetic pickup to access amplifiers and feedback.", juce::dontSendNotification);
    warningLabel.setFont(font(14.0f, true));
    warningLabel.setColour(juce::Label::textColourId, ui::TEXT);
    warningLabel.setJustificationType(juce::Justification::centred);
    warningLabel.setVisible(false);
    content.addAndMakeVisible(warningLabel);

    vblank = std::make_unique<juce::VBlankAttachment>(this, [this]
                                                      { updateVisibility(); });
}

void ParamPanel::resized()
{
    viewport.setBounds(getLocalBounds());
    layoutContent();
}

void ParamPanel::layoutContent()
{
    std::vector<Row *> visible;
    visible.reserve(rows.size());
    for (auto &r : rows)
        if (r.control && r.control->isVisible())
            visible.push_back(&r);

    const int viewW = viewport.getWidth();
    const int viewH = viewport.getHeight();
    if (viewW <= 0 || viewH <= 0)
        return;

    if (visible.empty())
    {
        content.setBounds(0, 0, viewW, viewH);
        warningLabel.setBounds(content.getLocalBounds().reduced(20));
        warningLabel.setVisible(true);
        return;
    }
    else
    {
        warningLabel.setVisible(false);
    }

    const int rowH = 26;
    const int rowSpacing = 2;
    const int margin = 6;

    const int cols = viewW > 520 ? 2 : 1;
    const int numVisible = (int)visible.size();
    const int totalRows = (numVisible + cols - 1) / cols;

    const int rowsH = totalRows * (rowH + rowSpacing);
    const int neededH = margin * 2 + rowsH + footerHeight;
    const int targetH = std::max(viewH, neededH);

    const bool willScroll = neededH > viewH;
    const int scrollbarW = willScroll ? viewport.getScrollBarThickness() : 0;
    const int targetW = viewW - scrollbarW;

    content.setBounds(0, 0, targetW, targetH);

    const int colW = targetW / cols;
    for (size_t k = 0; k < visible.size(); ++k)
    {
        const int r = (int)k / cols;
        const int c = (int)k % cols;
        auto area = juce::Rectangle<int>(c * colW + margin, margin + r * (rowH + rowSpacing), colW - (margin * 2), rowH);
        visible[k]->label->setBounds(area.removeFromLeft(std::min(150, area.getWidth() / 2)));
        visible[k]->control->setBounds(area.reduced(0, 1));
    }
    if (footer)
        footer->setBounds(margin, margin + rowsH, targetW - margin * 2, footerHeight);
}

void ParamPanel::setFooter(std::unique_ptr<juce::Component> component, int height)
{
    footer = std::move(component);
    footerHeight = footer ? height : 0;
    if (footer)
        content.addAndMakeVisible(*footer);
    layoutContent();
}

void ParamPanel::showOnlyWhen(const juce::String &id, const juce::String &switchId, std::initializer_list<int> values)
{
    int mask = 0;
    for (int v : values)
        mask |= (1 << v);
    const auto *value = state.getRawParameterValue(switchId);
    jassert(value != nullptr);
    for (auto &r : rows)
        if (r.id == id)
        {
            r.switchValue = value;
            r.switchMask = mask;
        }
    updateVisibility();
}

void ParamPanel::updateVisibility()
{
    bool anyChanged = false;
    for (auto &r : rows)
    {
        const int v = r.switchValue != nullptr ? (int)std::lround(r.switchValue->load()) : 0;
        const bool visible = r.switchValue == nullptr || (v >= 0 && v < 31 && ((r.switchMask >> v) & 1) != 0);
        if (r.label->isVisible() != visible)
            anyChanged = true;
        r.label->setVisible(visible);
        r.control->setVisible(visible);
    }
    if (anyChanged)
        layoutContent();
}