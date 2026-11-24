// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// smoothed.hpp
//
// A gain a knob moves: it glides to its new value over LEVEL_SMOOTH_S instead of jumping (no zipper, no click).
#pragma once

#include <cmath>

#include "../../constants/core.hpp"

namespace grmg
{
    class Smoothed
    {
    public:
        void prepare(double sr) { coef_ = 1.0 - std::exp(-1.0 / (LEVEL_SMOOTH_S * sr)); }
        void set(double target) { target_ = target; }
        void jump(double value) { value_ = target_ = value; }
        double next() { return value_ += (target_ - value_) * coef_; }
        double value() const { return value_; }

    private:
        double value_ = 0.0, target_ = 0.0, coef_ = 1.0;
    };
}
