// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// listener_path.hpp
//
// A guitar heard by a listener in a room: each ear gets the direct sound after its way (a delay) and sends the same
// delayed sound, at its level, to the room's reverberation (a convolution, shared by the guitars of that room). The
// taps (listener_taps) glide: moving the listener slides the delays and the levels, the reverberation stays.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

#include "../constants/core.hpp"
#include "../models/core.hpp"
#include "smoothed.hpp"

namespace grmg
{
    class ListenerPath
    {
    public:
        void prepare(double sr)
        {
            sr_ = sr;
            size_t n = 4;
            while (n < (size_t)(LISTENER_MAX_DELAY_S * sr) + 4)
                n *= 2;
            ring_.assign(n, 0.0f);
            mask_ = n - 1;
            write_ = 0;
            for (auto *s : {&delay_[0], &delay_[1], &send_[0], &send_[1]})
                s->prepare(sr, LISTENER_GLIDE_S);
        }

        void reset()
        {
            std::fill(ring_.begin(), ring_.end(), 0.0f);
        }

        // where the listener is now: the taps glide there (`glide` false: they are there at once)
        void place(const models::ListenerTaps &taps, bool glide)
        {
            for (size_t ear = 0; ear < 2; ++ear)
            {
                const double samples = std::min(taps.delay_s[ear] * sr_, (double)(mask_ - 3));
                if (glide)
                {
                    delay_[ear].set(samples);
                    send_[ear].set(taps.send[ear]);
                }
                else
                {
                    delay_[ear].jump(samples);
                    send_[ear].jump(taps.send[ear]);
                }
            }
        }

        // `in` (it may be direct[0]) -> the two ears' direct sounds and what they send to the room
        void process(const float *in, float *const direct[2], float *const send[2], int n)
        {
            for (int t = 0; t < n; ++t)
            {
                ring_[write_] = in[t];
                for (size_t ear = 0; ear < 2; ++ear)
                {
                    const float x = read(delay_[ear].next());
                    direct[ear][t] = x;
                    send[ear][t] = (float)send_[ear].next() * x;
                }
                write_ = (write_ + 1) & mask_;
            }
        }

    private:
        // the sound `delay` samples ago (Hermite between the four nearest)
        float read(double delay) const
        {
            const double at = (double)write_ - delay;
            const double floor = std::floor(at);
            const float f = (float)(at - floor);
            const size_t k = (size_t)((long long)floor & (long long)mask_);
            const float y0 = ring_[(k - 1) & mask_], y1 = ring_[k], y2 = ring_[(k + 1) & mask_], y3 = ring_[(k + 2) & mask_];
            const float c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3, c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
            return ((c3 * f + c2) * f + c1) * f + y1;
        }

        std::vector<float> ring_ = std::vector<float>(4, 0.0f);
        size_t mask_ = 3, write_ = 0;
        double sr_ = 48000.0;
        std::array<Smoothed, 2> delay_, send_;
    };
}
