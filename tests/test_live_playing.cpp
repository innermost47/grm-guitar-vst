// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// The live player (PlayerFeel and the gesture of the pick): a groove is first what repeats, then what wanders
// slowly, then the gesture's own error. Checked against what was measured on players (Rasanen et al. 2015, Hennig
// et al. 2011): the bar's accents follow its meter, the player's way of each place of the bar comes back bar after
// bar, successive intervals are anticorrelated, the tempo and the energy wander slowly. A chord placed on the roll is
// struck with the same gesture as a strum of the Strums lane; Imperfection brushes and misses strings.
#include <cmath>
#include <cstdio>
#include <numeric>
#include <map>
#include <set>
#include <vector>

#include "../core/performance.hpp"

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

static double correlation(const std::vector<double> &a, const std::vector<double> &b)
{
    const double ma = std::accumulate(a.begin(), a.end(), 0.0) / (double)a.size();
    const double mb = std::accumulate(b.begin(), b.end(), 0.0) / (double)b.size();
    double ab = 0, aa = 0, bb = 0;
    for (size_t i = 0; i < a.size(); ++i)
    {
        ab += (a[i] - ma) * (b[i] - mb);
        aa += (a[i] - ma) * (a[i] - ma);
        bb += (b[i] - mb) * (b[i] - mb);
    }
    return ab / std::sqrt(std::max(aa * bb, 1e-300));
}

static double sd(const std::vector<double> &a)
{
    const double m = std::accumulate(a.begin(), a.end(), 0.0) / (double)a.size();
    double v = 0;
    for (double x : a)
        v += (x - m) * (x - m);
    return std::sqrt(v / (double)a.size());
}

// sixteenths played one after the other by a player, `bars` bars of `beats` beats at 120 bpm
static std::vector<StrokeFeel> play(const Settings &s, int beats, int bars, unsigned seed)
{
    PlayerFeel player(s, beats, seed);
    std::vector<StrokeFeel> out;
    for (int k = 0; k < bars * beats * 4; ++k)
        out.push_back(player.next(k * 0.125, k));
    return out;
}

static void accents()
{
    Settings exact;
    exact.human = false;
    // 4/4: downbeat, third beat, the other beats, the eighths, the sixteenths
    const auto f = play(exact, 4, 1, 1);
    check(f[0].force_dex > f[8].force_dex && f[8].force_dex > f[4].force_dex && f[4].force_dex > f[2].force_dex &&
              f[2].force_dex > f[1].force_dex,
          "4/4: the downbeat strongest, then the half bar, the beats, the eighths, the sixteenths");
    bool exact_time = true;
    for (const auto &x : f)
        exact_time &= x.dt == 0.0 && x.pluck == 0.0 && x.force_dex == x.accent;
    check(exact_time, "without human feel: only the bar's accents, on the grid");
    // 3/4: no half bar, its beats alike; 7/4 grouped 2 + 2 + 3
    const auto w = play(exact, 3, 1, 1);
    check(std::abs(w[4].force_dex - w[8].force_dex) < 1e-12 && w[0].force_dex > w[4].force_dex, "3/4: a downbeat and two beats alike");
    const auto seven = play(exact, 7, 1, 1);
    check(seven[8].force_dex > seven[4].force_dex && seven[16].force_dex > seven[12].force_dex && seven[16].force_dex > seven[20].force_dex,
          "7/4: the groups (2 + 2 + 3) start stronger");
    // Feel depth 0: every stroke alike
    auto flat = exact;
    flat.feel_depth = 0.0;
    const auto z = play(flat, 4, 1, 1);
    check(std::abs(z[0].force_dex - z[1].force_dex) < 1e-12, "Feel depth 0: no accents");
}

static void groove()
{
    Settings s;
    const int beats = 4, per_bar = 16, bars = 400;
    const auto f = play(s, beats, bars, 7);
    // the player's way of each place of the bar comes back bar after bar (Porcaro: force 88-93 %, timing ~50 %), the
    // accents set aside
    std::vector<double> fa, fb, ta, tb;
    for (int bar = 0; bar + 1 < bars; ++bar)
        for (int p = 0; p < per_bar; ++p)
        {
            const auto &x = f[(size_t)(bar * per_bar + p)], &y = f[(size_t)((bar + 1) * per_bar + p)];
            fa.push_back(x.force_dex - x.accent);
            fb.push_back(y.force_dex - y.accent);
            ta.push_back(x.dt);
            tb.push_back(y.dt);
        }
    const double cf = correlation(fa, fb), ct = correlation(ta, tb);
    std::printf("  bar to bar: force %.2f, timing %.2f\n", cf, ct);
    check(cf > 0.5, "the player's force at each place of the bar comes back from bar to bar");
    check(ct > 0.3, "the player's timing at each place of the bar comes back from bar to bar");
    // successive intervals are anticorrelated (a late stroke, then an early one: Porcaro -0.48)
    std::vector<double> ioi;
    for (size_t k = 1; k < f.size(); ++k)
        ioi.push_back(f[k].dt - f[k - 1].dt);
    const std::vector<double> a(ioi.begin(), ioi.end() - 1), b(ioi.begin() + 1, ioi.end());
    const double lag1 = correlation(a, b);
    std::printf("  successive intervals: %.2f; deviations %.1f ms\n", lag1, 1000 * sd(std::vector<double>(ioi.begin(), ioi.end())) / std::sqrt(2.0));
    check(lag1 < -0.2 && lag1 > -0.6, "successive intervals anticorrelated");
    // the slow wandering: the mean over a phrase moves far more than white noise would let it
    std::vector<double> dt, means;
    for (const auto &x : f)
        dt.push_back(x.dt);
    const size_t span = 32;
    for (size_t k = 0; k + span <= dt.size(); k += span)
        means.push_back(std::accumulate(dt.begin() + (long)k, dt.begin() + (long)(k + span), 0.0) / span);
    const double white = sd(dt) / std::sqrt((double)span);
    std::printf("  phrase means wander %.1f ms (white noise: %.1f ms)\n", 1000 * sd(means), 1000 * white);
    check(sd(means) > 2.0 * white, "the tempo wanders slowly (correlated), not as white noise");
    // the same seed, the same player; another seed, another player
    const auto again = play(s, beats, 4, 7), other = play(s, beats, 4, 8);
    check(again[5].dt == f[5].dt && other[5].dt != f[5].dt, "a seed is a player: the same again, another for another seed");
}

// the onsets of the plucks of a compiled song (seconds at 120 bpm), the strings struck
static std::vector<Event> plucks(const Song &song, const Settings &s, unsigned seed = 1)
{
    auto m = std::make_shared<Model>();
    m->solve(s, 48000.0, nullptr, false);
    Compiler c;
    CompileOptions o;
    o.tempo = 120.0;
    o.seed = seed;
    const auto perf = c.compile(song, m, o);
    std::vector<Event> out;
    for (const auto &e : perf->events)
        if (e.type == EventType::pluck && e.pluck.kind == Kind::strike)
            out.push_back(e);
    return out;
}

static double spread_ms(const std::vector<Event> &e)
{
    double lo = 1e9, hi = -1e9;
    for (const auto &x : e)
    {
        lo = std::min(lo, x.beat);
        hi = std::max(hi, x.beat);
    }
    return (hi - lo) * 500.0; // beats at 120 bpm
}

static void roll_chords()
{
    Settings s;
    s.human = false;
    // the same three strings, a chord of the roll and a strum of the Strums lane: the same gesture
    Song roll, lane;
    for (int i = 0; i < 3; ++i)
    {
        RollNote n;
        n.string = i;
        n.fret = i == 0 ? 0 : 2;
        n.start = 1.0;
        n.length = 1.0;
        roll.notes.push_back(n);
    }
    RollStrum st;
    st.start = 1.0;
    st.length = 1.0;
    st.frets = {{0, 2, 2, -1, -1, -1}};
    lane.strums.push_back(st);
    const double a = spread_ms(plucks(roll, s)), b = spread_ms(plucks(lane, s));
    std::printf("  a power chord: %.1f ms across it from the roll, %.1f ms from the Strums lane\n", a, b);
    check(a > 2.0 && std::abs(a / b - 1.0) < 0.25, "a chord of the roll is strummed as a chord of the Strums lane");
    // and the same accidents: a hundred E5 placed on the roll, an imperfect player brushes the D string now and then
    Song many;
    many.bars = 25;
    for (int k = 0; k < 100; ++k)
        for (auto n : roll.notes)
        {
            n.start = 1.0 * k;
            n.length = 0.5;
            many.notes.push_back(n);
        }
    Settings imperfect;
    imperfect.imperfection = 1.0;
    int brushed = 0;
    for (const auto &e : plucks(many, imperfect, 3))
        brushed += e.string == 3;
    std::printf("  100 E5 placed on the roll, imperfect: %d brushes of the D string\n", brushed);
    check(brushed > 10, "a chord of the roll is struck by the same imperfect hand");
}

// Freire et al. (SMC 2018): a 6-note downstroke spreads 8-11 ms from string to string at 64 bpm, 5-12.5 ms at 86 bpm;
// the beat lands on the B and G strings, not on the first string met
static void strums()
{
    Settings s;
    s.human = false;
    Song song;
    RollStrum st;
    st.start = 2.0;
    st.length = 1.0;
    st.frets = {{0, 2, 2, 0, 0, 0}}; // Em
    song.strums.push_back(st);
    for (double tempo : {64.0, 86.0})
    {
        auto m = std::make_shared<Model>();
        m->solve(s, 48000.0, nullptr, false);
        Compiler c;
        CompileOptions o;
        o.tempo = tempo;
        const auto perf = c.compile(song, m, o);
        std::vector<double> t(NUM_STRINGS, 0.0);
        for (const auto &e : perf->events)
            if (e.type == EventType::pluck && e.pluck.kind == Kind::strike)
                t[(size_t)e.string] = e.beat * 60000.0 / tempo; // ms
        const double gap = (t[5] - t[0]) / 5.0, beat = 2.0 * 60000.0 / tempo;
        std::printf("  Em downstroke at %.0f bpm: %.1f ms across it (%.1f ms a string); the beat %.1f ms after the low E, %.1f "
                    "ms before the high E\n",
                    tempo, t[5] - t[0], gap, beat - t[0], t[5] - beat);
        const double lo = tempo < 70 ? 8.0 : 5.0, hi = tempo < 70 ? 11.0 : 12.5;
        check(gap > lo && gap < hi, "a strum spreads across the strings as Freire's players' do");
        check(t[0] < beat - 1.0 && t[5] > beat && std::abs(0.5 * (t[3] + t[4]) - beat) < gap,
              "the beat lands on the B and G strings: the downstroke starts before it");
    }
}

static void imperfection()
{
    // a hundred E5 chords strummed down: an imperfect player brushes the next string and misses the last one now
    // and then, a perfect one never
    Song song;
    song.bars = 25;
    for (int k = 0; k < 100; ++k)
    {
        RollStrum st;
        st.start = 1.0 * k;
        st.length = 0.5;
        st.frets = {{0, 2, 2, -1, -1, -1}};
        song.strums.push_back(st);
    }
    auto count = [&](double amount)
    {
        Settings s;
        s.imperfection = amount;
        std::set<int> brushed;
        int strikes = 0;
        for (const auto &e : plucks(song, s, 3))
        {
            ++strikes;
            if (e.string == 3)
                brushed.insert((int)std::floor(e.beat + 0.5));
        }
        return std::make_pair(strikes, (int)brushed.size());
    };
    const auto none = count(0.0), full = count(1.0);
    std::printf("  100 E5 chords: %d strikes, %d brushes (perfect); %d strikes, %d brushes (imperfect)\n", none.first, none.second,
                full.first, full.second);
    check(none.first == 300 && none.second == 0, "Imperfection 0: every string struck, none brushed");
    check(full.second > 15 && full.second < 60, "Imperfection 1: the next string brushed now and then");
    check(full.first - full.second < 300, "Imperfection 1: a last string missed now and then");
    // an accident chokes a string, it never silences a whole stroke (Anthony, 25/11/2026: a silent beat in bar 3 of a
    // palm-muted drop D riff, a whole stroke muted at once)
    auto m = std::make_shared<Model>();
    Settings imperfect;
    imperfect.imperfection = 1.0;
    m->solve(imperfect, 48000.0, nullptr, false);
    Compiler c;
    CompileOptions o;
    o.tempo = 120.0;
    o.seed = 3;
    const auto perf = c.compile(song, m, o);
    std::map<long, std::pair<int, int>> chords; // by chord: strings struck, strings choked at once
    for (const auto &e : perf->events)
    {
        const long chord = std::lround(e.beat);
        if (e.type == EventType::pluck && e.pluck.kind == Kind::strike && e.string < 3)
            ++chords[chord].first;
        if (e.type == EventType::damp && e.extra >= RELEASE_DEAD && e.string < 3)
            ++chords[chord].second;
    }
    int choked = 0;
    bool always_sounds = true;
    for (const auto &[beat, n] : chords)
    {
        choked += n.second > 0;
        always_sounds &= n.second < n.first;
    }
    std::printf("  100 E5 chords, imperfect: %d with a choked string\n", choked);
    check(choked > 0 && always_sounds, "Imperfection: a string chokes now and then, a whole stroke never");
    Settings exact;
    exact.human = false;
    exact.imperfection = 1.0;
    const auto machine = plucks(song, exact, 3);
    check(machine.size() == 300, "without human feel, nothing goes wrong");
}

int main()
{
    accents();
    groove();
    roll_chords();
    strums();
    imperfection();
    std::printf("%d checks, %d failed\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
