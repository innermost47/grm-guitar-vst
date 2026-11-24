// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// tone_stack.hpp
//
// The passive tone stack (Fender, Marshall and the rest share its circuit): the third-order H(s) its network solved
// (models::ToneStackModel), turned digital by the bilinear transform s = c (1 - z^-1) / (1 + z^-1), c = 2 sr.
// Its three knobs pull on each other, as on the real amplifier (Yeh & Smith, DAFx-06).
#pragma once

#include <array>

#include "../../models/amp.hpp"

namespace grmg
{
    class ToneStack
    {
    public:
        void set_model(const models::ToneStackModel &m, double sr)
        {
            const double c = 2.0 * sr;
            const std::array<double, 4> k = {1.0, c, c * c, c * c * c};
            // s^n (1 + z^-1)^3 for n = 0..3, as coefficients of 1, z^-1, z^-2, z^-3
            static constexpr double expand[4][4] = {
                {1.0, 3.0, 3.0, 1.0},
                {1.0, 1.0, -1.0, -1.0},
                {1.0, -1.0, -1.0, 1.0},
                {1.0, -3.0, 3.0, -1.0},
            };
            std::array<double, 4> num{}, den{};
            for (int n = 0; n < 4; ++n)
                for (int j = 0; j < 4; ++j)
                {
                    num[(size_t)j] += m.b[(size_t)n] * k[(size_t)n] * expand[n][j];
                    den[(size_t)j] += m.a[(size_t)n] * k[(size_t)n] * expand[n][j];
                }
            for (int j = 0; j < 4; ++j)
            {
                b_[(size_t)j] = num[(size_t)j] / den[0];
                a_[(size_t)j] = den[(size_t)j] / den[0];
            }
        }

        void reset() { z_ = {0.0, 0.0, 0.0}; }

        double process(double x)
        {
            const double y = b_[0] * x + z_[0];
            z_[0] = b_[1] * x - a_[1] * y + z_[1];
            z_[1] = b_[2] * x - a_[2] * y + z_[2];
            z_[2] = b_[3] * x - a_[3] * y;
            return y;
        }

    private:
        std::array<double, 4> b_{}, a_{1.0, 0.0, 0.0, 0.0};
        std::array<double, 3> z_{};
    };
}
