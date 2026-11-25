// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// tonal_balance.hpp
//
// A sound's tonal balance: its third-octave bands (100 Hz to 8 kHz) averaged over its loud frames, in dB under its
// loudest band. Two balances compare whatever their levels or their sample rates: how far a render lies from a real
// guitar's recording, over the whole range and above 2 kHz (where a pickup's window, coils and circuit show).
#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

#include "spectrum_probe.hpp"

namespace grmg::timbre
{
    class TonalBalance
    {
    public:
        static constexpr size_t FRAME = 2048;
        static constexpr double LOUD = 0.1;     // a frame counts when it peaks over this share of the sound's peak
        static constexpr double HIGHS_HZ = 2000.0;

        TonalBalance(const std::vector<double> &x, double sr)
        {
            const SpectrumProbe spectrum(sr);
            double peak = 1e-30;
            for (double v : x)
                peak = std::max(peak, std::abs(v));
            std::vector<double> sum;
            int frames = 0;
            for (size_t s = 0; s + FRAME <= x.size(); s += FRAME)
            {
                double p = 0.0;
                for (size_t i = s; i < s + FRAME; ++i)
                    p = std::max(p, std::abs(x[i]));
                if (p < LOUD * peak)
                    continue;
                const auto bands = spectrum.band_levels_db(x, s, FRAME);
                if (sum.empty())
                    sum.assign(bands.size(), 0.0);
                for (size_t k = 0; k < bands.size(); ++k)
                    sum[k] += bands[k];
                ++frames;
            }
            db_ = sum;
            if (frames > 0)
            {
                for (auto &v : db_)
                    v /= frames;
                const double top = *std::max_element(db_.begin(), db_.end());
                for (auto &v : db_)
                    v -= top;
            }
        }

        const std::vector<double> &bands_db() const { return db_; }

        // the RMS of the bands' differences with another balance (dB), from `from_hz` up
        double gap_db(const TonalBalance &other, double from_hz = 0.0) const
        {
            double squares = 0.0;
            int count = 0;
            double centre = 100.0;
            for (size_t k = 0; k < db_.size() && k < other.db_.size(); ++k, centre *= SpectrumProbe::THIRD_OCTAVE)
            {
                if (centre < from_hz * 0.999)
                    continue;
                squares += (db_[k] - other.db_[k]) * (db_[k] - other.db_[k]);
                ++count;
            }
            return count > 0 ? std::sqrt(squares / count) : 0.0;
        }

    private:
        std::vector<double> db_;
    };
}
