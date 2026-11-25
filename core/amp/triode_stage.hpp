// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// triode_stage.hpp
//
// One triode gain stage as it plays, from what the preamp network solved (models::TriodeModel):
//   - the divider in front (the gain pot at the gain stage), the coupling capacitor and the grid leak (a high-pass),
//     the Miller capacitance against what drives the grid (a low-pass), the cathode bypass (a low shelf);
//   - the grid: past the cathode it conducts, the overshoot is mostly swallowed and the coupling capacitor charges:
//     the grid's bias slides down and comes back through the grid leak (blocking distortion: the note "sags" and
//     breathes with the playing);
//   - the plate: its swing against the grid voltage, the tube's own curve on its load line (asymmetric: it saturates
//     on one side, cuts off on the other).
#pragma once

#include <algorithm>

#include "../../models/amp.hpp"
#include "one_pole.hpp"
#include "../smoothed.hpp"

namespace grmg
{
    class TriodeStage
    {
    public:
        void set_model(const models::TriodeModel &m, double sr, double pot)
        {
            const bool first = model_ == nullptr;
            model_ = &m;
            coupling_.set_highpass(sr, m.grid_hz);
            miller_.set_lowpass(sr, m.miller_hz);
            cathode_.set_low_shelf(sr, m.shelf_hz, m.shelf_gain);
            att_.prepare(sr);
            if (first)
                att_.jump(m.att * pot);
            else
                att_.set(m.att * pot);
            charge_ = 1.0 / (m.charge_s * sr);
            leak_ = std::exp(-1.0 / (m.leak_s * sr));
            noise_ = m.noise_density * std::sqrt(0.5 * sr) * std::sqrt(3.0); // a uniform noise of that rms
            scale_ = (double)(m.curve.size() - 1) / (m.curve_hi - m.curve_lo);
        }

        void reset()
        {
            coupling_.reset();
            miller_.reset();
            cathode_.reset();
            shift_ = 0.0;
        }

        // x: the swing of what feeds the grid (V); noise: a uniform noise in [-1, 1]
        double process(double x, double noise)
        {
            const models::TriodeModel &m = *model_;
            double v = cathode_.process(miller_.process(coupling_.process(x * att_.next()))) + noise * noise_;
            v += shift_;
            if (v > m.grid_on)
            {
                const double over = v - m.grid_on;
                v = m.grid_on + over * m.grid_share;
                shift_ -= over * charge_;
            }
            shift_ *= leak_;
            return plate(v);
        }

    private:
        const models::TriodeModel *model_ = nullptr;
        OnePole coupling_, miller_, cathode_;
        Smoothed att_;
        double shift_ = 0.0, charge_ = 0.0, leak_ = 1.0, noise_ = 0.0, scale_ = 1.0;

        // the plate swing at grid voltage v: the tube's curve, read between its points
        double plate(double v) const
        {
            const auto &curve = model_->curve;
            const double at = std::clamp((v - model_->curve_lo) * scale_, 0.0, (double)(curve.size() - 1));
            const size_t i = std::min((size_t)at, curve.size() - 2);
            const double f = at - (double)i;
            return curve[i] + (curve[i + 1] - curve[i]) * f;
        }
    };
}
