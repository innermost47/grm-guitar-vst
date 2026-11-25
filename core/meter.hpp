// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// meter.hpp
//
// The bar as a player feels it: its beats (quarter notes), grouped (7 = 2 + 2 + 3), each sixteenth at its level:
// 0 the downbeat, 1 the start of a group (or the half bar), 2 a beat, 3 an eighth between beats, 4 a sixteenth.
#pragma once

#include <algorithm>
#include <numeric>
#include <vector>

namespace grmg
{
    class Meter
    {
    public:
        // grouping: the beats of each group, summing to beats_per_bar (empty or wrong: the usual one)
        Meter(int beats_per_bar, std::vector<int> grouping = {})
            : beats_(beats_per_bar < 1 ? 1 : beats_per_bar), groups_(std::move(grouping))
        {
            if (groups_.empty() || std::accumulate(groups_.begin(), groups_.end(), 0) != beats_)
                groups_ = usual(beats_);
        }

        int beats() const { return beats_; }
        int sixteenths() const { return beats_ * 4; }
        const std::vector<int> &groups() const { return groups_; }

        // the usual grouping: 1 to 3 beats in one group, 5 = 3 + 2, by threes for 6, 9 and 12, else by twos (an odd
        // count ending on a three: 7 = 2 + 2 + 3)
        static std::vector<int> usual(int beats)
        {
            if (beats <= 3)
                return {beats};
            if (beats == 5)
                return {3, 2};
            if ((beats % 3 == 0 && beats % 2 != 0) || beats == 6 || beats == 12)
                return std::vector<int>((size_t)(beats / 3), 3);
            std::vector<int> g((size_t)(beats / 2), 2);
            if (beats % 2 != 0)
                g.back() = 3;
            return g;
        }

        // every way to group `beats` into twos and threes, the usual one first (5: 3 + 2, 2 + 3; 7: 2 + 2 + 3, ...)
        static std::vector<std::vector<int>> groupings(int beats)
        {
            std::vector<std::vector<int>> all;
            std::vector<int> current;
            auto walk = [&](auto &&self, int left) -> void
            {
                if (left == 0)
                {
                    all.push_back(current);
                    return;
                }
                for (int g : {2, 3})
                    if (g <= left)
                    {
                        current.push_back(g);
                        self(self, left - g);
                        current.pop_back();
                    }
            };
            walk(walk, beats);
            const auto first = usual(beats);
            all.erase(std::remove(all.begin(), all.end(), first), all.end());
            all.insert(all.begin(), first);
            return all;
        }

        // the level of a sixteenth (counted from any bar's start)
        int level(int sixteenth) const
        {
            const int per_bar = sixteenths();
            const int s = ((sixteenth % per_bar) + per_bar) % per_bar;
            if (s == 0)
                return 0;
            if (s % 4 == 0)
                return group_start(s / 4) ? 1 : 2;
            return s % 2 == 0 ? 3 : 4;
        }

        bool group_start(int beat) const
        {
            int at = 0;
            for (int g : groups_)
            {
                if (beat == at)
                    return true;
                at += g;
            }
            return false;
        }

    private:
        int beats_;
        std::vector<int> groups_;
    };

} // namespace grmg
