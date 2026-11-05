// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// What one guitarist can play: the hands are the constraints.
//   - the left hand has four fingers (the thumb over the neck, optional, a fifth on the low E); a finger lying
//     across several strings at the lowest fret is a barre (one finger), and one finger can press neighbouring
//     strings at the same fret;
//   - its stretch is a GRM network: each fingertip sits just behind its fret (the frets come closer up the
//     neck, by the rule of the twelfth root of two), the hand has its natural spread (one finger per fret), and
//     the tension the network is left with is the strain of the hand; above HAND_TENSION_MAX it cannot be done.
//     A span of four frets low on the neck (3 to 7) is hard but possible; five is not; higher up, where the frets
//     are closer, more frets fit;
//   - the right hand with a pick strikes several strings at once only if they are neighbours (a sweep: the pick
//     cannot jump a string); with the fingers, at most five strings at once.
// The roll editor refuses a change that adds an impossible moment; the player does not invent impossible ones.
#pragma once

#include <algorithm>
#include <cmath>
#include <set>
#include <string>
#include <vector>

#include "grm.hpp"
#include "model.hpp"
#include "song.hpp"

namespace grmg {

constexpr double FINGER_BEHIND = 0.010;    // m: a fingertip presses this far behind its fret wire
constexpr double TIP_SIGMA = 0.003;        // m: how far a fingertip may sit from that place
constexpr double FINGER_SPREAD = 0.022;    // m: the natural distance between two neighbouring fingertips
constexpr double HAND_SIGMA = 0.015;       // m: how much the hand gives, per finger gap
constexpr double HAND_TENSION_MAX = 4.0;   // strain (cost of the hand network) the hand can still hold

inline double fret_position(int fret, double L = STRING_SPECS[0].L) {     // m from the nut
    return L * (1.0 - std::pow(2.0, -fret / 12.0));
}

struct HandResult {
    bool possible = true;
    int fingers = 0;
    double tension = 0.0;     // strain of the hand network (0: at ease)
    std::string why;
};

// the left hand for the fretted notes {string, fret} held at the same moment
inline HandResult left_hand(std::vector<std::pair<int, int>> held, bool thumb = false) {
    HandResult r;
    held.erase(std::remove_if(held.begin(), held.end(), [](auto h) { return h.second <= 0; }), held.end());
    if (held.empty()) return r;
    std::sort(held.begin(), held.end(), [](auto a, auto b) { return a.second != b.second ? a.second < b.second : a.first < b.first; });
    const int low = held.front().second;
    // fingers: the lowest fret is one finger (a barre if several strings); at another fret, neighbouring strings
    // share a finger; a bass note at the lowest fret can be the thumb
    struct Finger { int fret; int number; };
    std::vector<Finger> fingers;
    fingers.push_back({low, 1});
    bool thumb_used = false;
    for (size_t k = 0; k < held.size(); ++k) {
        const auto [s, f] = held[k];
        if (f == low) continue;
        const bool shares = k > 0 && held[k - 1].second == f && std::abs(held[k - 1].first - s) == 1;
        if (shares) continue;
        fingers.push_back({f, std::min(4, 1 + f - low)});
    }
    if (thumb && fingers.size() > 4)
        for (const auto& [s, f] : held)
            if (s == 0 && f == low) thumb_used = true;
    r.fingers = (int) fingers.size();
    if (r.fingers > (thumb_used ? 5 : 4)) {
        r.possible = false;
        r.why = std::to_string(r.fingers) + " fingers needed, the left hand has " + (thumb ? "5 with the thumb" : "4");
        return r;
    }
    if (fingers.size() < 2) return r;
    // the stretch: fingertips behind their frets, against the hand's natural spread
    grm::Network net;
    std::map<std::string, grm::Measurement> m;
    for (size_t j = 0; j < fingers.size(); ++j) {
        const std::string n = "p" + std::to_string(j);
        net.add_node(n, fret_position(fingers[j].fret) - FINGER_BEHIND, true);
        m[n] = {fret_position(fingers[j].fret) - FINGER_BEHIND, TIP_SIGMA};
        if (j) {
            const int gaps = std::max(1, fingers[j].number - fingers[j - 1].number);
            net.add_sum("spread " + std::to_string(j), {{n, 1.0}, {"p" + std::to_string(j - 1), -1.0}}, gaps * FINGER_SPREAD,
                        HAND_SIGMA * std::sqrt((double) gaps));
        }
    }
    const auto res = net.solve(m, 50, 1e-3, 1e-12, false);
    r.tension = res.cost;
    if (r.tension > HAND_TENSION_MAX) {
        r.possible = false;
        r.why = "stretch from fret " + std::to_string(low) + " to fret " + std::to_string(fingers.back().fret) + " is too wide here";
    }
    return r;
}

// the right hand for the strings struck at the same moment
inline std::string right_hand(std::vector<int> strings, bool pick) {
    std::sort(strings.begin(), strings.end());
    strings.erase(std::unique(strings.begin(), strings.end()), strings.end());
    if (strings.size() <= 1) return "";
    if (pick) {
        for (size_t k = 1; k < strings.size(); ++k)
            if (strings[k] - strings[k - 1] != 1) return "a pick cannot strike strings " + std::to_string(6 - strings[k - 1]) + " and "
                                                           + std::to_string(6 - strings[k]) + " without the ones between";
    } else if (strings.size() > 5) {
        return "the right hand has five fingers";
    }
    return "";
}

struct Problem {
    double beat;
    std::string why;
};

// every moment of the song one guitarist cannot play
inline std::vector<Problem> impossible_moments(const Song& song, bool pick, bool thumb = false) {
    std::vector<Problem> out;
    std::set<long> starts;
    for (const auto& n : song.notes) starts.insert(std::lround(n.start * 960.0));
    for (const auto& s : song.strums) starts.insert(std::lround(s.start * 960.0));
    for (long k : starts) {
        const double t = k / 960.0;
        // the left hand: every fretted note sounding at t (notes and strums together: one guitar)
        std::vector<std::pair<int, int>> held;
        std::vector<int> struck;
        bool strum_here = false;
        for (const auto& n : song.notes) {
            if (n.start <= t + 1e-9 && t < n.start + n.length - 1e-9) held.push_back({n.string, n.fret});
            if (std::abs(n.start - t) < 1e-9) struck.push_back(n.string);
        }
        for (const auto& s : song.strums)
            if (s.start <= t + 1e-9 && t < s.start + s.length - 1e-9) {
                for (int i = 0; i < NUM_STRINGS; ++i)
                    if (s.frets[(size_t) i] > 0) held.push_back({i, s.frets[(size_t) i]});
                if (std::abs(s.start - t) < 1e-9) strum_here = true;
            }
        // one string holds one fret: a later note on a string replaces the earlier one
        std::vector<std::pair<int, int>> per_string;
        for (int i = 0; i < NUM_STRINGS; ++i) {
            int fret = -1;
            for (const auto& [s, f] : held)
                if (s == i) fret = f;
            if (fret > 0) per_string.push_back({i, fret});
        }
        const auto hand = left_hand(per_string, thumb);
        if (!hand.possible) out.push_back({t, "left hand: " + hand.why});
        if (!strum_here) {
            const std::string why = right_hand(struck, pick);
            if (!why.empty()) out.push_back({t, "right hand: " + why});
        }
    }
    return out;
}

}  // namespace grmg
