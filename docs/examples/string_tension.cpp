// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// The smallest useful GRM network: a guitar string. Three relations tie its length, its mass per length, its
// tension, its wave speed and its pitch. Measure any three of them, with their uncertainties, and the network gives
// the others, with theirs. Build: g++ -std=c++17 -I <path to cppgrm> string_tension.cpp
#include <cmath>
#include <cstdio>

#include "grm.hpp"

int main()
{
    grm::Network net;
    // nodes: positive quantities, solved in log10 space; the guess only starts the solver
    net.add_node("L", 0.65);   // vibrating length (m)
    net.add_node("mu", 1e-3);  // mass per length (kg/m)
    net.add_node("T", 70.0);   // tension (N)
    net.add_node("c", 200.0);  // wave speed (m/s)
    net.add_node("f1", 100.0); // fundamental (Hz)

    // relations, written as sums of log10: c^2 = T / mu, f1 = c / (2 L)
    net.add_log("wave speed", {{"c", 2}, {"mu", 1}, {"T", -1}}, 0.0, 1e-9);
    net.add_log("fundamental", {{"f1", 1}, {"L", 1}, {"c", -1}}, -std::log10(2.0), 1e-9);

    // what is measured: a 648 mm scale, the low E of a nickel 10-46 set (D'Addario NW046, 0.00038216 lb/in), tuned to E2.
    // sigma is in dex (log10 units): 0.01 / ln 10 is a 1 % uncertainty
    const double one_percent = 0.01 / grm::LN10;
    const auto r = net.solve({{"L", {0.648, one_percent / 10}},
                              {"mu", {0.00682, one_percent}},
                              {"f1", {82.41, 1e-9}}});

    // r.uncertainty: relative for positive nodes
    std::printf("tension    %.1f N  (+/- %.1f %%)\n", r["T"], 100 * r.uncertainty[(size_t)net.index("T")]);
    std::printf("wave speed %.1f m/s\n", r["c"]);
    std::printf("converged  %s in %d iterations\n", r.converged ? "yes" : "no", r.iterations);
    return 0;
}
