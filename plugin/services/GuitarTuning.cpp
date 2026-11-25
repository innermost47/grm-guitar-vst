// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
#include "GuitarTuning.h"

using namespace grmg;

GuitarTuning::GuitarTuning(juce::AudioProcessorValueTreeState &state, const juce::String &tuningId,
                           const std::array<juce::String, NUM_STRINGS> &noteIds)
    : tuning(state.getParameter(tuningId))
{
    for (size_t i = 0; i < openNotes.size(); ++i)
        openNotes[i] = state.getParameter(noteIds[i]);
    jassert(tuning != nullptr);
    const int now = listIndex();
    listed.store(now < CUSTOM_TUNING ? now : 0);
    tuning->addListener(this);
}

GuitarTuning::~GuitarTuning()
{
    tuning->removeListener(this);
    cancelPendingUpdate();
}

int GuitarTuning::listIndex() const
{
    return juce::jlimit(0, CUSTOM_TUNING, (int)std::lround(tuning->convertFrom0to1(tuning->getValue())));
}

std::array<int, NUM_STRINGS> GuitarTuning::notes() const
{
    const int t = listIndex();
    if (t < CUSTOM_TUNING)
        return tuning_notes(t);
    std::array<int, NUM_STRINGS> n{};
    for (size_t i = 0; i < n.size(); ++i)
        n[i] = OPEN_NOTE_LOWEST + (int)std::lround(openNotes[i]->convertFrom0to1(openNotes[i]->getValue()));
    return n;
}

void GuitarTuning::parameterGestureChanged(int, bool starting)
{
    gesture.store(starting);
}

void GuitarTuning::parameterValueChanged(int, float)
{
    const int t = listIndex();
    if (t < CUSTOM_TUNING)
        listed.store(t);
    else if (gesture.load())
    {
        // by hand in the interface: at once; from another thread (a host's gesture): on the message thread
        if (juce::MessageManager::existsAndIsCurrentThread())
            copy(listed.load());
        else
        {
            copyFrom.store(listed.load());
            triggerAsyncUpdate();
        }
    }
}

void GuitarTuning::handleAsyncUpdate()
{
    const int from = copyFrom.exchange(-1);
    if (from >= 0)
        copy(from);
}

void GuitarTuning::copy(int from)
{
    const auto notes = tuning_notes(from);
    for (size_t i = 0; i < openNotes.size(); ++i)
    {
        auto *p = openNotes[i];
        p->beginChangeGesture();
        p->setValueNotifyingHost(p->convertTo0to1((float)(notes[i] - OPEN_NOTE_LOWEST)));
        p->endChangeGesture();
    }
}
