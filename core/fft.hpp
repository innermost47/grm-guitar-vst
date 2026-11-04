// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// A small radix-2 FFT, used off the audio thread (impulse responses, finger-noise sounds).
#pragma once

#include <cmath>
#include <complex>
#include <vector>

namespace grmg {

using cplx = std::complex<double>;

inline size_t next_pow2(size_t n) {
    size_t p = 1;
    while (p < n) p <<= 1;
    return p;
}

// in place, size a power of two; inverse = true divides by n
inline void fft(std::vector<cplx>& a, bool inverse) {
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i) {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(a[i], a[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1) {
        const double ang = 2 * 3.14159265358979323846 / (double) len * (inverse ? 1 : -1);
        const cplx wl(std::cos(ang), std::sin(ang));
        for (size_t i = 0; i < n; i += len) {
            cplx w(1);
            for (size_t j = 0; j < len / 2; ++j) {
                const cplx u = a[i + j], v = a[i + j + len / 2] * w;
                a[i + j] = u + v;
                a[i + j + len / 2] = u - v;
                w *= wl;
            }
        }
    }
    if (inverse)
        for (auto& x : a) x /= (double) n;
}

// linear convolution of two real signals, the first len(x) samples kept unless full = true
inline std::vector<double> convolve(const std::vector<double>& x, const std::vector<double>& h, bool full = false) {
    if (x.empty() || h.empty()) return {};
    const size_t L = x.size() + h.size() - 1, n = next_pow2(L);
    std::vector<cplx> A(n), B(n);
    for (size_t i = 0; i < x.size(); ++i) A[i] = x[i];
    for (size_t i = 0; i < h.size(); ++i) B[i] = h[i];
    fft(A, false);
    fft(B, false);
    for (size_t i = 0; i < n; ++i) A[i] *= B[i];
    fft(A, true);
    std::vector<double> y(full ? L : x.size());
    for (size_t i = 0; i < y.size(); ++i) y[i] = A[i].real();
    return y;
}

}  // namespace grmg
