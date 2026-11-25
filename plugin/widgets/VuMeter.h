// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// VuMeter.h
//
// A stereo level meter: two columns of segments (green, orange, red: constants/ui.hpp vu), the peak held a moment,
// a clip light on top. It does not refresh itself: its owner gives it the peaks of each frame (from its own
// VBlankAttachment), it rises at once and falls slowly.
#pragma once

#include <JuceHeader.h>

#include "../../constants/ui.hpp"

class VuMeter : public juce::Component
{
public:
    VuMeter();
    // the peaks (linear, 1 = full scale) since the last frame
    void setPeaks(float left, float right);
    void paint(juce::Graphics &) override;

private:
    struct Side
    {
        float level = 0.0f, held = 0.0f;
        int hold = 0;
        void take(float peak); // one frame
    };
    Side left, right;
    void paintSide(juce::Graphics &g, juce::Rectangle<float> area, const Side &side) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VuMeter)
};
