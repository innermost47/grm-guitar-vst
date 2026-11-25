// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// wav.hpp
//
// A 16-bit stereo WAV for the offline tools, normalised to its peak (-1 dB).
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

namespace grmg::wav
{
    inline bool write_stereo(const std::string &path, const std::array<std::vector<double>, 2> &x, int sr)
    {
        std::ofstream f(path, std::ios::binary);
        if (!f)
            return false;
        auto u32 = [&](uint32_t v)
        { f.write((const char *)&v, 4); };
        auto u16 = [&](uint16_t v)
        { f.write((const char *)&v, 2); };
        const uint32_t n = (uint32_t)std::min(x[0].size(), x[1].size());
        f.write("RIFF", 4);
        u32(36 + n * 4);
        f.write("WAVEfmt ", 8);
        u32(16);
        u16(1);
        u16(2);
        u32((uint32_t)sr);
        u32((uint32_t)sr * 4);
        u16(4);
        u16(16);
        f.write("data", 4);
        u32(n * 4);
        double peak = 1e-12;
        for (const auto &c : x)
            for (size_t t = 0; t < n; ++t)
                peak = std::max(peak, std::abs(c[t]));
        for (uint32_t t = 0; t < n; ++t)
            for (int ch = 0; ch < 2; ++ch)
            {
                const int16_t s = (int16_t)(x[(size_t)ch][t] / peak * 0.89 * 32767);
                f.write((const char *)&s, 2);
            }
        return (bool)f;
    }

    inline bool write_mono(const std::string &path, const std::vector<double> &x, int sr)
    {
        return write_stereo(path, {x, x}, sr);
    }
}
