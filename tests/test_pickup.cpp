// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// The magnetic pickup (MagneticPickup, pickup_sensing_network, the strings heard through it): Novak et al.'s flux
// curve and its slope, the window of the poles, the coils of a humbucker and the partials they cancel, the plane
// along the top heard at twice its frequency, the small-signal limit, the distortion close to the poles.
#include <cmath>
#include <cstdio>
#include <vector>

#include "../core/offline_renderer.hpp"
#include "support/rigs.hpp"
#include "timbre/partial_tracker.hpp"

using namespace grmg;

static int failures = 0, checks = 0;
static void ok(const char *name, bool pass)
{
    ++checks;
    if (!pass)
    {
        ++failures;
        std::printf("FAIL %s\n", name);
    }
}

static const double SR = 48000.0;
static const models::PickupType &SINGLE = PICKUP_TYPES[0];
static const models::PickupType &HUMBUCKER = PICKUP_TYPES[1];

// the level (amplitude) of the component at f in y (Goertzel over the whole buffer)
static double tone_at(const std::vector<double> &y, double f)
{
    double re = 0.0, im = 0.0;
    for (size_t i = 0; i < y.size(); ++i)
    {
        const double a = 2 * PI * f * (double)i / SR;
        re += y[i] * std::cos(a);
        im += y[i] * std::sin(a);
    }
    return 2.0 * std::hypot(re, im) / (double)y.size();
}

// a coil's voltage while the string swings sinusoidally (amplitude in m, at f Hz) towards the poles or along the top
static std::vector<double> swing(const MagneticPickup &p, double amplitude, double f, bool along)
{
    std::vector<double> y((size_t)SR); // one second: whole periods for 80 Hz and its harmonics
    for (size_t i = 0; i < y.size(); ++i)
    {
        const double w = 2 * PI * f, t = (double)i / SR;
        const double d = amplitude * std::sin(w * t), v = amplitude * w * std::cos(w * t);
        y[i] = along ? p.voltage(0.0, 0.0, d, v) : p.voltage(d, v, 0.0, 0.0);
    }
    return y;
}

// the harmonics (2 to 10) of a swing against its fundamental
static double distortion(const std::vector<double> &y, double f)
{
    double h = 0.0;
    for (int n = 2; n <= 10; ++n)
        h += std::pow(tone_at(y, n * f), 2);
    return std::sqrt(h) / tone_at(y, f);
}

static void flux_curve()
{
    // Eq. 9 is the slope of Eq. 6, and the flux falls as the string moves away (Fig. 9)
    for (const auto *t : {&SINGLE, &HUMBUCKER})
        for (double x : {1.0, 3.0, 5.0, 10.0})
        {
            const double h = 1e-5;
            const double numeric = (MagneticPickup::flux(x + h, t->nl_A, t->nl_Leq_mm, t->nl_req_mm) -
                                    MagneticPickup::flux(x - h, t->nl_A, t->nl_Leq_mm, t->nl_req_mm)) / (2 * h);
            const double slope = MagneticPickup::slope(x, t->nl_A, t->nl_Leq_mm, t->nl_req_mm);
            ok("the slope is the flux curve's derivative", std::abs(slope - numeric) < 1e-6 * std::abs(numeric) + 1e-12);
            ok("the flux falls as the string moves away", slope < 0.0);
        }
    // close to the poles the humbucker is far steeper than the single coil; farther they come together (Fig. 9)
    auto ratio = [](double x)
    { return MagneticPickup::slope(x, HUMBUCKER.nl_A, HUMBUCKER.nl_Leq_mm, HUMBUCKER.nl_req_mm) /
             MagneticPickup::slope(x, SINGLE.nl_A, SINGLE.nl_Leq_mm, SINGLE.nl_req_mm); };
    ok("a humbucker is hotter close to the strings", ratio(3.0) > 1.3);
    // Novak et al.: "when the string is placed at the distance of 5 mm, the slopes are smaller and similar"
    ok("pickups come together as the string moves away from 2 to 5 mm", ratio(5.0) < ratio(2.0) - 0.2);
}

static void window()
{
    const double a = 0.004;
    ok("a pole hears a long wave whole", std::abs(MagneticPickup::window(0.0, a) - 1.0) < 1e-12);
    ok("the window at ka = 1", std::abs(MagneticPickup::window(1.0 / a, a) - std::exp(-1.0) * (1.0 + 1.0 + 1.0 / 3.0)) < 1e-12);
    bool falls = true;
    for (double k = 10.0; k < 3000.0; k += 10.0)
        falls &= MagneticPickup::window(k, a) < MagneticPickup::window(k - 10.0, a);
    ok("a pole hears short waves less and less", falls);
}

static void sensing()
{
    const double centre = 0.06;
    const auto single = pickup_sensing_network(0, centre, PICKUP_HEIGHT_REF_MM);
    ok("a single coil: one coil at its place", single.coils == 1 && std::abs(single.coil_x[0] - centre) < 1e-6);
    const double r = 1000.0 * SINGLE.pole_radius_m, h = PICKUP_HEIGHT_REF_MM;
    ok("its width: the height and the pole's spread", std::abs(single.window_m - std::sqrt(h * h + 0.5 * r * r) / 1000.0) < 1e-6);
    ok("a single coil at the reference height is the unit", std::abs(single.gain - 1.0) < 1e-6);
    const auto hb = pickup_sensing_network(1, centre, PICKUP_HEIGHT_REF_MM);
    ok("a humbucker: two coils around its centre", hb.coils == 2 && std::abs(hb.coil_x[0] - (centre - 0.5 * HUMBUCKER.coil_spacing_m)) < 1e-6 &&
                                                         std::abs(hb.coil_x[1] - (centre + 0.5 * HUMBUCKER.coil_spacing_m)) < 1e-6);
    ok("a humbucker is hotter than a single coil", hb.gain > 1.3);
    const auto split = pickup_sensing_network(1, centre, PICKUP_HEIGHT_REF_MM, true);
    ok("a split humbucker: its coil on the neck side", split.coils == 1 && std::abs(split.coil_x[0] - hb.coil_x[1]) < 1e-6);
    const auto low = pickup_sensing_network(0, centre, 6.0);
    ok("a pickup lowered away from the strings is quieter", low.gain < 0.6 && low.window_m > single.window_m);
}

static void voltage()
{
    const MagneticPickup single(pickup_sensing_network(0, 0.06, PICKUP_HEIGHT_REF_MM));
    const MagneticPickup hb(pickup_sensing_network(1, 0.06, PICKUP_HEIGHT_REF_MM));
    // the table the audio loop reads the slope from: within a thousandth of the exact slope, from the closest to far
    double worst = 0.0;
    for (const auto *p : {&single, &hb})
        for (double r = PICKUP_HEIGHT_MIN_MM; r < PICKUP_HEIGHT_MIN_MM + PICKUP_SLOPE_SPAN_MM + 4.0; r += 0.013)
            worst = std::max(worst, std::abs(p->relative_slope(r) / (p->slope(r) / p->slope(p->sensing().height_mm)) - 1.0));
    std::printf("  tabulated slope: worst relative error %.2e\n", worst);
    ok("the tabulated slope is the exact one", worst < 1e-3);
    // a tiny motion: the voltage is the speed times the gain (the engine's unit for a single coil)
    ok("small signals: a single coil gives the string's speed", std::abs(single.voltage(1e-7, 0.01, 0.0, 0.0) - 0.01) < 1e-6);
    ok("small signals: a humbucker coil, its gain", std::abs(hb.voltage(1e-7, 0.01, 0.0, 0.0) - 0.01 * hb.sensing().gain) < 1e-6 * hb.sensing().gain);
    // 2 mm at 80 Hz, as Novak et al. drove an E string (Fig. 5): the humbucker distorts more, lowered both distort less
    const double d_single = distortion(swing(single, 0.002, 80.0, false), 80.0);
    const double d_hb = distortion(swing(hb, 0.002, 80.0, false), 80.0);
    std::printf("  2 mm at 80 Hz, 3 mm from the poles: single coil %.1f %%, humbucker %.1f %%\n", 100 * d_single, 100 * d_hb);
    ok("a swing towards the poles is heard distorted", d_single > 0.05);
    ok("a humbucker distorts more than a single coil", d_hb > d_single);
    const MagneticPickup lowered(pickup_sensing_network(1, 0.06, 5.0));
    ok("lowered, a pickup distorts less", distortion(swing(lowered, 0.002, 80.0, false), 80.0) < d_hb);
    // along the top: heard at twice the frequency, far under the same swing towards the poles (Horton & Moore)
    const auto along = swing(single, 0.001, 80.0, true), toward = swing(single, 0.001, 80.0, false);
    std::printf("  1 mm along the top: fundamental %.1f dB, second %.1f dB under the same swing towards the poles\n",
                20 * std::log10(tone_at(along, 80.0) / tone_at(toward, 80.0) + 1e-30),
                20 * std::log10(tone_at(along, 160.0) / tone_at(toward, 80.0) + 1e-30));
    ok("along the top: no fundamental", tone_at(along, 80.0) < 1e-6 * tone_at(toward, 80.0));
    // Paté et al. 2014: the polarisation along the top gives at least ten times less energy (amplitude under 0.32)
    ok("along the top: heard at twice its frequency, far weaker", tone_at(along, 160.0) > 0.0 && tone_at(along, 160.0) < 0.32 * tone_at(toward, 80.0));
}

// the low E string under a pickup, the mean level (dB) of each partial asked, over its first half second
static std::vector<double> partial_levels(int pickup_type, const std::vector<int> &partials, double force,
                                          const models::Voicing &voicing = models::Voicing())
{
    auto s = rigs::named("pickup");
    s.force = force;
    s.pickup_type = pickup_type;
    s.pickup_mm = 60.0;
    s.human = false;
    OfflineRenderer renderer(s, SR);
    Song song;
    song.bars = 1;
    song.tempo = 120.0;
    models::RollNote note;
    note.string = 0;
    note.length = 2.0;
    song.notes.push_back(note);
    const auto take = renderer.render_song(song, 7, 0.0, voicing);
    const auto &pl = renderer.model().place[0][0];
    timbre::PartialTracker tracker(SR);
    auto level = [&](int k)
    {
        tracker.track(take.pickup, k * pl.f1 * std::sqrt(1.0 + pl.B * k * k), pl.f1, 0.05, 0.5);
        double sum = 0.0;
        for (double a : tracker.amplitudes())
            sum += a;
        return sum / (double)std::max<size_t>(tracker.amplitudes().size(), 1);
    };
    std::vector<double> db;
    for (int k : partials)
        db.push_back(20.0 * std::log10(level(k) + 1e-30));
    return db;
}

// partial n's level against partial m's
static double partial_drop(int pickup_type, int n, int m, double force)
{
    const auto db = partial_levels(pickup_type, {n, m}, force);
    return db[0] - db[1];
}

static void strings_heard()
{
    // a humbucker's coils 18 mm apart cancel the low E's partials whose half wave fits between them: n = L / d = 36.
    // Struck softly: struck hard, the pickup's own distortion fills the hole with its neighbours' products (physical)
    const int null = (int)std::lround(SCALE_DEFAULT_M / HUMBUCKER.coil_spacing_m);
    const double single = partial_drop(0, null, null / 2, 0.1), hb = partial_drop(1, null, null / 2, 0.1);
    std::printf("  low E, partial %d against %d: single coil %.1f dB, humbucker %.1f dB\n", null, null / 2, single, hb);
    ok("a humbucker cancels the partials its coils straddle", hb < single - 20.0);
    // the plane along the top alone (an attack angle of 0): the pickup hears it only through the distance it adds,
    // far weaker than the same pluck towards the poles (Paté et al. 2014: at least ten times less energy). Its partials
    // still come out (their sums and differences: n + 1 less n is the fundamental), not only twice the frequency.
    models::Voicing flat, straight;
    flat.angle_down = flat.angle_up = 0.0;
    straight.angle_down = straight.angle_up = 90.0;
    const auto along = partial_levels(0, {1, 2, 3}, 2.0, flat), toward = partial_levels(0, {1, 2, 3}, 2.0, straight);
    double gap = -1e9;
    for (size_t k = 0; k < along.size(); ++k)
        gap = std::max(gap, along[k] - toward[k]);
    std::printf("  low E plucked along the top only: partials 1 to 3 at most %.1f dB against the same pluck towards the pickup\n", gap);
    ok("the pickup hears little of the plane along the top", gap < -10.0);
}

int main()
{
    flux_curve();
    window();
    sensing();
    voltage();
    strings_heard();
    std::printf("%d checks, %d failed\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
