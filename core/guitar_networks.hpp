// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// The GRM networks of the guitar (C++ port of grm_guitar.py), one function per network.
// Each one is the same set of relations as the Python version; tests/golden.hpp holds the Python results and
// tests/test_core.cpp checks that both give the same numbers.
#pragma once

#include <cmath>
#include <string>

#include "grm.hpp"
#include "../constants/core.hpp"

namespace grmg
{
    inline double dex(double rel) { return rel / grm::LN10; }

    // ---------------------------------------------------------------- the string
    // the diameter that bends, against the mass per length (d^2 / mu, m^3/kg): what the string is made of
    inline double bending_factor(double bending_diameter, double mu) { return bending_diameter * bending_diameter / mu; }
    // the same for a steel core, a share of the diameter of an all-steel string of the same mass
    inline double steel_core_factor(double core_fraction) { return 4.0 * core_fraction * core_fraction / (PI * RHO_STEEL); }

    // bending: d^2 / mu of the string (bending_factor), E: its core's Young's modulus
    inline grm::Network string_network(int fret, double bending, double E)
    {
        grm::Network net;
        net.add_node("L", 0.65);
        net.add_node("Leff", 0.65);
        net.add_node("mu", 1e-3);
        net.add_node("T", 70.0);
        net.add_node("c", 200.0);
        net.add_node("f1", 200.0);
        net.add_node("d", 5e-4);
        net.add_node("B", 1e-4);
        net.add_node("Zs", 0.5);
        net.add_log("wave speed", {{"c", 2}, {"mu", 1}, {"T", -1}}, 0.0, 1e-9);
        net.add_log("fundamental", {{"Leff", 1}, {"f1", 1}, {"c", -1}}, -std::log10(2.0), 1e-9);
        net.add_log("fret", {{"Leff", 1}, {"L", -1}}, -fret * std::log10(2.0) / 12.0, 1e-9);
        net.add_log("core", {{"d", 2}, {"mu", -1}}, std::log10(bending), 1e-9);
        net.add_log("stiffness", {{"B", 1}, {"d", -4}, {"T", 1}, {"Leff", 2}}, std::log10(PI * PI * PI * E / 64.0), 1e-9);
        net.add_log("string impedance", {{"Zs", 2}, {"T", -1}, {"mu", -1}}, 0.0, 1e-9);
        return net;
    }

    // L, mu, T measured -> f1, c, B, Zs with uncertainties
    inline grm::Result play_string_model(double L, double mu, double T, int fret, double bending, double E)
    {
        auto net = string_network(fret, bending, E);
        return net.solve({{"L", {L, dex(SIGMA_L / L)}}, {"mu", {mu, dex(SIGMA_MU_REL)}}, {"T", {T, dex(SIGMA_T / T)}}});
    }

    // L, mu and the target pitch measured -> the tension that gives that pitch (open string)
    inline grm::Result tune_string_model(double L, double mu, double target, double bending, double E)
    {
        auto net = string_network(0, bending, E);
        return net.solve({{"L", {L, dex(SIGMA_L / L)}}, {"mu", {mu, dex(SIGMA_MU_REL)}}, {"f1", {target, 1e-9}}});
    }

    // the losses of a string at an age between new (0) and played (1):
    //   - the air's (Valette): a viscous drag and a boundary layer that grows with the gauge, over the mass per length;
    //   - its material's (STRING_MATERIALS): each moves geometrically from new to played, a GRM relation per loss.
    inline grm::Result string_losses_network(const models::GuitarString &g, double age)
    {
        age = std::min(std::max(age, 0.0), 1.0);
        grm::Network net;
        std::map<std::string, grm::Measurement> m;
        const auto &mat = *g.material;
        const char *names[2] = {"etaF", "etaB"};
        const double fresh[2] = {mat.etaF, mat.etaB};
        const double played[2] = {mat.etaF_played, mat.etaB_played};
        for (int k = 0; k < 2; ++k)
        {
            const std::string n = names[k], a = n + "_new", b = n + "_played";
            net.add_node(n, fresh[k]);
            net.add_node(a, fresh[k]);
            net.add_node(b, played[k]);
            net.add_log("age of " + n, {{n, 1}, {a, -(1.0 - age)}, {b, -age}}, 0.0, 1e-9);
            m[a] = {fresh[k], LOSS_SIGMA_DEX};
            m[b] = {played[k], LOSS_SIGMA_DEX};
        }
        net.add_node("mu", g.mu);
        net.add_node("D", g.gauge);
        net.add_node("etaA", 0.1);
        net.add_node("etaA_sqrt", 0.01);
        net.add_log("air drag", {{"etaA", 1}, {"mu", 1}}, std::log10(2 * PI * AIR_VISCOSITY), 1e-9);
        net.add_log("boundary layer", {{"etaA_sqrt", 1}, {"mu", 1}, {"D", -1}},
                    std::log10(2 * PI * std::sqrt(PI * AIR_VISCOSITY * AIR_DENSITY)), 1e-9);
        m["mu"] = {g.mu, dex(SIGMA_MU_REL)};
        m["D"] = {g.gauge, dex(SIGMA_MU_REL)};
        return net.solve(m);
    }

    // ---------------------------------------------------------------- the attack: pick or finger meets the string
    inline grm::Result pluck_network(double mu, double L, double T, int fret, double p, double force, bool pick,
                                     double pick_mm)
    {
        grm::Network net;
        const char *names[] = {"mu", "L", "Leff", "T", "p", "x", "xr", "ks", "F", "y0", "w", "mc", "kc", "tau", "fc", "Ec"};
        const double guesses[] = {1e-3, 0.65, 0.6, 70.0, 0.2, 0.13, 0.5, 600.0, 2.0, 0.003, 0.002, 1e-5, 2e4, 1e-4, 2000.0, 1e-4};
        for (int k = 0; k < 16; ++k)
            net.add_node(names[k], guesses[k]);
        net.add_log("fret", {{"Leff", 1}, {"L", -1}}, -fret * std::log10(2.0) / 12.0, 1e-9);
        net.add_log("pluck point", {{"x", 1}, {"p", -1}, {"Leff", -1}}, 0.0, 1e-9);
        net.add_sum("other side", {{"xr", 1}, {"Leff", -1}, {"x", 1}}, 0.0, 1e-12);
        net.add_log("string stiffness", {{"ks", 1}, {"T", -1}, {"Leff", -1}, {"x", 1}, {"xr", 1}}, 0.0, 1e-9);
        net.add_log("release height", {{"y0", 1}, {"F", -1}, {"ks", 1}}, 0.0, 1e-9);
        net.add_log("mass in contact", {{"mc", 1}, {"mu", -1}, {"w", -1}}, 0.0, 1e-9);
        net.add_log("release time", {{"tau", 1}, {"mc", -0.5}, {"kc", 0.5}}, std::log10(PI), 1e-9);
        net.add_log("brightness limit", {{"fc", 1}, {"tau", 1}}, -std::log10(2 * PI), 1e-9);
        net.add_log("click energy", {{"Ec", 1}, {"F", -2}, {"kc", 1}}, -std::log10(2.0), 1e-9);
        std::map<std::string, grm::Measurement> m = {
            {"mu", {mu, 0.004}}, {"L", {L, 3e-4}}, {"T", {T, 0.003}}, {"p", {p, 0.01}}, {"F", {force, 0.02}}};
        if (pick)
        {
            net.add_node("Ep", 2e9);
            net.add_node("b", 0.005);
            net.add_node("tp", 7e-4);
            net.add_node("lp", 0.004);
            net.add_log("pick tip", {{"kc", 1}, {"Ep", -1}, {"b", -1}, {"tp", -3}, {"lp", 3}}, -std::log10(4.0), 1e-9);
            m["Ep"] = {PICK_E, 0.04};
            m["b"] = {PICK_TIP_WIDTH, 0.02};
            m["tp"] = {pick_mm / 1000.0, 0.01};
            m["lp"] = {PICK_FREE, 0.04};
            m["w"] = {PICK_CONTACT, 0.04};
        }
        else
        {
            m["kc"] = {K_FLESH, 0.1};
            m["w"] = {FINGER_CONTACT, 0.04};
        }
        return net.solve(m);
    }

} // namespace grmg
