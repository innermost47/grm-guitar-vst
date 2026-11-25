// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// The files embedded in the plugin (assets/), found by their original name.
#pragma once

#include <JuceHeader.h>
#include <memory>
#include <vector>

namespace Assets
{
    // the SVG file of that name, parsed; nullptr when it is not embedded
    std::unique_ptr<juce::XmlElement> svg(const char *file);
    // every embedded file whose name ends with `extension` (".grmpreset"), parsed
    std::vector<std::unique_ptr<juce::XmlElement>> xmlFiles(const char *extension);
}
