// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// From the guitar roll to what the hands do (off the audio thread), as the sequencer of grm_guitar.py:
//   1) the player network: one stroke per moment the right hand strikes, over the whole song (the arm keeps
//      its swing; timing, force, pluck point, pick depth and palm wander as a player's do);
//      then the muscle network: every stroke costs an effort the arm recovers from slowly (a tired arm strikes
//      softer and slower, a stroke after a rest is fresh);
//   2) the strokes meet the strings: with a pick, a sweep network carries the hand's energy across the strings
//      (each string pushed aside takes some, the pick slows, the later strings get less), a strum reaching the
//      strings the arc of the wrist allows (strum_contacts); single notes are picked down on the sixteenths of
//      the beat and up between; with the fingers, a pinch (the thumb leads); the palm is one hand, a band of
//      neighbouring strings that also stops the strings ringing under it (palm network for each);
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
#include <utility>
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
    double extra = 40.0;       // damp: extra losses per second (at the fundamental; more for the higher modes)
};

// how a note stops (s^-1, extra losses at the fundamental): a fretted note dies as the finger relaxes on it, an
// open string waits for a hand to touch it (softer); a lifted finger leaves the string to the lift pluck
constexpr double RELEASE_FRETTED = 32.0, RELEASE_OPEN = 14.0, RELEASE_LIFT = 45.0;
// a chord released: the fingers leave one after the other (s between strings, before the player's own spread)
constexpr double RELEASE_SPREAD = 0.006;
// the palm: its edge presses less (share of the pressure on the strings next to the ones asked); how much harder
// it presses on a harder stroke (exponent of the stroke's force); how fast it stops a string it lands on (s^-1)
constexpr double PALM_EDGE = 0.45, PALM_FORCE_FOLLOW = 0.3, PALM_LAND_DAMP = 50.0;
// the sweep: the mass that carries the pick across the strings (kg): the pick and the fingers holding it, the
// arm behind a supple wrist; each string it pushes aside takes some of its energy
constexpr double HAND_MASS = 0.04, SWEEP_LOSS_MAX = 0.6;
// the muscle: recovery time (s), effort of a stroke of force 1 (fatigue units), how much fatigue weakens the stroke
constexpr double MUSCLE_RECOVER = 2.0, MUSCLE_EFFORT = 0.04, MUSCLE_WEAKEN = 0.3, MUSCLE_LIMITS[2] = {0.75, 1.1};
// fingers plucking together (a pinch): spread of their timing (s); the thumb leads a little
constexpr double PINCH_SPREAD = 0.004, THUMB_LEAD = 0.003, PINCH_FORCE_SIGMA = 0.06;

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
        if (m.s.palm_intensity < PALM_OFF) return nullptr;      // the palm is lifted
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
    bool loop = false;     // the song plays in a loop: its end leads into its start (hands, ringing notes)
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
        muscle(strokes, step);

        // ---------------------------------------------------------------- 2) the strokes meet the strings
        struct Played { double t; int string, fret; double factor, pluck, length; const Palm* palm; Stroke stroke; bool palm_on;
                        int chord_order; };   // place of the string in its strum (-1: a single note)
        std::vector<Played> played;
        std::vector<std::pair<double, double>> palm_spans;    // seconds
        struct Touch { double t; int string; double extra; };
        std::vector<Touch> touches;                           // the palm lands on strings that ring
        for (size_t k = 0; k < order.size(); ++k) {
            const auto& mo = order[k];
            const Stroke& stroke = strokes[k];
            // the palm is one hand: one band of neighbouring strings, around the ones asked, pressing less on its
            // edges, a little harder when the stroke is harder
            int lo = NUM_STRINGS, hi = -1;
            double longest = 0.0;
            for (const auto& st : mo.strikes)
                if (st.palm) {
                    lo = std::min(lo, st.string);
                    hi = std::max(hi, st.string);
                    longest = std::max(longest, st.length);
                }
            if (mo.strum >= 0 && song.strums[(size_t) mo.strum].palm) {
                const auto& st = song.strums[(size_t) mo.strum];
                for (int i = 0; i < NUM_STRINGS; ++i)
                    if (st.frets[(size_t) i] >= 0) {
                        lo = std::min(lo, i);
                        hi = std::max(hi, i);
                    }
                longest = std::max(longest, st.length);
            }
            const bool palm_here = hi >= 0;
            const bool palm_down = palm_here && m.s.palm_intensity >= PALM_OFF;
            auto palm_share = [&](int i) {
                if (!palm_here || i < lo - 1 || i > hi + 1) return 0.0;
                return (i >= lo && i <= hi) ? 1.0 : PALM_EDGE;
            };
            auto palm_for = [&](int i, int fret) -> const Palm* {
                const double share = palm_share(i);
                if (share <= 0) return nullptr;
                const double press = share * std::pow(std::max(stroke.force, 0.2), PALM_FORCE_FOLLOW);
                return keep(*perf, palms.get(m, i, fret, stroke.palm_x, stroke.palm_N * press));
            };
            std::vector<bool> struck(NUM_STRINGS, false);

            // single notes struck together: with a pick, one sweep across them (in the arm's direction: down on
            // the sixteenths of the beat, up between); with the fingers, a pinch
            if (!mo.strikes.empty()) {
                const bool up = s.pick && (std::lround(mo.beat * 4.0) % 2 != 0);
                std::vector<const Strike*> by_string;
                for (const auto& st : mo.strikes) by_string.push_back(&st);
                std::sort(by_string.begin(), by_string.end(), [&](auto a, auto b) { return up ? a->string > b->string : a->string < b->string; });
                std::vector<int> strings;
                std::vector<double> base;
                for (auto* st : by_string) {
                    strings.push_back(st->string);
                    base.push_back(st->velocity / 0.8);
                }
                std::vector<int> frets;
                for (auto* st : by_string) frets.push_back(st->fret);
                const auto contacts = s.pick ? sweep(m, strings, base, frets, stroke, 0.0)
                                             : pinch(strings, base, s.human ? &rng : nullptr);
                for (size_t q = 0; q < contacts.size(); ++q) {
                    const auto* st = by_string[q];
                    played.push_back({stroke.t + contacts[q].delay, st->string, st->fret, stroke.force * contacts[q].share, stroke.p,
                                      st->length * spb, palm_for(st->string, st->fret), stroke, palm_share(st->string) > 0,
                                      contacts.size() > 1 ? (int) q : -1});
                    struck[(size_t) st->string] = true;
                }
            }
            if (mo.strum >= 0) {
                const auto& st = song.strums[(size_t) mo.strum];
                std::vector<int> crossing;
                for (int i = 0; i < NUM_STRINGS; ++i)
                    if (st.frets[(size_t) i] >= 0 && st.frets[(size_t) i] < NUM_FRETS) crossing.push_back(i);
                if (st.up) std::reverse(crossing.begin(), crossing.end());     // index 0 is string 6: down = from the bass
                if (crossing.empty()) continue;
                // which strings the arc of the wrist reaches, and how deep (strum_contacts); then the sweep: each
                // string takes some of the arm's energy, so the pick slows and the later strings get less
                const auto reach = crossing.size() == 1 ? std::vector<Contact>{{crossing[0], 0.0, 1.0}}
                                                        : strum_contacts(crossing, stroke, st.up);
                if (reach.empty()) continue;
                std::vector<int> strings;
                std::vector<double> base;
                std::vector<int> frets;
                for (const auto& c : reach) {
                    strings.push_back(c.string);
                    base.push_back(c.share * st.velocity / 0.8);
                    frets.push_back(st.frets[(size_t) c.string]);
                }
                const auto contacts = sweep(m, strings, base, frets, stroke, reach.front().delay);
                for (size_t q = 0; q < contacts.size(); ++q) {
                    const int i = strings[q];
                    played.push_back({stroke.t + contacts[q].delay, i, frets[q], stroke.force * contacts[q].share, stroke.p,
                                      st.length * spb, palm_for(i, frets[q]), stroke, palm_share(i) > 0, (int) q});
                    struck[(size_t) i] = true;
                }
            }
            if (palm_here) {
                palm_spans.push_back({stroke.t - 0.05, stroke.t + longest * spb});
                for (int i = 0; i < NUM_STRINGS && palm_down; ++i)   // strings under the palm, ringing, not struck
                    if (!struck[(size_t) i] && palm_share(i) > 0)
                        touches.push_back({stroke.t - 0.03, i, PALM_LAND_DAMP * palm_share(i)});
            }
        }
        std::stable_sort(played.begin(), played.end(), [](const Played& a, const Played& b) { return a.t < b.t; });

        // ---------------------------------------------------------------- 3) and 4) string by string
        // in a loop the song's end leads into its start: times wrap around the song, and each string starts
        // its cycle with the hand as the end of the song left it
        const double song_s = perf->length_beats * spb;
        const bool wrap = o.loop && song_s > 0;
        auto to_beat = [&](double t) {
            if (!wrap) return std::max(0.0, t / spb);
            double b = std::fmod(t, song_s);
            if (b < 0) b += song_s;
            return b / spb;
        };
        auto under_palm = [&](double t) {
            for (auto [a, b] : palm_spans)
                if (a <= t && t < b) return true;
            return false;
        };
        // how a note ends: the finger relaxes (fretted) or a hand touches the string (open); the strings of a
        // chord are left one after the other, a few milliseconds apart
        auto release = [&](const Played& p, double t) {
            const double spread = p.chord_order >= 0 ? p.chord_order * RELEASE_SPREAD + std::abs(jitter(0.004)) : 0.0;
            return std::make_pair(t + spread, p.fret > 0 ? RELEASE_FRETTED : RELEASE_OPEN);
        };
        for (int i = 0; i < NUM_STRINGS; ++i) {
            const auto& sp = STRING_SPECS[i];
            std::vector<Played> mine;
            for (const auto& p : played)
                if (p.string == i) mine.push_back(p);
            if (mine.empty()) continue;
            int held = 0;                                // the fret the left hand holds on this string
            double ringing_until = -1.0, last_t = -1e300;  // the note it rings, the last stroke (s; -1e300: none)
            double end_of_note = -1.0, end_extra = RELEASE_FRETTED;   // when the note that rings ends, and how
            if (wrap) {                                  // the hand as the end of the song left it
                held = mine.back().fret;
                last_t = mine.back().t - song_s;
                ringing_until = last_t + mine.back().length;
            }
            auto settle = [&](double t_next) {           // the next thing on this string comes at t_next
                // a note that ends just before its string plays again is held to it (legato): the player does not
                // damp a string 60 ms before striking it
                if (end_of_note >= 0 && end_of_note < t_next - 0.06) add_damp(*perf, i, to_beat(end_of_note), false, end_extra);
                end_of_note = -1.0;
            };
            for (const auto& p : mine) {
                if (p.fret != held) {
                    // the hand changes shape just before the stroke (as late as it can: a player keeps the notes
                    // ringing until the fingers must move): lift from the old fret, land on the new one
                    const double gap = last_t < -1e299 ? 0.25 : p.t - last_t;
                    const double lead = std::max(0.025, std::min(0.09, 0.35 * gap)) + std::abs(jitter(0.006));
                    const double t_change = p.t - lead;
                    const double t_lift = t_change + std::max(0.0, 0.1 * lead + jitter(0.006));
                    const double t_land = t_change + std::max(0.3 * lead, 0.65 * lead + jitter(0.006));
                    const bool palm_now = under_palm(p.t);
                    settle(std::min(t_lift, t_land));
                    if (held > 0) {
                        if (ringing_until > t_lift) {      // the old note stops when the finger leaves
                            add_damp(*perf, i, to_beat(t_lift), true, RELEASE_LIFT);
                            ringing_until = t_lift;
                        }
                        if (s.finger_noises) {
                            const Action& a = m.action[(size_t) held];
                            Pluck lift = make_pluck(m, i, 0, a.h / Y_REF, TAU_LIFT, std::max(a.xf / sp.L, 0.02), Kind::lift,
                                                    palm_now ? keep(*perf, palms.get(m, i, 0, p.stroke.palm_x, p.stroke.palm_N)) : nullptr);
                            add_pluck(*perf, to_beat(t_lift), lift);
                            ringing_until = t_lift + 1.5;
                            end_of_note = ringing_until;
                            end_extra = RELEASE_OPEN;
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
                        end_extra = RELEASE_FRETTED;
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
                std::tie(end_of_note, end_extra) = release(p, p.t + p.length);
            }
            // the last note: in a loop it ends before the string's first stroke of the next cycle
            settle(wrap ? mine.front().t + song_s - 0.1 : 1e300);
        }
        for (const auto& t : touches) add_damp(*perf, t.string, to_beat(t.t), false, t.extra);
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

public:
    // the pick sweeps across `strings` (in the order it meets them), each asked with its own share of the stroke
    // (`base`). A GRM network carries the energy of the hand along the sweep: pushing string j aside takes the
    // elastic energy k_j d_j^2 / 2 of a string pressed at its pluck point (stiffness T Leff / (x (Leff - x))),
    // so w_{j+1} = (1 - c_j) w_j, with w the share of the energy left and c_j that energy over the hand's. The
    // pick slows (delays grow), the later strings get less: a slow, soft strum fades across the chord, a fast
    // one hardly. The delays start at `first` (the first string the arc of the wrist reaches).
    static std::vector<Contact> sweep(const Model& m, const std::vector<int>& strings, const std::vector<double>& base,
                                      const std::vector<int>& frets, const Stroke& stroke, double first) {
        std::vector<Contact> out;
        if (strings.empty()) return out;
        const double v0 = std::max(stroke.v, 0.05);
        const double kinetic = 0.5 * HAND_MASS * v0 * v0;
        grm::Network net;
        std::map<std::string, grm::Measurement> meas;
        for (size_t j = 0; j < strings.size(); ++j) net.add_node("w" + std::to_string(j), 1.0, true);
        meas["w0"] = {1.0, 1e-6};
        for (size_t j = 0; j + 1 < strings.size(); ++j) {
            const int i = strings[j];
            const double leff = STRING_SPECS[i].L * std::pow(2.0, -std::max(frets[j], 0) / 12.0);
            const double x = std::min(std::max(stroke.p, 0.05), 0.95);
            const double k = m.tension[(size_t) i] / (leff * x * (1.0 - x));
            const double d = FULL_DEPTH * std::min(1.0, std::max(base[j], 0.0));
            const double c = std::min(SWEEP_LOSS_MAX, 0.5 * k * d * d / kinetic);
            net.add_sum("sweep " + std::to_string(j), {{"w" + std::to_string(j + 1), 1.0}, {"w" + std::to_string(j), -(1.0 - c)}},
                        0.0, 1e-6);
        }
        const auto r = net.solve(meas, 20, 1e-3, 1e-14, false);
        double delay = first;
        for (size_t j = 0; j < strings.size(); ++j) {
            const double w = std::max(r["w" + std::to_string(j)], 1e-6);
            if (j) delay += std::abs(strings[j] - strings[j - 1]) * STRING_SPACING / (v0 * std::sqrt(w));
            out.push_back({strings[j], delay, base[j] * std::sqrt(w)});
        }
        return out;
    }

    // fingers plucking together (a pinch): each finger its own force and timing; the thumb (the bass) a little
    // ahead of the others
    template <class Rng>
    static std::vector<Contact> pinch(const std::vector<int>& strings, const std::vector<double>& base, Rng* rng) {
        std::vector<Contact> out;
        if (strings.empty()) return out;
        const int bass = *std::min_element(strings.begin(), strings.end());
        std::normal_distribution<double> normal(0.0, 1.0);
        for (size_t j = 0; j < strings.size(); ++j) {
            const bool thumb = strings.size() > 1 && strings[j] == bass;
            double delay = thumb ? 0.0 : THUMB_LEAD * (strings.size() > 1);
            double share = base[j];
            if (rng) {
                delay += std::abs(PINCH_SPREAD * normal(*rng));
                share *= std::pow(10.0, PINCH_FORCE_SIGMA * normal(*rng));
            }
            out.push_back({strings[j], delay, share});
        }
        return out;
    }

    // the muscle: every stroke costs an effort (its force squared) the arm recovers from slowly; a GRM network
    // carries the fatigue F along the strokes, F_k = e^{-dt/RECOVER} F_{k-1} + EFFORT force_k^2, and a tired arm
    // strikes softer and slower. Measured against the arm playing eighths at force 1 (the player's ordinary
    // effort): a stroke after a rest comes out a little stronger, the end of a long run of sixteenths weaker.
    static void muscle(std::vector<Stroke>& strokes, double step) {
        if (strokes.empty()) return;
        const double reference = MUSCLE_EFFORT / (1.0 - std::exp(-2.0 * step / MUSCLE_RECOVER));
        double carried = 0.0, t_carried = strokes.front().t - 10.0 * MUSCLE_RECOVER;
        const size_t window = 64;
        for (size_t a = 0; a < strokes.size(); a += window) {
            const size_t b = std::min(strokes.size(), a + window);
            grm::Network net;
            std::map<std::string, grm::Measurement> meas;
            net.add_node("F_", carried, true);
            meas["F_"] = {carried, 1e-9};
            double t_prev = t_carried;
            std::string prev = "F_";
            for (size_t k = a; k < b; ++k) {
                const std::string n = "F" + std::to_string(k);
                net.add_node(n, carried, true);
                const double decay = std::exp(-std::max(strokes[k].t - t_prev, 0.0) / MUSCLE_RECOVER);
                net.add_sum("effort " + std::to_string(k), {{n, 1.0}, {prev, -decay}}, MUSCLE_EFFORT * strokes[k].force * strokes[k].force,
                            1e-9);
                prev = n;
                t_prev = strokes[k].t;
            }
            const auto r = net.solve(meas, 20, 1e-3, 1e-16, false);
            for (size_t k = a; k < b; ++k) {
                const double f = r["F" + std::to_string(k)];
                const double factor = std::clamp((1.0 + MUSCLE_WEAKEN * reference) / (1.0 + MUSCLE_WEAKEN * f), MUSCLE_LIMITS[0],
                                                 MUSCLE_LIMITS[1]);
                strokes[k].force *= factor;
                strokes[k].v *= factor;
            }
            carried = r[prev];
            t_carried = t_prev;
        }
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
    static void add_damp(Performance& perf, int i, double beat, bool left, double extra) {
        Event e;
        e.type = EventType::damp;
        e.beat = beat;
        e.string = i;
        e.squeak = left ? 1 : 0;
        e.extra = extra;
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
