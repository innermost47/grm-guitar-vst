// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// cabinet.hpp
//
// The speaker in its cabinet, as a close microphone hears it, from what its network solved (models::CabinetModel):
// the box resonance, the dip and the breakup peak of the cone, the fall above it (what takes the fizz off a
// distorted amplifier). Minimum-phase filters, as a speaker is: no latency, nothing to convolve.
#pragma once

#include <array>

#include "../../models/amp.hpp"
#include "../../models/core.hpp"

namespace grmg
{
    class Cabinet
    {
    public:
        static constexpr int MAX_FILTERS = 8;

        void set_model(const models::CabinetModel &m, double sr)
        {
            count_ = 0;
            for (const auto &f : m.filters)
            {
                if (count_ >= MAX_FILTERS)
                    break;
                auto &bq = filters_[(size_t)count_++];
                const double hz = std::min(f.hz, 0.45 * sr);
                switch (f.kind)
                {
                case models::CabinetFilter::Kind::highpass:
                    bq.set_highpass(sr, hz, f.q);
                    break;
                case models::CabinetFilter::Kind::lowpass:
                    bq.set_lowpass(sr, hz, f.q);
                    break;
                case models::CabinetFilter::Kind::peak:
                    bq.set_peaking(sr, hz, f.q, f.db);
                    break;
                }
            }
        }

        void reset()
        {
            for (auto &f : filters_)
                f.reset();
        }

        double process(double x)
        {
            for (int i = 0; i < count_; ++i)
                x = filters_[(size_t)i].process(x);
            return x;
        }

    private:
        std::array<models::Biquad, MAX_FILTERS> filters_{};
        int count_ = 0;
    };
}
