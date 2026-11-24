// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
#pragma once

#include <JuceHeader.h>

class GRMGuitarProcessor;

class SolverThread : public juce::Thread
{
public:
    explicit SolverThread(GRMGuitarProcessor &processor);
    ~SolverThread() override;

    void run() override;

private:
    GRMGuitarProcessor &owner;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SolverThread)
};