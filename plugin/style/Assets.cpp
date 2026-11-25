// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
#include "Assets.h"
#include "BinaryData.h"

namespace
{
    // every embedded file whose original name passes `wanted`, parsed as XML
    template <class Wanted>
    std::vector<std::unique_ptr<juce::XmlElement>> parsed(Wanted &&wanted)
    {
        std::vector<std::unique_ptr<juce::XmlElement>> found;
        for (int k = 0; k < BinaryData::namedResourceListSize; ++k)
        {
            const char *resource = BinaryData::namedResourceList[k];
            if (!wanted(juce::String(BinaryData::getNamedResourceOriginalFilename(resource))))
                continue;
            int size = 0;
            const char *data = BinaryData::getNamedResource(resource, size);
            if (auto xml = juce::parseXML(juce::String::fromUTF8(data, size)))
                found.push_back(std::move(xml));
        }
        return found;
    }
}

std::unique_ptr<juce::XmlElement> Assets::svg(const char *file)
{
    auto found = parsed([file](const juce::String &name)
                        { return name == file; });
    return found.empty() ? nullptr : std::move(found.front());
}

std::vector<std::unique_ptr<juce::XmlElement>> Assets::xmlFiles(const char *extension)
{
    return parsed([extension](const juce::String &name)
                  { return name.endsWithIgnoreCase(extension); });
}
