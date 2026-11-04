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

namespace grmg {

constexpr double PI = 3.14159265358979323846;
constexpr double RHO_STEEL = 7850.0;      // kg/m^3
constexpr double E_STEEL = 2.0e11;        // Pa
constexpr double SIGMA_L = 0.0005;        // m
constexpr double SIGMA_MU_REL = 0.01;
constexpr double SIGMA_T = 0.5;           // N
constexpr double PICK_E = 2.5e9;          // Pa (hypothesis)
constexpr double PICK_TIP_WIDTH = 0.005;  // m (hypothesis)
constexpr double PICK_FREE = 0.004;       // m (hypothesis)
constexpr double PICK_CONTACT = 0.0015;   // m
constexpr double K_FLESH = 2.0e3;         // N/m (hypothesis)
constexpr double FINGER_CONTACT = 0.012;  // m

inline double dex(double rel) { return rel / grm::LN10; }

// ---------------------------------------------------------------- the string
inline grm::Network string_network(int fret, double core_fraction) {
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
    net.add_log("steel core", {{"d", 2}, {"mu", -1}}, std::log10(4.0 * core_fraction * core_fraction / (PI * RHO_STEEL)), 1e-9);
    net.add_log("stiffness", {{"B", 1}, {"d", -4}, {"T", 1}, {"Leff", 2}}, std::log10(PI * PI * PI * E_STEEL / 64.0), 1e-9);
    net.add_log("string impedance", {{"Zs", 2}, {"T", -1}, {"mu", -1}}, 0.0, 1e-9);
    return net;
}

// L, mu, T measured -> f1, c, B, Zs with uncertainties
inline grm::Result play_string_model(double L, double mu, double T, int fret = 0, double core_fraction = 1.0) {
    auto net = string_network(fret, core_fraction);
    return net.solve({{"L", {L, dex(SIGMA_L / L)}}, {"mu", {mu, dex(SIGMA_MU_REL)}}, {"T", {T, dex(SIGMA_T / T)}}});
}

// L, mu and the target pitch measured -> the tension that gives that pitch (open string)
inline grm::Result tune_string_model(double L, double mu, double target, double core_fraction = 1.0) {
    auto net = string_network(0, core_fraction);
    return net.solve({{"L", {L, dex(SIGMA_L / L)}}, {"mu", {mu, dex(SIGMA_MU_REL)}}, {"f1", {target, 1e-9}}});
}

// ---------------------------------------------------------------- the attack: pick or finger meets the string
inline grm::Result pluck_network(double mu, double L, double T, int fret, double p, double force, bool pick,
                                 double pick_mm) {
    grm::Network net;
    const char* names[] = {"mu", "L", "Leff", "T", "p", "x", "xr", "ks", "F", "y0", "w", "mc", "kc", "tau", "fc", "Ec"};
    const double guesses[] = {1e-3, 0.65, 0.6, 70.0, 0.2, 0.13, 0.5, 600.0, 2.0, 0.003, 0.002, 1e-5, 2e4, 1e-4, 2000.0, 1e-4};
    for (int k = 0; k < 16; ++k) net.add_node(names[k], guesses[k]);
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
    if (pick) {
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
    } else {
        m["kc"] = {K_FLESH, 0.1};
        m["w"] = {FINGER_CONTACT, 0.04};
    }
    return net.solve(m);
}

}  // namespace grmg
