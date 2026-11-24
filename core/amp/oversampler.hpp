// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// oversampler.hpp
//
// Four times the sample rate for the tubes and the diodes, whose harmonics would otherwise fold back below Nyquist
// (the "digital" fizz): two halfband octaves, a sharp one at the original Nyquist, a short one above it.
#pragma once

#include <array>

#include "halfband_fir.hpp"

namespace grmg
{
    class Oversampler
    {
    public:
        static constexpr int FACTOR = AMP_OVERSAMPLE;

        void prepare()
        {
            first_up_.design(HALFBAND_TAPS[0]);
            first_down_.design(HALFBAND_TAPS[0]);
            second_up_.design(HALFBAND_TAPS[1]);
            second_down_.design(HALFBAND_TAPS[1]);
        }

        void reset()
        {
            first_up_.reset();
            first_down_.reset();
            second_up_.reset();
            second_down_.reset();
        }

        void up(double x, std::array<double, FACTOR> &out)
        {
            double a = 0.0, b = 0.0;
            first_up_.up(x, a, b);
            second_up_.up(a, out[0], out[1]);
            second_up_.up(b, out[2], out[3]);
        }

        double down(const std::array<double, FACTOR> &in)
        {
            const double a = second_down_.down(in[0], in[1]);
            const double b = second_down_.down(in[2], in[3]);
            return first_down_.down(a, b);
        }

    private:
        HalfbandFir first_up_, first_down_, second_up_, second_down_;
    };
}
