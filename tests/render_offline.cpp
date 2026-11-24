// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// The whole plugin path without a host: networks -> model, guitar roll -> performance, the engine block by
// block as a host would call it, then the body and the room (or a magnetic pickup into an amplifier). Writes a
// stereo WAV and prints the timings.
// Usage: render_offline [out.wav] [song.txt] [acoustic | clean | crunch | lead | metal]
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
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

// the settings of a rig to listen to: an acoustic guitar, or a magnetic pickup into the amplifier
static grmg::models::Settings rig(const std::string& name) {
    grmg::models::Settings s;
    if (name.empty() || name == "acoustic") return s;
    s.sensor = 1;
    s.amp.on = true;
    s.amp.gate_db = -60.0;
    if (name == "clean") {
        s.amp.mix = 0.0;
        s.amp.cabinet.kind = 0;
    } else if (name == "crunch") {
        s.amp.lead.voicing = 1;
        s.amp.lead.gain = 6.0;
    } else if (name == "lead") {
        s.amp.lead.voicing = 2;
        s.amp.lead.gain = 7.0;
    } else if (name == "metal") { // high gain, the pedal as a tight boost in front, the gate
        s.amp.lead.gain = 6.5;
        s.amp.drive.on = true;
        s.amp.drive.drive = 0.1;
        s.amp.drive.level = 0.9;
        s.amp.gate_db = -50.0;
    }
    return s;
}

static double ms_since(std::chrono::steady_clock::time_point t0) {
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
}

int main(int argc, char** argv) {
    using namespace grmg;
    using namespace grmg::models;
    const int sr = 48000, block = 256;
    auto t0 = std::chrono::steady_clock::now();
    auto model = std::make_shared<Model>();
    const Settings settings = rig(argc > 3 ? argv[3] : "");
    model->solve(settings, sr, nullptr, false);
    std::printf("model without palm and room: %.0f ms\n", ms_since(t0));
    t0 = std::chrono::steady_clock::now();
    auto full = std::make_shared<Model>();
    full->solve(settings, sr, model.get(), true);
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
    std::vector<float> out(total, 0.0f), pickup(total, 0.0f);
    Transport tr;
    tr.playing = true;
    tr.loop = false;
    tr.beats_per_sample = 1.0 / (spb * sr);
    double busy = 0.0;
    for (size_t pos = 0; pos < total; pos += block) {
        const int n = (int) std::min<size_t>(block, total - pos);
        tr.beat = (double) pos * tr.beats_per_sample;
        auto a = std::chrono::steady_clock::now();
        engine.render(out.data() + pos, n, tr, pickup.data() + pos);
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
    const auto stereo = sensor_output(force, std::vector<double>(pickup.begin(), pickup.end()), *full);
    std::printf("body and room (offline convolution): %.0f ms\n", ms_since(t0));
    write_wav(argc > 1 ? argv[1] : "render_offline.wav", stereo, sr);
    return finite ? 0 : 1;
}
