// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
#include "MixerView.h"

using namespace grmg;
using namespace ui;

MixerView::MixerView(GRMGuitarProcessor &p) : processor(p)
{
    rebuild();
    vblank = std::make_unique<juce::VBlankAttachment>(this, [this]
                                                      { updateMixer(); });
}

void MixerView::updateMixer()
{
    if (processor.getGuitarCount() != shownCount || processor.getSelectedGuitar() != shownSelected)
        rebuild();
}

void MixerView::rebuild()
{
    using Att = juce::AudioProcessorValueTreeState;
    strips.clear();
    shownCount = processor.getGuitarCount();
    shownSelected = processor.getSelectedGuitar();
    for (int g = 0; g < shownCount; ++g)
    {
        Strip s;
        auto id = [g](const char *k)
        { return GuitarTrack::paramId(g, k); };
        s.name = std::make_unique<juce::TextButton>(processor.getGuitarName(g));
        s.name->setToggleState(g == shownSelected, juce::dontSendNotification);
        s.name->setColour(juce::TextButton::buttonOnColourId, ui::WAVE.darker(0.4f));
        s.name->onClick = [this, g]
        { processor.selectGuitar(g); };
        s.gain = std::make_unique<juce::Slider>(juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow);
        s.level = std::make_unique<juce::Slider>(juce::Slider::LinearVertical, juce::Slider::TextBoxBelow);
        s.pan = std::make_unique<juce::Slider>(juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow);
        for (auto *sl : {s.gain.get(), s.level.get(), s.pan.get()})
            sl->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 54, 16);
        s.gain->setTooltip("Gain (dB)");
        s.pan->setTooltip("Pan");
        s.level->setTooltip("Level");
        s.mute = std::make_unique<IconButton>(ui::IconId::speakerSlash, "Mute this guitar", IconButton::Behaviour::toggle);
        s.solo = std::make_unique<IconButton>(ui::IconId::headphones, "Solo: hear only the soloed guitars", IconButton::Behaviour::toggle);
        s.out = std::make_unique<juce::ComboBox>();
        s.out->addItemList({"Main", "Own output"}, 1);
        s.out->setTooltip("Main: the plugin's main output. Own output: this guitar's output (enable it in the host)");
        s.gainA = std::make_unique<Att::SliderAttachment>(processor.params, id("mix_gain"), *s.gain);
        s.levelA = std::make_unique<Att::SliderAttachment>(processor.params, id("mix_level"), *s.level);
        s.panA = std::make_unique<Att::SliderAttachment>(processor.params, id("mix_pan"), *s.pan);
        s.muteA = std::make_unique<Att::ButtonAttachment>(processor.params, id("mix_mute"), *s.mute);
        s.soloA = std::make_unique<Att::ButtonAttachment>(processor.params, id("mix_solo"), *s.solo);
        s.outA = std::make_unique<Att::ComboBoxAttachment>(processor.params, id("mix_out"), *s.out);
        for (juce::Component *c : std::initializer_list<juce::Component *>{s.name.get(), s.gain.get(), s.level.get(), s.pan.get(),
                                                                           s.mute.get(), s.solo.get(), s.out.get()})
            addAndMakeVisible(c);
        strips.push_back(std::move(s));
    }
    resized();
    repaint();
}

void MixerView::paint(juce::Graphics &g)
{
    g.fillAll(ui::PANEL);
    g.setColour(ui::GRID);
    const int w = 96;
    for (size_t k = 1; k < strips.size(); ++k)
        g.drawVerticalLine((int)k * w, 4.0f, (float)getHeight() - 4.0f);
    g.setColour(ui::TEXT);
    g.setFont(juce::Font(juce::FontOptions(10.0f)));
    for (const auto &s : strips)
    {
        g.drawText("gain dB", s.gain->getBounds().withHeight(11).translated(0, -2), juce::Justification::centredLeft);
        g.drawText("pan", s.pan->getBounds().withHeight(11).translated(0, -2), juce::Justification::centredLeft);
        g.drawText("level", s.level->getBounds().withHeight(11).translated(0, -2), juce::Justification::centredRight);
    }
}

void MixerView::resized()
{
    const int w = 96;
    for (size_t k = 0; k < strips.size(); ++k)
    {
        auto a = juce::Rectangle<int>((int)k * w, 0, w, getHeight()).reduced(4);
        auto &s = strips[k];
        s.name->setBounds(a.removeFromTop(22));
        a.removeFromTop(10);
        s.out->setBounds(a.removeFromBottom(22));
        auto ms = a.removeFromBottom(appTheme.dimensions.iconButton);
        const int iconSize = appTheme.dimensions.iconButton;
        s.mute->setBounds(ms.removeFromLeft(ms.getWidth() / 2).withSizeKeepingCentre(iconSize, iconSize));
        s.solo->setBounds(ms.withSizeKeepingCentre(iconSize, iconSize));
        auto knobs = a.removeFromLeft(a.getWidth() / 2);
        s.gain->setBounds(knobs.removeFromTop(knobs.getHeight() / 2));
        s.pan->setBounds(knobs);
        s.level->setBounds(a);
    }
}
