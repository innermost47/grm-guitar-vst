// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// halfband_fir.hpp
//
// One octave of oversampling, up and down: a linear-phase halfband FIR (windowed sinc, Kaiser window). Every other
// tap of a halfband filter is zero and its centre is 1/2, so each direction costs half its taps:
//   - up: the even output is the filter of the input on the non-zero taps, the odd one the input delayed;
//   - down: the same taps on the even inputs, plus the centre tap on the odd ones.
#pragma once

#include <cmath>
#include <vector>

#include "../../constants/core.hpp"

namespace grmg
{
    class HalfbandFir
    {
    public:
        // taps: 4 K + 3 (the centre on an odd tap)
        void design(int taps)
        {
            taps_ = taps;
            const int centre = (taps - 1) / 2;
            half_ = centre / 2 + 1; // the non-zero taps off the centre: h[0], h[2] ... h[taps - 1]
            even_.assign((size_t)half_, 0.0);
            const double i0_beta = bessel_i0(HALFBAND_KAISER_BETA);
            for (int j = 0; j < half_; ++j)
            {
                const int n = 2 * j;
                const double x = (n - centre) / 2.0;
                const double sinc = std::sin(PI * x) / (PI * x);
                const double r = (double)(n - centre) / centre;
                const double window = bessel_i0(HALFBAND_KAISER_BETA * std::sqrt(std::max(0.0, 1.0 - r * r))) / i0_beta;
                even_[(size_t)j] = 0.5 * sinc * window;
            }
            // unit gain at DC: the even taps sum to 1/2, as the centre does
            double sum = 0.0;
            for (double h : even_)
                sum += h;
            for (double &h : even_)
                h *= 0.5 / sum;
            delay_ = (centre - 1) / 2;
            up_.assign((size_t)(2 * half_), 0.0);
            down_even_.assign((size_t)(2 * half_), 0.0);
            down_odd_.assign((size_t)(2 * (delay_ + 2)), 0.0);
            reset();
        }

        void reset()
        {
            std::fill(up_.begin(), up_.end(), 0.0);
            std::fill(down_even_.begin(), down_even_.end(), 0.0);
            std::fill(down_odd_.begin(), down_odd_.end(), 0.0);
            up_pos_ = down_pos_ = odd_pos_ = 0;
        }

        // one input sample, two output samples at twice the rate
        void up(double x, double &first, double &second)
        {
            push(up_, up_pos_, half_, x);
            double sum = 0.0;
            for (int j = 0; j < half_; ++j)
                sum += even_[(size_t)j] * up_[(size_t)(up_pos_ + j)];
            first = 2.0 * sum;
            second = up_[(size_t)(up_pos_ + delay_)];
        }

        // two input samples, one output sample at half the rate
        double down(double first, double second)
        {
            push(down_even_, down_pos_, half_, first);
            const int odd_len = delay_ + 2;
            push(down_odd_, odd_pos_, odd_len, second);
            double sum = 0.0;
            for (int j = 0; j < half_; ++j)
                sum += even_[(size_t)j] * down_even_[(size_t)(down_pos_ + j)];
            return sum + 0.5 * down_odd_[(size_t)(odd_pos_ + delay_ + 1)];
        }

        int taps() const { return taps_; }

    private:
        int taps_ = 0, half_ = 0, delay_ = 0;
        std::vector<double> even_;
        // histories written twice (at p and p + length) so that a read never wraps; newest at p
        std::vector<double> up_, down_even_, down_odd_;
        int up_pos_ = 0, down_pos_ = 0, odd_pos_ = 0;

        static void push(std::vector<double> &line, int &pos, int length, double x)
        {
            pos = pos == 0 ? length - 1 : pos - 1;
            line[(size_t)pos] = x;
            line[(size_t)(pos + length)] = x;
        }

        static double bessel_i0(double x)
        {
            double sum = 1.0, term = 1.0;
            for (int k = 1; k < 50; ++k)
            {
                term *= (x / (2.0 * k)) * (x / (2.0 * k));
                sum += term;
                if (term < 1e-12 * sum)
                    break;
            }
            return sum;
        }
    };
}
