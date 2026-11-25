// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// The string sets: each string's mass from its maker's chart, the losses of its material and of the air around it,
// what a magnetic pickup senses of it. Checked against what was measured: D'Addario's published tensions, Pate et
// al. (JASA 2014) for a plain steel G3 between rigid ends and on a solid body, Woodhouse (Acta Acustica 2012) for the
// nylon strings' stiffness and for what playing does to wound strings.
#include <cmath>
#include <cstdio>

#include "../core/offline_renderer.hpp"
#include "support/rigs.hpp"

using namespace grmg;

static int failures = 0, checks = 0;
static void check(bool ok, const char *what)
{
    ++checks;
    if (!ok)
    {
        ++failures;
        std::printf("FAIL  %s\n", what);
    }
}

// the menu entry of a set of STRING_SETS by its name
static int set_entry(const char *name)
{
    for (int k = 0; k < NUM_STRING_SETS; ++k)
        if (std::string(STRING_SETS[k].name) == name)
            return k + 1;
    return STRING_SET_AUTOMATIC;
}

static double quality(const models::StringLosses &l, double f, double B)
{
    return 2 * PI * f / (2.0 * l.sigma(f, 1, B));
}

static void tensions()
{
    // D'Addario's published tensions of the 10-46 nickel set (lb) at 25.5", standard tuning, low string first
    const double published_lb[NUM_STRINGS] = {17.5, 19.5, 18.4, 16.6, 15.4, 16.2};
    auto s = rigs::named("pickup");
    s.string_set = set_entry("Nickel 10-46");
    s.scale_mm = 25.5 * 25.4;
    Model m;
    m.solve(s, 48000.0, nullptr, false);
    bool close = true;
    for (int i = 0; i < NUM_STRINGS; ++i)
    {
        const double lb = m.tension[(size_t)i] / 4.448222;
        std::printf("  10-46, string %d: %.1f lb (published %.1f)\n", NUM_STRINGS - i, lb, published_lb[i]);
        close &= std::abs(lb / published_lb[i] - 1.0) < 0.015;
    }
    check(close, "the 10-46 set at 25.5\": D'Addario's published tensions, within 1.5 %");
    // a shorter scale, the same notes: less tension (as the square of the length)
    auto gibson = s;
    gibson.scale_mm = 628.0;
    Model g;
    g.solve(gibson, 48000.0, nullptr, false);
    const double ratio = std::pow(628.0 / s.scale_mm, 2.0);
    check(std::abs(g.tension[0] / m.tension[0] / ratio - 1.0) < 1e-6, "on a 628 mm scale the strings hold less tension");
    check(!Model::same_strings(s, gibson), "another scale: the strings are solved again");
}

static void steel_losses()
{
    // Pate et al.: the G3 of an EXL110 between rigid ends, Q = 2100 at 196 Hz to 3400 near 1 kHz (their Fig. 5)
    auto s = rigs::named("pickup");
    s.string_set = set_entry("Nickel 10-46");
    Model m;
    m.solve(s, 48000.0, nullptr, false);
    const auto &l = m.losses[3];
    const double B = m.place[3][0].B;
    const double q196 = quality(l, 196.0, B), q1000 = quality(l, 1000.0, B);
    std::printf("  plain G3, rigid ends: Q %.0f at 196 Hz, %.0f at 1 kHz (Pate: 2100, 3400)\n", q196, q1000);
    check(q196 > 1800 && q196 < 2400, "a plain steel G3 between rigid ends: Pate's Q at 196 Hz");
    check(q1000 > 3000 && q1000 < 3800, "a plain steel G3 between rigid ends: Pate's Q near 1 kHz");
    // on the solid body (the bridge and the neck take their share): Pate measured 1200 to 2300 over the frets
    const auto &pl = m.place[3][0];
    const double sg = l.sigma(pl.f1, 1, pl.B) + m.body.loss(pl.f1, pl.c, pl.Leff, pl.Zs) + m.body.neck_loss(pl.c, pl.Leff, pl.Zs);
    const double on_body = 2 * PI * pl.f1 / (2.0 * sg);
    std::printf("  plain G3 on the solid body: Q %.0f (Pate: 1200 to 2300)\n", on_body);
    // the middle of their range (the neck's dead spots, lot 4, will spread it)
    check(on_body > 1750 / 1.15 && on_body < 1750 * 1.15, "a plain steel G3 on the solid body: the middle of Pate's Q range");

    // played wound strings lose their highs (dirt between the windings); plain ones do not change
    auto old = s;
    old.string_age = 1.0;
    Model p;
    p.solve(old, 48000.0, nullptr, false);
    const double B0 = m.place[0][0].B;
    check(p.losses[0].sigma(3000.0, 36, B0) > 3.0 * m.losses[0].sigma(3000.0, 36, B0), "a played wound string loses its highs sooner");
    check(std::abs(p.losses[5].sigma(3000.0, 9, m.place[5][0].B) / m.losses[5].sigma(3000.0, 9, m.place[5][0].B) - 1.0) < 1e-6,
          "a played plain string decays as a new one");
}

static void nylon()
{
    // Woodhouse 2012 table I, nylon trebles (Pro Arte hard tension, 650 mm): EI 130, 160, 310 x 1e-6 N m^2
    auto s = rigs::named("acoustic");
    s.string_set = set_entry("Nylon, hard tension");
    s.scale_mm = 650.0;
    Model m;
    m.solve(s, 48000.0, nullptr, false);
    const double measured[3] = {310e-6, 160e-6, 130e-6}; // G3, B3, E4
    bool close = true;
    for (int i = 3; i < NUM_STRINGS; ++i)
    {
        const auto &pl = m.place[(size_t)i][0];
        const double EI = pl.B * pl.T * pl.L * pl.L / (PI * PI);
        std::printf("  nylon string %d: EI %.0f e-6 N m^2 (Woodhouse %.0f), %.1f N\n", NUM_STRINGS - i, EI * 1e6,
                    measured[i - 3] * 1e6, pl.T);
        close &= std::abs(std::log(EI / measured[i - 3])) < std::log(1.4);
    }
    check(close, "nylon trebles: Woodhouse's bending stiffness within 40 %");
    check(m.tension[0] > STRING_SLACK_N && m.tension[0] < 100.0, "a nylon set holds a classical guitar's tensions");
    // nylon damps its highs far more than steel (Woodhouse: eta_B 2e-2 against 1e-2 and less)
    auto e = rigs::named("acoustic");
    e.string_set = set_entry("Phosphor bronze 12-53");
    Model st;
    st.solve(e, 48000.0, nullptr, false);
    const double Bn = m.place[5][0].B, Bs = st.place[5][0].B;
    check(m.losses[5].sigma(4000.0, 12, Bn) > 1.5 * st.losses[5].sigma(4000.0, 12, Bs), "a nylon treble loses its highs sooner than steel");
}

static double pickup_rms(const models::Settings &s, int midi)
{
    OfflineRenderer renderer(s, 48000.0);
    const auto take = renderer.render_live({{0.0, 1.0, midi, 0.8, 1}}, 1.0);
    double e = 0.0;
    for (double x : take.pickup)
        e += x * x;
    return std::sqrt(e / (double)std::max<size_t>(take.pickup.size(), 1));
}

static void pickups()
{
    // Automatic: nickel under a magnetic pickup, bronze otherwise
    check(string_set_index(rigs::named("pickup")) == STRING_SET_ELECTRIC, "Automatic under a magnetic pickup: nickel");
    check(string_set_index(rigs::named("acoustic")) == STRING_SET_ACOUSTIC, "Automatic on an acoustic: bronze");
    check(!Model::same_strings(rigs::named("pickup"), rigs::named("acoustic")), "Automatic follows the sensor: other strings to solve");

    // what a magnetic pickup senses: nickel all of it, a bronze-wound string its core, nylon nothing
    auto s = rigs::named("pickup");
    s.string_set = set_entry("Nickel 10-46");
    const double nickel = pickup_rms(s, 40);
    s.string_set = set_entry("Phosphor bronze 12-53");
    const double bronze = pickup_rms(s, 40);
    s.string_set = set_entry("Nylon, normal tension");
    const double nylon = pickup_rms(s, 40);
    std::printf("  low E at the magnetic pickup: nickel %.3g, bronze %.3g (%.1f dB), nylon %.3g\n", nickel, bronze,
                20 * std::log10(bronze / nickel), nylon);
    check(nickel > 0.0 && bronze < 0.5 * nickel, "a bronze-wound string: the pickup senses its core only (6 dB under nickel and more)");
    check(nylon < 1e-6 * nickel, "a nylon string is silent at a magnetic pickup");
}

int main()
{
    tensions();
    steel_losses();
    nylon();
    pickups();
    std::printf("%d checks, %d failed\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
