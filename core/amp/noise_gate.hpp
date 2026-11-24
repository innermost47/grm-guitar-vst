// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// noise_gate.hpp
//
// A noise gate keyed by the guitar itself (not by the amplifier's hiss): it opens as soon as the strings speak,
// holds, and closes fast below the threshold (with some hysteresis, so that it does not chatter). Placed after the
// amplifier it silences its hiss between the notes: the silence between two chugs that metal and djent need.
#pragma once

#include <cmath>

#include "../../constants/core.hpp"

namespace grmg
{
    class NoiseGate
    {
    public:
        void prepare(double sr)
        {
            detect_ = std::exp(-1.0 / (GATE_DETECT_S * sr));
            attack_ = 1.0 - std::exp(-1.0 / (GATE_ATTACK_S * sr));
            release_ = 1.0 - std::exp(-1.0 / (GATE_RELEASE_S * sr));
            hold_samples_ = (long)(GATE_HOLD_S * sr);
        }

        void set_threshold(double db)
        {
            on_ = db > GATE_OFF_DB + 0.5;
            open_at_ = std::pow(10.0, db / 20.0);
            close_at_ = open_at_ * GATE_HYSTERESIS;
        }

        void reset()
        {
            envelope_ = 0.0;
            gain_ = 1.0;
            held_ = 0;
            open_ = true;
        }

        // key: the guitar (V); x: what the gate lets through
        double process(double key, double x)
        {
            if (!on_)
                return x;
            envelope_ = std::max(std::abs(key), envelope_ * detect_);
            if (envelope_ > open_at_)
            {
                open_ = true;
                held_ = hold_samples_;
            }
            else if (envelope_ < close_at_)
            {
                if (held_ > 0)
                    --held_;
                else
                    open_ = false;
            }
            gain_ += ((open_ ? 1.0 : 0.0) - gain_) * (open_ ? attack_ : release_);
            return x * gain_;
        }

    private:
        double detect_ = 0.0, attack_ = 1.0, release_ = 1.0;
        double open_at_ = 0.0, close_at_ = 0.0, envelope_ = 0.0, gain_ = 1.0;
        long hold_samples_ = 0, held_ = 0;
        bool on_ = false, open_ = true;
    };
}
