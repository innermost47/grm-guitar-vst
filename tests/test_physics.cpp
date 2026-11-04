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
    // body and bridge
    const auto body = body_network_result();
    for (const char* n : {"f_lo", "f_hi", "f3", "f7", "Mm", "Zp", "mb", "fT", "frad", "fb", "fH"}) expect("body", n, body[n]);
    const Body b = body_network();
    for (double f : {82.0, 196.0, 1000.0, 3000.0}) {
        const auto Y = b.admittance(f);
        expect("admittance", "re" + std::to_string((int) f), Y.real());
        expect("admittance", "im" + std::to_string((int) f), Y.imag());
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
            voice.start(p, b, 3.0, 0.6, 44100.0);
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
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
