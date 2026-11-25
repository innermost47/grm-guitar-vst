// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// physics.hpp
//
// The other GRM networks of the guitar (C++ port of grm_guitar.py): body and bridge, palm, room, finger on a
// wound string, finger on a fret, the player's arm, the pick crossing the strings. Same relations, same
// constants; tests/golden.hpp holds the Python results they are checked against.
#pragma once

#include <array>
#include <cmath>
#include <complex>
#include <random>
#include <string>
#include <vector>

#include "../constants/core.hpp"
#include "fft.hpp"
#include "guitar_networks.hpp"
#include "magnetic_pickup.hpp"

namespace grmg
{

    using cplx = std::complex<double>;

    inline double coupled(double fh, double ft, double k, double sign)
    {
        const double s = fh * fh + ft * ft;
        const double d = std::sqrt((fh * fh - ft * ft) * (fh * fh - ft * ft) + 4 * k * k * fh * fh * ft * ft);
        return std::sqrt(std::max((s + sign * d) / 2.0, 1e-12));
    }

    inline grm::Result body_network_result(const models::BodySpec &b);

    inline grm::Result body_network_result(double size = 1.0, double top_mm = 2.8, double k = 0.35, double braces_kg = -1.0,
                                           double q = BODY_Q)
    {
        models::BodySpec b;
        b.size = size;
        b.top_mm = top_mm;
        b.coupling = k;
        b.braces_kg = braces_kg;
        b.q = q;
        return body_network_result(b);
    }

    inline grm::Result body_network_result(const models::BodySpec &spec)
    {
        const double size = std::max(spec.size, 0.01);
        const double braces_kg = spec.braces_kg;
        const double q = spec.q;
        const double hollow = std::min(std::max(spec.hollow, 0.0), 1.0);
        const double air = std::max(hollow, 1e-3); // what is left of the cavity
        const double k = spec.coupling * hollow;
        const double top_m = std::pow(spec.top_mm / 1000.0, hollow) * std::pow(SLAB_M, 1.0 - hollow);
        grm::Network net;
        const std::pair<const char *, double> g[] = {
            {"Tair", T_AIR}, {"cair", 340.0}, {"V", 0.02}, {"r", 0.05}, {"A", 0.008}, {"t", 0.003}, {"lneck", 0.09}, {"fH", 120.0}, {"a", 0.17}, {"h", 0.003}, {"E", 1e10}, {"rho", 450.0}, {"fT", 200.0}, {"f_lo", 100.0}, {"f_hi", 220.0}, {"frad", 300.0}, {"D", 20.0}, {"Zp", 40.0}, {"Mtop", 0.1}, {"Mm", 0.05}, {"Vb", 3e-5}, {"rhob", 850.0}, {"mb", 0.008}, {"fb", 800.0}};
        for (auto &[n, v] : g)
            net.add_node(n, v, false, std::string(n) == "Tair");
        for (int j = 0; j < 5; ++j)
            net.add_node("f" + std::to_string(j + 3), 400.0);
        net.add_log("speed of sound", {{"cair", 2}, {"Tair", -1}}, std::log10(GAMMA_AIR * R_GAS / M_AIR), 1e-9);
        net.add_log("sound hole area", {{"A", 1}, {"r", -2}}, std::log10(PI), 1e-9);
        net.add_sum("neck of air", {{"lneck", 1}, {"t", -1}, {"r", -1.7}}, 0.0, 1e-9);
        net.add_log("Helmholtz", {{"fH", 1}, {"cair", -1}, {"A", -0.5}, {"V", 0.5}, {"lneck", 0.5}}, -std::log10(2 * PI), 1e-9);
        net.add_log("top plate", {{"fT", 1}, {"a", 2}, {"h", -1}, {"E", -0.5}, {"rho", 0.5}},
                    std::log10(10.21 / (2 * PI) / std::sqrt(12 * (1 - NU_WOOD * NU_WOOD))), 1e-9);
        net.add_log("radiation", {{"frad", 1}, {"cair", -1}, {"a", 1}}, -std::log10(2 * PI), 1e-9);
        const int ilo = net.index("f_lo"), ihi = net.index("f_hi"), iH = net.index("fH"), iT = net.index("fT");
        net.add_fn("air / top, low", {"f_lo", "fH", "fT"}, [=](const grm::Values &v)
                   { return std::log10(v[ilo]) - std::log10(coupled(v[iH], v[iT], k, -1)); }, 1e-9);
        net.add_fn("air / top, high", {"f_hi", "fH", "fT"}, [=](const grm::Values &v)
                   { return std::log10(v[ihi]) - std::log10(coupled(v[iH], v[iT], k, +1)); }, 1e-9);
        net.add_log("plate rigidity", {{"D", 1}, {"E", -1}, {"h", -3}}, std::log10(BRACING / (12 * (1 - NU_WOOD * NU_WOOD))), 1e-9);
        net.add_log("plate impedance", {{"Zp", 1}, {"D", -0.5}, {"rho", -0.5}, {"h", -0.5}}, std::log10(8.0), 1e-9);
        net.add_log("top mass", {{"Mtop", 1}, {"rho", -1}, {"h", -1}, {"a", -2}}, std::log10(PI), 1e-9);
        const bool loaded = braces_kg >= 0;
        if (loaded)
        {
            net.add_node("Mbraces", 0.06);
            net.add_node("Mbridge", 0.03);
            net.add_node("Mloaded", 0.2);
            net.add_node("Q", 50.0);
            net.add_log("bridge block", {{"Mbridge", 1}, {"rhob", -1}, {"Vb", -1}}, 0.0, 1e-9);
            net.add_sum("loaded top", {{"Mloaded", 1}, {"Mtop", -1}, {"Mbraces", -1}, {"Mbridge", -1}}, 0.0, 1e-9);
            net.add_log("modal mass", {{"Mm", 1}, {"Mloaded", -1}}, std::log10(BETA_MODAL), 1e-9);
        }
        else
        {
            net.add_log("modal mass", {{"Mm", 1}, {"Mtop", -1}}, std::log10(BETA_MODAL), 1e-9);
        }
        net.add_log("bridge mass", {{"mb", 1}, {"rhob", -1}, {"Vb", -1}}, std::log10(BRIDGE_SHARE), 1e-9);
        net.add_log("bridge corner", {{"fb", 1}, {"mb", 1}, {"Zp", -1}}, -std::log10(2 * PI), 1e-9);
        for (int j = 0; j < 5; ++j)
            net.add_log("top mode " + std::to_string(j + 3), {{"f" + std::to_string(j + 3), 1}, {"fT", -1}},
                        std::log10(PLATE_RATIOS[j]), 1e-9);
        std::map<std::string, grm::Measurement> m = {
            {"V", {0.016 * size * size * size * air, 0.01}}, {"r", {0.05 * size, 0.01}}, {"t", {0.0028, 0.02}}, {"a", {0.17 * size, 0.01}}, {"h", {top_m, 0.02}}, {"E", {spec.E, 0.05}}, {"rho", {spec.rho, 0.02}}, {"Vb", {BRIDGE_VOLUME, 0.02}}, {"rhob", {RHO_BRIDGE, 0.02}}};
        if (loaded)
        {
            m["Mbraces"] = {std::max(braces_kg, 1e-4), 0.02};
            m["Q"] = {q, 0.02};
        }
        return net.solve(m);
    }

    inline models::Body body_network(const models::BodySpec &spec)
    {
        const auto r = body_network_result(spec);
        const double hollow = std::min(std::max(spec.hollow, 0.0), 1.0);
        models::Body b;
        b.f = {r["f_lo"], r["f_hi"], r["f3"], r["f4"], r["f5"], r["f6"], r["f7"]};
        for (int m = 0; m < 7; ++m)
        {
            b.gain[(size_t)m] = MODE_GAINS[m];
            b.q[(size_t)m] = spec.braces_kg >= 0 ? r["Q"] : MODE_QS[m];
        }
        // the air mode (the cavity breathing through the hole) fades with the cavity: none in a solid body
        if (hollow < 1.0)
            b.gain[0] *= hollow;
        b.Mm = r["Mm"];
        b.Zp = r["Zp"];
        b.mb = r["mb"];
        b.fT = r["fT"];
        b.frad = r["frad"];
        b.fH = r["fH"];
        b.f_lo = r["f_lo"];
        b.f_hi = r["f_hi"];
        b.fb = r["fb"];
        b.neck = NECK_CONDUCTANCE;
        return b;
    }

    inline models::Body body_network(double size = 1.0, double top_mm = 2.8, double coupling = 0.35, double braces_kg = -1.0,
                                     double q = BODY_Q)
    {
        models::BodySpec b;
        b.size = size;
        b.top_mm = top_mm;
        b.coupling = coupling;
        b.braces_kg = braces_kg;
        b.q = q;
        return body_network(b);
    }

    // Impulse response of the body seen as radiation: Y(f) * i ka / (1 + i ka). Normalised so that its
    // response is 1 on average over 100 Hz - 5 kHz (the Python version matches the dry level by RMS).
    inline std::vector<double> body_impulse(const models::Body &b, double sr, size_t length = 8192)
    {
        const size_t n = next_pow2(length * 2);
        std::vector<cplx> H(n);
        double norm = 0.0;
        int count = 0;
        for (size_t k = 0; k <= n / 2; ++k)
        {
            const double f = k * sr / (double)n;
            const double ka = f / b.frad;
            const cplx h = (k == 0) ? cplx(0) : b.admittance(f) * cplx(0, ka) / cplx(1, ka);
            H[k] = h;
            if (k > 0 && k < n / 2)
                H[n - k] = std::conj(h);
            if (f > 100 && f < 5000)
            {
                norm += std::norm(h);
                ++count;
            }
        }
        fft(H, true);
        const double scale = 1.0 / std::sqrt(norm / std::max(count, 1));
        std::vector<double> ir(length);
        for (size_t i = 0; i < length; ++i)
        {
            const double fade = i > length * 3 / 4 ? 0.5 * (1 + std::cos(PI * (double)(i - length * 3 / 4) / (double)(length / 4))) : 1.0;
            ir[i] = H[i].real() * scale * fade;
        }
        return ir;
    }

    inline models::PickupCircuit pickup_network(int type, double cable_pf)
    {
        const auto &t = PICKUP_TYPES[std::min(std::max(type, 0), NUM_PICKUP_TYPES - 1)];
        grm::Network net;
        for (auto [n, v] : {std::pair<const char *, double>{"L", 2.0}, {"R", 6e3}, {"Cc", 1e-10}, {"Ck", 5e-10}, {"C", 6e-10}, {"Rl", 2e5}, {"fr", 4000.0}})
            net.add_node(n, v);
        net.add_node("Q", 1.0);
        const int iL = net.index("L"), iR = net.index("R"), iC = net.index("C"), iRl = net.index("Rl"), iQ = net.index("Q");
        net.add_sum("capacitance", {{"C", 1}, {"Cc", -1}, {"Ck", -1}}, 0.0, 1e-15);
        net.add_log("resonance", {{"fr", 1}, {"L", 0.5}, {"C", 0.5}}, -std::log10(2 * PI), 1e-9);
        // Q of the denominator: sqrt(L C (1 + R / Rl)) / (R C + L / Rl)
        net.add_fn("damping", {"Q", "L", "R", "C", "Rl"}, [=](const grm::Values &v)
                   {
        const double q = std::sqrt(v[iL] * v[iC] * (1 + v[iR] / v[iRl])) / (v[iR] * v[iC] + v[iL] / v[iRl]);
        return std::log10(v[iQ]) - std::log10(q); }, 1e-9);
        const auto r = net.solve({{"L", {t.L, 0.02}}, {"R", {t.R, 0.01}}, {"Cc", {t.C, 0.05}}, {"Ck", {std::max(cable_pf, 1.0) * 1e-12, 0.05}}, {"Rl", {PICKUP_LOAD, 0.02}}});
        models::PickupCircuit c;
        c.L = r["L"];
        c.R = r["R"];
        c.C = r["C"];
        c.Rload = r["Rl"];
        c.fr = r["fr"];
        c.Q = r["Q"];
        return c;
    }

    // a pickup of `type` centred `centre_m` from the saddle, the strings `height_mm` above its poles: where its coils
    // sense them, how wide, how hot (models::PickupSensing, see MagneticPickup). `one_coil`: a humbucker split, only its
    // coil on the neck's side is heard.
    inline models::PickupSensing pickup_sensing_network(int type, double centre_m, double height_mm, bool one_coil = false)
    {
        const auto &t = PICKUP_TYPES[std::min(std::max(type, 0), NUM_PICKUP_TYPES - 1)];
        const auto &single = PICKUP_TYPES[0];
        const bool humbucker = t.coils == 2;
        grm::Network net;
        net.add_node("x", 0.1);
        const int ih = net.add_node("h", 3.0), ir = net.add_node("r", 2.5), ia = net.add_node("a", 3.0), ig = net.add_node("g", 1.0);
        std::map<std::string, grm::Measurement> measured = {{"x", {centre_m, 1e-4}},
                                                            {"h", {std::max(height_mm, PICKUP_HEIGHT_MIN_MM), 1e-3}},
                                                            {"r", {1000.0 * t.pole_radius_m, 1e-3}}};
        if (humbucker)
        {
            // its two coils, each half the spacing from the centre
            net.add_node("d", 0.018);
            net.add_node("x1", 0.1);
            net.add_node("x2", 0.1);
            net.add_sum("coil on the bridge side", {{"x1", 1.0}, {"x", -1.0}, {"d", 0.5}}, 0.0, 1e-12);
            net.add_sum("coil on the neck side", {{"x2", 1.0}, {"x", -1.0}, {"d", -0.5}}, 0.0, 1e-12);
            measured["d"] = {t.coil_spacing_m, 1e-4};
        }
        // the width a pole hears: the string's height and the pole's spread (mm)
        net.add_fn("listening width", {"a", "h", "r"}, [=](const grm::Values &v)
                   { return v[ia] * v[ia] - v[ih] * v[ih] - 0.5 * v[ir] * v[ir]; }, 1e-9);
        // how hot: its flux curve's slope at this height against a single coil's at the reference height
        net.add_fn("gain", {"g", "h"}, [=](const grm::Values &v)
                   { return v[ig] * MagneticPickup::slope(PICKUP_HEIGHT_REF_MM, single.nl_A, single.nl_Leq_mm, single.nl_req_mm) -
                            MagneticPickup::slope(v[ih], t.nl_A, t.nl_Leq_mm, t.nl_req_mm); }, 1e-12);
        const auto r = net.solve(measured);
        models::PickupSensing s;
        s.coils = humbucker && !one_coil ? 2 : 1;
        if (!humbucker)
            s.coil_x = {r["x"], 0.0};
        else if (one_coil)
            s.coil_x = {r["x2"], 0.0};
        else
            s.coil_x = {r["x1"], r["x2"]};
        s.window_m = r["a"] / 1000.0;
        s.height_mm = r["h"];
        s.nl_A = t.nl_A;
        s.nl_Leq_mm = t.nl_Leq_mm;
        s.nl_req_mm = t.nl_req_mm;
        s.gain = r["g"];
        return s;
    }

    inline models::Biquad pickup_filter(const models::PickupCircuit &c, double sr)
    {
        // H(s) = 1 / (A2 s^2 + A1 s + A0)
        const double A2 = c.L * c.C, A1 = c.R * c.C + c.L / c.Rload, A0 = 1.0 + c.R / c.Rload;
        const double w = 2 * PI * std::min(c.fr, 0.45 * sr);
        const double K = w / std::tan(w / (2 * sr));
        const double d0 = A2 * K * K + A1 * K + A0, d1 = 2 * (A0 - A2 * K * K), d2 = A2 * K * K - A1 * K + A0;
        models::Biquad f;
        f.b0 = 1.0 / d0;
        f.b1 = 2.0 / d0;
        f.b2 = 1.0 / d0;
        f.a1 = d1 / d0;
        f.a2 = d2 / d0;
        return f;
    }

    inline cplx palm_shape(cplx w, double c, double mu, double T, double x, double width, double length, double rp)
    {
        const cplx k = w / c;
        const cplx kappa = std::sqrt((mu * w * w - cplx(0, 1) * w * rp) / T);
        cplx y = 0.0, dy = 1.0;
        const std::pair<cplx, double> parts[3] = {{k, x}, {kappa, width}, {k, length - x - width}};
        for (auto &[kk, ll] : parts)
        {
            const cplx cs = std::cos(kk * ll), sn = std::sin(kk * ll);
            const cplx y2 = y * cs + dy * sn / kk, dy2 = -y * kk * sn + dy * cs;
            y = y2;
            dy = dy2;
        }
        if (!std::isfinite(y.real()) || !std::isfinite(y.imag()))
            return cplx(1e6, 1e6);
        return y * k;
    }

    inline models::PalmMode palm_mode(double c, double mu, double T, double x, double width, double length, double rp, double f_guess,
                                      double s_guess)
    {
        grm::Network net;
        const int iF = net.add_node("F", f_guess), iS = net.add_node("S", std::max(s_guess, 1e-3));
        auto value = [=](const grm::Values &v)
        { return palm_shape(cplx(2 * PI * v[iF], v[iS]), c, mu, T, x, width, length, rp); };
        net.add_fn("real part", {"F", "S"}, [=](const grm::Values &v)
                   { return value(v).real(); }, 1e-7);
        net.add_fn("imaginary part", {"F", "S"}, [=](const grm::Values &v)
                   { return value(v).imag(); }, 1e-7);
        const auto r = net.solve({}, 25, 1e-3, 1e-12, false);
        return {r["F"], r["S"], r.cost};
    }

    inline models::Palm palm_network(double mu, double L, int fret, double x_hand = PALM_X, double force = PALM_FORCE, double T = 73.0,
                                     int steps = 4, const models::Palm *start = nullptr)
    {
        grm::Network net;
        const std::pair<const char *, double> g[] = {{"mu", 1e-3}, {"L", 0.65}, {"Leff", 0.6}, {"x", 0.02}, {"N", 1.0}, {"r", 3.0}, {"w", 0.025}, {"rp", 100.0}, {"q", 0.03}, {"T", 70.0}, {"c", 100.0}, {"Zs", 0.5}};
        for (auto &[n, v] : g)
            net.add_node(n, v);
        net.add_log("fret", {{"Leff", 1}, {"L", -1}}, -fret * std::log10(2.0) / 12, 1e-9);
        net.add_log("skin damping", {{"r", 1}, {"N", -1}}, std::log10(R_SKIN), 1e-9);
        net.add_log("contact width", {{"w", 1}, {"N", -1.0 / 3}}, std::log10(PALM_WIDTH / std::pow(PALM_FORCE, 1.0 / 3)), 0.02);
        net.add_log("per metre", {{"rp", 1}, {"r", -1}, {"w", 1}}, 0.0, 1e-9);
        net.add_log("position", {{"q", 1}, {"x", -1}, {"Leff", 1}}, 0.0, 1e-9);
        net.add_log("wave speed", {{"c", 2}, {"T", -1}, {"mu", 1}}, 0.0, 1e-9);
        net.add_log("impedance", {{"Zs", 2}, {"T", -1}, {"mu", -1}}, 0.0, 1e-9);
        const auto v = net.solve({{"mu", {mu, 0.004}}, {"L", {L, 3e-4}}, {"x", {x_hand, 0.05}}, {"N", {force, 0.05}}, {"T", {T, 0.003}}});
        models::Palm p;
        const double c = v["c"], Leff = v["Leff"], rp = v["rp"];
        p.f1 = c / (2 * Leff);
        p.r = v["r"];
        p.w = v["w"];
        p.x = v["x"];
        const double vmu = v["mu"], vT = v["T"], vx = v["x"], vw = v["w"];
        auto follow = [&](int n, int stages, double &F, double &S)
        {
            F = n * p.f1;
            S = 0.0;
            for (int st = 1; st <= stages; ++st)
            {
                const double s = (double)st / stages;
                models::PalmMode m;
                try
                {
                    m = palm_mode(c, vmu, vT, vx, vw, Leff, rp * s, F, S);
                }
                catch (...)
                {
                    return false;
                }
                if (m.cost > 1e-6 || std::abs(m.F - F) > 0.3 * p.f1 || m.S < 0)
                    return false;
                F = m.F;
                S = m.S;
            }
            return true;
        };
        const int count = (int)std::min(80.0, PALM_F_MAX / p.f1);
        std::vector<bool> lost;
        for (int n = 1; n <= count; ++n)
        {
            double F = 0, S = 0;
            bool found = false;
            if (start && (size_t)n <= start->ratio.size())
            {
                const double gF = start->ratio[(size_t)n - 1] * n * p.f1, gS = start->sigma[(size_t)n - 1];
                try
                {
                    const auto m = palm_mode(c, vmu, vT, vx, vw, Leff, rp, gF, gS);
                    if (m.cost <= 1e-6 && std::abs(m.F - gF) < 0.3 * p.f1 && m.S >= 0)
                    {
                        F = m.F;
                        S = m.S;
                        found = true;
                    }
                }
                catch (...)
                {
                }
            }
            for (int stages : {steps, 2 * steps})
                if (!found)
                    found = follow(n, stages, F, S);
            p.ratio.push_back(found ? F / (n * p.f1) : 0.0);
            p.sigma.push_back(found ? S : 0.0);
            lost.push_back(!found);
        }
        // a mode lost at a crossing behaves like its neighbours (linear interpolation)
        bool any_found = false;
        for (bool l : lost)
            any_found |= !l;
        if (!any_found)
        {
            for (size_t k = 0; k < p.ratio.size(); ++k)
            {
                p.ratio[k] = 1.0;
                p.sigma[k] = 0.0;
            }
        }
        else
        {
            for (size_t k = 0; k < lost.size(); ++k)
            {
                if (!lost[k])
                    continue;
                int a = (int)k - 1, b = (int)k + 1;
                while (a >= 0 && lost[(size_t)a])
                    --a;
                while (b < (int)lost.size() && lost[(size_t)b])
                    ++b;
                if (a < 0)
                {
                    p.ratio[k] = p.ratio[(size_t)b];
                    p.sigma[k] = p.sigma[(size_t)b];
                }
                else if (b >= (int)lost.size())
                {
                    p.ratio[k] = p.ratio[(size_t)a];
                    p.sigma[k] = p.sigma[(size_t)a];
                }
                else
                {
                    const double u = (double)((int)k - a) / (double)(b - a);
                    p.ratio[k] = p.ratio[(size_t)a] + u * (p.ratio[(size_t)b] - p.ratio[(size_t)a]);
                    p.sigma[k] = p.sigma[(size_t)a] + u * (p.sigma[(size_t)b] - p.sigma[(size_t)a]);
                }
            }
        }
        return p;
    }

    inline models::Room room_network(const models::RoomSize &size, int material, int floor = 2)
    {
        const double L = size.L, W = size.W, H = size.H;
        const auto &walls = MATERIALS[material].alpha;
        const auto &fl = MATERIALS[floor].alpha;
        grm::Network net;
        const int iL = net.add_node("L", L), iW = net.add_node("W", W), iH = net.add_node("H", H);
        const int iV = net.add_node("V", 60.0), iS = net.add_node("S", 90.0);
        net.add_node("rc", 1.0);
        net.add_node("fs", 200.0);
        net.add_log("volume", {{"V", 1}, {"L", -1}, {"W", -1}, {"H", -1}}, 0.0, 1e-9);
        net.add_fn("surface", {"S", "L", "W", "H"}, [=](const grm::Values &v)
                   { return std::log10(v[iS]) - std::log10(2 * (v[iL] * v[iW] + v[iL] * v[iH] + v[iW] * v[iH])); }, 1e-9);
        models::Room room;
        const double floor_share = (L * W) / (2 * (L * W + L * H + W * H));
        for (int b = 0; b < 6; ++b)
        {
            const std::string rt = "RT" + std::to_string(b);
            const int iR = net.add_node(rt, 1.0);
            const double a = floor_share * fl[b] + (1 - floor_share) * walls[b];
            room.alpha[(size_t)b] = a;
            net.add_fn("Eyring " + std::to_string((int)ROOM_BANDS[b]), {rt, "V", "S"}, [=](const grm::Values &v)
                       { return std::log10(v[iR]) - std::log10(0.161 * v[iV] / (-v[iS] * std::log(1 - a))); }, 1e-9);
        }
        net.add_log("critical distance", {{"rc", 1}, {"V", -0.5}, {"RT3", 0.5}}, std::log10(0.057), 1e-9);
        net.add_log("Schroeder", {{"fs", 1}, {"RT3", -0.5}, {"V", 0.5}}, std::log10(2000.0), 1e-9);
        const auto r = net.solve({{"L", {L, 0.005}}, {"W", {W, 0.005}}, {"H", {H, 0.005}}});
        room.V = r["V"];
        room.S = r["S"];
        room.rc = r["rc"];
        room.fs = r["fs"];
        for (int b = 0; b < 6; ++b)
            room.RT[(size_t)b] = r["RT" + std::to_string(b)];
        return room;
    }

    // the way from the guitar to an ear `distance` metres in front of it (within the room), above it
    inline double direct_path(const models::RoomSize &size, double distance)
    {
        const double d = std::min(distance, LISTENER_REACH * size.L);
        return std::sqrt(d * d + LISTENER_HEIGHT * LISTENER_HEIGHT);
    }

    // the listener at `distance`: each ear hears the direct sound after its way, and the room's reverberation (heard
    // from ROOM_REF_DISTANCE, relative to its direct sound) scaled as the direct sound falls: the reflections and the
    // diffuse field stay, the direct sound goes as 1 / r (direct to reverberant: (rc / r)^2, the critical distance).
    // The direct sound keeps its level (the guitar is not quieter, the room is louder); `wet` scales the room.
    inline models::ListenerTaps listener_taps(const models::RoomSize &size, double distance, double wet)
    {
        models::ListenerTaps taps;
        for (int ear = 0; ear < 2; ++ear)
        {
            const double r = direct_path(size, distance + EAR_SPACING * ear);
            taps.delay_s[(size_t)ear] = r / C_ROOM;
            taps.send[(size_t)ear] = wet * r / direct_path(size, ROOM_REF_DISTANCE + EAR_SPACING * ear);
        }
        return taps;
    }

    // what an ear at ROOM_REF_DISTANCE (+ `ear_offset`) hears of the room, without the direct sound, from the direct
    // sound's arrival on (it is at 0, of level 1): image sources up to the second order, a diffuse tail decaying band
    // by band. The direct sound and the listener's distance are listener_taps'.
    inline std::vector<double> room_reverb(const models::Room &room, const models::RoomSize &size, double ear_offset, double sr, unsigned seed = 3)
    {
        const double L = size.L, W = size.W, H = size.H;
        const double src[3] = {0.35 * L, 0.5 * W, 1.0};
        const double lis[3] = {src[0] + std::min(ROOM_REF_DISTANCE + ear_offset, LISTENER_REACH * L), src[1], src[2] + LISTENER_HEIGHT};
        double rtmax = 0;
        for (double r : room.RT)
            rtmax = std::max(rtmax, r);
        const size_t length = (size_t)(std::min(rtmax * 1.2, 6.0) * sr) + 1;
        std::vector<double> h(length, 0.0);
        auto dist = [](const double *a, const double *b)
        {
            return std::sqrt((a[0] - b[0]) * (a[0] - b[0]) + (a[1] - b[1]) * (a[1] - b[1]) + (a[2] - b[2]) * (a[2] - b[2]));
        };
        const double r_direct = dist(lis, src);
        const double reflect = std::sqrt(1 - room.alpha[3]);
        double early = 0.0;
        for (int nx = -2; nx <= 2; ++nx)
            for (int ny = -2; ny <= 2; ++ny)
                for (int nz = -2; nz <= 2; ++nz)
                {
                    const int order = std::abs(nx) + std::abs(ny) + std::abs(nz);
                    if (order == 0 || order > 2)
                        continue; // the direct sound is the listener's
                    const double img[3] = {nx * L + ((nx % 2 == 0) ? src[0] : L - src[0]),
                                           ny * W + ((ny % 2 == 0) ? src[1] : W - src[1]),
                                           nz * H + ((nz % 2 == 0) ? src[2] : H - src[2])};
                    const double r = dist(img, lis);
                    const size_t k = (size_t)((r - r_direct) / C_ROOM * sr);
                    if (k < length)
                    {
                        const double a = std::pow(reflect, order) * r_direct / r;
                        h[k] += a;
                        early += a * a;
                    }
                }
        const double rev_energy = std::max((r_direct / room.rc) * (r_direct / room.rc) - early, 0.0);
        const double t0 = 2 * (4 * room.V / room.S) / C_ROOM;
        const size_t n = next_pow2(length);
        std::mt19937 rng(seed);
        std::normal_distribution<double> normal(0.0, 1.0);
        std::vector<double> tail(length, 0.0);
        double edges[6][2];
        double total_bw = 0;
        for (int b = 0; b < 6; ++b)
        {
            const double lo = ROOM_BANDS[b] / std::sqrt(2.0), hi = ROOM_BANDS[b] * std::sqrt(2.0);
            edges[b][0] = b == 0 ? 20.0 : lo;
            edges[b][1] = b == 5 ? sr / 2 : hi;
            total_bw += edges[b][1] - edges[b][0];
        }
        for (int b = 0; b < 6; ++b)
        {
            std::vector<cplx> band(n);
            for (auto &z : band)
                z = normal(rng);
            fft(band, false);
            for (size_t k = 0; k < n; ++k)
            {
                const size_t kk = k <= n / 2 ? k : n - k;
                const double f = kk * sr / (double)n;
                if (f < edges[b][0] || f >= edges[b][1])
                    band[k] = 0.0;
            }
            fft(band, true);
            std::vector<double> piece(length);
            double e = 0;
            for (size_t i = 0; i < length; ++i)
            {
                const double t = i / sr + r_direct / C_ROOM; // since the guitar played
                const double env = std::exp(-6.91 * std::max(t - t0, 0.0) / room.RT[(size_t)b]) * (t >= t0 * 0.6 ? 1.0 : 0.0);
                piece[i] = band[i].real() * env;
                e += piece[i] * piece[i];
            }
            const double share = (edges[b][1] - edges[b][0]) / total_bw * room.RT[(size_t)b] / room.RT[3];
            const double g = std::sqrt(share / (e + 1e-30));
            for (size_t i = 0; i < length; ++i)
                tail[i] += piece[i] * g;
        }
        for (size_t i = 0; i < length; ++i)
            h[i] += std::sqrt(rev_energy) * tail[i];
        return h;
    }

    inline models::Squeak squeak_network(double mu, double L, double core_fraction, int fret_from, int fret_to, double dt, double force)
    {
        grm::Network net;
        const std::pair<const char *, double> g[] = {{"mu", 1e-3}, {"deq", 5e-4}, {"dcore", 3e-4}, {"D", 6e-4}, {"p", 2e-4}, {"L", 0.65}, {"L1", 0.6}, {"dx", 0.03}, {"dt", 0.1}, {"v", 0.3}, {"fsq", 1500.0}, {"N", 0.5}, {"P", 0.05}, {"A", 1e-7}, {"cL", 2000.0}, {"fL", 1500.0}};
        for (auto &[n, v] : g)
            net.add_node(n, v);
        const bool slide = fret_from > 0 && fret_to > 0;
        if (slide)
            net.add_node("L2", 0.5);
        net.add_log("equivalent diameter", {{"deq", 2}, {"mu", -1}}, std::log10(4 / (PI * RHO_STEEL)), 1e-9);
        net.add_log("core", {{"dcore", 1}, {"deq", -1}}, std::log10(core_fraction), 1e-9);
        net.add_log("outer diameter", {{"D", 2}, {"deq", -2}}, -std::log10(WIND_FILL), 1e-9);
        net.add_sum("winding wire", {{"p", 2}, {"D", -1}, {"dcore", 1}}, 0.0, 1e-12);
        net.add_log("speed", {{"v", 1}, {"dx", -1}, {"dt", 1}}, 0.0, 1e-9);
        net.add_log("squeak pitch", {{"fsq", 1}, {"v", -1}, {"p", 1}}, 0.0, 1e-9);
        net.add_log("friction power", {{"P", 1}, {"N", -1}, {"v", -1}}, std::log10(MU_FRICTION), 1e-9);
        net.add_log("core section", {{"A", 1}, {"dcore", -2}}, std::log10(PI / 4), 1e-9);
        net.add_log("longitudinal wave", {{"cL", 2}, {"mu", 1}, {"A", -1}}, std::log10(E_STEEL), 1e-9);
        net.add_log("from", {{"L1", 1}, {"L", -1}}, -fret_from * std::log10(2.0) / 12, 1e-9);
        net.add_log("longitudinal mode", {{"fL", 1}, {"L", 1}, {"cL", -1}}, -std::log10(2.0), 1e-9);
        std::map<std::string, grm::Measurement> m = {{"mu", {mu, 0.004}}, {"L", {L, 3e-4}}, {"dt", {dt, 0.01}}, {"N", {force, 0.05}}};
        if (slide)
        {
            const double sign = fret_to > fret_from ? 1.0 : -1.0;
            net.add_log("to", {{"L2", 1}, {"L", -1}}, -fret_to * std::log10(2.0) / 12, 1e-9);
            net.add_sum("slide", {{"dx", 1}, {"L1", -sign}, {"L2", sign}}, 0.0, 1e-12);
        }
        else
        {
            m["dx"] = {LIFT_DX, 0.05};
        }
        const auto r = net.solve(m);
        return {r["fsq"], r["P"], r["fL"], r["v"], r["N"], r["L"], r["p"], r["cL"]};
    }

    inline models::Action action_network(double L, int fret)
    {
        grm::Network net;
        net.add_node("L", 0.65);
        net.add_node("xf", 0.05);
        net.add_node("h", 0.001);
        net.add_sum("finger position", {{"xf", 1.0}, {"L", std::pow(2.0, -fret / 12.0) - 1.0}}, 0.0, 1e-12);
        net.add_sum("string height", {{"h", 1.0}, {"xf", -(H_12 - H_NUT) / (L / 2)}}, H_NUT, 1e-12);
        const auto r = net.solve({{"L", {L, 3e-4}}}, 30);
        return {r["xf"], r["h"]};
    }

    // force on the bridge of a finger sliding on a wound string (see grm_guitar.synthesize_squeak)
    inline std::vector<double> synthesize_squeak(double fsq, double power, double duration, double f_long, double x_ratio, double v,
                                                 double sr, unsigned seed, const std::vector<double> &bumps, double force)
    {
        const size_t count = std::max<size_t>((size_t)(duration * sr), 2);
        std::mt19937 rng(seed);
        std::normal_distribution<double> normal(0.0, 1.0);
        std::uniform_real_distribution<double> uni(0.0, 1.0);
        std::vector<double> noise(count), rate(count), shocks(count, 0.0);
        for (auto &z : noise)
            z = normal(rng);
        // slow wobble of the speed (moving average of 200 samples)
        std::vector<double> wob(count, 0.0);
        {
            double acc = 0;
            for (size_t i = 0; i < count; ++i)
            {
                acc += noise[i];
                if (i >= 200)
                    acc -= noise[i - 200];
                wob[i] = acc / 200.0;
            }
        }
        double turns = 0, prev = 0;
        for (size_t i = 0; i < count; ++i)
        {
            const double s = count > 1 ? (double)i / (double)(count - 1) : 0.0;
            double r = std::min(std::max(fsq * (PI / 2) * std::sin(PI * s), 1.0), 0.45 * sr);
            r *= 1.0 + 0.04 * wob[i];
            turns += r / sr;
            if (i > 0 && std::floor(turns) > std::floor(prev))
                shocks[i] = std::pow(std::sin(PI * s), 1.2) * (0.7 + 0.6 * uni(rng));
            prev = turns;
        }
        double ref = 0;
        for (double z : shocks)
            ref += z * z;
        ref = std::sqrt(ref / (double)count) + 1e-12;
        const size_t width = std::max<size_t>(3, (size_t)(PAD_EDGE / std::max(v, 1e-3) * sr));
        std::vector<double> pad(width);
        double psum = 0;
        for (size_t i = 0; i < width; ++i)
        {
            pad[i] = 0.5 - 0.5 * std::cos(2 * PI * (double)i / (double)(width - 1));
            psum += pad[i];
        }
        for (auto &z : pad)
            z = z / psum * std::sqrt(width / 3.0);
        auto soft = convolve(shocks, pad);
        // remove the steady drag (moving average of 10 ms)
        const size_t slow = (size_t)(0.01 * sr);
        std::vector<double> avg(count, 0.0);
        {
            double acc = 0;
            for (size_t i = 0; i < count; ++i)
            {
                acc += soft[i];
                if (i >= slow)
                    acc -= soft[i - slow];
                avg[i] = acc / (double)slow;
            }
        }
        for (size_t i = 0; i < count; ++i)
            soft[i] -= avg[i];
        const size_t hl = (size_t)(5 * LONG_TAU * sr);
        std::vector<double> h(hl, 0.0);
        double hmax = 0;
        for (int k = 1; k <= 8; ++k)
        {
            const double f = k * f_long;
            if (f > 0.45 * sr)
                break;
            const double a = std::abs(std::sin(k * PI * x_ratio)) / k;
            for (size_t i = 0; i < hl; ++i)
                h[i] += a * std::sin(2 * PI * f * i / sr) * std::exp(-(i / sr) / LONG_TAU);
        }
        for (double z : h)
            hmax = std::max(hmax, std::abs(z));
        for (auto &z : h)
            z /= hmax + 1e-12;
        auto ring = convolve(soft, h, true);
        for (size_t i = 0; i < count; ++i)
            ring[i] += 0.5 * soft[i];
        const double friction = std::max(power, 0.0) / V_REF;
        for (auto &z : ring)
            z *= LONG_COUPLING * friction / ref / std::sqrt(hl / 3.0);
        for (double u : bumps)
        {
            const double v_here = std::max(v * (PI / 2) * std::sin(PI * u), 0.02);
            const size_t w = std::max<size_t>(4, (size_t)(FRET_CROWN / v_here * sr));
            const long i0 = (long)(u * (double)count) - (long)w / 2;
            for (size_t j = 0; j < w; ++j)
            {
                const long at = i0 + (long)j;
                if (at >= 0 && at < (long)ring.size())
                    ring[(size_t)at] += FRET_BUMP * force * std::sin(PI * (double)j / (double)(w - 1));
            }
        }
        return ring;
    }

    // ============================================================ the player

    // strokes: (grid time in s, step index in the bar). One network for the whole sequence: what the arm does at
    // one stroke depends on the strokes around it.
    inline std::vector<models::Stroke> player_network(const std::vector<std::pair<double, int>> &strokes, double step, double pluck,
                                                      models::PlayerNoise &noise, double swing = SWING)
    {
        std::vector<models::Stroke> out;
        if (strokes.empty())
            return out;
        grm::Network net;
        std::map<std::string, grm::Measurement> m;
        for (size_t k = 0; k < strokes.size(); ++k)
        {
            const auto [grid, idx] = strokes[k];
            const double acc = ACCENT_DEX[idx % 8];
            const std::string s = std::to_string(k);
            net.add_node("t" + s, grid, true);
            net.add_node("A" + s, swing);
            net.add_node("p" + s, pluck);
            net.add_node("d" + s, PICK_DEPTH, true);
            net.add_node("x" + s, PALM_X);
            net.add_node("N" + s, PALM_FORCE);
            m["t" + s] = {grid + noise.normal(TIMING_SIGMA), TIMING_SIGMA};
            m["A" + s] = {swing * std::pow(10.0, acc + noise.normal(AMPLITUDE_SIGMA)), AMPLITUDE_SIGMA};
            m["p" + s] = {pluck * (1.0 + noise.normal(PLUCK_SIGMA)), PLUCK_SIGMA / grm::LN10};
            m["d" + s] = {PICK_DEPTH + noise.normal(DEPTH_SIGMA), DEPTH_SIGMA};
            m["x" + s] = {std::max(PALM_X + noise.normal(PALM_X_SIGMA), 0.004), PALM_X_SIGMA / PALM_X / grm::LN10};
            m["N" + s] = {PALM_FORCE * std::pow(10.0, acc + noise.normal(PALM_N_SIGMA)), PALM_N_SIGMA};
            if (k)
            {
                const auto [grid0, idx0] = strokes[k - 1];
                const double gap = std::max((grid - grid0) / step, 1.0);
                const double acc0 = ACCENT_DEX[idx0 % 8];
                const std::string p = std::to_string(k - 1);
                net.add_sum("arm period " + s, {{"t" + s, 1.0}, {"t" + p, -1.0}}, grid - grid0, ARM_SIGMA * std::sqrt(gap));
                net.add_log("arm swing " + s, {{"A" + s, 1}, {"A" + p, -1}}, acc - acc0, AMPLITUDE_SIGMA * std::sqrt(gap));
                net.add_log("hand along " + s, {{"p" + s, 1}, {"p" + p, -1}}, 0.0, PLUCK_SIGMA / grm::LN10 * std::sqrt(gap));
                net.add_sum("pick depth " + s, {{"d" + s, 1.0}, {"d" + p, -1.0}}, 0.0, DEPTH_SIGMA * std::sqrt(gap));
                net.add_log("palm place " + s, {{"x" + s, 1}, {"x" + p, -1}}, 0.0, PALM_X_SIGMA / PALM_X / grm::LN10 * std::sqrt(gap));
                net.add_log("palm pressure " + s, {{"N" + s, 1}, {"N" + p, -1}}, acc - acc0, PALM_N_SIGMA * std::sqrt(gap));
            }
        }
        const auto r = net.solve(m, 50, 1e-3, 1e-12, false);
        for (size_t k = 0; k < strokes.size(); ++k)
        {
            const std::string s = std::to_string(k);
            const double A = r["A" + s];
            out.push_back({r["t" + s], A, PI * A / step, A / swing, r["p" + s], r["d" + s], r["x" + s], r["N" + s]});
        }
        return out;
    }

    // which strings of `order` (in the order the pick crosses them) the pick reaches, when, with what share of the force
    inline std::vector<models::Contact> strum_contacts(const std::vector<int> &order, const models::Stroke &stroke, bool up)
    {
        const double centre = (up ? 4.5 : 2.5) * STRING_SPACING;
        std::vector<models::Contact> out;
        bool have_first = false;
        double first = 0;
        for (int i : order)
        {
            const double y = i * STRING_SPACING;
            const double depth = stroke.d - (y - centre) * (y - centre) / (2 * WRIST_RADIUS);
            if (!have_first)
            {
                first = y;
                have_first = true;
            }
            if (depth <= 0)
                continue;
            out.push_back({i, std::abs(y - first) / stroke.v, std::min(1.0, depth / FULL_DEPTH) * (up ? UP_FORCE : 1.0)});
        }
        return out;
    }

} // namespace grmg
