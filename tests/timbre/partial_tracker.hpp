// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// partial_tracker.hpp
//
// One partial of a note followed through time: the sound is moved down by the partial's expected frequency and
// averaged over a Hann window long enough to leave its neighbours out (`periods` periods of the fundamental). Its
// modulus is the partial's amplitude, the turn of its phase from one frame to the next its true frequency. From
// them: its decay (T60, a straight line through its level in dB) and its pitch.
#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

#include "../../constants/core.hpp"

namespace grmg::timbre
{
    class PartialTracker
    {
    public:
        PartialTracker(double sr, double hop_s = 0.01) : sr_(sr), hop_((size_t)std::max(1.0, hop_s * sr)) {}

        // the partial near `f` of a note of fundamental `f1`, from `from_s` to `to_s`
        void track(const std::vector<double> &x, double f, double f1, double from_s, double to_s, double periods = 6.0)
        {
            t_.clear();
            amp_.clear();
            freq_.clear();
            const size_t n = (size_t)std::max(8.0, periods * sr_ / f1);
            std::vector<double> w(n);
            double sum = 0.0;
            for (size_t j = 0; j < n; ++j)
                sum += (w[j] = 0.5 - 0.5 * std::cos(2 * PI * (double)j / (double)(n - 1)));
            const double turn = 2 * PI * f / sr_;
            double last_phase = 0.0;
            for (size_t s = (size_t)std::max(0.0, from_s * sr_); s + n < x.size() && (double)s / sr_ < to_s; s += hop_)
            {
                double re = 0.0, im = 0.0;
                for (size_t j = 0; j < n; ++j)
                {
                    const double a = -turn * (double)(s + j);
                    re += w[j] * x[s + j] * std::cos(a);
                    im += w[j] * x[s + j] * std::sin(a);
                }
                const double phase = std::atan2(im, re);
                if (!t_.empty())
                {
                    double d = phase - last_phase;
                    d -= 2 * PI * std::round(d / (2 * PI));
                    freq_.push_back(f + d * sr_ / (2 * PI * (double)hop_));
                }
                else
                {
                    freq_.push_back(f);
                }
                last_phase = phase;
                t_.push_back(((double)s + 0.5 * (double)n) / sr_);
                amp_.push_back(2.0 * std::hypot(re, im) / sum);
            }
            if (freq_.size() > 1)
                freq_[0] = freq_[1]; // the first frame has no turn of its own
        }

        const std::vector<double> &times() const { return t_; }
        const std::vector<double> &amplitudes() const { return amp_; }
        const std::vector<double> &frequencies() const { return freq_; }

        // seconds to fall by 60 dB: a line through the level in dB, from `after_peak_s` past the loudest frame down to
        // `range_db` below it (or the end); 0 when there is too little to fit
        double t60(double after_peak_s = 0.05, double range_db = 40.0) const
        {
            if (amp_.size() < 5)
                return 0.0;
            const size_t peak = (size_t)(std::max_element(amp_.begin(), amp_.end()) - amp_.begin());
            const double top = db(amp_[peak]);
            double sx = 0, sy = 0, sxx = 0, sxy = 0;
            int count = 0;
            for (size_t k = peak; k < amp_.size(); ++k)
            {
                if (t_[k] < t_[peak] + after_peak_s)
                    continue;
                const double y = db(amp_[k]);
                if (y < top - range_db)
                    break;
                sx += t_[k];
                sy += y;
                sxx += t_[k] * t_[k];
                sxy += t_[k] * y;
                ++count;
            }
            if (count < 5)
                return 0.0;
            const double slope = (count * sxy - sx * sy) / (count * sxx - sx * sx);
            return slope < 0.0 ? -60.0 / slope : 1e9;
        }

        // the median frequency of the frames between two times (Hz), 0 when there is none
        double median_frequency(double from_s, double to_s) const
        {
            std::vector<double> f;
            for (size_t k = 0; k < t_.size(); ++k)
                if (t_[k] >= from_s && t_[k] <= to_s)
                    f.push_back(freq_[k]);
            if (f.empty())
                return 0.0;
            std::nth_element(f.begin(), f.begin() + (long)(f.size() / 2), f.end());
            return f[f.size() / 2];
        }

        static double cents(double f, double reference) { return f > 0 && reference > 0 ? 1200.0 * std::log2(f / reference) : 0.0; }

    private:
        double sr_;
        size_t hop_;
        std::vector<double> t_, amp_, freq_;

        static double db(double a) { return 20.0 * std::log10(a + 1e-15); }
    };
}
