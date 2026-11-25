// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// sustain_movement.hpp
//
// How much a held sound moves inside: the level of each third-octave band followed through time (frames of
// FRAME samples, every HOP_S), its slow decay taken out (a straight line in dB), what is left measured (its standard
// deviation, dB) and averaged over the bands. Strings slightly out of tune with each other beat, a saturated chord
// breathes; a static chord is a flat line once its decay is removed.
#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

#include "spectrum_probe.hpp"

namespace grmg::timbre
{
    class SustainMovement
    {
    public:
        static constexpr size_t FRAME = 2048;
        static constexpr double HOP_S = 0.01;

        explicit SustainMovement(double sr) : sr_(sr), spectrum_(sr) {}

        // the movement (dB) of x between `from_s` and `to_s`, over the bands from `lo` to `hi` Hz
        double measure(const std::vector<double> &x, double from_s, double to_s, double lo = 250.0, double hi = 4000.0) const
        {
            std::vector<std::vector<double>> frames; // each frame's band levels
            std::vector<double> times;
            const size_t hop = (size_t)std::max(1.0, HOP_S * sr_);
            for (size_t s = (size_t)(from_s * sr_); s + FRAME < x.size() && (double)s / sr_ < to_s; s += hop)
            {
                frames.push_back(spectrum_.band_levels_db(x, s, FRAME, lo, hi));
                times.push_back((double)s / sr_);
            }
            if (frames.size() < 8)
                return 0.0;
            const size_t bands = frames.front().size();
            double total = 0.0;
            for (size_t b = 0; b < bands; ++b)
            {
                std::vector<double> y(frames.size());
                for (size_t k = 0; k < frames.size(); ++k)
                    y[k] = frames[k][b];
                total += detrended_sd(times, y);
            }
            return total / (double)bands;
        }

    private:
        double sr_;
        SpectrumProbe spectrum_;

        // the standard deviation of y around its least-squares line through t
        static double detrended_sd(const std::vector<double> &t, const std::vector<double> &y)
        {
            const double n = (double)y.size();
            double st = 0, sy = 0, stt = 0, sty = 0;
            for (size_t k = 0; k < y.size(); ++k)
            {
                st += t[k];
                sy += y[k];
                stt += t[k] * t[k];
                sty += t[k] * y[k];
            }
            const double slope = (n * sty - st * sy) / (n * stt - st * st);
            const double offset = (sy - slope * st) / n;
            double squares = 0.0;
            for (size_t k = 0; k < y.size(); ++k)
            {
                const double r = y[k] - (offset + slope * t[k]);
                squares += r * r;
            }
            return std::sqrt(squares / (n - 1.0));
        }
    };
}
