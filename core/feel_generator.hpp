// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// feel_generator.hpp
//
// What writes the Feel lanes for the player (a pattern's lanes, steps [from, from + count)):
//   - clone: a stretch copied over the bars after it, to the pattern's end;
//   - vary: the same stretch played another way: its strong places kept, the weak ones moved a notch or two, and the
//     end of a phrase sometimes pushed (a fill);
//   - groove: a new feel drawn at random but musical: a one-bar motif from the meter (strong places stay strong, some
//     syncopations, ghost strokes, palm on the soft ones), then repeated with variations, a fill closing each phrase.
// Everything is in notches (FEEL_NOTCHES across a lane) and drawn from a seed: the same seed, the same feel.
#pragma once

#include <algorithm>
#include <cmath>
#include <random>

#include "song.hpp"

namespace grmg
{
    class FeelGenerator
    {
    public:
        // a lane value on its notches: force and timing from -1 to 1, palm from 0 to 1
        static double notch(double v, bool signed_lane = true)
        {
            const double lo = signed_lane ? -1.0 : 0.0;
            return lo + std::round((std::clamp(v, lo, 1.0) - lo) / (1.0 - lo) * FEEL_NOTCHES) / FEEL_NOTCHES * (1.0 - lo);
        }

        // the pattern's lanes, written out from its preset when it has none, and over the whole pattern when it grew
        // (lanes written past the pattern's end are kept, as notes are)
        static void ensure_lanes(Song &song)
        {
            if ((int)song.feel.force.size() < song.steps())
                song.feel = resized(song);
        }

        // the most steps a lane holds: as far as notes can be written
        static int max_steps(const Song &song) { return (int)std::lround(song.max_beats() / FEEL_STEP_BEATS); }

        // the lanes made to reach step `upto` (an edit past the pattern's end): the new steps carry on the pattern's
        // feel, as it would play there
        static void reach(Song &song, int upto)
        {
            ensure_lanes(song);
            upto = std::min(upto, max_steps(song));
            const int period = song.steps();
            auto grow = [&](std::vector<double> &lane)
            {
                for (int s = (int)lane.size(); s < upto; ++s)
                    lane.push_back(lane[(size_t)(s % period)]);
            };
            grow(song.feel.force);
            grow(song.feel.timing);
            grow(song.feel.palm);
        }

        // another preset for the pattern: a preset only says which strokes hit harder, so it draws the Force lane again
        // and keeps the Timing and Palm lanes as written (a pattern with no lanes written simply follows the preset)
        static void apply_preset(Song &song, int preset)
        {
            if (song.feel.empty())
            {
                song.feel_preset = preset;
                return;
            }
            ensure_lanes(song);
            song.feel_preset = preset;
            const auto drawn = FeelCurve::preset_lanes(preset, song.meter(), (int)song.feel.force.size());
            for (size_t s = 0; s < song.feel.force.size(); ++s)
                song.feel.force[s] = notch(drawn.force[s]);
        }

        static void clone(Song &song, int from, int count)
        {
            ensure_lanes(song);
            if (count <= 0)
                return;
            for (int s = from + count; s < song.steps(); ++s)
            {
                const int src = from + (s - from) % count;
                copy_step(song.feel, src, s);
            }
        }

        static void vary(Song &song, int from, int count, unsigned seed)
        {
            ensure_lanes(song);
            std::mt19937 rng(seed);
            const Meter meter = song.meter();
            for (int s = from; s < std::min(from + count, song.steps()); ++s)
            {
                const int level = meter.level(s);
                if (level <= 1)
                    continue; // the downbeat and the groups' starts stay
                auto &f = song.feel.force[(size_t)s];
                f = notch(f + step(rng, level >= 4 ? 2 : 1));
                auto &t = song.feel.timing[(size_t)s];
                t = notch(t + step(rng, 1) * 0.5);
            }
            // the end of the stretch pushed now and then: a crescendo over its last beat
            if (chance(rng, 0.4))
                for (int k = 0; k < 4 && from + count - 1 - k >= from; ++k)
                {
                    auto &f = song.feel.force[(size_t)(from + count - 1 - k)];
                    f = notch(f + (4 - k) * 2.0 / FEEL_NOTCHES);
                }
        }

        static void groove(Song &song, int from, int count, unsigned seed)
        {
            ensure_lanes(song);
            std::mt19937 rng(seed);
            const Meter meter = song.meter();
            const int per_bar = meter.sixteenths();
            // one bar: the meter's weights, some places syncopated (an eighth pushed up), ghosts on the sixteenths
            std::vector<double> force((size_t)per_bar), timing((size_t)per_bar), palm((size_t)per_bar);
            const bool chugs = chance(rng, 0.6); // a palm-muted rhythm, opened on its accents
            const double lay_back = chance(rng, 0.5) ? 1.0 / FEEL_NOTCHES * 2.0 : 0.0;
            for (int s = 0; s < per_bar; ++s)
            {
                const int level = meter.level(s);
                double w = METER_WEIGHTS[level];
                if (level == 3 && chance(rng, 0.3))
                    w = 0.9; // a syncopation
                if (level == 4 && chance(rng, 0.25))
                    w = 0.0; // a ghost stroke
                force[(size_t)s] = notch((w - 0.4) / FEEL_WEIGHT_SPAN);
                timing[(size_t)s] = level == 2 ? lay_back : 0.0;
                palm[(size_t)s] = chugs ? notch(w >= 0.75 ? 0.0 : 0.75 - 0.5 * w, false) : 0.0;
            }
            for (int s = from; s < std::min(from + count, song.steps()); ++s)
            {
                const int p = ((s % per_bar) + per_bar) % per_bar;
                song.feel.force[(size_t)s] = force[(size_t)p];
                song.feel.timing[(size_t)s] = timing[(size_t)p];
                song.feel.palm[(size_t)s] = palm[(size_t)p];
            }
            // every other bar a little different, a fill closing each phrase
            for (int bar = from / per_bar + 1; bar * per_bar < std::min(from + count, song.steps()); bar += 2)
                vary(song, bar * per_bar, per_bar, seed + (unsigned)bar);
        }

    private:
        // the lanes over the pattern's steps: the preset's, with what is written played over them (a pattern grown or
        // cut keeps the curve it had)
        static models::FeelLanes resized(const Song &song)
        {
            auto lanes = FeelCurve::preset_lanes(song.feel_preset, song.meter(), song.steps());
            const FeelCurve written = song.feel_curve();
            if (!song.feel.empty())
                for (int s = 0; s < song.steps(); ++s)
                {
                    lanes.force[(size_t)s] = written.force(0.25 * s);
                    lanes.timing[(size_t)s] = written.timing(0.25 * s);
                    lanes.palm[(size_t)s] = written.palm(0.25 * s);
                }
            // written out to be edited: on the notches
            for (size_t s = 0; s < lanes.force.size(); ++s)
            {
                lanes.force[s] = notch(lanes.force[s]);
                lanes.timing[s] = notch(lanes.timing[s]);
                lanes.palm[s] = notch(lanes.palm[s], false);
            }
            return lanes;
        }

        static void copy_step(models::FeelLanes &l, int from, int to)
        {
            l.force[(size_t)to] = l.force[(size_t)from];
            l.timing[(size_t)to] = l.timing[(size_t)from];
            l.palm[(size_t)to] = l.palm[(size_t)from];
        }

        // a move of -notches .. notches (never 0), in lane units
        static double step(std::mt19937 &rng, int notches)
        {
            std::uniform_int_distribution<int> d(1, notches);
            const int n = d(rng) * (chance(rng, 0.5) ? 1 : -1);
            return 2.0 * n / FEEL_NOTCHES;
        }

        static bool chance(std::mt19937 &rng, double p) { return std::uniform_real_distribution<double>(0.0, 1.0)(rng) < p; }
    };

} // namespace grmg
