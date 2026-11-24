// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
#include "../PluginProcessor.h"

class NetworksView : public juce::Component
{
public:
    explicit NetworksView(GRMGuitarProcessor &p);
    void resized() override { text.setBounds(getLocalBounds().reduced(4)); }

private:
    GRMGuitarProcessor &processor;
    juce::TextEditor text;
    const void *shown = nullptr;
    void updateNetworks();
    std::unique_ptr<juce::VBlankAttachment> vblank;
};