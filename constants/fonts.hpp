// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
#pragma once
#include <JuceHeader.h>
#include <BinaryData.h>

namespace ui::theme
{
    struct FontFamily
    {
        juce::Typeface::Ptr notoBold;
        juce::Typeface::Ptr notoItalic;
        juce::Typeface::Ptr notoRegular;
        juce::Typeface::Ptr michroma;

        static FontFamily load()
        {
            return FontFamily{
                juce::Typeface::createSystemTypefaceFor(BinaryData::notobold_ttf, BinaryData::notobold_ttfSize),
                juce::Typeface::createSystemTypefaceFor(BinaryData::notoitalic_ttf, BinaryData::notoitalic_ttfSize),
                juce::Typeface::createSystemTypefaceFor(BinaryData::notoregular_ttf, BinaryData::notoregular_ttfSize),
                juce::Typeface::createSystemTypefaceFor(BinaryData::michroma_ttf, BinaryData::michroma_ttfSize)};
        }

        static void applyFontSize(juce::Label &label, float newSize)
        {
            label.setFont(label.getFont().withHeight(newSize));
        }
    };
}