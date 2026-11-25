// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// The body and the room after the engine, offline (tests, renders). The plugin does the same with a
// real-time convolution of the same impulse responses.
#pragma once

#include <array>
#include <vector>

#include "fft.hpp"
#include "listener_path.hpp"
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
        if (!m.s.room_on || m.room_ir[0].empty() || m.room.size < 0)
            return {guitar, guitar};
        // the listener: the direct sound to each ear, and what it sends to the room's reverberation
        ListenerPath listener;
        listener.prepare(m.sample_rate);
        listener.place(listener_taps(ROOMS[m.room.size], m.s.room_distance, 1.0), false);
        const std::vector<float> in(guitar.begin(), guitar.end());
        std::array<std::vector<float>, 2> direct, send;
        for (auto *v : {&direct[0], &direct[1], &send[0], &send[1]})
            v->resize(in.size());
        float *const d[2] = {direct[0].data(), direct[1].data()};
        float *const r[2] = {send[0].data(), send[1].data()};
        listener.process(in.data(), d, r, (int)in.size());
        for (size_t ear = 0; ear < 2; ++ear)
        {
            const std::vector<double> h(m.room_ir[ear].begin(), m.room_ir[ear].end());
            out[ear] = convolve(std::vector<double>(send[ear].begin(), send[ear].end()), h);
            for (size_t t = 0; t < direct[ear].size() && t < out[ear].size(); ++t)
                out[ear][t] += direct[ear][t];
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
