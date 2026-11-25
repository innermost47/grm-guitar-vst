// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// sound_check.hpp
//
// The sounds a probe rendered are checked as they come: every one finite and audible, or the run fails.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace grmg::timbre
{
    class SoundCheck
    {
    public:
        static constexpr double SILENCE = 1e-9;

        void check(const std::string &what, const std::vector<double> &x)
        {
            double peak = 0.0;
            bool finite = true;
            for (double v : x)
            {
                finite &= std::isfinite(v);
                peak = std::max(peak, std::abs(v));
            }
            if (finite && peak > SILENCE)
                return;
            std::printf("FAILED: %s is %s\n", what.c_str(), finite ? "silent" : "not finite");
            ok_ = false;
        }

        bool ok() const { return ok_; }

    private:
        bool ok_ = true;
    };
}
