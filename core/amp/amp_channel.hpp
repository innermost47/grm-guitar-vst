// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// amp_channel.hpp
//
// One channel of the amplifier (clean or lead), at the oversampled rate: its triode stages in a row, the gain pot
// (and its bright capacitor) in front of its gain stage, the tone stack where its circuit has it, the power stage.
#pragma once

#include <array>

#include "../../models/amp.hpp"
#include "one_pole.hpp"
#include "power_amp.hpp"
#include "tone_stack.hpp"
#include "triode_stage.hpp"

namespace grmg
{
    class AmpChannel
    {
    public:
        void set_model(const models::AmpChannelModel &m, double sr)
        {
            count_ = std::min((int)m.stages.size(), MAX_TRIODES);
            for (int i = 0; i < count_; ++i)
                stages_[(size_t)i].set_model(m.stages[(size_t)i], sr, i == m.gain_stage ? m.pot : 1.0);
            gain_stage_ = m.gain_stage;
            tone_after_ = m.tone_after;
            bright_on_ = m.bright_hz > 0.0;
            if (bright_on_)
                bright_.set_high_shelf(sr, m.bright_hz, m.bright_gain);
            tone_.set_model(m.tone, sr);
            power_.set_model(m.power, sr);
        }

        void reset()
        {
            for (auto &s : stages_)
                s.reset();
            bright_.reset();
            tone_.reset();
            power_.reset();
        }

        // x: the guitar, or the pedal (V); noise: a uniform noise in [-1, 1]. Returns the power stage's output.
        double process(double x, double noise)
        {
            for (int i = 0; i < count_; ++i)
            {
                if (i == gain_stage_ && bright_on_)
                    x = bright_.process(x);
                x = stages_[(size_t)i].process(x, i == 0 ? noise : 0.0);
                if (i == tone_after_)
                    x = tone_.process(x);
            }
            return power_.process(x);
        }

    private:
        std::array<TriodeStage, MAX_TRIODES> stages_{};
        int count_ = 0, gain_stage_ = 0, tone_after_ = 0;
        bool bright_on_ = false;
        OnePole bright_;
        ToneStack tone_;
        PowerAmp power_;
    };
}
