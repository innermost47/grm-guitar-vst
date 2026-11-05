// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// How the guitar is played (no Python reference here: behaviours): the palm lifted, the song in a loop, notes
// that end, the live palm; the hands' limits, the sweep of the pick, the pinch, the muscle, the palm's band.
#include <cmath>
#include <cstdio>

#include "../core/engine.hpp"
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
    {
        Engine e;
        e.prepare(sr);
        e.set_model(model.get());
        Transport tr;
        auto ratio = [&](bool damped) {
            Engine g;
            g.prepare(sr);
            g.set_model(model.get());
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

    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
