// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// spectrum_probe.hpp
//
// The spectrum of a stretch of sound (Hann window, FFT): its magnitudes, its centroid (where its energy sits, the
// brightness one hears), and the bumps of a response (peaks and dips that stand out of their neighbourhood).
#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

#include "../../core/fft.hpp"

namespace grmg::timbre
{
    class SpectrumProbe
    {
    public:
        static constexpr double THIRD_OCTAVE = 1.2599210498948732; // 2^(1/3)

        explicit SpectrumProbe(double sr) : sr_(sr) {}

        // magnitudes of the `size` samples of x from `from` (zero past the end), bins 0..size/2
        std::vector<double> magnitudes(const std::vector<double> &x, size_t from, size_t size) const
        {
            size = next_pow2(std::max<size_t>(size, 2));
            std::vector<cplx> a(size);
            for (size_t i = 0; i < size; ++i)
            {
                const double w = 0.5 - 0.5 * std::cos(2 * PI * (double)i / (double)(size - 1));
                a[i] = from + i < x.size() ? x[from + i] * w : 0.0;
            }
            fft(a, false);
            std::vector<double> m(size / 2 + 1);
            for (size_t k = 0; k < m.size(); ++k)
                m[k] = std::abs(a[k]);
            return m;
        }

        // the spectral centroid (Hz) of the stretch, between 20 Hz and 0.45 of the sample rate; 0 when it is silent
        double centroid(const std::vector<double> &x, size_t from, size_t size = 4096) const
        {
            const auto m = magnitudes(x, from, size);
            const double bin = sr_ / (double)((m.size() - 1) * 2);
            double num = 0.0, den = 0.0;
            for (size_t k = 0; k < m.size(); ++k)
            {
                const double f = (double)k * bin;
                if (f < 20.0 || f > 0.45 * sr_)
                    continue;
                num += f * m[k];
                den += m[k];
            }
            return den > 0.0 ? num / den : 0.0;
        }

        // the frequency (Hz) below which `share` of the whole signal's energy lies (Paté et al. 2014, Fig. 2: 95 %
        // below 2.5 kHz for a P-90 near the bridge)
        double energy_below_hz(const std::vector<double> &x, double share) const
        {
            const size_t size = next_pow2(std::max<size_t>(x.size(), 2));
            std::vector<cplx> a(size);
            for (size_t i = 0; i < x.size(); ++i)
                a[i] = x[i];
            fft(a, false);
            double total = 0.0;
            for (size_t k = 1; k <= size / 2; ++k)
                total += std::norm(a[k]);
            double sum = 0.0;
            for (size_t k = 1; k <= size / 2; ++k)
            {
                sum += std::norm(a[k]);
                if (sum >= share * total)
                    return (double)k * sr_ / (double)size;
            }
            return 0.5 * sr_;
        }

        // the levels (dB) of the third-octave bands from `lo` to `hi` Hz of the stretch: the spectrum as the ear
        // groups it, smooth enough to compare two strokes without their partials' exact places
        std::vector<double> band_levels_db(const std::vector<double> &x, size_t from, size_t size, double lo = 100.0, double hi = 8000.0) const
        {
            const auto m = magnitudes(x, from, size);
            const double bin = sr_ / (double)((m.size() - 1) * 2);
            std::vector<double> levels;
            for (double centre = lo; centre <= hi * 1.0001; centre *= THIRD_OCTAVE)
            {
                const double a = centre / std::sqrt(THIRD_OCTAVE), b = centre * std::sqrt(THIRD_OCTAVE);
                double energy = 0.0;
                for (size_t k = (size_t)std::ceil(a / bin); k < m.size() && (double)k * bin < b; ++k)
                    energy += m[k] * m[k];
                levels.push_back(10.0 * std::log10(energy + 1e-30));
            }
            return levels;
        }

        // a response's level (dB) at `points` log-spaced frequencies between lo and hi
        std::vector<double> response_db(const std::vector<double> &impulse, double lo, double hi, int points) const
        {
            const auto m = magnitudes_flat(impulse);
            const double bin = sr_ / (double)((m.size() - 1) * 2);
            std::vector<double> db((size_t)points);
            for (int p = 0; p < points; ++p)
            {
                const double f = lo * std::pow(hi / lo, (double)p / (double)(points - 1));
                const double k = f / bin;
                const size_t k0 = std::min((size_t)k, m.size() - 2);
                const double u = k - (double)k0;
                db[(size_t)p] = 20.0 * std::log10((1.0 - u) * m[k0] + u * m[k0 + 1] + 1e-12);
            }
            return db;
        }

        // the bumps of a curve: its local maxima and minima that stand at least `prominence` dB above or below both
        // sides of them before the curve turns back past that much
        static int bumps(const std::vector<double> &db, double prominence)
        {
            int count = 0;
            for (size_t i = 1; i + 1 < db.size(); ++i)
            {
                const bool peak = db[i] > db[i - 1] && db[i] >= db[i + 1];
                const bool dip = db[i] < db[i - 1] && db[i] <= db[i + 1];
                if (!peak && !dip)
                    continue;
                const double sign = peak ? 1.0 : -1.0;
                auto stands = [&](int step)
                {
                    for (long j = (long)i + step; j >= 0 && j < (long)db.size(); j += step)
                    {
                        if (sign * (db[i] - db[(size_t)j]) >= prominence)
                            return true;
                        if (sign * (db[(size_t)j] - db[i]) > 0.0)
                            return false;
                    }
                    return false;
                };
                if (stands(-1) && stands(1))
                    ++count;
            }
            return count;
        }

    private:
        double sr_;

        // an impulse response's magnitudes, without a window (it has died out within its length)
        static std::vector<double> magnitudes_flat(const std::vector<double> &impulse)
        {
            const size_t size = next_pow2(std::max<size_t>(impulse.size(), 2));
            std::vector<cplx> a(size);
            for (size_t i = 0; i < impulse.size(); ++i)
                a[i] = impulse[i];
            fft(a, false);
            std::vector<double> m(size / 2 + 1);
            for (size_t k = 0; k < m.size(); ++k)
                m[k] = std::abs(a[k]);
            return m;
        }
    };
}
