// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
#include "Assets.h"
#include "BinaryData.h"

std::unique_ptr<juce::XmlElement> Assets::svg(const char *file)
{
    for (int k = 0; k < BinaryData::namedResourceListSize; ++k)
    {
        const char *resource = BinaryData::namedResourceList[k];
        if (juce::String(BinaryData::getNamedResourceOriginalFilename(resource)) != file)
            continue;
        int size = 0;
        const char *data = BinaryData::getNamedResource(resource, size);
        return juce::parseXML(juce::String::fromUTF8(data, size));
    }
    return nullptr;
}
