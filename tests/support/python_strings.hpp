// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// What the Python app plays: its six strings (grm_guitar.py, STRINGS: equal tension, 648 mm) and its player's
// accents. The golden values (tests/golden.hpp) were computed on them, so the parity checks use them, whatever
// the plugin plays.
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

    // the Python app's accents (dex of force) on the eight sixteenths of two beats
    inline constexpr double ACCENT_DEX[8] = {0.06, -0.04, 0.0, -0.04, 0.03, -0.04, 0.0, -0.04};
} // namespace python_app
