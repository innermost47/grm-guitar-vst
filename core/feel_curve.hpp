// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// feel_curve.hpp
//
// The Feel of a pattern as the player plays it: its three lanes (force, timing, palm), one step per sixteenth,
// written in notches but played as a smooth curve through them (a monotone cubic: it never overshoots the steps), so
// a stroke between two steps takes a value between theirs and nothing jumps. A pattern without lanes plays its Feel
// preset, drawn from its meter (Meter): the Natural preset is the bar's accents of a pattern without lanes.
#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

#include "../constants/core.hpp"
#include "meter.hpp"

namespace grmg
{
    class FeelCurve
    {
    public:
        // lanes: one step per sixteenth (empty: the preset's) over the pattern's `steps` sixteenths: the curve repeats
        // every `steps` (what is written past the pattern's end waits there, as notes do, until the pattern reaches it),
        // or every lane's length when the lanes are shorter
        FeelCurve(const models::FeelLanes &lanes, int preset, const Meter &meter, int steps)
            : lanes_(lanes.empty() ? preset_lanes(preset, meter, steps) : lanes),
              period_(std::max(1, std::min(steps, (int)lanes_.force.size())))
        {
        }

        double force(double beat) const { return at(lanes_.force, beat, period_); }
        double timing(double beat) const { return at(lanes_.timing, beat, period_); }
        double palm(double beat) const { return at(lanes_.palm, beat, period_); }
        const models::FeelLanes &lanes() const { return lanes_; }

        // the weight a preset gives each place of the bar
        static double weight(int preset, const Meter &meter, int sixteenth)
        {
            const int s = ((sixteenth % meter.sixteenths()) + meter.sixteenths()) % meter.sixteenths();
            const int level = meter.level(s), in_beat = s % 4;
            switch (preset)
            {
            case FEEL_BACKBEAT: // the beats between the groups' starts hit hardest (2 and 4)
                if (level == 2)
                    return 1.0;
                return level <= 1 ? 0.7 : METER_WEIGHTS[level];
            case FEEL_OFFBEAT: // the eighths between the beats
                return in_beat == 2 ? 1.0 : (in_beat == 0 ? 0.45 : METER_WEIGHTS[4]);
            case FEEL_GALLOP: // an eighth and two sixteenths
                return in_beat == 0 ? 1.0 : (in_beat == 2 ? 0.55 : (in_beat == 3 ? 0.7 : METER_WEIGHTS[4]));
            case FEEL_WALTZ: // one strong beat, the others light
                return level == 0 ? 1.0 : (level <= 2 ? 0.35 : (level == 3 ? 0.25 : METER_WEIGHTS[4]));
            case FEEL_FLAT:
                return 0.5;
            default: // FEEL_NATURAL: the meter's own hierarchy
                return METER_WEIGHTS[level];
            }
        }

        // a preset's lanes over `steps` sixteenths: its force from the weights, no timing nor palm
        static models::FeelLanes preset_lanes(int preset, const Meter &meter, int steps)
        {
            double mean = 0.0;
            for (int s = 0; s < meter.sixteenths(); ++s)
                mean += weight(preset, meter, s);
            mean /= meter.sixteenths();
            models::FeelLanes l;
            for (int s = 0; s < std::max(steps, 1); ++s)
            {
                l.force.push_back(std::clamp((weight(preset, meter, s) - mean) / FEEL_WEIGHT_SPAN, -1.0, 1.0));
                l.timing.push_back(0.0);
                l.palm.push_back(0.0);
            }
            return l;
        }

    private:
        // the curve through the steps of a lane at `beat` (a step every quarter of a beat): monotone cubic Hermite
        // (Fritsch-Carlson slopes), wrapping every `period` steps
        static double at(const std::vector<double> &lane, double beat, int period)
        {
            const int n = std::min(period, (int)lane.size());
            if (n <= 0)
                return 0.0;
            const double x = beat * 4.0;
            const int k = (int)std::floor(x);
            const double u = x - k;
            auto y = [&](int i) { return lane[(size_t)(((i % n) + n) % n)]; };
            const double y0 = y(k - 1), y1 = y(k), y2 = y(k + 1), y3 = y(k + 2);
            if (u < 1e-12)
                return y1;
            auto slope = [](double a, double b, double c)
            { // at the middle point: zero at a peak or a valley, the harmonic mean of the two sides otherwise
                const double d0 = b - a, d1 = c - b;
                return d0 * d1 <= 0.0 ? 0.0 : 2.0 * d0 * d1 / (d0 + d1);
            };
            const double m1 = slope(y0, y1, y2), m2 = slope(y1, y2, y3);
            const double u2 = u * u, u3 = u2 * u;
            return (2 * u3 - 3 * u2 + 1) * y1 + (u3 - 2 * u2 + u) * m1 + (-2 * u3 + 3 * u2) * y2 + (u3 - u2) * m2;
        }

        models::FeelLanes lanes_;
        int period_;
    };

} // namespace grmg
