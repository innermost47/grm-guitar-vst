// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// The six strings of the Python app (grm_guitar.py, STRINGS): equal tension, 648 mm. The golden values
// (tests/golden.hpp) were computed on them, so the parity checks use them, whatever set the plugin holds.
#pragma once

namespace python_app
{
    struct String
    {
        double L, mu, T, target;
        bool wound;
    };

    inline constexpr String STRINGS[6] = {
        {0.648, 0.0061370673, 70.0, 82.4069, true}, {0.648, 0.0034443134, 70.0, 110.0, true},
        {0.648, 0.0019330657, 70.0, 146.832, true}, {0.648, 0.0010848655, 70.0, 196.0, false},
        {0.648, 0.0006834364, 70.0, 246.942, false}, {0.648, 0.0003835658, 70.0, 329.628, false},
    };

    // the Python app's wound core: a share of the diameter of an all-steel string of the same mass
    inline constexpr double CORE = 0.45;
} // namespace python_app
