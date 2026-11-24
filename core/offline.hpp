// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// The body and the room after the engine, offline (tests, renders). The plugin does the same with a
// real-time convolution of the same impulse responses.
#pragma once

#include <array>
#include <vector>

#include "fft.hpp"
#include "model.hpp"

namespace grmg
{

    // from the force on the bridge to the sound of the guitar in the room, left and right ears
    inline std::array<std::vector<double>, 2> body_and_room(const std::vector<double> &force, const Model &m)
    {
        std::vector<double> body_ir(m.body_ir.begin(), m.body_ir.end());
        auto wet = convolve(force, body_ir);
        const double a = m.s.body_amount;
        std::vector<double> guitar(force.size());
        for (size_t t = 0; t < force.size(); ++t)
            guitar[t] = (1 - a) * force[t] + a * wet[t];
        std::array<std::vector<double>, 2> out;
        for (int ear = 0; ear < 2; ++ear)
        {
            if (m.s.room_on && !m.room_ir[(size_t)ear].empty())
            {
                std::vector<double> h(m.room_ir[(size_t)ear].begin(), m.room_ir[(size_t)ear].end());
                out[(size_t)ear] = convolve(guitar, h);
            }
            else
            {
                out[(size_t)ear] = guitar;
            }
        }
        return out;
    }

    inline std::array<std::vector<double>, 2> sensor_output(const std::vector<double> &force, const std::vector<double> &pickup,
                                                            const Model &m)
    {
        if (m.s.sensor == 0)
            return body_and_room(force, m);
        std::vector<double> y(force.size());
        if (m.s.sensor == 2)
        {
            for (size_t t = 0; t < y.size(); ++t)
                y[t] = SENSOR_SCALE[2] * force[t];
        }
        else // the engine's pickup output is already the jack's (pickups, cable, amplifier)
        {
            for (size_t t = 0; t < y.size() && t < pickup.size(); ++t)
                y[t] = SENSOR_SCALE[1] * pickup[t];
        }
        return {y, y};
    }

} // namespace grmg
