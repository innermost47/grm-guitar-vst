// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// stroke_spread.hpp
//
// How much the same stroke, played again and again, differs from itself: for each stroke, its onset against the
// grid, its level just after it and its brightness. A player never strikes twice the same way; a machine does. The
// spread of these values (standard deviations) says how far the sound is from "the same sample played again".
//
// An onset is found where the attack's broadband click rises out of what was ringing (the sound above
// ONSET_HP_HZ): the low notes' own waves are longer than the attack, a plain envelope would follow them. A
// saturated amplifier fills the highs all the time: its strokes are measured at the onsets found before it.
#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

#include "../../models/core.hpp"
#include "spectrum_probe.hpp"

namespace grmg::timbre
{
    class StrokeSpread
    {
    public:
        static constexpr double BEFORE_S = 0.03, AFTER_S = 0.06; // where a stroke is looked for, around its time
        static constexpr double BASE_S = 0.01;                     // what rang before it: the first of this
        static constexpr double ONSET_RISE = 0.5;                  // of the way from what rang to the attack's peak
        static constexpr double ONSET_HP_HZ = 1000.0, ENVELOPE_S = 0.002;
        static constexpr double LEVEL_S = 0.05; // the level is the RMS over this
        static constexpr size_t CENTROID_SIZE = 2048;

        explicit StrokeSpread(double sr) : sr_(sr), spectrum_(sr) {}

        // the onsets (seconds) of the strokes expected at `times_s` in x
        std::vector<double> find_onsets(const std::vector<double> &x, const std::vector<double> &times_s) const
        {
            models::Biquad hp;
            hp.set_highpass(sr_, ONSET_HP_HZ, 0.7071);
            const size_t width = (size_t)std::max(1.0, ENVELOPE_S * sr_);
            std::vector<double> env(x.size(), 0.0);
            double sum = 0.0;
            std::vector<double> squares(x.size());
            for (size_t i = 0; i < x.size(); ++i)
            {
                const double y = hp.process(x[i]);
                squares[i] = y * y;
                sum += squares[i];
                if (i >= width)
                    sum -= squares[i - width];
                env[i] = std::sqrt(std::max(sum, 0.0) / (double)width);
            }
            std::vector<double> onsets;
            for (double t : times_s)
            {
                const size_t a = (size_t)std::max(0.0, (t - BEFORE_S) * sr_);
                const size_t b = std::min(x.size(), (size_t)((t + AFTER_S) * sr_));
                const size_t base_end = std::min(b, a + (size_t)(BASE_S * sr_));
                if (base_end <= a)
                {
                    onsets.push_back(t);
                    continue;
                }
                std::vector<double> before(env.begin() + (long)a, env.begin() + (long)base_end);
                std::nth_element(before.begin(), before.begin() + (long)(before.size() / 2), before.end());
                const double base = before[before.size() / 2];
                const double peak = *std::max_element(env.begin() + (long)a, env.begin() + (long)b);
                size_t onset = a;
                while (onset < b && env[onset] <= base + ONSET_RISE * (peak - base))
                    ++onset;
                // the envelope lags the click by half its width
                onsets.push_back(((double)onset - 0.5 * (double)width) / sr_);
            }
            return onsets;
        }

        // the strokes expected at `times_s`, that started at `onsets_s`
        void measure(const std::vector<double> &x, const std::vector<double> &times_s, const std::vector<double> &onsets_s)
        {
            onset_ms_.clear();
            level_db_.clear();
            centroid_hz_.clear();
            bands_.clear();
            for (size_t k = 0; k < times_s.size() && k < onsets_s.size(); ++k)
            {
                const size_t onset = (size_t)std::max(0.0, onsets_s[k] * sr_);
                const size_t end = std::min(x.size(), onset + (size_t)(LEVEL_S * sr_));
                if (end <= onset)
                    continue;
                double energy = 0.0;
                for (size_t i = onset; i < end; ++i)
                    energy += x[i] * x[i];
                onset_ms_.push_back(1000.0 * (onsets_s[k] - times_s[k]));
                level_db_.push_back(10.0 * std::log10(energy / (double)(end - onset) + 1e-30));
                centroid_hz_.push_back(spectrum_.centroid(x, onset, CENTROID_SIZE));
                bands_.push_back(spectrum_.band_levels_db(x, onset, CENTROID_SIZE));
            }
        }

        size_t strokes() const { return level_db_.size(); }
        double onset_sd_ms() const { return sd(onset_ms_); }
        double level_sd_db() const { return sd(level_db_); }
        // the brightness's spread, as a share of its mean (%)
        double centroid_sd_pct() const
        {
            const double m = mean(centroid_hz_);
            return m > 0.0 ? 100.0 * sd(centroid_hz_) / m : 0.0;
        }
        double centroid_mean_hz() const { return mean(centroid_hz_); }

        // how far each stroke's spectrum (third-octave bands) lies from the strokes' mean spectrum, its overall
        // level set aside: the RMS of the bands' differences (dB), averaged over the strokes. What changes in the
        // colour of a stroke, not in its loudness.
        double spectral_spread_db() const
        {
            if (bands_.size() < 2)
                return 0.0;
            const size_t count = bands_.front().size();
            std::vector<std::vector<double>> shapes;
            for (const auto &b : bands_)
            {
                const double level = mean(b);
                std::vector<double> shape(count);
                for (size_t k = 0; k < count; ++k)
                    shape[k] = b[k] - level;
                shapes.push_back(shape);
            }
            std::vector<double> average(count, 0.0);
            for (const auto &shape : shapes)
                for (size_t k = 0; k < count; ++k)
                    average[k] += shape[k] / (double)shapes.size();
            double total = 0.0;
            for (const auto &shape : shapes)
            {
                double squares = 0.0;
                for (size_t k = 0; k < count; ++k)
                    squares += (shape[k] - average[k]) * (shape[k] - average[k]);
                total += std::sqrt(squares / (double)count);
            }
            return total / (double)shapes.size();
        }

    private:
        double sr_;
        SpectrumProbe spectrum_;
        std::vector<double> onset_ms_, level_db_, centroid_hz_;
        std::vector<std::vector<double>> bands_; // each stroke's third-octave levels

        static double mean(const std::vector<double> &v)
        {
            if (v.empty())
                return 0.0;
            double s = 0.0;
            for (double x : v)
                s += x;
            return s / (double)v.size();
        }
        static double sd(const std::vector<double> &v)
        {
            if (v.size() < 2)
                return 0.0;
            const double m = mean(v);
            double s = 0.0;
            for (double x : v)
                s += (x - m) * (x - m);
            return std::sqrt(s / (double)(v.size() - 1));
        }
    };
}
