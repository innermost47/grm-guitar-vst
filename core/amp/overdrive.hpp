// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// overdrive.hpp
//
// The overdrive pedal in front of the amplifier (a Tube Screamer's circuit, Yeh, Abel & Smith, DAFx-07): the
// op-amp amplifies only what lies above hp_hz, its feedback diodes clip that part softly (their logarithmic knee)
// and its feedback capacitor rounds it; the clean signal passes beside it. Then the tone's low-pass and the level.
// Alone it is a mild overdrive; in front of a high-gain channel it tightens the lows and pushes the mids.
#pragma once

#include <cmath>

#include "../../models/amp.hpp"
#include "one_pole.hpp"
#include "smoothed.hpp"

namespace grmg
{
    class Overdrive
    {
    public:
        void set_model(const models::OverdriveModel &m, double sr)
        {
            const bool first = model_ == nullptr;
            model_ = &m;
            band_.set_highpass(sr, m.hp_hz);
            round_.set_lowpass(sr, m.lp_hz);
            tone_.set_lowpass(sr, m.tone_hz);
            wet_.prepare(sr);
            level_.prepare(sr);
            if (first)
            {
                wet_.jump(m.on ? 1.0 : 0.0);
                level_.jump(m.level);
            }
            else
            {
                wet_.set(m.on ? 1.0 : 0.0);
                level_.set(m.level);
            }
        }

        void reset()
        {
            band_.reset();
            round_.reset();
            tone_.reset();
        }

        // x: the guitar (V)
        double process(double x)
        {
            const double wet = wet_.next(), level = level_.next();
            if (wet <= 1e-6)
                return x;
            const double v = round_.process(model_->gain * band_.process(x));
            const double clipped = OD_DIODE_V * std::asinh(v / OD_DIODE_V);
            const double pedal = tone_.process(x + clipped) * level;
            return x + (pedal - x) * wet;
        }

    private:
        const models::OverdriveModel *model_ = nullptr;
        OnePole band_, round_, tone_;
        Smoothed wet_, level_;
    };
}
