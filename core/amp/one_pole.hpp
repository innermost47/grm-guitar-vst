// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// one_pole.hpp
//
// A first-order filter, H(s) = (b0 s + b1 w) / (s + w), by the bilinear transform with the corner prewarped: what
// a resistor and a capacitor do in the amplifier (coupling caps, Miller capacitance, cathode and bright caps).
#pragma once

#include <cmath>

#include "../../constants/core.hpp"

namespace grmg
{
    class OnePole
    {
    public:
        void set_lowpass(double sr, double hz) { set(sr, hz, 0.0, 1.0); }
        void set_highpass(double sr, double hz) { set(sr, hz, 1.0, 0.0); }
        // the gain below the corner over the gain above it
        void set_low_shelf(double sr, double hz, double gain) { set(sr, hz, 1.0, gain); }
        // the gain above the corner over the gain below it
        void set_high_shelf(double sr, double hz, double gain) { set(sr, hz, gain, 1.0); }

        void reset() { z_ = 0.0; }

        double process(double x)
        {
            const double y = b0_ * x + z_;
            z_ = b1_ * x - a1_ * y;
            return y;
        }

    private:
        double b0_ = 1.0, b1_ = 0.0, a1_ = 0.0, z_ = 0.0;

        // H(s) = (high s + low w) / (s + w): `high` its gain far above the corner, `low` far below
        void set(double sr, double hz, double high, double low)
        {
            const double w = std::tan(PI * std::min(hz, 0.49 * sr) / sr); // prewarped, s = (1 - z^-1) / (1 + z^-1)
            const double norm = 1.0 / (1.0 + w);
            b0_ = (high + low * w) * norm;
            b1_ = (low * w - high) * norm;
            a1_ = (w - 1.0) * norm;
        }
    };
}
