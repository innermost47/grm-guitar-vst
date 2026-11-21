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
        tuning.tuning = 2; // Drop D
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
    // ---- every chord, on every tuning: a shape the left hand can play, the root in the bass, every needed tone
    {
        bool all = true;
        std::string bad;
        for (int tun = 0; tun < NUM_TUNINGS; ++tun)
            for (int q = 0; q < (int) chord_qualities().size(); ++q)
                for (int r = 0; r < 12; ++r) {
                    const auto c = chord_for(r, q, tun);
                    std::vector<std::pair<int, int>> held;
                    int bass = -1;
                    std::set<int> tones;
                    for (int i = 0; i < 6; ++i)
                        if (c.frets[(size_t) i] >= 0) {
                            const int iv = ((TUNINGS[tun].midi[i] + c.frets[(size_t) i] - r) % 12 + 12) % 12;
                            if (bass < 0) bass = iv;
                            tones.insert(iv);
                            if (c.frets[(size_t) i] > 0) held.push_back({i, c.frets[(size_t) i]});
                        }
                    bool ok_here = bass == 0 && left_hand(held).possible;
                    for (int need : chord_qualities()[(size_t) q].needed) ok_here = ok_here && tones.count(need);
                    if (!ok_here && bad.empty()) bad = c.name + " in " + TUNINGS[tun].name;
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
        const auto g = chord_for(7, 0, 0);
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
    // ---- another pattern takes over: the old one's notes still end as they were asked
    {
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
            e.set_model(model.get());
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
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
