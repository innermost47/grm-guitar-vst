// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// How the guitar is played (no Python reference here: behaviours): the palm lifted, the song in a loop, notes
// that end, the live palm; the hands' limits, the sweep of the pick, the pinch, the muscle, the palm's band.
#include <cmath>
#include <cstdio>
#include <set>

#include "../core/engine.hpp"
#include "../core/chords.hpp"
#include "../core/playability.hpp"

using namespace grmg;
using namespace grmg::models;
static int checks = 0, failures = 0;
static void ok(const char* name, bool cond) {
    ++checks;
    if (!cond) {
        ++failures;
        std::printf("FAIL %s\n", name);
    }
}

static double rms(const std::vector<float>& y, double sr, double a, double b) {
    double e = 0;
    const size_t i0 = (size_t) (a * sr), i1 = std::min(y.size(), (size_t) (b * sr));
    for (size_t i = i0; i < i1; ++i) e += (double) y[i] * y[i];
    return std::sqrt(e / std::max<size_t>(1, i1 - i0));
}

int main() {
    const double sr = 48000;
    auto model = std::make_shared<Model>();
    model->solve(Settings{}, sr, nullptr, true);

    // ---- the palm lifted: pressure 0 solves no palm and leaves palm-muted notes free
    {
        Settings s;
        s.palm_intensity = 0.0;
        auto m = std::make_shared<Model>();
        m->solve(s, sr, nullptr, true);
        ok("palm lifted: no palm networks", !m->palm_ready);
        Song song;
        song.notes.push_back({1, 3, 0.0, 1.0, 0.8, true});
        Compiler c;
        auto perf = c.compile(song, m, {});
        bool free = true;
        for (const auto& e : perf->events)
            if (e.type == EventType::pluck && e.pluck.palm) free = false;
        ok("palm lifted: palm-muted notes ring free", free);
    }

    // ---- a quick solve may borrow the old palm, but the full solve must refresh it
    {
        auto checkRefresh = [&](const char* name, const Settings& settings) {
            Model quick, full, fresh;
            quick.solve(settings, sr, model.get(), false);
            full.solve(settings, sr, &quick, true);
            fresh.solve(settings, sr, nullptr, true);

            bool matches = full.palm_ready && fresh.palm_ready;
            for (int string = 0; string < NUM_STRINGS; ++string) {
                for (int fret = 0; fret < NUM_FRETS; ++fret) {
                    const auto& staged = full.palm[(size_t) string][(size_t) fret];
                    const auto& direct = fresh.palm[(size_t) string][(size_t) fret];
                    matches = matches && staged && direct && staged->sigma == direct->sigma && staged->ratio == direct->ratio;
                }
            }
            ok(name, matches);
        };

        auto position = model->s;
        position.palm_mm = 60.0;
        checkRefresh("staged palm solve matches a fresh solve after moving the palm", position);
        auto pressure = model->s;
        pressure.palm_intensity = 0.3;
        checkRefresh("staged palm solve matches a fresh solve after changing pressure", pressure);
        auto tuning = model->s;
        tuning.open_midi = tuning_notes(2); // Drop D
        checkRefresh("staged palm solve matches a fresh solve after retuning", tuning);

        auto body = model->s;
        body.body_size = 1.2;
        Model quick, full;
        quick.solve(body, sr, model.get(), false);
        full.solve(body, sr, &quick, true);
        ok("staged palm solve reuses the table after a body-only change", full.palm_ready && full.palm == model->palm);
    }

    // ---- in a loop the hands lead from the end of the song into its start
    {
        Song song = Song::demo();
        Compiler c;
        CompileOptions o;
        o.tempo = song.tempo;
        o.loop = true;
        auto perf = c.compile(song, model, o);
        bool inside = true;
        for (const auto& e : perf->events) inside = inside && e.beat >= 0 && e.beat < perf->length_beats;
        ok("loop: every event inside the song", inside);
        // the demo's first notes need a shape change from its last ones: done at the end of the song
        int late = 0;
        for (const auto& e : perf->events)
            if (e.beat > perf->length_beats - 0.5 && e.type == EventType::pluck && e.pluck.kind != Kind::strike) ++late;
        ok("loop: the fingers move for the first notes at the end of the song", late > 0);
    }

    // ---- a damped note darkens as it dies: the high modes go faster than the fundamental
    // (the string alone: the open strings answering through the bridge keep their own colour)
    {
        auto alone = std::make_shared<Model>(*model);
        alone->s.sympathy = false;
        Transport tr;
        auto ratio = [&](bool damped) {
            Engine g;
            g.prepare(sr);
            g.set_model(alone.get());
            g.pluck_string(1, 5, 0.8);
            std::vector<float> y(256 * 200);
            for (size_t p = 0; p < y.size(); p += 256) {
                if (damped && p == 256 * 40) g.all_off();
                g.render(y.data() + p, 256, tr);
            }
            // high band (first difference) over the whole level, late in the note
            std::vector<float> d(y.size());
            for (size_t i = 1; i < y.size(); ++i) d[i] = y[i] - y[i - 1];
            return rms(d, sr, 0.5, 0.9) / std::max(rms(y, sr, 0.5, 0.9), 1e-30);
        };
        ok("damping: the dying note is darker", ratio(true) < 0.8 * ratio(false));
    }


    // ---- the hands: every built-in chord shape can be played; impossible stretches and jumps cannot
    {
        bool all = true;
        for (int c = 0; c < NUM_CHORDS; ++c) {
            std::vector<std::pair<int, int>> held;
            for (int i = 0; i < 6; ++i)
                if (CHORD_SHAPES[c].frets[i] > 0) held.push_back({i, CHORD_SHAPES[c].frets[i]});
            if (!left_hand(held).possible) {
                all = false;
                std::printf("  chord %s judged impossible\n", CHORD_SHAPES[c].name);
            }
        }
        ok("hands: every built-in chord shape is playable", all);
        ok("hands: fret 3 on E with fret 7 on A is possible", left_hand({{0, 3}, {1, 7}}).possible);
        ok("hands: fret 3 on E with fret 8 on A is not", !left_hand({{0, 3}, {1, 8}}).possible);
        ok("hands: five frets five fingers is not", !left_hand({{0, 1}, {1, 2}, {2, 3}, {3, 4}, {4, 5}}).possible);
        ok("hands: a pick cannot jump a string", !right_hand({0, 2}, true).empty() && right_hand({0, 1}, true).empty());
        ok("hands: the fingers can", right_hand({0, 2, 5}, false).empty());
        Song song;
        song.notes.push_back({0, 3, 0.0, 1.0, 0.8, false});
        song.notes.push_back({1, 8, 0.0, 1.0, 0.8, false});
        ok("hands: the song check finds the stretch", impossible_moments(song, true).size() == 1);
    }

    // ---- the sweep: each string takes energy from the pick, which slows; a slow sweep loses more
    {
        const std::vector<int> strings = {0, 1, 2, 3, 4, 5};
        const std::vector<double> base(6, 1.0);
        const std::vector<int> frets(6, 0);
        Stroke fast {0, 0.10, 2.0, 1, 0.22, PICK_DEPTH, PALM_X, 1};
        Stroke slow = fast;
        slow.v = 0.4;
        const auto a = Compiler::sweep(*model, strings, base, frets, fast, 0.0);
        const auto b = Compiler::sweep(*model, strings, base, frets, slow, 0.0);
        bool slower = true;
        for (size_t j = 2; j < b.size(); ++j) slower = slower && (b[j].delay - b[j - 1].delay) >= (b[j - 1].delay - b[j - 2].delay) - 1e-12;
        ok("sweep: the pick slows across the strings", slower);
        ok("sweep: the last string gets less", b.back().share < 0.95 * b.front().share);
        ok("sweep: a slow sweep loses more than a fast one", b.back().share / b.front().share < a.back().share / a.front().share);
    }

    // ---- the pinch: the thumb leads the fingers
    {
        const auto p = Compiler::pinch<std::mt19937>({0, 3, 4}, {1, 1, 1}, nullptr);
        ok("pinch: the thumb first", p[0].delay < p[1].delay && std::abs(p[1].delay - p[2].delay) < 1e-12);
    }

    // ---- the muscle: a long run of sixteenths tires the arm; a stroke after a rest is fresh
    {
        std::vector<Stroke> run;
        for (int k = 0; k < 40; ++k) run.push_back({k * 0.125, 0.1, 2.0, 1.0, 0.22, PICK_DEPTH, PALM_X, 1});
        Compiler::muscle(run, 0.125);
        ok("muscle: the end of a long run is softer", run.back().force < 0.97 * run.front().force);
        ok("muscle: the first stroke is fresh", run.front().force > 1.0);
    }

    // ---- with a pick, notes on the off-sixteenths are struck upwards (alternate picking)
    {
        Settings s;
        s.human = false;
        auto m = std::make_shared<Model>();
        m->solve(s, sr, nullptr, true);
        Song song;
        song.notes.push_back({2, 0, 0.0, 0.5, 0.8, false});
        song.notes.push_back({3, 0, 0.0, 0.5, 0.8, false});
        song.notes.push_back({2, 0, 0.25, 0.5, 0.8, false});
        song.notes.push_back({3, 0, 0.25, 0.5, 0.8, false});
        Compiler c;
        auto perf = c.compile(song, m, {});
        double t[2][2] = {{-1, -1}, {-1, -1}};   // [moment][string 2 or 3]
        for (const auto& e : perf->events)
            if (e.type == EventType::pluck && e.pluck.kind == Kind::strike) t[e.beat > 0.15][e.string - 2] = e.beat;
        ok("picking: down on the beat (bass string first)", t[0][0] < t[0][1]);
        ok("picking: up on the off-sixteenth (treble string first)", t[1][1] < t[1][0]);
    }

    // ---- the palm is a hand: it lands on the strings next to the muted ones, not across the neck
    {
        Song song;
        song.notes.push_back({2, 0, 0.0, 4.0, 0.8, false});    // rings, next to the palm
        song.notes.push_back({5, 0, 0.0, 4.0, 0.8, false});    // rings, far from it
        song.notes.push_back({0, 3, 1.0, 1.0, 0.8, true});
        song.notes.push_back({1, 5, 1.0, 1.0, 0.8, true});
        Compiler c;
        auto perf = c.compile(song, model, {});
        bool near = false, far = false;
        for (const auto& e : perf->events)
            if (e.type == EventType::damp && e.beat > 0.8 && e.beat < 1.0) {
                near = near || e.string == 2;
                far = far || e.string == 5;
            }
        ok("palm: it stops the ringing string next to it", near);
        ok("palm: not the one across the neck", !far);
    }

    // ---- the live palm mutes the roll too, and lands on what rings
    {
        Song song;
        for (int k = 0; k < 4; ++k) song.notes.push_back({2, 2, k * 1.0, 1.0, 0.8, false});
        song.bars = 1;
        Compiler c;
        CompileOptions o;
        o.tempo = 120;
        o.loop = false;
        auto perf = c.compile(song, model, o);
        auto play = [&](bool palm) {
            Engine g;
            g.prepare(sr);
            g.set_model(model.get());
            g.set_performance(perf.get());
            g.live_palm = palm;
            Transport tr;
            tr.playing = true;
            tr.loop = false;
            tr.beats_per_sample = 2.0 / sr;
            std::vector<float> y(256 * 180);
            for (size_t p = 0; p < y.size(); p += 256) {
                tr.beat = p * tr.beats_per_sample;
                g.render(y.data() + p, 256, tr);
            }
            return rms(y, sr, 0.30, 0.48) / std::max(rms(y, sr, 0.0, 0.1), 1e-30);   // decay inside a note
        };
        ok("live palm: the roll's notes are muted", play(true) < 0.6 * play(false));
    }

    // ---- the loop's flags: in a loop only the part between them plays, from its start
    {
        Song song;
        song.bars = 4;
        for (int bar = 0; bar < 4; ++bar) {
            RollNote n;
            n.string = 1;
            n.fret = bar;
            n.start = 4.0 * bar;
            n.length = 2.0;
            song.notes.push_back(n);
        }
        song.loop_start = 8.0;
        song.loop_end = 12.0;
        Song back;
        ok("loop: the flags are saved with the song", back.from_text(song.to_text()) && back.loop_start == 8.0 && back.loop_end == 12.0);
        Compiler c;
        CompileOptions o;
        o.loop = true;
        auto perf = compile_song(c, song, model, o);
        bool only_third = true;
        int strikes = 0;
        for (const auto& e : perf->events)
            if (e.type == EventType::pluck && e.pluck.kind == Kind::strike) {
                ++strikes;
                only_third = only_third && e.pluck.fret == 2;
            }
        ok("loop: only the notes between the flags play", strikes == 1 && only_third);
        ok("loop: the performance starts at the start flag", perf->origin == 8.0 && std::abs(perf->length_beats - 4.0) < 1e-9);
        o.loop = false;
        auto whole = compile_song(c, song, model, o);
        int all = 0;
        for (const auto& e : whole->events) all += e.type == EventType::pluck && e.pluck.kind == Kind::strike;
        ok("no loop: the whole song plays", all == 4 && whole->origin == 0.0);

    }
    // ---- one more note: refused only where the hands cannot reach (the roll greys those places)
    {
        Song song;
        RollNote held;
        held.string = 0;
        held.fret = 3;
        held.start = 0.0;
        held.length = 4.0;
        song.notes.push_back(held);
        RollNote far = held, near = held;
        far.string = 1;
        far.fret = 8;
        far.start = 1.0;
        far.length = 1.0;
        near.string = 1;
        near.fret = 5;
        near.start = 1.0;
        near.length = 1.0;
        ok("one more note: fret 8 beside fret 3 is out of reach", !playable_with(song, far, true));
        ok("one more note: fret 5 beside fret 3 is in reach", playable_with(song, near, true));
        RollNote after = far;
        after.start = 4.0;                    // fret 3 is released by then
        ok("one more note: once the hand is free, fret 8 is fine", playable_with(song, after, true));
    }
    // ---- notes ring as long as the hands let them
    {
        Song song;
        song.bars = 2;
        auto note = [](int string, int fret, double start, double length, int end = RollNote::ring) {
            RollNote n;
            n.string = string;
            n.fret = fret;
            n.start = start;
            n.length = length;
            n.end = end;
            return n;
        };
        song.notes = {note(0, 0, 0.0, 1.0), note(0, 3, 3.0, 1.0)};
        auto r = ringing(song, 8.0, false);
        ok("ringing: an open string rings until it is played again", std::abs(r.note_end[0] - 3.0) < 1e-9);
        song.notes = {note(0, 3, 0.0, 1.0), note(1, 8, 2.0, 1.0)};
        r = ringing(song, 8.0, false);
        ok("ringing: a fretted note rings until the hand must leave its fret", std::abs(r.note_end[0] - 2.0) < 1e-9);
        song.notes = {note(0, 3, 0.0, 1.0), note(1, 5, 2.0, 1.0)};
        r = ringing(song, 8.0, false);
        ok("ringing: ... and on while the hand can hold it", r.note_end[0] > 7.0);
        song.notes = {note(0, 0, 0.0, 1.0, RollNote::stop), note(0, 3, 3.0, 1.0)};
        r = ringing(song, 8.0, false);
        ok("ringing: a note asked to stop stops at its end", std::abs(r.note_end[0] - 1.0) < 1e-9);
        song.notes = {note(0, 3, 1.0, 1.0), note(0, 0, 6.0, 1.0)};
        r = ringing(song, 8.0, true);
        ok("ringing: in a loop the last note rings into the next cycle", std::abs(r.note_end[1] - 9.0) < 1e-9);
        Song saved;
        song.notes[0].end = RollNote::hand;
        ok("ringing: the ending is saved with the song", saved.from_text(song.to_text()) && saved.notes[0].end == RollNote::hand);
        // the strings: no damp on the open string before it is played again; a flat hand damps its neighbours
        song.notes = {note(0, 0, 0.0, 1.0), note(0, 3, 3.0, 1.0)};
        Compiler c;
        CompileOptions o;
        auto perf = c.compile(song, model, o);
        bool early = false;
        for (const auto& e : perf->events) early |= e.type == EventType::damp && e.string == 0 && e.beat > 0.5 && e.beat < 2.8;
        ok("ringing: the strings are not damped while they ring", !early);
        song.notes = {note(1, 0, 0.0, 4.0), note(2, 2, 1.0, 1.0, RollNote::hand)};
        perf = c.compile(song, model, o);
        bool beside = false;
        for (const auto& e : perf->events) beside |= e.type == EventType::damp && e.string == 1 && e.beat > 1.9 && e.beat < 2.3;
        ok("ringing: a flat hand also stops the string beside", beside);
    }
    // ---- every chord, on every tuning of the list and on free ones (B standard, drop A, a baritone in F#): a shape
    // the left hand can play, the root in the bass, every needed tone
    {
        std::vector<std::pair<std::string, std::array<int, NUM_STRINGS>>> tunings;
        for (int tun = 0; tun < NUM_TUNINGS; ++tun)
            tunings.push_back({TUNINGS[tun].name, tuning_notes(tun)});
        tunings.push_back({"B standard", {35, 40, 45, 50, 54, 59}});
        tunings.push_back({"Drop A", {33, 40, 45, 50, 54, 59}});
        tunings.push_back({"F# baritone", {30, 35, 40, 45, 49, 54}});
        bool all = true;
        std::string bad;
        for (const auto &[name, open] : tunings)
            for (int q = 0; q < (int) chord_qualities().size(); ++q)
                for (int r = 0; r < 12; ++r) {
                    const auto c = chord_for(r, q, open);
                    std::vector<std::pair<int, int>> held;
                    int bass = -1;
                    std::set<int> tones;
                    for (int i = 0; i < 6; ++i)
                        if (c.frets[(size_t) i] >= 0) {
                            const int iv = ((open[(size_t) i] + c.frets[(size_t) i] - r) % 12 + 12) % 12;
                            if (bass < 0) bass = iv;
                            tones.insert(iv);
                            if (c.frets[(size_t) i] > 0) held.push_back({i, c.frets[(size_t) i]});
                        }
                    bool ok_here = bass == 0 && left_hand(held).possible;
                    for (int need : chord_qualities()[(size_t) q].needed) ok_here = ok_here && tones.count(need);
                    if (!ok_here && bad.empty()) bad = c.name + " in " + name;
                    all = all && ok_here;
                }
        if (!all) std::printf("  first bad chord: %s\n", bad.c_str());
        ok("chords: every chord on every tuning has a playable shape with its root in the bass", all);
        const auto fsm = parse_chord("F#m7"), bb = parse_chord("Bbmaj7"), db = parse_chord("Db");
        ok("chords: names with sharps and flats are read", fsm.first == 6 && bb.first == 10 && db.first == 1 && db.second == 0);
        ok("scales: a third above E in C major is G", scale_step(64, 0, 0, 2) == 67);
        ok("scales: a third above A in A minor is C", scale_step(69, 9, 1, 2) == 72);
        ok("scales: a third below C in C major is A", scale_step(72, 0, 0, -2) == 69);
    }
    // ---- filling strums keeps the chords already written, over any length
    {
        Song song;
        song.bars = 4;
        RollStrum am;
        am.start = 4.0;
        am.length = 4.0;
        for (int i = 0; i < 6; ++i) am.frets[(size_t) i] = CHORD_SHAPES[3].frets[i];
        am.chord = "Am";
        song.strums.push_back(am);
        const auto g = chord_for(7, 0, tuning_notes(0));
        song.fill_strums_range(0.0, 12.0, 0, g.frets, g.name, 2.0);
        bool kept = true, before = true, past_bar = false;
        for (const auto& t : song.strums) {
            if (t.start >= 4.0 && t.start < 8.0) kept = kept && t.chord == "Am";
            if (t.start < 4.0 || t.start >= 8.0) before = before && t.chord == "G";
            past_bar = past_bar || t.start >= 8.0;
        }
        ok("fill strums: the chord written there is kept (rhythm only)", kept);
        ok("fill strums: elsewhere the chord of the boxes", before);
        ok("fill strums: over three bars at once", past_bar);
    }
    // ---- a dead note: struck, then damped at once; one hand cannot strum and pick notes at the same moment
    {
        Song song;
        RollNote x;
        x.string = 1;
        x.fret = 5;
        x.start = 1.0;
        x.length = 1.0;
        x.dead = true;
        song.notes.push_back(x);
        Compiler c;
        CompileOptions o;
        auto perf = c.compile(song, model, o);
        double strike = -1, damp = -1;
        for (const auto& e : perf->events) {
            if (e.type == EventType::pluck && e.pluck.kind == Kind::strike && e.string == 1) strike = e.beat;
            if (e.type == EventType::damp && e.string == 1 && strike >= 0 && damp < 0) damp = e.beat;
        }
        ok("dead note: damped right after its stroke", strike >= 0 && damp > strike && (damp - strike) * 60.0 / o.tempo < 0.02);
        Song both;
        RollStrum t;
        t.start = 0.0;
        for (int i = 0; i < 6; ++i) t.frets[(size_t) i] = CHORD_SHAPES[1].frets[i];
        both.strums.push_back(t);
        RollNote n;
        n.string = 0;
        n.start = 0.0;
        both.notes.push_back(n);
        ok("hands: strumming and picking a note at the same moment is refused", !impossible_moments(both, true).empty());
    }
    // ---- hand stops: whatever rings stops there (saved with the song, in the compiled strings)
    {
        Song song;
        song.bars = 2;
        RollNote a;
        a.string = 0;
        a.fret = 0;
        a.start = 0.0;
        a.length = 1.0;
        song.notes.push_back(a);
        song.stops.push_back(2.5);
        const auto r = ringing(song, 8.0, false);
        ok("hand stop: the open string rings up to it", std::abs(r.note_end[0] - 2.5) < 1e-9);
        Song back;
        ok("hand stop: saved with the song", back.from_text(song.to_text()) && back.stops.size() == 1 && std::abs(back.stops[0] - 2.5) < 1e-9);
        Compiler c;
        CompileOptions o;
        auto perf = c.compile(song, model, o);
        bool damped = false;
        for (const auto& e : perf->events) damped |= e.type == EventType::damp && e.string == 0 && std::abs(e.beat - 2.5) < 1e-6;
        ok("hand stop: the strings are damped there", damped);
    }
    // ---- another pattern takes over: the old one's notes still end as they were asked (the string alone: the open
    // strings answering it through the bridge ring on, and are not asked to stop)
    {
        auto alone = std::make_shared<Model>(*model);
        alone->s.sympathy = false;
        Song one, two;
        RollNote n;
        n.string = 2;
        n.fret = 2;
        n.start = 0.0;
        n.length = 1.0;
        n.end = RollNote::stop;
        one.notes.push_back(n);
        one.bars = two.bars = 1;
        Compiler c;
        CompileOptions o;
        o.tempo = 120;
        auto p1 = c.compile(one, model, o), p2 = c.compile(two, model, o);
        auto level = [&](bool carry) {
            Engine e;
            e.prepare(sr);
            e.set_model(alone.get());
            e.set_performance(p1.get());
            Transport tr;
            tr.playing = true;
            tr.loop = true;
            tr.beats_per_sample = 2.0 / sr;
            std::vector<float> y(256);
            double late = 0;
            for (int b = 0; b < 400; ++b) {
                tr.beat = b * 256 * tr.beats_per_sample;
                if (b == 40) e.set_performance(p2.get(), carry);     // at 0.21 s: the other pattern
                e.render(y.data(), 256, tr);
                if (b > 250)
                    for (float v : y) late += v * v;
            }
            return late;
        };
        ok("pattern change: a note asked to stop still stops", level(true) < 0.2 * level(false));
    }
    // ---- legato: two notes in a row on one string are joined. The string sounds until the next stroke, whether
    // the fret changes or not: the hand changes shape at the last moment, it does not silence the string first.
    {
        const double beat_s = 0.5; // 120 bpm
        const double change = beat_s;
        auto render = [&](const Song& song, const std::shared_ptr<Model>& m) {
            Compiler c;
            CompileOptions o;
            o.tempo = 120;
            auto perf = c.compile(song, m, o);
            Engine g;
            g.prepare(sr);
            g.set_model(m.get());
            g.set_performance(perf.get());
            Transport tr;
            tr.playing = true;
            tr.loop = false;
            tr.beats_per_sample = 1.0 / (beat_s * sr);
            std::vector<float> y(256 * 240);
            for (size_t p = 0; p < y.size(); p += 256) {
                tr.beat = p * tr.beats_per_sample;
                g.render(y.data() + p, 256, tr);
            }
            return y;
        };
        struct Heard
        {
            double kept;  // the lowest share of the first note's level (alone) still heard before the 2nd stroke
            double pitch; // after the 2nd stroke: how much more the sound repeats at the new note's period than at the old one's
        };
        auto play = [&](int from_fret, int to_fret, bool finger_noises, bool sympathy = true) {
            auto m = std::make_shared<Model>(*model);
            m->s.finger_noises = finger_noises;
            m->s.sympathy = sympathy;
            Song alone;
            alone.bars = 1;
            alone.notes.push_back({2, from_fret, 0.0, 1.0, 0.8, false});
            Song both = alone;
            both.notes.push_back({2, to_fret, 1.0, 1.0, 0.8, false});
            const auto first = render(alone, m), y = render(both, m);
            // 20 ms windows (two periods of the lowest note) ending before the stroke, which may come ~20 ms early
            double kept = 1e30;
            for (double t = change - 0.14; t + 0.02 <= change - 0.03; t += 0.005)
                kept = std::min(kept, rms(y, sr, t, t + 0.02) / std::max(rms(first, sr, t, t + 0.02), 1e-30));
            auto repeats = [&](int fret) { // normalised autocorrelation at the period of `fret`, 150 to 400 ms after
                const size_t lag = (size_t) std::lround(sr / m->place[2][(size_t) fret].f1);
                const size_t i0 = (size_t) ((change + 0.15) * sr), i1 = (size_t) ((change + 0.40) * sr);
                double xy = 0, xx = 0, yy = 0;
                for (size_t i = i0; i < i1 && i + lag < y.size(); ++i) {
                    xy += (double) y[i] * y[i + lag];
                    xx += (double) y[i] * y[i];
                    yy += (double) y[i + lag] * y[i + lag];
                }
                return xy / std::sqrt(std::max(xx * yy, 1e-60));
            };
            const Heard h{kept, repeats(to_fret) - repeats(from_fret)};
            std::printf("     legato %2d -> %2d%s: level kept %.3f, new pitch over old %+.3f\n", from_fret, to_fret,
                        finger_noises ? "" : " (no finger noises)", h.kept, h.pitch);
            return h;
        };
        auto joined = [&](const char* name, int from_fret, int to_fret, bool finger_noises) {
            const Heard h = play(from_fret, to_fret, finger_noises);
            ok(name, h.kept > 0.7 && h.pitch > 0.2);
        };
        joined("legato: up the neck (5 -> 7), the string rings until the stroke", 5, 7, true);
        joined("legato: down the neck (7 -> 5), the string rings until the stroke", 7, 5, true);
        joined("legato: open string to fret 3, the string rings until the stroke", 0, 3, true);
        joined("legato: fret 3 to the open string, the string rings until the stroke", 3, 0, true);
        joined("legato: without finger noises the string still rings until the stroke", 5, 7, false);
        // a chord whose shape changes: the whole hand lifts and touches the strings, so that the next strum is heard
        // string by string on quiet strings instead of a new chord already sounding under the left hand (heard
        // without the finger noises, whose taps sound as the fingers land): G to a barred F, every string changes
        {
            auto m = std::make_shared<Model>(*model);
            m->s.finger_noises = false;
            Song alone;
            alone.bars = 1;
            RollStrum g_chord;
            g_chord.start = 0.0;
            g_chord.length = 2.0;
            g_chord.frets = {{3, 2, 0, 0, 0, 3}};
            alone.strums.push_back(g_chord);
            Song both = alone;
            RollStrum f_chord;
            f_chord.start = 2.0;
            f_chord.length = 2.0;
            f_chord.frets = {{1, 3, 3, 2, 1, 1}};
            both.strums.push_back(f_chord);
            const auto first = render(alone, m), y = render(both, m);
            const double strum = 2.0 * beat_s;
            const double quiet = rms(y, sr, strum - 0.07, strum - 0.04) / std::max(rms(first, sr, strum - 0.07, strum - 0.04), 1e-30);
            std::printf("     strum G -> F: level kept just before the strum %.3f\n", quiet);
            ok("strum: a new chord shape lifts the hand, the strings are quiet before the pick", quiet < 0.3);
        }
        // the hand travelling along the neck touches the string: the old note stops before the new one (the string
        // alone: without the finger noises, whose taps sound in its place, nor the other strings answering it)
        ok("legato: a shift of the hand (2 -> 12) still stops the old note", play(2, 12, false, false).kept < 0.3);
    }
    // ---- the two planes of the string: across the top the energy leaves fast into the body (the loud start), along
    // it the note rings on; the attack's angle shares the pluck between them
    {
        auto alone = std::make_shared<Model>(*model);
        alone->s.sympathy = false;
        Transport tr;
        auto note = [&](const Voicing& v) {
            Engine g;
            g.prepare(sr);
            g.set_model(alone.get());
            g.voicing = v;
            g.pluck_string(1, 5, 0.8);
            std::vector<float> y(256 * 260);
            for (size_t p = 0; p < y.size(); p += 256) g.render(y.data() + p, 256, tr);
            return y;
        };
        auto db = [](double x) { return 20.0 * std::log10(std::max(x, 1e-30)); };
        // each plane alone (no beats between them to blur the measure): the decay in dB/s over the first second
        auto decay = [&](const std::vector<float>& y) {
            return (db(rms(y, sr, 0.02, 0.07)) - db(rms(y, sr, 1.0, 1.05))) / 0.98;
        };
        Voicing across_only = Voicing::single_plane(), along_only;
        along_only.angle_down = 0.0;
        const double across = decay(note(across_only)), along_top = decay(note(along_only));
        std::printf("     planes: decay %.1f dB/s across the top, %.1f dB/s along it\n", across, along_top);
        ok("planes: across the top the energy leaves fast into the body, along it the note rings on", across > 1.3 * along_top);
        Voicing into, along;
        into.angle_down = 80.0;
        along.angle_down = 5.0;
        const auto a = note(into), b = note(along);
        const double start_into = rms(a, sr, 0.0, 0.05), start_along = rms(b, sr, 0.0, 0.05);
        std::printf("     planes: start into the top %.4f, along it %.4f\n", start_into, start_along);
        ok("planes: a stroke into the top hits the body harder", start_into > 1.5 * start_along);
        ok("planes: a stroke along the top rings longer, relative to its start",
           rms(b, sr, 1.0, 1.2) / start_along > rms(a, sr, 1.0, 1.2) / start_into);
    }
    // ---- the hard attack: the string bounces off the last fret (past ~2 N the lows hardly grow, the highs do), the
    // pick pushes and scrapes before it lets go, the tension of a wide vibration makes the note start sharp
    {
        auto alone = std::make_shared<Model>(*model);
        alone->s.sympathy = false;
        Transport tr;
        auto note = [&](const Voicing& v, int string, double velocity, double seconds) {
            Engine g;
            g.prepare(sr);
            g.set_model(alone.get());
            g.voicing = v;
            g.pluck_string(string, 0, velocity);
            std::vector<float> y((size_t)(seconds * sr / 256 + 1) * 256);
            for (size_t p = 0; p < y.size(); p += 256) g.render(y.data() + p, 256, tr);
            return y;
        };
        auto db = [](double x) { return 20.0 * std::log10(std::max(x, 1e-30)); };
        // the level below 400 Hz and above 3 kHz (one-pole filters), from a to b seconds
        auto bands = [&](const std::vector<float>& y, double a, double b) {
            const double cl = 1.0 - std::exp(-2 * 3.14159265358979 * 400.0 / sr);
            const double ch = std::exp(-2 * 3.14159265358979 * 3000.0 / sr);
            double lo = 0, hp = 0, last = 0, el = 0, eh = 0;
            const size_t i0 = (size_t)(a * sr), i1 = std::min(y.size(), (size_t)(b * sr));
            for (size_t i = 0; i < i1; ++i) {
                lo += (y[i] - lo) * cl;
                hp = ch * (hp + y[i] - last);
                last = y[i];
                if (i >= i0) { el += lo * lo; eh += hp * hp; }
            }
            return std::pair<double, double>{db(std::sqrt(el)), db(std::sqrt(eh))};
        };
        // the period near `f` from a to b seconds: the best normalised autocorrelation, refined by a parabola
        auto frequency = [&](const std::vector<float>& y, double a, double b, double f) {
            const size_t i0 = (size_t)(a * sr), i1 = (size_t)(b * sr);
            auto corr = [&](int lag) {
                double xy = 0, xx = 0, yy = 0;
                for (size_t i = i0; i + lag < i1; ++i) { xy += y[i] * y[i + lag]; xx += y[i] * y[i]; yy += y[i + lag] * y[i + lag]; }
                return xy / std::sqrt(std::max(xx * yy, 1e-30));
            };
            const int lo = (int)(sr / (f * 1.05)), hi = (int)(sr / (f * 0.97));
            int best = lo;
            for (int l = lo; l <= hi; ++l) if (corr(l) > corr(best)) best = l;
            const double c0 = corr(best - 1), c1 = corr(best), c2 = corr(best + 1);
            const double shift = 0.5 * (c0 - c2) / std::min(c0 - 2 * c1 + c2, -1e-12);
            return sr / (best + shift);
        };

        Voicing frets;
        frets.push_ms = 0.0;
        frets.slip_stick = frets.scrape = 0.0;
        frets.glide = frets.longitudinal = 0.0;
        frets.angle_down = 45.0;
        Voicing free = frets;
        free.action_mm = 0.0;
        auto growth = [&](const Voicing& v, double& lows, double& highs) {
            const auto soft = bands(note(v, 0, 0.2, 0.3), 0.0, 0.3), hard = bands(note(v, 0, 2.4, 0.3), 0.0, 0.3);
            lows = hard.first - soft.first;
            highs = hard.second - soft.second;
        };
        double lows_free, highs_free, lows_frets, highs_frets;
        growth(free, lows_free, highs_free);
        growth(frets, lows_frets, highs_frets);
        std::printf("     hard attack: soft -> hard, no frets lows +%.1f dB highs +%.1f dB; frets lows +%.1f dB highs +%.1f dB\n",
                    lows_free, highs_free, lows_frets, highs_frets);
        ok("hard attack: without frets the string is linear (a harder stroke only louder)", std::abs(highs_free - lows_free) < 1.0);
        ok("hard attack: the frets compress the lows and open the highs", highs_frets - lows_frets > 6.0);
        ok("hard attack: a soft stroke does not reach the frets",
           std::abs(bands(note(frets, 0, 0.2, 0.3), 0.0, 0.3).second - bands(note(free, 0, 0.2, 0.3), 0.0, 0.3).second) < 0.5);
        // pushed onto the fret, the string is released from it bent there: the fret takes energy, it never adds any
        // (pushed out of the fret at once, it would be thrown off with a jolt)
        const double held = rms(note(frets, 0, 2.4, 0.3), sr, 0.0, 0.3), open = rms(note(free, 0, 2.4, 0.3), sr, 0.0, 0.3);
        std::printf("     hard attack: level of a hard stroke %.3f on the frets, %.3f free\n", held, open);
        ok("hard attack: the frets never add energy", held < open);

        // the pitch glide: a hard stroke starts sharp and settles, a soft one does not move
        Voicing glide = free;
        glide.glide = 1.0;
        auto settles = [&](const Voicing& v, double velocity) {
            const auto y = note(v, 0, velocity, 1.2);
            const double f1 = 82.4;
            return 1200.0 * std::log2(frequency(y, 0.03, 0.13, f1) / frequency(y, 0.9, 1.1, f1));
        };
        const double hard_glide = settles(glide, 2.4), soft_glide = settles(glide, 0.2), no_glide = settles(free, 2.4);
        std::printf("     hard attack: pitch glide %.2f cents (hard), %.2f (soft), %.2f (glide off)\n", hard_glide, soft_glide, no_glide);
        ok("hard attack: a hard stroke starts sharp and settles", hard_glide > 2.0 && hard_glide > 4.0 * std::abs(no_glide) + 1.0);
        ok("hard attack: a soft stroke hardly glides", soft_glide < 0.25 * hard_glide);

        // the pick: it pushes before it lets go (the bridge feels it), the scrape is louder on a wound string
        Voicing push = free;
        push.push_ms = 2.0;
        const auto pushed = note(push, 0, 0.8, 0.3), released = note(free, 0, 0.8, 0.3);
        std::printf("     hard attack: first ms %.4f pushed, %.4f released at once\n", rms(pushed, sr, 0.0, 0.001), rms(released, sr, 0.0, 0.001));
        ok("hard attack: the push is heard before the release", rms(pushed, sr, 0.0, 0.001) > 0.0);
        Voicing scrape = push;
        scrape.scrape = 1.0;
        auto scrape_noise = [&](int string) {
            return bands(note(scrape, string, 0.8, 0.01), 0.0, 0.001).second - bands(note(push, string, 0.8, 0.01), 0.0, 0.001).second;
        };
        const double wound = scrape_noise(1), plain = scrape_noise(4);
        std::printf("     hard attack: scrape adds %.1f dB of highs on a wound string, %.1f dB on a plain one\n", wound, plain);
        ok("hard attack: the pick scrapes, more on a wound string", wound > 3.0 && wound > plain + 3.0);
    }

    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
