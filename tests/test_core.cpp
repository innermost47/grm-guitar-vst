// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// The C++ GRM must give the same numbers as the Python one (tests/golden.hpp, made by tools/make_golden.py).
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>

#include "../core/guitar_networks.hpp"
#include "golden.hpp"

struct Strings { double L, mu, T, target; bool wound; };
// the same six strings as grm_guitar.py (STRINGS)
static const Strings STRINGS[6] = {
    {0.648, 0.0061370673, 70.0, 82.4069, true}, {0.648, 0.0034443134, 70.0, 110.0, true},
    {0.648, 0.0019330657, 70.0, 146.832, true}, {0.648, 0.0010848655, 70.0, 196.0, false},
    {0.648, 0.0006834364, 70.0, 246.942, false}, {0.648, 0.0003835658, 70.0, 329.628, false},
};

static int failures = 0, checks = 0;

static void check(const char* test, const char* node, const grm::Network* /*unused*/, const grm::Result& r) {
    for (const auto& gv : GOLDEN) {
        if (std::strcmp(gv.test, test) || std::strcmp(gv.node, node)) continue;
        ++checks;
        const double v = r[node];
        // values: same to 1e-9 relative (or 1e-12 absolute near zero); uncertainties: same to 1e-6 relative
        const double tol_v = std::max(1e-9 * std::abs(gv.value), 1e-12);
        size_t idx = 0;
        for (const auto& [name, val] : r.by_name) { (void) val; if (name == node) break; ++idx; }
        if (std::abs(v - gv.value) > tol_v) {
            ++failures;
            std::printf("FAIL %s %s: C++ %.15g, Python %.15g\n", test, node, v, gv.value);
        }
        return;
    }
    std::printf("no golden value for %s %s\n", test, node);
    ++failures;
}

static void check_sigma(const char* test, const char* node, const grm::Result& r, int index) {
    for (const auto& gv : GOLDEN) {
        if (std::strcmp(gv.test, test) || std::strcmp(gv.node, node)) continue;
        ++checks;
        const double s = r.sigma_dex[(size_t) index];
        if (std::abs(s - gv.sigma_dex) > 1e-6 * std::max(std::abs(gv.sigma_dex), 1e-12)) {
            ++failures;
            std::printf("FAIL sigma %s %s: C++ %.10g, Python %.10g\n", test, node, s, gv.sigma_dex);
        }
        return;
    }
}

int main() {
    for (int i = 0; i < 6; ++i) {
        const auto& s = STRINGS[i];
        const double core = s.wound ? 0.45 : 1.0;
        for (int fret : {0, 5}) {
            auto net = grmg::string_network(fret, core);
            auto r = grmg::play_string_model(s.L, s.mu, s.T, fret, core);
            const std::string t = "string" + std::to_string(i) + "_fret" + std::to_string(fret);
            for (const char* n : {"f1", "c", "B", "Zs", "Leff", "d"}) {
                check(t.c_str(), n, nullptr, r);
                check_sigma(t.c_str(), n, r, net.index(n));
            }
        }
        auto net = grmg::string_network(0, core);
        auto r = grmg::tune_string_model(s.L, s.mu, s.target, core);
        const std::string t = "tune" + std::to_string(i);
        for (const char* n : {"T", "c", "B"}) {
            check(t.c_str(), n, nullptr, r);
            check_sigma(t.c_str(), n, r, net.index(n));
        }
    }
    for (int i : {0, 5}) {
        const auto& s = STRINGS[i];
        for (bool pick : {true, false}) {
            auto r = grmg::pluck_network(s.mu, s.L, s.T, 3, 0.18, 2.0, pick, 0.71);
            const std::string t = "pluck" + std::to_string(i) + (pick ? "_pick" : "_finger");
            for (const char* n : {"ks", "y0", "tau", "fc", "Ec", "xr"}) check(t.c_str(), n, nullptr, r);
        }
    }
    {
        grm::Network net;
        const int x = net.add_node("x", 0.5, true), y = net.add_node("y", 0.5, true), rr = net.add_node("r", 2.0);
        net.add_fn("circle", {"x", "y", "r"}, [=](const grm::Values& v) { return v[x] * v[x] + v[y] * v[y] - v[rr] * v[rr]; }, 1e-9);
        net.add_sum("sum", {{"x", 1}, {"y", 1}}, 1.3, 1e-3);
        auto r = net.solve({{"r", {1.0, 1e-6}}});
        for (const char* n : {"x", "y", "r"}) {
            check("circle", n, nullptr, r);
            check_sigma("circle", n, r, net.index(n));
        }
    }
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
