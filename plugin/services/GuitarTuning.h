// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// GuitarTuning.h
//
// A guitar's tuning as its parameters hold it: a tuning of the list (TUNINGS), or Custom, where each open string has
// its own note. Choosing Custom by hand starts from the tuning of the list chosen before it (the six notes are copied,
// at once from the interface, on the message thread from elsewhere), so that retuning one string from a Drop D keeps
// the others where they were. A session that loads with Custom keeps its own notes: only a gesture (a control moved
// by hand) copies.
#pragma once

#include <array>
#include <atomic>

#include <JuceHeader.h>

#include "../../constants/core.hpp"

class GuitarTuning : private juce::AudioProcessorParameter::Listener, private juce::AsyncUpdater
{
public:
    // tuningId: the tuning menu's parameter; noteIds: the open strings' notes, low string to high
    GuitarTuning(juce::AudioProcessorValueTreeState &state, const juce::String &tuningId,
                 const std::array<juce::String, grmg::NUM_STRINGS> &noteIds);
    ~GuitarTuning() override;

    // the open strings' MIDI notes now, low string to high
    std::array<int, grmg::NUM_STRINGS> notes() const;

private:
    void parameterValueChanged(int, float) override;
    void parameterGestureChanged(int, bool starting) override;
    void handleAsyncUpdate() override;
    void copy(int from);   // the six notes of the list's tuning `from` into the Custom notes
    int listIndex() const; // the tuning menu's entry (CUSTOM_TUNING: Custom)

    juce::RangedAudioParameter *tuning = nullptr;
    std::array<juce::RangedAudioParameter *, grmg::NUM_STRINGS> openNotes{};
    std::atomic<bool> gesture{false};
    std::atomic<int> listed{0};   // the last tuning of the list chosen
    std::atomic<int> copyFrom{-1}; // a copy to make on the message thread (-1: none)
};
