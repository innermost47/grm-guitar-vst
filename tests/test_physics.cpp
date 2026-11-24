// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// The body, room, palm, finger and player networks of the C++ port against grm-core (tests/golden.hpp).
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

#include "../core/engine.hpp"
#include "../core/physics.hpp"
#include "../core/voice.hpp"
#include "golden.hpp"

static int failures = 0, checks = 0;

static void expect(const char* test, const std::string& node, double value, double rel = 1e-8) {
    for (const auto& gv : GOLDEN) {
        if (std::strcmp(gv.test, test) || node != gv.node) continue;
        ++checks;
        if (std::abs(value - gv.value) > std::max(rel * std::abs(gv.value), 1e-12)) {
            ++failures;
            std::printf("FAIL %s %s: C++ %.15g, Python %.15g\n", test, node.c_str(), value, gv.value);
        }
        return;
    }
    ++failures;
    std::printf("no golden value for %s %s\n", test, node.c_str());
}

int main() {
    using namespace grmg;
    using namespace grmg::models;
    // body and bridge
    const auto body = body_network_result();
    for (const char* n : {"f_lo", "f_hi", "f3", "f7", "Mm", "Zp", "mb", "fT", "frad", "fb", "fH"}) expect("body", n, body[n]);
    const Body b = body_network();
    for (double f : {82.0, 196.0, 1000.0, 3000.0}) {
        const auto Y = b.admittance(f);
        expect("admittance", "re" + std::to_string((int) f), Y.real());
        expect("admittance", "im" + std::to_string((int) f), Y.imag());
    }
    // the loaded top: braces and bridge on the plate (about 200 g by default), the measured Q on every mode
    {
        const auto loaded = body_network_result(1.0, 2.8, 0.35, BRACES_KG, BODY_Q);
        ++checks;
        if (std::abs(loaded["Mloaded"] - 0.2) > 0.01 || std::abs(loaded["Q"] - BODY_Q) > 1e-6
            || std::abs(loaded["Mm"] / body["Mm"] - loaded["Mloaded"] / body["Mtop"]) > 1e-6) {
            ++failures;
            std::printf("FAIL loaded top: %.4f kg, Q %.2f\n", loaded["Mloaded"], loaded["Q"]);
        }
        // a heavier top moves less under the string: lower admittance off its resonances
        const Body heavy = body_network(1.0, 2.8, 0.35, BRACES_KG, BODY_Q);
        ++checks;
        if (!(heavy.admittance(1000.0).real() < b.admittance(1000.0).real())) {
            ++failures;
            std::printf("FAIL loaded top admittance\n");
        }
    }
    // room
    const std::pair<int, int> rooms[2] = {{1, 1}, {3, 0}};
    const char* room_tests[2] = {"room_Living", "room_Church"};
    for (int k = 0; k < 2; ++k) {
        const auto r = room_network(ROOMS[rooms[k].first], rooms[k].second);
        expect(room_tests[k], "V", r.V);
        expect(room_tests[k], "S", r.S);
        expect(room_tests[k], "rc", r.rc);
        expect(room_tests[k], "fs", r.fs);
        for (int band = 0; band < 6; ++band) expect(room_tests[k], "RT" + std::to_string(band), r.RT[(size_t) band]);
    }
    // palm
    const std::pair<int, int> palms[2] = {{0, 0}, {5, 3}};
    for (auto [i, fret] : palms) {
        const auto& s = STRING_SPECS[i];
        const Palm p = palm_network(s.mu, s.L, fret, PALM_X, PALM_FORCE, s.T);
        const std::string t = "palm" + std::to_string(i);
        expect(t.c_str(), "f1", p.f1);
        expect(t.c_str(), "count", (double) p.ratio.size());
        for (size_t n : {0u, 1u, 4u, 9u}) {
            if (n >= p.ratio.size()) continue;
            expect(t.c_str(), "ratio" + std::to_string(n), p.ratio[n], 1e-6);
            expect(t.c_str(), "sigma" + std::to_string(n), p.sigma[n], 1e-6);
        }
    }
    // finger on a wound string, finger on a fret
    {
        const auto& s = STRING_SPECS[1];
        const Squeak sl = squeak_network(s.mu, s.L, 0.45, 3, 7, 0.12, 0.8);
        expect("squeak_slide", "fsq", sl.fsq);
        expect("squeak_slide", "P", sl.P);
        expect("squeak_slide", "fL", sl.fL);
        expect("squeak_slide", "v", sl.v);
        expect("squeak_slide", "p", sl.p);
        expect("squeak_slide", "cL", sl.cL);
        const Squeak li = squeak_network(s.mu, s.L, 0.45, 5, 0, 0.05, 0.8);
        expect("squeak_lift", "fsq", li.fsq);
        expect("squeak_lift", "P", li.P);
        expect("squeak_lift", "fL", li.fL);
        expect("squeak_lift", "v", li.v);
        const Action a = action_network(0.648, 7);
        expect("action7", "xf", a.xf);
        expect("action7", "h", a.h);
    }
    // the player, without imprecision
    {
        PlayerNoise exact;
        exact.exact = true;
        const auto st = player_network({{0.0, 0}, {0.25, 1}, {0.5, 2}, {1.0, 4}, {1.25, 5}}, 0.25, 0.2, exact);
        for (size_t k = 0; k < st.size(); ++k) {
            const std::string s = std::to_string(k);
            expect("player", "t" + s, st[k].t, 1e-7);
            expect("player", "A" + s, st[k].A, 1e-7);
            expect("player", "v" + s, st[k].v, 1e-7);
            expect("player", "p" + s, st[k].p, 1e-7);
            expect("player", "d" + s, st[k].d, 1e-7);
            expect("player", "palm_x" + s, st[k].palm_x, 1e-7);
            expect("player", "palm_N" + s, st[k].palm_N, 1e-7);
        }
        for (const auto& c : strum_contacts({0, 1, 2, 3, 4, 5}, st[1], false)) {
            expect("contacts_down", "delay" + std::to_string(c.string), c.delay, 1e-6);
            expect("contacts_down", "share" + std::to_string(c.string), c.share, 1e-6);
        }
        for (const auto& c : strum_contacts({5, 4, 3, 2, 1, 0}, st[1], true)) {
            expect("contacts_up", "delay" + std::to_string(c.string), c.delay, 1e-6);
            expect("contacts_up", "share" + std::to_string(c.string), c.share, 1e-6);
        }
    }
    // impulse responses: finite, of the expected length, not silent
    {
        const auto ir = body_impulse(b, 48000.0);
        double e = 0;
        bool finite = true;
        for (double x : ir) { e += x * x; finite &= std::isfinite(x); }
        ++checks;
        if (!finite || e <= 0) { ++failures; std::printf("FAIL body impulse\n"); }
        const auto room = room_network(ROOMS[1], 1);
        const auto h = room_impulse(room, ROOMS[1], 1.0, 48000.0);
        double eh = 0;
        for (double x : h) { eh += x * x; finite &= std::isfinite(x); }
        ++checks;
        if (!finite || eh <= 0 || std::abs(h[0]) > 0) { ++failures; std::printf("FAIL room impulse\n"); }
        const auto sq = synthesize_squeak(1500, 0.1, 0.12, 1500, 0.3, 0.3, 48000.0, 7, {0.5}, 0.8);
        double es = 0;
        for (double x : sq) { es += x * x; finite &= std::isfinite(x); }
        ++checks;
        if (!finite || es <= 0) { ++failures; std::printf("FAIL squeak\n"); }
    }
    // one string as it sounds, against synthesize_string (44.1 kHz, as the Python app)
    {
        const auto& sp = STRING_SPECS[1];
        const auto r = play_string_model(sp.L, sp.mu, sp.T, 3, 0.45);
        const auto a = pluck_network(sp.mu, sp.L, sp.T, 3, 0.22, 2.0, true, 0.71);
        const Palm palm = palm_network(sp.mu, sp.L, 3, PALM_X, PALM_FORCE, sp.T);
        for (int with_palm = 0; with_palm < 2; ++with_palm) {
            Pluck p;
            p.string = 1;
            p.fret = 3;
            p.place = {r["f1"], r["B"], sp.T, r["Leff"], r["c"], r["Zs"]};
            p.gain = a["y0"] / Y_REF;
            p.tau = a["tau"];
            p.pluck = 0.22;
            p.palm = with_palm ? &palm : nullptr;
            StringVoice voice;
            StringLosses legacy;
            legacy.legacy = 3.0;          // the loss curve of the Python app, for this parity check
            voice.start(p, b, legacy, 0.6, 44100.0);
            std::vector<double> y(44100, 0.0);
            voice.render(y.data(), 44100);
            const char* t = with_palm ? "voice_palm" : "voice";
            // the palm's recovery is computed every 16 samples here, every sample in Python
            const double tol = with_palm ? 2e-2 : 1e-8;
            const size_t w[3][2] = {{0, 441}, {441, 4410}, {4410, 44100}};
            for (int k = 0; k < 3; ++k) {
                double e = 0;
                for (size_t i = w[k][0]; i < w[k][1]; ++i) e += y[i] * y[i];
                expect(t, "rms" + std::to_string(k), std::sqrt(e / (double) (w[k][1] - w[k][0])), tol);
            }
            expect(t, "sample100", y[100], with_palm ? 5e-2 : 1e-8);
        }
    }
    // the losses network: new strings give Woodhouse's measured losses, and the decay follows his law
    {
        for (int i = 0; i < NUM_STRINGS; ++i) {
            const auto r = string_losses_network(i, 0.0);
            ++checks;
            if (std::abs(std::log10(r["etaB"] / LOSSES_NEW[i].etaB)) > 1e-6
                || std::abs(std::log10(r["etaA"] / LOSSES_NEW[i].etaA)) > 1e-6) {
                ++failures;
                std::printf("FAIL losses of string %d (new)\n", i);
            }
            const auto q = string_losses_network(i, 1.0);
            ++checks;
            if (std::abs(std::log10(q["etaF"] / LOSSES_PLAYED[i].etaF)) > 1e-6) {
                ++failures;
                std::printf("FAIL losses of string %d (played)\n", i);
            }
        }
        // low E, new: the fundamental rings longest, the partial near 3 kHz much less (Woodhouse: ~8 s, ~1.5 s)
        const auto r = string_losses_network(0, 0.0);
        const StringLosses L {r["etaF"], r["etaA"], r["etaB"], 0.0};
        const double B = 1.06e-4, f1 = 82.41;
        const double t60_1 = 6.9078 / L.sigma(f1, 1, B);
        const int n = 36;
        const double t60_36 = 6.9078 / L.sigma(n * f1 * std::sqrt(1 + B * n * n), n, B);
        ++checks;
        if (std::abs(t60_1 - 8.0) > 0.3 || std::abs(t60_36 - 1.5) > 0.3 || !(t60_1 > 3 * t60_36)) {
            ++failures;
            std::printf("FAIL decay law: T60 %.2f s (n = 1), %.2f s (n = 36)\n", t60_1, t60_36);
        }
        // a played string loses its highs sooner than a new one
        const auto old = string_losses_network(0, 1.0);
        const StringLosses O {old["etaF"], old["etaA"], old["etaB"], 0.0};
        ++checks;
        if (!(O.sigma(3000, 36, B) > 2 * L.sigma(3000, 36, B))) {
            ++failures;
            std::printf("FAIL played strings\n");
        }
    }
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
