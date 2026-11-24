// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// How long each note really rings. A guitarist does not damp a string that has nothing else to do: an open string
// rings until it is played again (or a hand stops it), a fretted note rings as long as its finger can stay on its
// fret, that is until its string is played again or until the left hand needs that finger elsewhere (a later
// moment the hand cannot play while holding it). Notes asked to stop do stop at their end.
// Used by the compiler (what the strings do) and by the roll (the ringing drawn after each note).
#pragma once

#include <array>
#include <set>
#include <vector>

#include "../constants/core.hpp"
#include "playability.hpp"
#include "song.hpp"

namespace grmg
{

    // length: the beats the song lasts; loop: its end leads into its start (a note may ring into the next cycle)
    inline models::Ringing ringing(const Song &song, double length, bool loop, bool thumb = false)
    {
        constexpr double FAR = 1e6;
        struct Item
        {
            int string, fret;
            double start, nominal;
            bool rings;
            int note, strum;
            double end;
            bool lifted = false;
        };
        std::vector<Item> items;
        for (size_t k = 0; k < song.notes.size(); ++k)
        {
            const auto &n = song.notes[k];
            if (n.start >= length)
                continue;
            if (n.dead)
            { // a dead note does not ring (and holds no fret)
                items.push_back({n.string, 0, n.start, n.start + std::min(n.length, 0.1), false, (int)k, -1, n.start + std::min(n.length, 0.1)});
                continue;
            }
            items.push_back({n.string, n.fret, n.start, n.start + n.length, n.end == models::RollNote::ring, (int)k, -1, n.start + n.length});
        }
        for (size_t k = 0; k < song.strums.size(); ++k)
        {
            const auto &t = song.strums[k];
            if (t.start >= length)
                continue;
            for (int i = 0; i < 6; ++i)
                if (t.frets[(size_t)i] >= 0)
                    items.push_back({i, t.frets[(size_t)i], t.start, t.start + t.length, true, -1, (int)k, t.start + t.length});
        }
        std::stable_sort(items.begin(), items.end(), [](const Item &a, const Item &b)
                         { return a.start < b.start; });
        // every stroke time, and (in a loop) the same ones one cycle later
        std::set<long> starts;
        for (const auto &it : items)
        {
            starts.insert(std::lround(it.start * 960.0));
            if (loop)
                starts.insert(std::lround((it.start + length) * 960.0));
        }
        // the next strum (after `after`) whose shape differs from strum k's, and whether it plays `string`
        auto next_shape = [&](int k, int string, double after)
        {
            std::pair<double, bool> next{FAR, false};
            for (size_t j = 0; j < song.strums.size(); ++j)
            {
                const auto &o = song.strums[j];
                if (o.start >= length || o.frets == song.strums[(size_t)k].frets)
                    continue;
                for (double shift : {0.0, loop ? length : FAR})
                    if (shift < FAR && o.start + shift > after + 1e-9 && o.start + shift < next.first)
                        next = {o.start + shift, o.frets[(size_t)string] >= 0};
            }
            return next;
        };
        auto next_on = [&](int string, double after)
        {
            double next = FAR;
            for (const auto &it : items)
            {
                if (it.string != string)
                    continue;
                if (it.start > after + 1e-9)
                    next = std::min(next, it.start);
                if (loop && it.start + length > after + 1e-9)
                    next = std::min(next, it.start + length);
            }
            return next;
        };
        for (size_t k = 0; k < items.size(); ++k)
        {
            auto &it = items[k];
            if (!it.rings)
                continue;
            double end = next_on(it.string, it.start);
            if (it.fret > 0)
            {
                // the first later moment the hand cannot hold this fret with what it plays then
                for (long key : starts)
                {
                    const double t = key / 960.0;
                    if (t <= it.start + 1e-9)
                        continue;
                    if (t >= end - 1e-9)
                        break;
                    std::vector<std::pair<int, int>> held;
                    for (size_t q = 0; q < items.size(); ++q)
                    {
                        if (q == k)
                            continue;
                        const auto &o = items[q];
                        for (double shift : {0.0, loop ? length : FAR})
                        {
                            if (shift >= FAR)
                                continue;
                            const double a = o.start + shift, b = (q < k ? o.end : o.nominal) + shift;
                            if (a <= t + 1e-9 && t < b - 1e-9)
                                held.push_back({o.string, o.fret});
                        }
                    }
                    const bool without = left_hand(held, thumb).possible;
                    held.push_back({it.string, it.fret});
                    if (without && !left_hand(held, thumb).possible)
                    {
                        end = t;
                        break;
                    }
                }
            }
            // a strummer changing chord shape lifts the hand: the strings the new chord does not play stop there
            double lift_at = FAR;
            if (it.strum >= 0)
            {
                const auto [at, plays] = next_shape(it.strum, it.string, it.start);
                if (!plays && at < end - 1e-9)
                    end = lift_at = at;
            }
            it.end = std::max(end, it.nominal);
            it.lifted = lift_at < FAR && it.end <= lift_at + 1e-9;
        }
        // a hand laid flat on the strings stops whatever rings at that moment
        for (auto &it : items)
            for (double t : song.stops)
                for (double shift : {0.0, loop ? length : 1e300})
                {
                    if (shift > 1e299)
                        continue;
                    const double at = t + shift;
                    if (at > it.start + 1e-9 && at < it.end - 1e-9)
                    {
                        it.end = at;
                        it.lifted = false;
                    }
                }
        models::Ringing r;
        r.note_end.assign(song.notes.size(), 0.0);
        r.strum_end.assign(song.strums.size(), {-1, -1, -1, -1, -1, -1});
        r.strum_lifted.assign(song.strums.size(), {false, false, false, false, false, false});
        for (const auto &it : items)
        {
            if (it.note >= 0)
                r.note_end[(size_t)it.note] = it.end;
            else
            {
                r.strum_end[(size_t)it.strum][(size_t)it.string] = it.end;
                r.strum_lifted[(size_t)it.strum][(size_t)it.string] = it.lifted;
            }
        }
        for (size_t k = 0; k < song.notes.size(); ++k)
            if (r.note_end[k] <= 0)
                r.note_end[k] = song.notes[k].start + song.notes[k].length;
        return r;
    }

} // namespace grmg
