// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// The Feel lanes of a pattern (force, timing, palm): written in notches, played as a smooth curve through them; a
// pattern without lanes plays its Feel preset (Natural: the bar's accents as before the lanes); the lanes move the
// strokes' force and time and lay a light palm where no note asks a palm mute, less where the bar hits harder (Palm
// coupling); they are saved with the pattern and follow it when it is cut into sections; the meter's groups set the
// accents of odd bars.
#include <cmath>
#include <cstdio>
#include <random>

#include "../core/feel_generator.hpp"
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

static void curve()
{
    // through every step, never past its neighbours between them (no overshoot), smooth
    std::mt19937 rng(5);
    std::uniform_real_distribution<double> u(-1.0, 1.0);
    FeelLanes l;
    for (int k = 0; k < 64; ++k)
    {
        l.force.push_back(std::round(u(rng) * 8.0) / 8.0);
        l.timing.push_back(0.0);
        l.palm.push_back(0.0);
    }
    const FeelCurve c(l, FEEL_NATURAL, Meter(4), 64);
    bool through = true, bounded = true;
    for (int k = 0; k < 64; ++k)
    {
        through &= std::abs(c.force(0.25 * k) - l.force[(size_t)k]) < 1e-12;
        const double lo = std::min(l.force[(size_t)k], l.force[(size_t)((k + 1) % 64)]);
        const double hi = std::max(l.force[(size_t)k], l.force[(size_t)((k + 1) % 64)]);
        for (double f : {0.1, 0.3, 0.5, 0.7, 0.9})
        {
            const double v = c.force(0.25 * (k + f));
            bounded &= v >= lo - 1e-9 && v <= hi + 1e-9;
        }
    }
    check(through, "the curve goes through every step of the lane");
    check(bounded, "between two steps the curve stays between them (no overshoot)");
    // no lanes: the preset's; Natural gives the downbeat the strongest step, Flat none
    const FeelCurve natural({}, FEEL_NATURAL, Meter(4), 16), flat({}, FEEL_FLAT, Meter(4), 16), back({}, FEEL_BACKBEAT, Meter(4), 16);
    check(natural.force(0.0) > natural.force(1.0) && natural.force(1.0) > natural.force(0.25), "Natural: the downbeat, the beats, the sixteenths");
    check(std::abs(flat.force(0.0)) < 1e-12 && std::abs(flat.force(1.25)) < 1e-12, "Flat: every place alike");
    check(back.force(1.0) > back.force(0.0) && back.force(3.0) > back.force(2.0), "Backbeat: 2 and 4 hit hardest");
    // the meter's groups: 7 = 2 + 2 + 3, or 3 + 4 when written so
    const Meter seven(7), written(7, {3, 4});
    check(seven.level(8) == 1 && seven.level(16) == 1 && seven.level(12) == 2, "7/4: the groups start on beats 3 and 5");
    check(written.level(12) == 1 && written.level(8) == 2, "7/4 written 3 + 4: the group starts on beat 4");
}

// a pattern of quarter-note D5 power chords (strums), 2 bars
static Song quarters()
{
    Song song;
    song.bars = 2;
    for (int k = 0; k < 8; ++k)
    {
        RollStrum st;
        st.start = k;
        st.length = 0.9;
        st.frets = {{0, 0, 0, -1, -1, -1}};
        song.strums.push_back(st);
    }
    return song;
}

// the strikes of a compiled song: beat, force, palm network or not
struct Heard
{
    double beat, gain;
    bool palm;
};
static std::vector<Heard> strikes(const Song &song, const Settings &s)
{
    auto m = std::make_shared<Model>();
    m->solve(s, 48000.0, nullptr, true);
    Compiler c;
    CompileOptions o;
    o.tempo = 120.0;
    const auto perf = c.compile(song, m, o);
    std::vector<Heard> out;
    for (const auto &e : perf->events)
        if (e.type == EventType::pluck && e.pluck.kind == Kind::strike && e.string == 0)
            out.push_back({e.beat, e.pluck.gain, e.pluck.palm != nullptr});
    return out;
}

static void lanes_play()
{
    Settings exact;
    exact.human = false;
    // written lanes: the third beat hits hardest, the second comes late, the fourth under a light palm
    Song song = quarters();
    song.feel = FeelCurve::preset_lanes(FEEL_FLAT, song.meter(), song.steps());
    song.feel.force[8] = 1.0;
    song.feel.timing[4] = 1.0;
    song.feel.palm[12] = 1.0;
    const auto st = strikes(song, exact);
    std::printf("  lanes: forces %.3f %.3f %.3f %.3f; beat 2 at %.4f; palms %d %d %d %d\n", st[0].gain, st[1].gain, st[2].gain, st[3].gain,
                st[1].beat, st[0].palm, st[1].palm, st[2].palm, st[3].palm);
    check(st[2].gain > 1.2 * st[0].gain && std::abs(st[3].gain / st[0].gain - 1.0) < 0.05, "a force step: that stroke hits harder");
    const double late_ms = (st[1].beat - st[5].beat + 4.0) * 500.0; // against the same beat of the next bar (no lane)
    std::printf("  the timing step: %.1f ms behind\n", late_ms);
    check(std::abs(late_ms - 1000.0 * FEEL_LANE_TIMING_S) < 3.0, "a timing step of 1: the stroke comes FEEL_LANE_TIMING_S late");
    check(st[3].palm && !st[0].palm && !st[2].palm, "a palm step: a light palm on that stroke, none elsewhere");
    // Palm coupling: no palm written, the softer places get a light palm, the downbeat none
    Song natural = quarters();
    auto coupled = exact;
    coupled.palm_coupling = 1.0;
    const auto nc = strikes(natural, coupled);
    check(!nc[0].palm && nc[1].palm, "Palm coupling: the harder the bar hits, the less palm");
    // the note's Palm mute stays its own (the full palm), whatever the lane
    Song muted = song;
    muted.strums[3].palm = true;
    check(strikes(muted, exact)[3].palm, "a note's Palm mute is kept under the lanes");
}

static void saved()
{
    Song song = quarters();
    song.beats_per_bar = 7;
    song.grouping = {3, 4};
    song.feel_preset = FEEL_GALLOP;
    song.feel = FeelCurve::preset_lanes(FEEL_GALLOP, song.meter(), song.steps());
    song.feel.timing[3] = -0.5;
    song.feel.palm[5] = 0.75;
    Song back;
    check(back.from_text(song.to_text()), "a pattern with lanes reads back");
    check(back.grouping == song.grouping && back.feel_preset == FEEL_GALLOP && back.feel.timing == song.feel.timing &&
              back.feel.palm == song.feel.palm && back.feel.force.size() == song.feel.force.size(),
          "its groups, Feel preset and lanes are kept");
    Song old;
    check(old.from_text("grm-guitar-roll 1\nbars 4 4 90\n") && old.feel.empty() && old.feel_preset == FEEL_NATURAL,
          "an older pattern: no lanes, the Natural preset");
    // a section keeps its own part of the lanes
    Song four = quarters();
    four.bars = 4;
    four.feel = FeelCurve::preset_lanes(FEEL_FLAT, four.meter(), four.steps());
    four.feel.force[16 + 2] = 0.875;
    const Song part = four.section(4.0, 8.0);
    check(part.steps() == 16 && std::abs(part.feel.force[2] - 0.875) < 1e-12, "a section keeps the lanes of its bars");
}

static void generators()
{
    Song song = quarters();
    song.bars = 8;
    // clone: the first bar over all the others
    FeelGenerator::ensure_lanes(song);
    song.feel.force[1] = 0.5;
    song.feel.palm[6] = 0.25;
    FeelGenerator::clone(song, 0, 16);
    bool cloned = true;
    for (int bar = 1; bar < 8; ++bar)
        cloned &= song.feel.force[(size_t)(bar * 16 + 1)] == 0.5 && song.feel.palm[(size_t)(bar * 16 + 6)] == 0.25;
    check(cloned, "clone: a bar copied over the bars after it");
    // vary: the strong places kept, some weak ones moved, all on notches
    const auto before = song.feel;
    FeelGenerator::vary(song, 16, 16, 9);
    const Meter meter = song.meter();
    bool strong_kept = true, on_notches = true;
    int moved = 0;
    for (int s = 16; s < 32; ++s)
    {
        if (meter.level(s) <= 1 && s < 28)
            strong_kept &= song.feel.force[(size_t)s] == before.force[(size_t)s];
        moved += song.feel.force[(size_t)s] != before.force[(size_t)s];
        const double v = song.feel.force[(size_t)s];
        on_notches &= std::abs(v - FeelGenerator::notch(v)) < 1e-12;
    }
    check(strong_kept && moved >= 3 && on_notches, "vary: the downbeat and the groups' starts kept, weak places moved, on notches");
    // groove: musical and the same for the same seed
    Song a = quarters(), b = quarters(), c = quarters();
    a.bars = b.bars = c.bars = 8;
    FeelGenerator::groove(a, 0, a.steps(), 21);
    FeelGenerator::groove(b, 0, b.steps(), 21);
    FeelGenerator::groove(c, 0, c.steps(), 22);
    check(a.feel.force == b.feel.force && a.feel.palm == b.feel.palm && (a.feel.force != c.feel.force || a.feel.palm != c.feel.palm),
          "groove: a seed is a feel (the same again, another for another seed)");
    bool downbeats = true;
    for (int bar = 0; bar < 8; bar += 2)
        for (int s = 1; s < 16; ++s)
            downbeats &= a.feel.force[(size_t)(bar * 16)] >= a.feel.force[(size_t)(bar * 16 + s)];
    check(downbeats, "groove: the downbeat stays the strongest place of its bar");
}

// another preset keeps what the user wrote in the Timing and Palm lanes (Anthony, 25/11/2026: choosing a new Feel
// erased them) and draws the Force lane from the preset
static void preset_change()
{
    Song song = quarters();
    FeelGenerator::ensure_lanes(song);
    song.feel.timing[3] = 0.5;
    song.feel.palm[5] = 0.75;
    song.feel.force[2] = 1.0;
    FeelGenerator::apply_preset(song, FEEL_BACKBEAT);
    check(song.feel_preset == FEEL_BACKBEAT, "another preset: chosen");
    const bool kept = song.feel.timing.size() > 5 && song.feel.palm.size() > 5;
    check(kept && song.feel.timing[3] == 0.5 && song.feel.palm[5] == 0.75, "another preset: the Timing and Palm lanes kept");
    const auto backbeat = FeelCurve::preset_lanes(FEEL_BACKBEAT, song.meter(), song.steps());
    bool drawn = song.feel.force.size() == backbeat.force.size();
    for (size_t s = 0; drawn && s < song.feel.force.size(); ++s)
        drawn &= song.feel.force[s] == FeelGenerator::notch(backbeat.force[s]);
    check(drawn, "another preset: the Force lane drawn from it, on notches");
    // a pattern with no lanes written keeps following the preset itself
    Song fresh = quarters();
    FeelGenerator::apply_preset(fresh, FEEL_GALLOP);
    check(fresh.feel.empty() && fresh.feel_preset == FEEL_GALLOP, "another preset: unwritten lanes follow it");
}

// the lanes are written past the pattern's end as notes are (Anthony, 25/11/2026: nothing pasted past the loop):
// kept there, not played until the pattern reaches them
static void past_the_end()
{
    Song song = quarters(); // 2 bars of 4/4: 32 steps
    FeelGenerator::ensure_lanes(song);
    const int end = song.steps();
    FeelGenerator::reach(song, end + 8);
    song.feel.force[(size_t)end + 4] = 0.875;
    song.feel.palm[(size_t)end + 4] = 0.5;
    check((int)song.feel.force.size() == end + 8 && (int)song.feel.palm.size() == end + 8,
          "past the end: the lanes reach the step written");
    FeelGenerator::ensure_lanes(song);
    check((int)song.feel.force.size() == end + 8 && song.feel.force[(size_t)end + 4] == 0.875,
          "past the end: what is written there is kept");
    const auto curve = song.feel_curve();
    check(std::abs(curve.force(0.25 * (end + 4)) - song.feel.force[4]) < 1e-12,
          "past the end: the pattern still repeats every bar it has");
    song.bars = 3;
    FeelGenerator::ensure_lanes(song);
    check(std::abs(song.feel_curve().force(0.25 * (end + 4)) - 0.875) < 1e-12,
          "the pattern grown: it plays what was written there");
    FeelGenerator::apply_preset(song, FEEL_FLAT);
    check(song.feel.palm[(size_t)end + 4] == 0.5, "a preset past the end: the Palm lane kept");
}

int main()
{
    curve();
    lanes_play();
    saved();
    generators();
    preset_change();
    past_the_end();
    std::printf("%d checks, %d failed\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
