// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// Every chord (12 roots, sharps and flats, and the usual qualities) on any tuning: the shape is not written by
// hand but chosen by the left hand. Each candidate shape (the strings from a bass string up, each on a chord tone
// within a 4-fret window, or open) is asked to the hand network (left_hand): the ones it cannot play are dropped,
// the easiest of the rest is taken (root in the bass, every chord tone, as many strings as possible, low on the
// neck, little strain). The open shapes of the built-in list stay as they were in standard tuning.
// Also the scales of the roll (for harmonizing) and their degrees.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <map>
#include <tuple>
#include <mutex>
#include <string>
#include <vector>

#include "../constants/core.hpp"
#include "playability.hpp"
#include "song.hpp"

namespace grmg
{

    // tones: intervals above the root (semitones); needed: the ones a shape must contain (the fifth may go)
    inline const std::vector<models::ChordQuality> &chord_qualities()
    {
        static const std::vector<models::ChordQuality> q = {
            {"", "major", {0, 4, 7}, {0, 4}},
            {"m", "minor", {0, 3, 7}, {0, 3}},
            {"7", "7", {0, 4, 7, 10}, {0, 4, 10}},
            {"m7", "m7", {0, 3, 7, 10}, {0, 3, 10}},
            {"maj7", "maj7", {0, 4, 7, 11}, {0, 4, 11}},
            {"6", "6", {0, 4, 7, 9}, {0, 4, 9}},
            {"m6", "m6", {0, 3, 7, 9}, {0, 3, 9}},
            {"sus2", "sus2", {0, 2, 7}, {0, 2, 7}},
            {"sus4", "sus4", {0, 5, 7}, {0, 5, 7}},
            {"add9", "add9", {0, 2, 4, 7}, {0, 2, 4}},
            {"9", "9", {0, 2, 4, 7, 10}, {0, 2, 4, 10}},
            {"5", "power (5)", {0, 7}, {0, 7}},
            {"dim", "dim", {0, 3, 6}, {0, 3, 6}},
            {"aug", "aug", {0, 4, 8}, {0, 4, 8}},
            {"m7b5", "m7b5", {0, 3, 6, 10}, {0, 3, 6, 10}},
            {"dim7", "dim7", {0, 3, 6, 9}, {0, 3, 6, 9}},
        };
        return q;
    }

    inline std::string chord_name(int root, int quality)
    {
        return std::string(ROOT_NAMES[((root % 12) + 12) % 12]) + chord_qualities()[(size_t)quality].suffix;
    }

    // the root and quality of a chord name ("F#m7", "Bbmaj7", "Db"), or {-1, -1}
    inline std::pair<int, int> parse_chord(const std::string &name)
    {
        static const char *alt[12] = {"C", "Db", "D", "D#", "E", "F", "Gb", "G", "G#", "A", "A#", "B"};
        int root = -1;
        size_t len = 0;
        for (int r = 0; r < 12; ++r)
            for (const char *n : {ROOT_NAMES[r], alt[r]})
            {
                const size_t l = std::char_traits<char>::length(n);
                if (l > len && name.compare(0, l, n) == 0)
                {
                    root = r;
                    len = l;
                }
            }
        if (root < 0)
            return {-1, -1};
        const std::string rest = name.substr(len);
        const auto &q = chord_qualities();
        for (size_t k = 0; k < q.size(); ++k)
            if (rest == q[k].suffix)
                return {root, (int)k};
        return {-1, -1};
    }

    // the shape the hand chooses for this chord on this tuning
    inline models::Chord voice_chord(int root, int quality, const int *open_midi)
    {
        root = ((root % 12) + 12) % 12;
        const auto &q = chord_qualities()[(size_t)quality];
        models::Chord best;
        best.name = chord_name(root, quality);
        // the built-in open shapes, in standard tuning
        const bool standard = open_midi[0] == 40 && open_midi[1] == 45 && open_midi[2] == 50 && open_midi[3] == 55 && open_midi[4] == 59 && open_midi[5] == 64;
        if (standard)
            for (const auto &c : CHORD_SHAPES)
                if (best.name == c.name)
                {
                    for (int i = 0; i < 6; ++i)
                        best.frets[(size_t)i] = c.frets[i];
                    return best;
                }
        auto tone = [&](int midi)
        {
            const int iv = ((midi - root) % 12 + 12) % 12;
            for (int t : q.tones)
                if (t == iv)
                    return iv;
            return -1;
        };
        const int min_strings = q.tones.size() <= 2 ? 2 : 3;
        double best_score = -1e300;
        // the strings played: a run from a bass string `low` to a top string `high` (the others are not played)
        for (int low = 0; low <= 3; ++low)
            for (int high = low + min_strings - 1; high < 6; ++high)
                for (int window = 0; window <= 10; ++window)
                {
                    // each string: open (low on the neck only), or a fret of the window, on a chord tone
                    std::array<std::vector<int>, 6> options;
                    bool possible = true;
                    for (int i = low; i <= high && possible; ++i)
                    {
                        for (int f : {0, window + 1, window + 2, window + 3, window + 4})
                            if ((f > 0 || window <= 2) && f < NUM_FRETS && tone(open_midi[i] + f) >= 0 && std::find(options[(size_t)i].begin(), options[(size_t)i].end(), f) == options[(size_t)i].end())
                                options[(size_t)i].push_back(f);
                        possible = !options[(size_t)i].empty();
                    }
                    if (!possible)
                        continue;
                    std::array<int, 6> pick{{-1, -1, -1, -1, -1, -1}};
                    std::function<void(int)> walk = [&](int i)
                    {
                        if (i > high)
                        {
                            if (tone(open_midi[low] + pick[(size_t)low]) != 0)
                                return; // the root in the bass
                            std::vector<int> have;
                            std::vector<std::pair<int, int>> held;
                            int span_lo = 99, span_hi = 0;
                            for (int s = low; s <= high; ++s)
                            {
                                have.push_back(tone(open_midi[s] + pick[(size_t)s]));
                                if (pick[(size_t)s] > 0)
                                {
                                    held.push_back({s, pick[(size_t)s]});
                                    span_lo = std::min(span_lo, pick[(size_t)s]);
                                    span_hi = std::max(span_hi, pick[(size_t)s]);
                                }
                            }
                            for (int need : q.needed)
                                if (std::find(have.begin(), have.end(), need) == have.end())
                                    return;
                            const int strings = high - low + 1;
                            // more strings, lower on the neck, a narrower hand, fewer fingers; then the hand's own strain
                            double score = 2.0 * strings - 1.0 * (held.empty() ? 0 : span_lo) - 1.5 * (held.empty() ? 0 : span_hi - span_lo) - 0.4 * (double)held.size();
                            if (score <= best_score)
                                return; // the strain only lowers it
                            const auto hand = left_hand(held);
                            if (!hand.possible)
                                return;
                            score -= 2.0 * hand.tension;
                            if (score <= best_score)
                                return;
                            best_score = score;
                            best.frets = pick;
                            return;
                        }
                        for (int f : options[(size_t)i])
                        {
                            pick[(size_t)i] = f;
                            walk(i + 1);
                        }
                        pick[(size_t)i] = -1;
                    };
                    walk(low);
                }
        return best;
    }

    // the same with a cache (shapes are asked often by the roll)
    inline models::Chord chord_for(int root, int quality, int tuning)
    {
        static std::mutex lock;
        static std::map<std::tuple<int, int, int>, models::Chord> cache;
        std::lock_guard<std::mutex> g(lock);
        const auto key = std::make_tuple(((root % 12) + 12) % 12, quality, tuning);
        auto it = cache.find(key);
        if (it != cache.end())
            return it->second;
        const auto c = voice_chord(root, quality, TUNINGS[std::min(std::max(tuning, 0), NUM_TUNINGS - 1)].midi);
        cache[key] = c;
        return c;
    }

    inline const std::vector<models::Scale> &scales()
    {
        static const std::vector<models::Scale> s = {
            {"Major", {0, 2, 4, 5, 7, 9, 11}},
            {"Natural minor", {0, 2, 3, 5, 7, 8, 10}},
            {"Harmonic minor", {0, 2, 3, 5, 7, 8, 11}},
            {"Dorian", {0, 2, 3, 5, 7, 9, 10}},
            {"Mixolydian", {0, 2, 4, 5, 7, 9, 10}},
            {"Phrygian", {0, 1, 3, 5, 7, 8, 10}},
            {"Lydian", {0, 2, 4, 6, 7, 9, 11}},
            {"Major pentatonic", {0, 2, 4, 7, 9}},
            {"Minor pentatonic", {0, 3, 5, 7, 10}},
            {"Blues", {0, 3, 5, 6, 7, 10}},
        };
        return s;
    }

    // the pitch `degrees` scale steps above `midi` (a note outside the scale starts from the scale note under it)
    inline int scale_step(int midi, int key, int scale, int degrees)
    {
        const auto &st = scales()[(size_t)std::min(std::max(scale, 0), (int)scales().size() - 1)].steps;
        const int n = (int)st.size();
        int octave = (int)std::floor((midi - key) / 12.0);
        int rel = midi - key - 12 * octave;
        int idx = 0;
        for (int k = 0; k < n; ++k)
            if (st[(size_t)k] <= rel)
                idx = k;
        const int off = rel - st[(size_t)idx];
        idx += degrees;
        octave += (int)std::floor((double)idx / n);
        idx = ((idx % n) + n) % n;
        return key + 12 * octave + st[(size_t)idx] + off;
    }

} // namespace grmg
