// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// The files embedded in the plugin (assets/), found by their original name.
#pragma once

#include <JuceHeader.h>
#include <memory>

namespace Assets
{
    // the SVG file of that name, parsed; nullptr when it is not embedded
    std::unique_ptr<juce::XmlElement> svg(const char *file);
}
