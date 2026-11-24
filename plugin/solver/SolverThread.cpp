// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
#include "PluginProcessor.h"
#include "SolverThread.h"

SolverThread::SolverThread(GRMGuitarProcessor &processor)
    : juce::Thread("SolverThread"), owner(processor)
{
}

SolverThread::~SolverThread()
{
    stopThread(1000);
}

void SolverThread::run()
{
    while (!threadShouldExit())
    {
        const double sampleRateValue = owner.sampleRate.load();
        if (sampleRateValue <= 0.0)
        {
            wait(50);
            continue;
        }

        const unsigned currentSeed = owner.seed.load();
        const double tempo = owner.currentTempo.load();
        const auto *loopParam = owner.params.getRawParameterValue("loop");
        const bool loop = loopParam != nullptr && loopParam->load() > 0.5f;
        const int count = owner.guitarCount.load();
        const int first = owner.selected.load();

        for (int index = 0; index < count && !threadShouldExit(); ++index)
        {
            const int guitarIndex = (first + index) % count;
            owner.tracks[static_cast<size_t>(guitarIndex)]->solveStep(
                sampleRateValue,
                currentSeed,
                tempo,
                loop,
                owner.solving,
                owner.solvedStage,
                [this]
                { return threadShouldExit(); });
        }

        for (int guitarIndex = count; guitarIndex < GRMGuitarProcessor::MAX_GUITARS; ++guitarIndex)
        {
            owner.tracks[static_cast<size_t>(guitarIndex)]->collect();
        }

        wait(30);
    }
}