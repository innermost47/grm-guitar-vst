// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// player_feel.hpp
//
// What a live player aims at, stroke after stroke, against the exact grid: a groove is first what repeats, then
// what wanders slowly, then the gesture's own error (constants: FEEL_LANE_*, SIGNATURE_*, DRIFT_*, MOTOR_*):
//   - the pattern's Feel (FeelCurve): the force of each place of the bar and its lead or lag, written in its lanes
//     (or its preset's, drawn from the meter), scaled by Feel depth;
//   - the player's signature: each place of the bar has its own small lead or lag, force, hand position and depth,
//     drawn once (the seed: this player), played again every bar, renewed a little from one phrase to the next;
//   - the drift: tempo, energy, the hand along the string, the pick's depth wander slowly (three time scales);
//   - the gesture: each stroke's own small error (white: successive intervals come out anticorrelated).
// Looseness scales the player's own (signature, drift, gesture); without human feel, only the Feel remains.
#pragma once

#include <array>
#include <cmath>
#include <random>
#include <vector>

#include "../constants/core.hpp"
#include "feel_curve.hpp"
#include "meter.hpp"

namespace grmg
{
    class PlayerFeel
    {
    public:
        // seed: the player
        PlayerFeel(const models::Settings &s, const Meter &meter, const FeelCurve &curve, unsigned seed)
            : live_(s.human ? s.human_amount : 0.0), depth_(s.feel_depth), per_bar_(meter.sixteenths()), curve_(curve),
              rng_(seed), signature_((size_t)per_bar_)
        {
            for (auto &place : signature_)
                place = draw(SIGNATURE_TIME_S, SIGNATURE_FORCE_DEX, SIGNATURE_PLUCK, SIGNATURE_DEPTH_M);
            for (int k = 0; k < 3; ++k)
                drift_[(size_t)k] = draw(DRIFT_TIME_S, DRIFT_FORCE_DEX, DRIFT_PLUCK, DRIFT_DEPTH_M, std::sqrt(1.0 / 3.0));
        }

        // the Feel's accent (dex of force) and lead or lag (s) at `beat`
        double accent(double beat) const { return depth_ * FEEL_LANE_DEX * curve_.force(beat); }
        double lean(double beat) const { return depth_ * FEEL_LANE_TIMING_S * curve_.timing(beat); }

        // the stroke at `beat` (from the song's start) at grid time `t` (s), strokes in order
        models::StrokeFeel next(double t, double beat)
        {
            const int sixteenth = (int)std::lround(beat * 4.0);
            const int phrase = floor_div(sixteenth, per_bar_ * PHRASE_BARS);
            while (phrase_ < phrase)
            {
                renew_signature();
                ++phrase_;
            }
            const double dt = started_ ? std::max(t - last_t_, 0.0) : 0.0;
            started_ = true;
            last_t_ = t;
            for (int k = 0; k < 3; ++k)
                wander(drift_[(size_t)k], dt, DRIFT_TAU_S[k]);
            const auto &sig = signature_[(size_t)(((sixteenth % per_bar_) + per_bar_) % per_bar_)];
            const auto motor = draw(MOTOR_TIME_S, MOTOR_FORCE_DEX, MOTOR_PLUCK, MOTOR_DEPTH_M);
            models::StrokeFeel out;
            out.accent = accent(beat);
            out.dt = lean(beat) + live_ * (sig.dt + motor.dt);
            out.force_dex = out.accent + live_ * (sig.force_dex + motor.force_dex);
            out.pluck = live_ * (sig.pluck + motor.pluck);
            out.depth = live_ * (sig.depth + motor.depth);
            for (const auto &d : drift_)
            {
                out.dt += live_ * d.dt;
                out.force_dex += live_ * d.force_dex;
                out.pluck += live_ * d.pluck;
                out.depth += live_ * d.depth;
            }
            return out;
        }

    private:
        static int floor_div(int a, int b) { return a >= 0 ? a / b : -((-a + b - 1) / b); }

        models::StrokeFeel draw(double time, double force, double pluck, double depth, double share = 1.0)
        {
            models::StrokeFeel f;
            f.dt = share * time * normal_(rng_);
            f.force_dex = share * force * normal_(rng_);
            f.pluck = share * pluck * normal_(rng_);
            f.depth = share * depth * normal_(rng_);
            return f;
        }

        // a slow process over dt: it keeps exp(-dt / tau) of itself, the rest is new (its size stays the same)
        void wander(models::StrokeFeel &d, double dt, double tau)
        {
            const double keep = std::exp(-dt / tau), fresh = std::sqrt(std::max(1.0 - keep * keep, 0.0));
            const auto n = draw(DRIFT_TIME_S, DRIFT_FORCE_DEX, DRIFT_PLUCK, DRIFT_DEPTH_M, std::sqrt(1.0 / 3.0));
            d.dt = keep * d.dt + fresh * n.dt;
            d.force_dex = keep * d.force_dex + fresh * n.force_dex;
            d.pluck = keep * d.pluck + fresh * n.pluck;
            d.depth = keep * d.depth + fresh * n.depth;
        }

        // a new phrase: the player's way of each place mostly kept (its force more than its timing)
        void renew_signature()
        {
            const double kt = SIGNATURE_KEEP_TIME, kf = SIGNATURE_KEEP_FORCE;
            for (auto &place : signature_)
            {
                const auto n = draw(SIGNATURE_TIME_S, SIGNATURE_FORCE_DEX, SIGNATURE_PLUCK, SIGNATURE_DEPTH_M);
                place.dt = kt * place.dt + std::sqrt(1.0 - kt * kt) * n.dt;
                place.force_dex = kf * place.force_dex + std::sqrt(1.0 - kf * kf) * n.force_dex;
                place.pluck = kf * place.pluck + std::sqrt(1.0 - kf * kf) * n.pluck;
                place.depth = kf * place.depth + std::sqrt(1.0 - kf * kf) * n.depth;
            }
        }

        double live_, depth_;
        int per_bar_;
        FeelCurve curve_;
        std::mt19937 rng_;
        std::normal_distribution<double> normal_{0.0, 1.0};
        std::vector<models::StrokeFeel> signature_;
        std::array<models::StrokeFeel, 3> drift_{};
        int phrase_ = 0;
        bool started_ = false;
        double last_t_ = 0.0;
    };

} // namespace grmg
