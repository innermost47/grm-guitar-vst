// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// From the guitar roll to what the hands do (off the audio thread), as the sequencer of grm_guitar.py:
//   1) the player network: one stroke per moment the right hand strikes, over the whole song (the arm keeps
//      its swing; timing, force, pluck point, pick depth and palm wander as a player's do);
//   2) the strokes meet the strings: a strum crosses them one after the other (strum_contacts), a note is
//      struck by the pick or a finger (attack network, at the force and place the stroke gives), under the
//      palm if asked (palm network, where and how hard the stroke puts the palm);
//   3) the left hand: when a string changes fret, the finger leaves the old fret (the string jumps back to
//      open: a soft pluck, the note stops) and lands on the new one (a knock), and slides on a wound string
//      make their noise (finger networks);
//   4) a note that ends is damped; a string played again forgets the note it had.
// The result is a list of events in beats, read by the engine at the host's position.
#pragma once

#include <algorithm>
#include <map>
#include <memory>
#include <random>
#include <tuple>
#include <vector>

#include "model.hpp"
#include "song.hpp"
#include "voice.hpp"

namespace grmg {

enum class EventType { pluck, damp, squeak, palm_on, palm_off };

struct Event {
    double beat = 0.0;
    EventType type = EventType::pluck;
    int string = 0;
    Pluck pluck;               // pluck: what starts the string
    int squeak = -1;           // squeak: index of the sound
};

class Performance {
public:
    std::shared_ptr<const Model> model;                       // the networks it was compiled with
    std::vector<Event> events;                                // sorted by beat
    std::vector<std::vector<float>> squeaks;                  // force on the bridge (N), at the model's sample rate
    std::vector<std::shared_ptr<const Palm>> palms;           // the palms its plucks point to
    double length_beats = 16.0;
    double tempo = 90.0;

    size_t first_at(double beat) const {
        return (size_t) (std::lower_bound(events.begin(), events.end(), beat,
                                          [](const Event& e, double b) { return e.beat < b; })
                         - events.begin());
    }
};

// palms the strokes ask for, solved once (warm-started from the player's chosen palm)
class PalmCache {
public:
    std::shared_ptr<const Palm> get(const Model& m, int i, int fret, double x_stroke, double n_stroke) {
        const double home = m.s.palm_mm / 1000.0;
        const auto [x, n] = Model::palm_snap(home + (x_stroke - PALM_X), n_stroke * m.s.palm_intensity);
        const auto key = std::make_tuple(i, fret, std::lround(x * 1e4), std::lround(std::log10(n) * 1e3),
                                         std::lround(m.tension[(size_t) i] * 1e3));
        if (std::get<0>(cache_key_) != m.s.tuning || std::abs(std::get<1>(cache_key_) - m.s.core) > 1e-12) {
            cache_.clear();
            cache_key_ = {m.s.tuning, m.s.core};
        }
        auto it = cache_.find(key);
        if (it != cache_.end()) return it->second;
        const auto& sp = STRING_SPECS[i];
        const Palm* ref = m.palm_ready ? m.palm[(size_t) i][(size_t) fret].get() : nullptr;
        auto p = std::make_shared<const Palm>(palm_network(sp.mu, sp.L, fret, x, n, m.tension[(size_t) i], 4, ref));
        cache_[key] = p;
        return p;
    }

private:
    std::map<std::tuple<int, int, long, long, long>, std::shared_ptr<const Palm>> cache_;
    std::pair<int, double> cache_key_{-1, 0.0};
};

struct CompileOptions {
    double tempo = 90.0;
    unsigned seed = 1;
};

class Compiler {
public:
    PalmCache palms;

    std::unique_ptr<Performance> compile(const Song& song, std::shared_ptr<const Model> model_ptr, const CompileOptions& o) {
        const Model& m = *model_ptr;
        const Settings& s = m.s;
        auto perf = std::make_unique<Performance>();
        perf->model = model_ptr;
        perf->length_beats = song.length_beats();
        perf->tempo = o.tempo;
        const double spb = 60.0 / o.tempo;               // seconds per beat
        const double step = spb / 4.0;                   // a sixteenth: the half swing of the arm
        std::mt19937 rng(o.seed);
        std::normal_distribution<double> normal(0.0, 1.0);
        auto jitter = [&](double sd) { return s.human ? sd * normal(rng) : 0.0; };

        // ---------------------------------------------------------------- 1) the strokes of the right hand
        struct Strike { double beat; int string, fret; double velocity, length; bool palm, up; int strum; };
        struct Moment { double beat; std::vector<Strike> strikes; int strum = -1; };
        std::map<long, Moment> moments;                  // by start, to the 1/960 beat
        auto key = [](double beat) { return std::lround(beat * 960.0); };
        for (const auto& n : song.notes) {
            if (n.start >= perf->length_beats || n.fret < 0 || n.fret >= NUM_FRETS) continue;
            auto& mo = moments[key(n.start)];
            mo.beat = n.start;
            mo.strikes.push_back({n.start, n.string, n.fret, n.velocity, n.length, n.palm, false, -1});
        }
        for (size_t k = 0; k < song.strums.size(); ++k) {
            const auto& st = song.strums[k];
            if (st.start >= perf->length_beats) continue;
            auto& mo = moments[key(st.start)];
            mo.beat = st.start;
            mo.strum = (int) k;
        }
        std::vector<Moment> order;
        for (auto& [k, mo] : moments) order.push_back(mo);
        std::vector<Stroke> strokes = run_player(order, step, spb, s, rng);

        // ---------------------------------------------------------------- 2) the strokes meet the strings
        struct Played { double t; int string, fret; double factor, pluck, length; const Palm* palm; Stroke stroke; bool palm_on; };
        std::vector<Played> played;
        std::vector<std::pair<double, double>> palm_spans;    // seconds
        for (size_t k = 0; k < order.size(); ++k) {
            const auto& mo = order[k];
            const Stroke& stroke = strokes[k];
            for (const auto& st : mo.strikes) {
                const Palm* palm = st.palm ? keep(*perf, palms.get(m, st.string, st.fret, stroke.palm_x, stroke.palm_N)) : nullptr;
                played.push_back({stroke.t, st.string, st.fret, stroke.force * st.velocity / 0.8, stroke.p,
                                  st.length * spb, palm, stroke, st.palm});
                if (st.palm) palm_spans.push_back({stroke.t - 0.05, stroke.t + st.length * spb});
            }
            if (mo.strum >= 0) {
                const auto& st = song.strums[(size_t) mo.strum];
                std::vector<int> crossing;
                for (int i = 0; i < NUM_STRINGS; ++i)
                    if (st.frets[(size_t) i] >= 0 && st.frets[(size_t) i] < NUM_FRETS) crossing.push_back(i);
                if (st.up) std::reverse(crossing.begin(), crossing.end());     // index 0 is string 6: down = from the bass
                if (crossing.empty()) continue;
                const auto contacts = crossing.size() == 1 ? std::vector<Contact>{{crossing[0], 0.0, 1.0}}
                                                          : strum_contacts(crossing, stroke, st.up);
                for (const auto& c : contacts) {
                    const int fret = st.frets[(size_t) c.string];
                    const Palm* palm = st.palm ? keep(*perf, palms.get(m, c.string, fret, stroke.palm_x, stroke.palm_N)) : nullptr;
                    played.push_back({stroke.t + c.delay, c.string, fret, stroke.force * c.share * st.velocity / 0.8,
                                      stroke.p, st.length * spb, palm, stroke, st.palm});
                }
                if (st.palm) palm_spans.push_back({stroke.t - 0.05, stroke.t + st.length * spb});
            }
        }
        std::stable_sort(played.begin(), played.end(), [](const Played& a, const Played& b) { return a.t < b.t; });

        // ---------------------------------------------------------------- 3) and 4) string by string
        auto to_beat = [&](double t) { return std::max(0.0, t / spb); };
        auto under_palm = [&](double t) {
            for (auto [a, b] : palm_spans)
                if (a <= t && t < b) return true;
            return false;
        };
        for (int i = 0; i < NUM_STRINGS; ++i) {
            const auto& sp = STRING_SPECS[i];
            int held = 0;                                // the fret the left hand holds on this string
            double ringing_until = -1.0, last_t = -1.0;  // the note it rings (s)
            double end_of_note = -1.0;                   // when the note that rings ends, if nothing comes first
            auto settle = [&](double t_next) {           // the next thing on this string comes at t_next
                // a note that ends just before its string plays again is held to it (legato): the player does not
                // damp a string 60 ms before striking it
                if (end_of_note >= 0 && end_of_note < t_next - 0.06) add_damp(*perf, i, to_beat(end_of_note), false);
                end_of_note = -1.0;
            };
            std::vector<Played> mine;
            for (const auto& p : played)
                if (p.string == i) mine.push_back(p);
            for (const auto& p : mine) {
                if (p.fret != held) {
                    // the hand changes shape before the stroke: lift from the old fret, land on the new one
                    const double gap = last_t < 0 ? 0.25 : p.t - last_t;
                    const double lead = std::max(0.03, std::min(0.25, 0.7 * gap));
                    const double t_change = p.t - lead;
                    const double t_lift = t_change + std::max(0.0, 0.1 * lead + jitter(0.012));
                    const double t_land = t_change + std::max(0.3 * lead, 0.65 * lead + jitter(0.012));
                    const bool palm_now = under_palm(p.t);
                    settle(std::min(t_lift, t_land));
                    if (held > 0) {
                        if (ringing_until > t_lift) {      // the old note stops when the finger leaves
                            add_damp(*perf, i, to_beat(t_lift), true);
                            ringing_until = t_lift;
                        }
                        if (s.finger_noises) {
                            const Action& a = m.action[(size_t) held];
                            Pluck lift = make_pluck(m, i, 0, a.h / Y_REF, TAU_LIFT, std::max(a.xf / sp.L, 0.02), Kind::lift,
                                                    palm_now ? keep(*perf, palms.get(m, i, 0, p.stroke.palm_x, p.stroke.palm_N)) : nullptr);
                            add_pluck(*perf, to_beat(t_lift), lift);
                            ringing_until = t_lift + 1.5;
                            end_of_note = ringing_until;
                            if (sp.wound) add_squeak(*perf, m, i, held, p.fret, to_beat(t_lift), std::max(0.04, t_land - t_lift), (unsigned) perf->squeaks.size());
                        }
                    }
                    if (p.fret > 0 && s.finger_noises) {
                        const Action& a = m.action[(size_t) p.fret];
                        Pluck land = make_pluck(m, i, p.fret, LAND_SHARE * a.h / Y_REF, TAU_LAND, 0.03, Kind::land,
                                                palm_now ? keep(*perf, palms.get(m, i, p.fret, p.stroke.palm_x, p.stroke.palm_N)) : nullptr);
                        settle(t_land);
                        add_pluck(*perf, to_beat(t_land), land);
                        ringing_until = t_land + 1.5;
                        end_of_note = ringing_until;
                    }
                    held = p.fret;
                }
                settle(p.t);
                // the stroke itself: attack network at the stroke's force and pluck point
                const double force = s.force * std::max(p.factor, 0.02);
                const auto a = pluck_network(sp.mu, sp.L, m.tension[(size_t) i], p.fret, p.pluck, force, s.pick, s.pick_mm);
                Pluck pl = make_pluck(m, i, p.fret, a["y0"] / Y_REF, a["tau"], p.pluck, Kind::strike, p.palm);
                add_pluck(*perf, to_beat(p.t), pl);
                ringing_until = p.t + p.length;
                last_t = p.t;
                end_of_note = p.t + p.length;
            }
            settle(1e300);
        }
        for (auto [a, b] : palm_spans) {
            Event on, off;
            on.type = EventType::palm_on;
            on.beat = to_beat(a);
            off.type = EventType::palm_off;
            off.beat = to_beat(b);
            perf->events.push_back(on);
            perf->events.push_back(off);
        }
        // a damp written for a note that the string has already left (struck again first) does nothing: the
        // engine only damps the note that rings. Order: at the same beat, a damp comes before a pluck.
        std::stable_sort(perf->events.begin(), perf->events.end(), [](const Event& x, const Event& y) {
            if (std::abs(x.beat - y.beat) > 1e-12) return x.beat < y.beat;
            return (x.type == EventType::damp) && (y.type != EventType::damp);
        });
        return perf;
    }

private:
    static const Palm* keep(Performance& perf, std::shared_ptr<const Palm> p) {
        perf.palms.push_back(p);
        return p.get();
    }

    static Pluck make_pluck(const Model& m, int i, int fret, double gain, double tau, double pluck, Kind kind, const Palm* palm) {
        Pluck p;
        p.string = i;
        p.fret = fret;
        p.place = m.place[(size_t) i][(size_t) fret];
        p.gain = gain;
        p.tau = tau;
        p.pluck = pluck;
        p.kind = kind;
        p.palm = palm;
        return p;
    }

    static void add_pluck(Performance& perf, double beat, const Pluck& p) {
        Event e;
        e.type = EventType::pluck;
        e.beat = beat;
        e.string = p.string;
        e.pluck = p;
        perf.events.push_back(e);
    }

    // damp: the end of a note. `left`: the finger leaves the fret (the lift pluck takes over)
    static void add_damp(Performance& perf, int i, double beat, bool left) {
        Event e;
        e.type = EventType::damp;
        e.beat = beat;
        e.string = i;
        e.squeak = left ? 1 : 0;
        perf.events.push_back(e);
    }

    void add_squeak(Performance& perf, const Model& m, int i, int from, int to, double beat, double dur, unsigned seed) {
        const auto& sp = STRING_SPECS[i];
        const Squeak v = squeak_network(sp.mu, sp.L, m.core_fraction(i), from, to, dur, m.s.finger_force);
        std::vector<double> bumps;
        if (from > 0 && to > 0) {
            // the finger rides over the fret wires between the two frets (as GRMGuitarApp.squeak)
            auto pos = [&](int k) { return v.L * (1 - std::pow(2.0, -k / 12.0)); };
            const double xa = pos(from) - 0.005, xb = pos(to) - 0.005;
            for (int k = std::min(from, to); k < std::max(from, to); ++k) {
                const double c = (pos(k) - xa) / (xb - xa);
                if (c > 0.0 && c < 1.0) bumps.push_back(std::acos(1 - 2 * c) / PI);
            }
        }
        const double mid = (from + (to > 0 ? to : from)) / 2.0;
        const auto sound = synthesize_squeak(v.fsq, v.P, dur, v.fL, 1 - std::pow(2.0, -mid / 12.0), v.v, m.sample_rate, seed, bumps, v.N);
        perf.squeaks.emplace_back(sound.begin(), sound.end());
        Event e;
        e.type = EventType::squeak;
        e.beat = beat;
        e.string = i;
        e.squeak = (int) perf.squeaks.size() - 1;
        perf.events.push_back(e);
    }

    // the player network over the moments, in windows (one network per window, each starting where the
    // last one left the arm): what the arm does at one stroke depends on the strokes around it
    template <class Rng>
    static std::vector<Stroke> run_player(const std::vector<std::pair<double, int>>& grid, double step, const Settings& s,
                                          Rng& rng) {
        PlayerNoise noise;
        noise.exact = !s.human;
        noise.rng.seed(rng());
        std::vector<Stroke> out;
        const size_t window = 32;
        for (size_t a = 0; a < grid.size(); a += window) {
            const size_t b = std::min(grid.size(), a + window);
            std::vector<std::pair<double, int>> part(grid.begin() + (long) a, grid.begin() + (long) b);
            auto r = player_network(part, step, s.pluck, noise, s.swing_cm / 100.0);
            out.insert(out.end(), r.begin(), r.end());
        }
        return out;
    }

    template <class Moments, class Rng>
    static std::vector<Stroke> run_player(const Moments& order, double step, double spb, const Settings& s, Rng& rng) {
        std::vector<std::pair<double, int>> grid;
        for (const auto& mo : order) grid.push_back({mo.beat * spb, (int) std::lround(mo.beat * 4.0)});
        return run_player(grid, step, s, rng);
    }
};

}  // namespace grmg
