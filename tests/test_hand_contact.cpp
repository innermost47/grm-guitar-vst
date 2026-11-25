// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// A hand on a ringing string (Anthony, 25/11/2026: the end of a palm mute and the Hand mute sounded like two samples
// one after the other). A hand lands in a few milliseconds, not at once: the level glides down instead of stepping;
// a palm that lands moves the string's modes to those of the string under it (lower: the palm's thud); a palm that
// leaves lets them ring free again; palm-muted strokes one after the other keep the palm down between them.
#include <cmath>
#include <cstdio>
#include <vector>

#include "../core/performance.hpp"
#include "../core/voice.hpp"
#include "support/rigs.hpp"

using namespace grmg;
using namespace grmg::models;

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

static const double SR = 48000.0;

static double rms(const std::vector<double> &y, double a, double b)
{
    double e = 0.0;
    const size_t i0 = (size_t)(a * SR), i1 = std::min(y.size(), (size_t)(b * SR));
    for (size_t i = i0; i < i1; ++i)
        e += y[i] * y[i];
    return std::sqrt(e / (double)std::max<size_t>(i1 - i0, 1));
}

// the amplitude of the partial near f from a to b seconds (Goertzel, Hann window)
static double partial(const std::vector<double> &y, double a, double b, double f)
{
    const size_t i0 = (size_t)(a * SR), i1 = std::min(y.size(), (size_t)(b * SR));
    const double w = 2 * PI * f / SR;
    double re = 0.0, im = 0.0;
    for (size_t i = i0; i < i1; ++i)
    {
        const double h = 0.5 - 0.5 * std::cos(2 * PI * (double)(i - i0) / (double)(i1 - i0));
        re += h * y[i] * std::cos(w * (double)i);
        im += h * y[i] * std::sin(w * (double)i);
    }
    return std::hypot(re, im);
}

// the open low string struck, then `touch` at 0.5 s; its bridge force over 1 s
template <class Touch>
static std::vector<double> ring(const Model &m, const Palm *palm, Touch touch)
{
    Pluck p;
    p.string = 0;
    p.place = m.place[0][0];
    p.gain = 1.0;
    p.tau = 1e-4;
    p.palm = palm;
    StringVoice v;
    v.start(p, m.body, m.losses[0], m.s.brightness, SR);
    std::vector<double> y((size_t)SR, 0.0);
    const size_t half = (size_t)(0.5 * SR);
    v.render(y.data(), (int)half);
    touch(v);
    v.render(y.data() + half, (int)(y.size() - half));
    return y;
}

int main()
{
    auto s = rigs::named("acoustic");
    Model m;
    m.solve(s, SR, nullptr, true);
    const Palm *palm = m.palm[0][0].get();
    const double f1 = m.place[0][0].f1;

    // a hand lands: the level glides down over its landing, it does not step
    const auto free = ring(m, nullptr, [](StringVoice &) {});
    const auto hand = ring(m, nullptr, [](StringVoice &v) { v.damp(RELEASE_HAND); });
    const double first_ms = rms(hand, 0.5, 0.502) / rms(free, 0.5, 0.502), later = rms(hand, 0.56, 0.6) / rms(free, 0.56, 0.6);
    std::printf("  a flat hand lands: %.2f of the free string in its first 2 ms, %.2f 60 ms later\n", first_ms, later);
    check(first_ms > 0.97 && later < 0.5, "a hand lands over a few milliseconds: the level glides down, no step");

    // a palm lands: the string's modes become those under the palm, whose skin takes the middle partials (its 7th
    // here) and leaves the bass ringing: far more than a flat hand pressing as hard
    const auto palmed = ring(m, nullptr, [&](StringVoice &v) { v.damp(PALM_LAND_DAMP, palm); });
    const auto flat = ring(m, nullptr, [](StringVoice &v) { v.damp(PALM_LAND_DAMP); });
    auto bass_left = [&](const std::vector<double> &y)
    { // the fundamental against the 7th partial, after the landing, against before it
        const double after = partial(y, 0.55, 0.65, f1) / partial(y, 0.55, 0.65, 7 * f1);
        const double before = partial(y, 0.38, 0.48, f1) / partial(y, 0.38, 0.48, 7 * f1);
        return after / before;
    };
    const double under_palm = bass_left(palmed), under_hand = bass_left(flat);
    std::printf("  the palm lands on the low E: the bass against the 7th partial x%.1f (a flat hand: x%.1f)\n", under_palm, under_hand);
    check(under_palm > 3.0 * under_hand, "a palm that lands keeps the bass and takes the middle partials (its thud)");

    // a palm leaves: a palm-muted note rings free again
    const auto muted = ring(m, palm, [](StringVoice &) {});
    const auto opened = ring(m, palm, [](StringVoice &v) { v.lift(); });
    const double kept = rms(muted, 0.7, 0.8), freed = rms(opened, 0.7, 0.8);
    std::printf("  a palm-muted note 200 ms after the palm leaves: %.3g kept down, %.3g lifted\n", kept, freed);
    check(freed > 2.0 * kept, "the palm leaves: the note opens up again");

    // palm-muted strokes one after the other: the palm stays down between them (one span, one lift)
    Song song;
    for (int k = 0; k < 8; ++k)
    {
        RollNote n;
        n.string = 0;
        n.start = 0.5 * k;
        n.length = 0.5;
        n.palm = true;
        song.notes.push_back(n);
    }
    auto model = std::make_shared<Model>(m);
    Compiler c;
    const auto perf = c.compile(song, model, {});
    int on = 0, off = 0;
    for (const auto &e : perf->events)
    {
        on += e.type == EventType::palm_on;
        off += e.type == EventType::palm_off;
    }
    std::printf("  eight palm-muted eighths: %d palm down, %d palm up\n", on, off);
    check(on == 1 && off == 1, "palm-muted strokes in a row: the palm stays down between them");

    std::printf("%d checks, %d failed\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
