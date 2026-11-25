// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// magnetic_pickup.hpp
//
// How a magnetic pickup turns a string's motion into a voltage (models::PickupSensing, solved by
// pickup_sensing_network):
//   - where it listens: each coil's row of poles, at its distance from the saddle; a humbucker's two coils in series
//     add what they hear at two places, so the partials whose wave fits between them cancel (the comb that takes
//     the highs off a humbucker);
//   - how wide it listens: a pole magnetises the string and its coil hears the magnetised string back, each through
//     a field falling as (1 + s^2 / a^2)^(-3/2) along the string, so the string at s counts as (1 + s^2 / a^2)^-3.
//     Its transform, the share of a partial of wavenumber k it hears, is e^(-ka) (1 + ka + (ka)^2 / 3). The width
//     a^2 = h^2 + r^2 / 2 (the string's height, the pole's spread) gives the 13 mm at half height Horton & Moore
//     measured 7 mm over a 13 mm magnet (AJP 2009, Fig. 7);
//   - how it hears the distance: the flux through a coil against the string's distance to its poles is Novak et al.'s
//     curve (DAFx 2018, Eq. 6), the same whatever the height the string rests at. The voltage is its slope times the
//     speed at which that distance changes (Eq. 8). The distance counts both planes: the string moving along the top
//     only moves away from the pole by the square of its excursion (it is heard at twice its frequency, far weaker:
//     Horton & Moore, Fig. 16).
// The voltage comes out in the units of the string's speed (m/s), so that a single coil at PICKUP_HEIGHT_REF_MM,
// hearing a small vibration, gives the speed under it, as the engine's pickup always did; `gain` carries how much
// hotter (or weaker) this pickup at this height is.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>

#include "../constants/core.hpp"
#include "../models/core.hpp"

namespace grmg
{
    class MagneticPickup
    {
    public:
        MagneticPickup() = default;
        explicit MagneticPickup(const models::PickupSensing &s) : s_(s), rest_slope_(slope(s.height_mm))
        {
            // the slope against the slope at rest, tabulated (the audio loop reads it, see relative_slope); the points
            // fall on the height at rest, so that a small vibration is heard exactly at the slope at rest
            origin_mm_ = s.height_mm - std::floor((s.height_mm - PICKUP_HEIGHT_MIN_MM) / STEP_MM) * STEP_MM;
            for (int i = 0; i < PICKUP_SLOPE_POINTS; ++i)
                table_[(size_t)i] = slope(origin_mm_ + i * STEP_MM) / rest_slope_;
        }

        const models::PickupSensing &sensing() const { return s_; }
        int coils() const { return s_.coils; }

        // the share of a partial of wavenumber k (rad/m) a pole hears, from the width of the string it listens to
        static double window(double k, double width_m)
        {
            const double z = std::abs(k) * width_m;
            return std::exp(-z) * (1.0 + z + z * z / 3.0);
        }

        // how much coil c hears of mode n of a string of length `length` (displacement there, per unit of the mode)
        double coil_shape(int c, int n, double length) const
        {
            const double k = n * PI / length;
            return std::sin(k * s_.coil_x[(size_t)c]) * window(k, s_.window_m);
        }

        // what the whole pickup hears of mode n while the string moves little: the coils' shapes, at the slope at rest
        double small_signal_shape(int n, double length) const
        {
            double sum = 0.0;
            for (int c = 0; c < s_.coils; ++c)
                sum += coil_shape(c, n, length);
            return s_.gain * sum;
        }

        // the voltage of one coil (m/s units, see above) when the string, over it, is `toward` (m) closer to its poles
        // and `along` (m) aside along the top, moving at `toward_speed` and `along_speed` (m/s)
        double voltage(double toward, double toward_speed, double along, double along_speed) const
        {
            const double x = s_.height_mm - 1000.0 * toward, y = 1000.0 * along; // mm
            const double r = std::max(std::sqrt(x * x + y * y), PICKUP_HEIGHT_MIN_MM);
            // how fast the distance to the poles shrinks (m/s)
            const double closing = (x * toward_speed - y * along_speed) / r;
            return s_.gain * relative_slope(r) * closing;
        }

        // the slope at r (mm) against the slope at rest: read from the table between its points, exact past its end
        double relative_slope(double r) const
        {
            const double u = (r - origin_mm_) / STEP_MM;
            if (u < 0.0 || u >= (double)(PICKUP_SLOPE_POINTS - 1))
                return slope(r) / rest_slope_;
            const int i = (int)u;
            const double f = u - (double)i;
            return table_[(size_t)i] + f * (table_[(size_t)i + 1] - table_[(size_t)i]);
        }

        // the slope of Novak et al.'s flux curve at a distance x (mm) from the poles (Eq. 9), in mV s per mm
        double slope(double x) const { return slope(x, s_.nl_A, s_.nl_Leq_mm, s_.nl_req_mm); }

        static double slope(double x, double A, double Leq, double req)
        {
            // q^(4/3) as q cbrt(q): the same, several times cheaper than pow in the audio loop
            const double r2 = req * req, far = x + Leq;
            const double qf = far * far + r2, qx = x * x + r2;
            return A * ((far * far + 3.0 * r2) / (3.0 * qf * std::cbrt(qf)) - (x * x + 3.0 * r2) / (3.0 * qx * std::cbrt(qx)));
        }

        // the flux curve itself (Eq. 6), mV s (up to a constant): for the tests
        static double flux(double x, double A, double Leq, double req)
        {
            const double r2 = req * req, far = x + Leq;
            return A * (far / std::cbrt(r2 + far * far) - x / std::cbrt(r2 + x * x));
        }

    private:
        static constexpr double STEP_MM = PICKUP_SLOPE_SPAN_MM / (PICKUP_SLOPE_POINTS - 1);
        models::PickupSensing s_{};
        double rest_slope_ = -1.0, origin_mm_ = PICKUP_HEIGHT_MIN_MM;
        std::array<double, PICKUP_SLOPE_POINTS> table_{};
    };
}
