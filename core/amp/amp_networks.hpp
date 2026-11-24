// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// amp_networks.hpp
//
// The amplifier's networks: from its circuit (a voicing of constants/core.hpp) and its knobs, what the audio thread
// plays (models::AmpModel).
//   - the preamp: one network for all its triodes, which share the supply (each one's current drops the voltage of
//     the next ones through the decoupling resistors): their operating points, then each one's curve on its load
//     line, its gain, its cathode shelf, its grid conduction, its coupling and Miller corners, its thermal noise;
//   - the tone stack: its third-order H(s) from its parts and its three pots (Yeh & Smith, DAFx-06);
//   - the power stage: its sag (supply resistance and capacitor against the current at full power), its drive and
//     its feedback loop, normalised;
//   - the overdrive pedal: its gain, its corners;
//   - the cabinet: the closed box's resonance and Q from the speaker's Thiele-Small parameters, its filters.
#pragma once

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "../../constants/core.hpp"
#include "../../models/amp.hpp"
#include "grm.hpp"

namespace grmg
{
    // the 12AX7's plate current (A), after Koren
    inline double koren_current(double vgk, double vpk)
    {
        vpk = std::max(vpk, 1e-3);
        const double z = KOREN_KP * (1.0 / KOREN_MU + vgk / std::sqrt(KOREN_KVB + vpk * vpk));
        const double softplus = z > 30.0 ? z : std::log1p(std::exp(z));
        const double e1 = vpk / KOREN_KP * softplus;
        return e1 > 0.0 ? 2.0 * std::pow(e1, KOREN_EX) / KOREN_KG1 : 0.0;
    }

    // an audio taper pot's share of its track at position p (0 to 1)
    inline double audio_taper(double p)
    {
        const double full = std::pow(10.0, AUDIO_TAPER_DECADES) - 1.0;
        return (std::pow(10.0, AUDIO_TAPER_DECADES * std::clamp(p, 0.0, 1.0)) - 1.0) / full;
    }

    // a triode alone on its supply: the plate current where the tube's curve meets the load line (a first guess)
    inline double triode_rest(double B, double Rp, double Rk)
    {
        double lo = 0.0, hi = B / (Rp + Rk);
        for (int k = 0; k < 60; ++k)
        {
            const double I = 0.5 * (lo + hi);
            (I - koren_current(-I * Rk, B - I * (Rp + Rk)) > 0.0 ? hi : lo) = I;
        }
        return 0.5 * (lo + hi);
    }

    // the plate swing of a stage (V, around rest) at grid voltage v, on its AC load line; its cathode follows the
    // current when unbypassed, stays put when bypassed
    inline double triode_swing(double v, double Vp0, double Ip0, double Vk0, double Rac, double Rk, bool bypassed)
    {
        double lo = 0.0, hi = Ip0 + Vp0 / Rac;
        for (int k = 0; k < 48; ++k)
        {
            const double I = 0.5 * (lo + hi);
            const double Vk = bypassed ? Vk0 : I * Rk;
            const double Vp = Vp0 - (I - Ip0) * Rac;
            (I - koren_current(v - Vk, Vp - Vk) > 0.0 ? hi : lo) = I;
        }
        const double I = 0.5 * (lo + hi);
        return -(I - Ip0) * Rac;
    }

    inline std::vector<models::TriodeModel> preamp_network(const models::AmpVoicing &voicing)
    {
        const int n = std::min((int)voicing.stages.size(), MAX_TRIODES);
        grm::Network net;
        net.add_node("B", voicing.B, false, true);
        net.add_node("R_drop", voicing.drop, false, true);
        auto name = [](const char *what, int i) { return std::string(what) + std::to_string(i + 1); };
        for (int i = 0; i < n; ++i)
        {
            const auto &c = voicing.stages[(size_t)i];
            const double Ip = triode_rest(voicing.B, c.Rp, c.Rk);
            net.add_node(name("Rp", i), c.Rp, false, true);
            net.add_node(name("Rk", i), c.Rk, false, true);
            net.add_node(name("Bn", i), voicing.B, false, false);
            net.add_node(name("Ip", i), Ip, false, false);
            net.add_node(name("Vp", i), voicing.B - Ip * c.Rp, false, false);
            net.add_node(name("Vk", i), Ip * c.Rk, false, false);
        }
        const int iB = net.index("B"), iDrop = net.index("R_drop");
        for (int i = 0; i < n; ++i)
        {
            const int iBn = net.index(name("Bn", i)), iIp = net.index(name("Ip", i)), iVp = net.index(name("Vp", i));
            const int iVk = net.index(name("Vk", i)), iRp = net.index(name("Rp", i));
            const int iFrom = i == 0 ? iB : net.index(name("Bn", i - 1));
            // the supply node of this stage: the previous one, less the drop of every stage it feeds
            std::vector<std::string> nodes = {name("Bn", i), i == 0 ? std::string("B") : name("Bn", i - 1), "R_drop"};
            std::vector<int> fed;
            for (int j = i; j < n; ++j)
            {
                nodes.push_back(name("Ip", j));
                fed.push_back(net.index(name("Ip", j)));
            }
            net.add_fn("supply " + std::to_string(i + 1), nodes, [=](const grm::Values &v)
                       {
                double drawn = 0.0;
                for (int j : fed)
                    drawn += v[j];
                return (v[iBn] - (v[iFrom] - v[iDrop] * drawn)) / v[iB]; }, 1e-9);
            net.add_fn("plate " + std::to_string(i + 1), {name("Vp", i), name("Bn", i), name("Ip", i), name("Rp", i)}, [=](const grm::Values &v)
                       { return (v[iVp] - (v[iBn] - v[iIp] * v[iRp])) / v[iB]; }, 1e-9);
            net.add_log("cathode " + std::to_string(i + 1), {{name("Vk", i), 1.0}, {name("Ip", i), -1.0}, {name("Rk", i), -1.0}}, 0.0, 1e-9);
            net.add_fn("triode " + std::to_string(i + 1), {name("Ip", i), name("Vp", i), name("Vk", i)}, [=](const grm::Values &v)
                       { return (v[iIp] - koren_current(-v[iVk], v[iVp] - v[iVk])) / v[iIp]; }, 1e-9);
        }
        const auto r = net.solve({}, 60, 1e-3, 1e-14, false);

        std::vector<models::TriodeModel> stages((size_t)n);
        for (int i = 0; i < n; ++i)
        {
            const auto &c = voicing.stages[(size_t)i];
            auto &m = stages[(size_t)i];
            m.Ip = r[name("Ip", i)];
            m.Vp = r[name("Vp", i)];
            m.Vk = r[name("Vk", i)];
            // the AC load: the plate resistor and the next grid leak (or the tone stack and phase inverter)
            const double next = i + 1 < n ? voicing.stages[(size_t)i + 1].Rg : TRIODE_LAST_LOAD;
            const double Rac = c.Rp * next / (c.Rp + next);
            // transconductance and plate resistance at rest
            const double dv = 1e-3, vpk = m.Vp - m.Vk;
            const double gm = (koren_current(-m.Vk + dv, vpk) - koren_current(-m.Vk - dv, vpk)) / (2 * dv);
            const double rp = 2 * dv / std::max(koren_current(-m.Vk, vpk + dv) - koren_current(-m.Vk, vpk - dv), 1e-15);
            const double mu = gm * rp;
            const double bypassed_gain = gm * rp * Rac / (rp + Rac);
            const double unbypassed_gain = mu * Rac / (rp + Rac + (mu + 1.0) * c.Rk);
            const bool bypassed = c.Ck > 0.0;
            m.gain = bypassed ? bypassed_gain : unbypassed_gain;
            if (bypassed)
            {
                const double seen = c.Rk * ((rp + Rac) / (mu + 1.0)) / (c.Rk + (rp + Rac) / (mu + 1.0));
                m.shelf_hz = 1.0 / (2 * PI * c.Ck * seen);
                m.shelf_gain = unbypassed_gain / bypassed_gain;
            }
            m.att = c.att;
            m.grid_hz = 1.0 / (2 * PI * c.Cc * (c.Rg + c.Rin));
            m.miller_hz = 1.0 / (2 * PI * c.Rin * (TRIODE_CGK + TRIODE_CGP * (1.0 + m.gain)));
            m.grid_on = m.Vk + GRID_ONSET_V; // the grid meets the cathode
            m.grid_share = R_GRID_COND / (c.Rin + R_GRID_COND);
            m.charge_s = (c.Rin + R_GRID_COND) * c.Cc;
            m.leak_s = (c.Rg + c.Rin) * c.Cc;
            m.noise_density = i == 0 ? std::sqrt(4.0 * BOLTZMANN * T_AIR * c.Rin) : 0.0;
            // the curve: from past cutoff to past the grid's conduction (which keeps the grid below it)
            m.curve_lo = m.Vk - 1.5 * m.Vp / KOREN_MU - 2.0;
            m.curve_hi = m.grid_on + 2.0;
            m.curve.resize(TRIODE_CURVE_POINTS);
            for (int k = 0; k < TRIODE_CURVE_POINTS; ++k)
            {
                const double v = m.curve_lo + (m.curve_hi - m.curve_lo) * k / (TRIODE_CURVE_POINTS - 1);
                m.curve[(size_t)k] = triode_swing(v, m.Vp, m.Ip, m.Vk, Rac, c.Rk, bypassed);
            }
            // the curve's own zero: the swing at the grid's rest (0 V)
            const double rest = triode_swing(0.0, m.Vp, m.Ip, m.Vk, Rac, c.Rk, bypassed);
            for (double &y : m.curve)
                y -= rest;
        }
        return stages;
    }

    inline models::ToneStackModel tone_stack_network(const models::ToneStackParts &p, double bass, double middle, double treble)
    {
        grm::Network net;
        const char *parts[] = {"R1", "R2", "R3", "R4", "C1", "C2", "C3"};
        const double values[] = {p.R1, p.R2, p.R3, p.R4, p.C1, p.C2, p.C3};
        for (int k = 0; k < 7; ++k)
            net.add_node(parts[k], values[k], false, true);
        net.add_node("t", std::clamp(treble, 0.0, 1.0), true, true);
        net.add_node("m", std::clamp(middle, 0.0, 1.0), true, true);
        const double l = std::exp((std::clamp(bass, 0.0, 1.0) - 1.0) * TONE_BASS_TAPER);
        net.add_node("l", l, true, true);
        const int R1 = net.index("R1"), R2 = net.index("R2"), R3 = net.index("R3"), R4 = net.index("R4");
        const int C1 = net.index("C1"), C2 = net.index("C2"), C3 = net.index("C3");
        const int T = net.index("t"), M = net.index("m"), L = net.index("l");
        using Expr = double (*)(double, double, double, double, double, double, double, double, double, double);
        // the coefficients of H(s) (Yeh & Smith), each a relation of the parts and the pots
        static const std::pair<const char *, Expr> coefficients[] = {
            {"b1", [](double r1, double r2, double r3, double r4, double c1, double c2, double c3, double t, double m, double l)
             { (void)r4; return t * c1 * r1 + m * c3 * r3 + l * (c1 * r2 + c2 * r2) + (c1 * r3 + c2 * r3); }},
            {"b2", [](double r1, double r2, double r3, double r4, double c1, double c2, double c3, double t, double m, double l)
             { return t * (c1 * c2 * r1 * r4 + c1 * c3 * r1 * r4) - m * m * (c1 * c3 * r3 * r3 + c2 * c3 * r3 * r3) +
                      m * (c1 * c3 * r1 * r3 + c1 * c3 * r3 * r3 + c2 * c3 * r3 * r3) +
                      l * (c1 * c2 * r1 * r2 + c1 * c2 * r2 * r4 + c1 * c3 * r2 * r4) +
                      l * m * (c1 * c3 * r2 * r3 + c2 * c3 * r2 * r3) + (c1 * c2 * r1 * r3 + c1 * c2 * r3 * r4 + c1 * c3 * r3 * r4); }},
            {"b3", [](double r1, double r2, double r3, double r4, double c1, double c2, double c3, double t, double m, double l)
             { const double c = c1 * c2 * c3;
               return l * m * c * (r1 * r2 * r3 + r2 * r3 * r4) - m * m * c * (r1 * r3 * r3 + r3 * r3 * r4) +
                      m * c * (r1 * r3 * r3 + r3 * r3 * r4) + t * c * r1 * r3 * r4 - t * m * c * r1 * r3 * r4 + t * l * c * r1 * r2 * r4; }},
            {"a1", [](double r1, double r2, double r3, double r4, double c1, double c2, double c3, double t, double m, double l)
             { (void)t; return (c1 * r1 + c1 * r3 + c2 * r3 + c2 * r4 + c3 * r4) + m * c3 * r3 + l * (c1 * r2 + c2 * r2); }},
            {"a2", [](double r1, double r2, double r3, double r4, double c1, double c2, double c3, double t, double m, double l)
             { (void)t; return m * (c1 * c3 * r1 * r3 - c2 * c3 * r3 * r4 + c1 * c3 * r3 * r3 + c2 * c3 * r3 * r3) +
                      l * m * (c1 * c3 * r2 * r3 + c2 * c3 * r2 * r3) - m * m * (c1 * c3 * r3 * r3 + c2 * c3 * r3 * r3) +
                      l * (c1 * c2 * r2 * r4 + c1 * c2 * r1 * r2 + c1 * c3 * r2 * r4 + c2 * c3 * r2 * r4) +
                      (c1 * c2 * r1 * r4 + c1 * c3 * r1 * r4 + c1 * c2 * r3 * r4 + c1 * c2 * r1 * r3 + c1 * c3 * r3 * r4 + c2 * c3 * r3 * r4); }},
            {"a3", [](double r1, double r2, double r3, double r4, double c1, double c2, double c3, double t, double m, double l)
             { (void)t; const double c = c1 * c2 * c3;
               return l * m * c * (r1 * r2 * r3 + r2 * r3 * r4) - m * m * c * (r1 * r3 * r3 + r3 * r3 * r4) +
                      m * c * (r3 * r3 * r4 + r1 * r3 * r3 - r1 * r3 * r4) + l * c * r1 * r2 * r4 + c * r1 * r3 * r4; }},
        };
        for (const auto &[node, expr] : coefficients)
        {
            const double guess = expr(p.R1, p.R2, p.R3, p.R4, p.C1, p.C2, p.C3, treble, middle, l);
            const double scale = std::max(std::abs(expr(p.R1, p.R2, p.R3, p.R4, p.C1, p.C2, p.C3, 0.5, 0.5, 0.5)), 1e-30);
            const int at = net.add_node(node, guess, true, false);
            net.add_fn(node, {node, "R1", "R2", "R3", "R4", "C1", "C2", "C3", "t", "m", "l"}, [=](const grm::Values &v)
                       { return (v[at] - expr(v[R1], v[R2], v[R3], v[R4], v[C1], v[C2], v[C3], v[T], v[M], v[L])) / scale; }, 1e-9);
        }
        const auto r = net.solve({}, 20, 1e-3, 1e-14, false);
        models::ToneStackModel m;
        m.b = {0.0, r["b1"], r["b2"], r["b3"]};
        m.a = {1.0, r["a1"], r["a2"], r["a3"]};
        return m;
    }

    inline models::PowerAmpModel power_network(const models::PowerCircuit &c, double master, double presence, double resonance)
    {
        grm::Network net;
        net.add_node("B", c.B, false, true);
        net.add_node("R_supply", c.supply_R, false, true);
        net.add_node("C_supply", c.supply_C, false, true);
        net.add_node("I_full", c.full_current, false, true);
        net.add_node("bias", c.bias, false, true);
        net.add_node("pi_gain", c.pi_gain, false, true);
        net.add_node("beta", std::max(c.nfb, 1e-6), false, true);
        net.add_node("V_full", FULL_POWER_VOLTS, false, true);
        net.add_node("sag_depth", 0.05);
        net.add_node("sag_s", 0.01);
        net.add_node("drive", 0.5);
        net.add_node("loop", 1.0);
        // the supply drops by R I at full power, recovers in R C
        net.add_log("sag", {{"sag_depth", 1}, {"R_supply", -1}, {"I_full", -1}, {"B", 1}}, 0.0, 1e-9);
        net.add_log("supply time", {{"sag_s", 1}, {"R_supply", -1}, {"C_supply", -1}}, 0.0, 1e-9);
        // a volt in moves the grids by pi_gain volts, normalised by their bias
        net.add_log("drive", {{"drive", 1}, {"pi_gain", -1}, {"bias", 1}}, 0.0, 1e-9);
        // the feedback loop: beta of the speaker's volts back into the phase inverter
        net.add_log("feedback loop", {{"loop", 1}, {"beta", -1}, {"V_full", -1}, {"drive", -1}}, 0.0, 1e-9);
        const auto r = net.solve({}, 20, 1e-3, 1e-14, false);
        models::PowerAmpModel m;
        m.input = audio_taper(master);
        m.drive = r["drive"];
        m.idle = c.idle;
        m.knee = c.knee;
        m.sag_depth = r["sag_depth"];
        m.sag_s = r["sag_s"];
        m.nfb = c.nfb > 0.0 ? r["loop"] : 0.0;
        m.presence = std::clamp(presence, 0.0, 1.0);
        m.presence_hz = c.presence_hz;
        m.resonance = std::clamp(resonance, 0.0, 1.0);
        m.resonance_hz = c.resonance_hz;
        m.lo_hz = c.lo_hz;
        m.hi_hz = c.hi_hz;
        return m;
    }

    inline models::OverdriveModel overdrive_network(const models::OverdriveSettings &s)
    {
        grm::Network net;
        net.add_node("R_gain", OD_R_GAIN, false, true);
        net.add_node("C_gain", OD_C_GAIN, false, true);
        net.add_node("R_fixed", OD_R_FIXED, false, true);
        net.add_node("R_drive", OD_R_DRIVE * audio_taper(s.drive) + 1.0, false, true);
        net.add_node("C_feedback", OD_C_FEEDBACK, false, true);
        net.add_node("gain", 10.0);
        net.add_node("hp_hz", 700.0);
        net.add_node("lp_hz", 5000.0);
        const int iG = net.index("gain"), iRg = net.index("R_gain"), iRf = net.index("R_fixed"), iRd = net.index("R_drive");
        const int iLp = net.index("lp_hz"), iCf = net.index("C_feedback");
        // the op-amp: 1 + (R_drive + R_fixed) / R_gain above the corner of R_gain C_gain
        net.add_fn("op-amp gain", {"gain", "R_gain", "R_fixed", "R_drive"}, [=](const grm::Values &v)
                   { return v[iG] / (1.0 + (v[iRd] + v[iRf]) / v[iRg]) - 1.0; }, 1e-9);
        net.add_log("gain corner", {{"hp_hz", 1}, {"R_gain", 1}, {"C_gain", 1}}, -std::log10(2 * PI), 1e-9);
        net.add_fn("feedback corner", {"lp_hz", "R_drive", "R_fixed", "C_feedback"}, [=](const grm::Values &v)
                   { return v[iLp] * 2 * PI * (v[iRd] + v[iRf]) * v[iCf] - 1.0; }, 1e-9);
        const auto r = net.solve({}, 20, 1e-3, 1e-14, false);
        models::OverdriveModel m;
        m.on = s.on;
        m.gain = r["gain"];
        m.hp_hz = r["hp_hz"];
        m.lp_hz = r["lp_hz"];
        m.tone_hz = OD_TONE_LO_HZ * std::pow(OD_TONE_HI_HZ / OD_TONE_LO_HZ, std::clamp(s.tone, 0.0, 1.0));
        m.level = OD_LEVEL_MAX * audio_taper(s.level);
        return m;
    }

    inline models::CabinetModel cabinet_network(const models::CabinetSpec &spec, double mic)
    {
        grm::Network net;
        const bool closed = spec.Vb > 0.0;
        net.add_node("fs", spec.fs, false, true);
        net.add_node("Qts", spec.Qts, false, true);
        net.add_node("Vas", spec.Vas * spec.drivers, false, true); // the drivers share the box
        net.add_node("Vb", closed ? spec.Vb : 1.0, false, true);
        net.add_node("fc", spec.fs);
        net.add_node("Qtc", spec.Qts);
        const int iFs = net.index("fs"), iQts = net.index("Qts"), iVas = net.index("Vas"), iVb = net.index("Vb");
        const int iFc = net.index("fc"), iQtc = net.index("Qtc");
        // the air in a closed box stiffens the cone: both the resonance and the Q rise by sqrt(1 + Vas / Vb)
        const auto stiffening = [=](const grm::Values &v)
        { return closed ? std::sqrt(1.0 + v[iVas] / v[iVb]) : 1.0; };
        net.add_fn("box resonance", {"fc", "fs", "Vas", "Vb"}, [=](const grm::Values &v)
                   { return v[iFc] / (v[iFs] * stiffening(v)) - 1.0; }, 1e-9);
        net.add_fn("box Q", {"Qtc", "Qts", "Vas", "Vb"}, [=](const grm::Values &v)
                   { return v[iQtc] / (v[iQts] * stiffening(v)) - 1.0; }, 1e-9);
        const auto r = net.solve({}, 20, 1e-3, 1e-14, false);
        models::CabinetModel m;
        m.fc = r["fc"];
        m.Qtc = r["Qtc"];
        using K = models::CabinetFilter::Kind;
        mic = std::clamp(mic, 0.0, 1.0);
        const double top = spec.top_hz * (1.0 - CAB_MIC_TOP_DROP * mic);
        m.filters.push_back({K::highpass, m.fc, m.Qtc, 0.0});
        if (!closed) // an open back: the rear wave cancels the front below the path around the cabinet
            m.filters.push_back({K::highpass, C_AIR_DEFAULT / (4 * PI * spec.depth), 0.5, 0.0});
        m.filters.push_back({K::peak, spec.notch_hz, CAB_NOTCH_Q, spec.notch_db});
        m.filters.push_back({K::peak, spec.breakup_hz, CAB_BREAKUP_Q, spec.breakup_db * (1.0 - CAB_MIC_PEAK_DROP * mic)});
        m.filters.push_back({K::lowpass, top, CAB_TOP_Q, 0.0});
        m.filters.push_back({K::lowpass, top * CAB_TOP2_RATIO, CAB_TOP2_Q, 0.0});
        return m;
    }

    inline models::AmpChannelModel amp_channel_model(const models::AmpChannelSettings &s, const models::AmpChannelModel *previous)
    {
        const int index = std::clamp(s.voicing, 0, (int)AMP_VOICINGS.size() - 1);
        const auto &v = AMP_VOICINGS[(size_t)index];
        models::AmpChannelModel m;
        m.voicing = index;
        m.stages = previous && previous->voicing == index ? previous->stages : preamp_network(v);
        m.gain_stage = v.gain_stage;
        m.pot = audio_taper(s.gain / 10.0);
        m.bright_hz = v.bright_hz;
        m.bright_gain = std::min(1.0 / std::max(m.pot, 1e-6), BRIGHT_MAX);
        m.tone_after = v.tone_after;
        m.tone = tone_stack_network(v.tone, s.bass, s.middle, s.treble);
        m.power = power_network(v.power, s.master, s.presence, s.resonance);
        return m;
    }

    // the whole rig; the preamps' curves are taken from `previous` when their circuit has not changed
    inline models::AmpModel amp_model(const models::AmpSettings &s, const models::AmpModel *previous)
    {
        models::AmpModel m;
        m.clean = amp_channel_model(s.clean, previous ? &previous->clean : nullptr);
        m.lead = amp_channel_model(s.lead, previous ? &previous->lead : nullptr);
        m.drive = overdrive_network(s.drive);
        const int cab = std::clamp(s.cabinet.kind, 0, (int)(sizeof(CABINETS) / sizeof(CABINETS[0])) - 1);
        m.cabinet = cabinet_network(CABINETS[cab], s.cabinet.mic);
        m.mix = std::clamp(s.mix, 0.0, 1.0);
        m.gate_db = s.gate_db;
        return m;
    }
}
