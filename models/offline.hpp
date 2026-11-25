// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// offline.hpp
//
// What an offline render is asked and what it gives back (core/offline_renderer.hpp): notes played live, as a MIDI
// keyboard would, and the engine's two outputs.
#pragma once

#include <vector>

namespace grmg::models
{
    // a key pressed at `at_s` and released at `off_s` (seconds), as Engine::note_on / note_off receive it
    struct LiveNote
    {
        double at_s = 0.0, off_s = 1.0;
        int midi = 40;
        double velocity = 0.8;
        int channel = 1;
    };

    // the engine's outputs: the strings' force on the bridge (newtons) and the jack (pickups, cable, amplifier)
    struct OfflineTake
    {
        std::vector<double> force, pickup;
        double busy_s = 0.0; // the time the engine took to render it
    };
}
