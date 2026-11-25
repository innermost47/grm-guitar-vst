// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
#include "EquationsPanel.h"
#include "Assets.h"

EquationsPanel::EquationsPanel()
    : link("grm-core", juce::URL(ui::GRM_CORE_PAGE))
{
    if (const auto svg = Assets::svg(ui::EQUATIONS_FILE))
    {
        equations = juce::Drawable::createFromSVG(*svg);
        if (equations != nullptr)
            equations->replaceColour(juce::Colours::black, ui::TEXT);
    }
    link.setFont(ui::font(appTheme.typography.xs), false, juce::Justification::centred);
    link.setColour(juce::HyperlinkButton::textColourId, ui::TEXT.withAlpha(ui::MODAL_LINK_ALPHA));
    link.setTooltip("The GRM engine that brought these equations up from a network of known physics");
    addAndMakeVisible(link);
}

void EquationsPanel::paint(juce::Graphics &g)
{
    if (equations == nullptr)
        return;
    const auto area = getLocalBounds()
                          .withTrimmedBottom(appTheme.dimensions.modalRowHeight)
                          .reduced(appTheme.spacing.xl)
                          .toFloat();
    equations->drawWithin(g, area, juce::RectanglePlacement::centred, ui::EQUATIONS_ALPHA);
}

void EquationsPanel::resized()
{
    link.setBounds(getLocalBounds().removeFromBottom(appTheme.dimensions.modalRowHeight));
}
