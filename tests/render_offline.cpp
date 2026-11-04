// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// The whole plugin path without a host: networks -> model, guitar roll -> performance, the engine block by
// block as a host would call it, then the body and the room. Writes a stereo WAV and prints the timings.
// Usage: render_offline [out.wav] [song.txt]
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <vector>

#include "../core/engine.hpp"
#include "../core/offline.hpp"

static void write_wav(const char* path, const std::array<std::vector<double>, 2>& x, int sr) {
    std::ofstream f(path, std::ios::binary);
    auto u32 = [&](uint32_t v) { f.write((const char*) &v, 4); };
    auto u16 = [&](uint16_t v) { f.write((const char*) &v, 2); };
    const uint32_t n = (uint32_t) x[0].size();
    f.write("RIFF", 4); u32(36 + n * 4); f.write("WAVEfmt ", 8);
    u32(16); u16(1); u16(2); u32((uint32_t) sr); u32((uint32_t) sr * 4); u16(4); u16(16);
    f.write("data", 4); u32(n * 4);
    double peak = 1e-12;
    for (const auto& c : x)
        for (double v : c) peak = std::max(peak, std::abs(v));
    for (uint32_t t = 0; t < n; ++t)
        for (int ch = 0; ch < 2; ++ch) {
            const int16_t s = (int16_t) (x[(size_t) ch][t] / peak * 0.89 * 32767);
            f.write((const char*) &s, 2);
        }
}

static double ms_since(std::chrono::steady_clock::time_point t0) {
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
}

int main(int argc, char** argv) {
    using namespace grmg;
    const int sr = 48000, block = 256;
    auto t0 = std::chrono::steady_clock::now();
    auto model = std::make_shared<Model>();
    model->solve(Settings{}, sr, nullptr, false);
    std::printf("model without palm and room: %.0f ms\n", ms_since(t0));
    t0 = std::chrono::steady_clock::now();
    auto full = std::make_shared<Model>();
    full->solve(Settings{}, sr, model.get(), true);
    std::printf("palm (120 networks) and room: %.0f ms\n", ms_since(t0));

    Song song = Song::demo();
    if (argc > 2) {
        std::ifstream in(argv[2]);
        std::stringstream ss;
        ss << in.rdbuf();
        if (!song.from_text(ss.str())) std::printf("could not read %s, playing the demo\n", argv[2]);
    }
    t0 = std::chrono::steady_clock::now();
    Compiler compiler;
    CompileOptions opt;
    opt.tempo = song.tempo;
    opt.seed = 7;
    auto perf = compiler.compile(song, full, opt);
    std::printf("performance: %zu events, %zu finger noises, %zu palms, %.0f ms\n", perf->events.size(),
                perf->squeaks.size(), perf->palms.size(), ms_since(t0));

    Engine engine;
    engine.prepare(sr);
    engine.set_model(full.get());
    engine.set_performance(perf.get());
    const double spb = 60.0 / song.tempo;
    const double seconds = song.length_beats() * spb + 3.0;
    const size_t total = (size_t) (seconds * sr);
    std::vector<float> out(total, 0.0f);
    Transport tr;
    tr.playing = true;
    tr.loop = false;
    tr.beats_per_sample = 1.0 / (spb * sr);
    double busy = 0.0;
    for (size_t pos = 0; pos < total; pos += block) {
        const int n = (int) std::min<size_t>(block, total - pos);
        tr.beat = (double) pos * tr.beats_per_sample;
        auto a = std::chrono::steady_clock::now();
        engine.render(out.data() + pos, n, tr);
        busy += std::chrono::duration<double>(std::chrono::steady_clock::now() - a).count();
    }
    std::printf("strings: %.1f s of sound in %.3f s (%.1f %% of one core)\n", seconds, busy, 100.0 * busy / seconds);
    std::vector<double> force(out.begin(), out.end());
    double peak = 0;
    bool finite = true;
    for (double v : force) {
        peak = std::max(peak, std::abs(v));
        finite &= std::isfinite(v);
    }
    std::printf("peak force on the bridge: %.3f N%s\n", peak, finite ? "" : "  (NOT FINITE)");
    t0 = std::chrono::steady_clock::now();
    const auto stereo = body_and_room(force, *full);
    std::printf("body and room (offline convolution): %.0f ms\n", ms_since(t0));
    write_wav(argc > 1 ? argv[1] : "render_offline.wav", stereo, sr);
    return finite ? 0 : 1;
}
