// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// The whole plugin path without a host: networks -> model, guitar roll -> performance, the engine block by
// block as a host would call it, then the body and the room (or a magnetic pickup into an amplifier). Writes a
// stereo WAV and prints the timings.
// Usage: render_offline [out.wav] [song.txt] [acoustic | pickup | clean | crunch | lead | metal]
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

#include "../core/offline.hpp"
#include "../core/offline_renderer.hpp"
#include "support/rigs.hpp"
#include "support/wav.hpp"

int main(int argc, char **argv)
{
    using namespace grmg;
    const int sr = 48000;
    OfflineRenderer renderer(rigs::named(argc > 3 ? argv[3] : ""), sr);
    std::printf("model without palm and room: %.0f ms\n", renderer.quick_ms());
    std::printf("palm (120 networks) and room: %.0f ms\n", renderer.full_ms());

    Song song = Song::demo();
    if (argc > 2)
    {
        std::ifstream in(argv[2]);
        std::stringstream ss;
        ss << in.rdbuf();
        if (!song.from_text(ss.str()))
            std::printf("could not read %s, playing the demo\n", argv[2]);
    }
    const auto take = renderer.render_song(song, 7, 3.0);
    const auto *perf = renderer.performance();
    std::printf("performance: %zu events, %zu finger noises, %zu palms, %.0f ms\n", perf->events.size(),
                perf->squeaks.size(), perf->palms.size(), renderer.compile_ms());
    const double seconds = (double)take.force.size() / sr;
    std::printf("strings: %.1f s of sound in %.3f s (%.1f %% of one core)\n", seconds, take.busy_s, 100.0 * take.busy_s / seconds);
    double peak = 0;
    bool finite = true;
    for (double v : take.force)
    {
        peak = std::max(peak, std::abs(v));
        finite &= std::isfinite(v);
    }
    std::printf("peak force on the bridge: %.3f N%s\n", peak, finite ? "" : "  (NOT FINITE)");
    const auto t0 = std::chrono::steady_clock::now();
    const auto stereo = sensor_output(take.force, take.pickup, renderer.model());
    std::printf("body and room (offline convolution): %.0f ms\n",
                std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count());
    wav::write_stereo(argc > 1 ? argv[1] : "render_offline.wav", stereo, sr);
    return finite ? 0 : 1;
}
