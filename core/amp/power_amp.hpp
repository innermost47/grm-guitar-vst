// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// power_amp.hpp
//
// The push-pull power stage as it plays, from what its network solved (models::PowerAmpModel):
//   - the phase inverter drives two tubes in opposition, each conducting as (u + idle)^1.5 past its cutoff: their
//     difference cancels the even harmonics and leaves the class AB crossover;
//   - the plates saturate against the supply, which sags as the tubes draw more (the compression of a cranked amp);
//   - the output is fed back to the phase inverter (negative feedback), less of it above the presence corner and
//     below the resonance corner: those two knobs lift the highs and the lows. The loop is solved within each
//     sample (Newton on y = F(u - feedback(y))), so that it holds at any depth;
//   - the output transformer's band.
// Its output is normalised: 1 at full power.
#pragma once

#include <algorithm>
#include <cmath>

#include "../../models/amp.hpp"
#include "one_pole.hpp"
#include "smoothed.hpp"

namespace grmg
{
    class PowerAmp
    {
    public:
        void set_model(const models::PowerAmpModel &m, double sr)
        {
            const bool first = model_ == nullptr;
            model_ = &m;
            input_.prepare(sr);
            if (first)
                input_.jump(m.input * m.drive);
            else
                input_.set(m.input * m.drive);
            norm_ = 1.0 / std::pow(1.0 + m.idle, 1.5);
            hard_ = m.knee >= 3.0;
            presence_a_ = std::exp(-2 * PI * m.presence_hz / sr);
            resonance_k_ = 1.0 - std::exp(-2 * PI * m.resonance_hz / sr);
            sag_attack_ = 1.0 - std::exp(-1.0 / (SAG_ATTACK_S * sr));
            sag_release_ = 1.0 - std::exp(-1.0 / (std::max(m.sag_s, 1e-4) * sr));
            low_.set_highpass(sr, m.lo_hz);
            high_.set_lowpass(sr, m.hi_hz);
        }

        void reset()
        {
            y_ = hp_ = lp_ = sag_ = 0.0;
            low_.reset();
            high_.reset();
        }

        // x: the preamp's swing (V)
        double process(double x)
        {
            const models::PowerAmpModel &m = *model_;
            const double u0 = x * input_.next();
            const double supply = 1.0 - m.sag_depth * sag_;
            // the feedback, affine in this sample's output: nfb (K y + C)
            const double K = 1.0 - m.presence * presence_a_ - m.resonance * resonance_k_;
            const double C = -m.presence * presence_a_ * (hp_ - y_) - m.resonance * (1.0 - resonance_k_) * lp_;
            double y = y_;
            double u = u0;
            for (int i = 0; i < 3; ++i)
            {
                u = u0 - m.nfb * (K * y + C);
                double slope = 0.0;
                const double out = transfer(u, supply, slope);
                const double h = y - out;
                y -= h / (1.0 + m.nfb * K * slope);
            }
            u = u0 - m.nfb * (K * y + C);
            // the feedback filters and the supply follow the output
            hp_ = presence_a_ * (hp_ + y - y_);
            lp_ += resonance_k_ * (y - lp_);
            y_ = y;
            const double draw = (tube(u) + tube(-u)) * norm_;
            sag_ += (draw - sag_) * (draw > sag_ ? sag_attack_ : sag_release_);
            return high_.process(low_.process(y));
        }

    private:
        const models::PowerAmpModel *model_ = nullptr;
        Smoothed input_;
        OnePole low_, high_;
        double norm_ = 1.0, presence_a_ = 0.0, resonance_k_ = 0.0, sag_attack_ = 0.0, sag_release_ = 0.0;
        bool hard_ = false;
        double y_ = 0.0, hp_ = 0.0, lp_ = 0.0, sag_ = 0.0;

        // one tube's plate current, normalised: (u + idle)^1.5 past cutoff
        double tube(double u) const
        {
            const double t = std::max(0.0, u + model_->idle);
            return t * std::sqrt(t);
        }

        // the push-pull output against the plates' saturation: d / (1 + |d / S|^k)^(1 / k), k 2 (soft) or 4 (hard)
        double transfer(double u, double supply, double &slope) const
        {
            const double idle = model_->idle;
            const double d = (tube(u) - tube(-u)) * norm_;
            const double dd = 1.5 * (std::sqrt(std::max(0.0, u + idle)) + std::sqrt(std::max(0.0, idle - u))) * norm_;
            const double x = d / supply;
            const double x2 = x * x;
            double base, root;
            if (hard_)
            {
                base = 1.0 + x2 * x2;
                root = std::sqrt(std::sqrt(base));
            }
            else
            {
                base = 1.0 + x2;
                root = std::sqrt(base);
            }
            slope = dd / (base * root);
            return d / root;
        }
    };
}
