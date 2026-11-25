// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
#include "MixerView.h"

#include "CustomLookAndFeel.h"

using namespace grmg;
using namespace ui;

MixerView::MixerView(GRMGuitarProcessor &p) : processor(p)
{
    rebuild();
    vblank = std::make_unique<juce::VBlankAttachment>(this, [this]
                                                      { refresh(); });
}

void MixerView::refresh()
{
    if (processor.getGuitarCount() != shownCount || processor.getSelectedGuitar() != shownSelected)
        rebuild();
    for (size_t g = 0; g < strips.size(); ++g)
    {
        const auto [l, r] = processor.takeStripPeaks((int)g);
        strips[g].meter->setPeaks(l, r);
    }
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
        s.level->getProperties().set(CustomLookAndFeel::getDrawTicksPropertyId(), ui::FADER_TICKS);
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
        s.meter = std::make_unique<VuMeter>();
        for (juce::Component *c : std::initializer_list<juce::Component *>{s.name.get(), s.gain.get(), s.level.get(), s.pan.get(),
                                                                           s.mute.get(), s.solo.get(), s.out.get(), s.meter.get()})
            addAndMakeVisible(c);
        strips.push_back(std::move(s));
    }
    // as wide as its strips: the container it sits in scrolls through them
    setSize(preferredWidth(), getHeight());
    resized();
    repaint();
}

int MixerView::preferredWidth() const
{
    return juce::jmax(1, (int)strips.size()) * appTheme.dimensions.mixerStripWidth;
}

void MixerView::paint(juce::Graphics &g)
{
    g.fillAll(ui::PANEL);
    g.setColour(ui::GRID);
    const int w = appTheme.dimensions.mixerStripWidth;
    for (size_t k = 1; k < strips.size(); ++k)
        g.drawVerticalLine((int)k * w, (float)appTheme.spacing.lg, (float)(getHeight() - appTheme.spacing.lg));
    g.setColour(ui::TEXT);
    g.setFont(ui::font(appTheme.typography.small));
    for (const auto &s : strips)
    {
        g.drawText("gain dB", s.gain->getBounds().withHeight(12).translated(0, -12), juce::Justification::centred);
        g.drawText("pan", s.pan->getBounds().withHeight(12).translated(0, -12), juce::Justification::centred);
    }
}

// a strip: its name; its level fader beside its meter (the most room); gain and pan; mute and solo; its output
void MixerView::resized()
{
    const int w = appTheme.dimensions.mixerStripWidth, iconSize = appTheme.dimensions.iconButton;
    for (size_t k = 0; k < strips.size(); ++k)
    {
        auto a = juce::Rectangle<int>((int)k * w, 0, w, getHeight()).reduced(appTheme.spacing.md);
        auto &s = strips[k];
        s.name->setBounds(a.removeFromTop(appTheme.dimensions.modalRowHeight));
        a.removeFromTop(appTheme.spacing.md);
        s.out->setBounds(a.removeFromBottom(appTheme.dimensions.modalRowHeight));
        a.removeFromBottom(appTheme.spacing.base);
        auto ms = a.removeFromBottom(iconSize);
        s.mute->setBounds(ms.removeFromLeft(ms.getWidth() / 2).withSizeKeepingCentre(iconSize, iconSize));
        s.solo->setBounds(ms.withSizeKeepingCentre(iconSize, iconSize));
        a.removeFromBottom(appTheme.spacing.base);
        auto knobs = a.removeFromBottom(juce::jmin(64, a.getHeight() / 3));
        s.gain->setBounds(knobs.removeFromLeft(knobs.getWidth() / 2).withTrimmedTop(12));
        s.pan->setBounds(knobs.withTrimmedTop(12));
        a.removeFromBottom(appTheme.spacing.base);
        s.meter->setBounds(a.removeFromRight(appTheme.dimensions.vuMeterWidth));
        a.removeFromRight(appTheme.spacing.base);
        s.level->setBounds(a);
    }
}
