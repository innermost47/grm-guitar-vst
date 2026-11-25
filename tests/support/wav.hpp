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
#include <cstring>
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

    // the first channel of a PCM WAV (16, 24 or 32 bits integers, or 32 bits floats), in [-1, 1], and its sample rate;
    // false when it is not one
    inline bool read_mono(const std::string &path, std::vector<double> &x, int &sr)
    {
        std::ifstream f(path, std::ios::binary);
        char id[4];
        uint32_t size = 0;
        if (!f.read(id, 4) || std::string(id, 4) != "RIFF" || !f.read((char *)&size, 4) || !f.read(id, 4) || std::string(id, 4) != "WAVE")
            return false;
        uint16_t format = 0, channels = 0, bits = 0;
        uint32_t rate = 0;
        bool have_format = false;
        while (f.read(id, 4) && f.read((char *)&size, 4))
        {
            const std::string chunk(id, 4);
            if (chunk == "fmt ")
            {
                std::vector<char> b(size);
                if (!f.read(b.data(), size) || size < 16)
                    return false;
                std::memcpy(&format, b.data(), 2);
                std::memcpy(&channels, b.data() + 2, 2);
                std::memcpy(&rate, b.data() + 4, 4);
                std::memcpy(&bits, b.data() + 14, 2);
                if (format == 0xFFFE && size >= 26) // WAVE_FORMAT_EXTENSIBLE: the sub-format's first two bytes
                    std::memcpy(&format, b.data() + 24, 2);
                have_format = true;
            }
            else if (chunk == "data" && have_format)
            {
                const size_t bytes = bits / 8, frame = bytes * channels;
                if (frame == 0 || !(format == 1 || (format == 3 && bits == 32)))
                    return false;
                std::vector<unsigned char> b(size);
                f.read((char *)b.data(), size);
                const size_t frames = (size_t)f.gcount() / frame;
                x.resize(frames);
                for (size_t i = 0; i < frames; ++i)
                {
                    const unsigned char *p = b.data() + i * frame;
                    if (format == 3)
                    {
                        float v;
                        std::memcpy(&v, p, 4);
                        x[i] = v;
                    }
                    else
                    {
                        int32_t v = 0;
                        for (size_t k = 0; k < bytes; ++k)
                            v |= (int32_t)p[k] << (8 * k);
                        const int shift = 32 - (int)bits; // sign-extend
                        x[i] = (double)((v << shift) >> shift) / std::pow(2.0, (double)bits - 1.0);
                    }
                }
                sr = (int)rate;
                return true;
            }
            else
            {
                f.seekg(size + (size & 1), std::ios::cur);
            }
        }
        return false;
    }
}
