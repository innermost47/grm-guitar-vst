// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// offline_renderer.hpp
//
// The engine without a host (tools, tests): the networks solved once for some settings, then a guitar roll or live
// notes rendered block by block as a host would call it. Its outputs are the engine's (models::OfflineTake); the
// body and the room are offline.hpp's.
#pragma once

#include <algorithm>
#include <chrono>
#include <limits>
#include <memory>
#include <tuple>
#include <vector>

#include "engine.hpp"
#include "song.hpp"
#include "../models/offline.hpp"

namespace grmg
{
    class OfflineRenderer
    {
    public:
        static constexpr int BLOCK = 256;

        // the networks for `settings`: first without the palm and the room (the quick ones), then all of them
        OfflineRenderer(const models::Settings &settings, double sr) : sr_(sr)
        {
            const auto t0 = std::chrono::steady_clock::now();
            auto quick = std::make_shared<Model>();
            quick->solve(settings, sr, nullptr, false);
            quick_ms_ = since_ms(t0);
            const auto t1 = std::chrono::steady_clock::now();
            auto full = std::make_shared<Model>();
            full->solve(settings, sr, quick.get(), true);
            full_ms_ = since_ms(t1);
            model_ = full;
        }

        const Model &model() const { return *model_; }
        double sample_rate() const { return sr_; }
        double quick_ms() const { return quick_ms_; }
        double full_ms() const { return full_ms_; }
        double compile_ms() const { return compile_ms_; }
        const Performance *performance() const { return perf_.get(); }

        // the guitar roll `song`, then `tail_s` seconds of what still rings
        models::OfflineTake render_song(const Song &song, unsigned seed, double tail_s, const models::Voicing &voicing = models::Voicing())
        {
            const auto t0 = std::chrono::steady_clock::now();
            Compiler compiler;
            models::CompileOptions options;
            options.tempo = song.tempo;
            options.seed = seed;
            perf_ = compiler.compile(song, model_, options);
            compile_ms_ = since_ms(t0);
            auto engine = make_engine(voicing); // on the heap: an engine is too large for a thread's stack
            engine->set_performance(perf_.get());
            const double spb = 60.0 / song.tempo;
            models::Transport tr;
            tr.playing = true;
            tr.loop = false;
            tr.beats_per_sample = 1.0 / (spb * sr_);
            const size_t total = (size_t)((song.length_beats() * spb + tail_s) * sr_);
            return run(*engine, total, [&](size_t pos)
                       { tr.beat = (double)pos * tr.beats_per_sample; return tr; },
                       [total](size_t)
                       { return total; });
        }

        // keys pressed and released as a MIDI keyboard would, over `seconds`
        models::OfflineTake render_live(const std::vector<models::LiveNote> &notes, double seconds,
                                        const models::Voicing &voicing = models::Voicing())
        {
            auto engine = make_engine(voicing);
            // each key's sample, whether it is pressed or released, its note
            std::vector<std::tuple<size_t, bool, const models::LiveNote *>> keys;
            for (const auto &n : notes)
            {
                keys.emplace_back((size_t)std::max(0.0, n.at_s * sr_), true, &n);
                keys.emplace_back((size_t)std::max(0.0, n.off_s * sr_), false, &n);
            }
            std::stable_sort(keys.begin(), keys.end(), [](const auto &a, const auto &b)
                             { return std::get<0>(a) < std::get<0>(b); });
            size_t next = 0;
            const models::Transport idle;
            return run(*engine, (size_t)(seconds * sr_), [&](size_t pos)
                       {
                           for (; next < keys.size() && std::get<0>(keys[next]) <= pos; ++next)
                           {
                               const auto &[at, on, note] = keys[next];
                               const auto &n = *note;
                               if (on)
                                   engine->note_on(n.midi, n.velocity, n.channel);
                               else
                                   engine->note_off(n.midi, n.channel);
                           }
                           return idle; },
                       [&](size_t)
                       { return next < keys.size() ? std::get<0>(keys[next]) : std::numeric_limits<size_t>::max(); });
        }

    private:
        double sr_;
        std::shared_ptr<Model> model_;
        std::unique_ptr<Performance> perf_;
        double quick_ms_ = 0.0, full_ms_ = 0.0, compile_ms_ = 0.0;

        static double since_ms(std::chrono::steady_clock::time_point t0)
        {
            return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        }

        std::unique_ptr<Engine> make_engine(const models::Voicing &voicing) const
        {
            auto engine = std::make_unique<Engine>();
            engine->prepare(sr_);
            engine->voicing = voicing;
            engine->set_model(model_.get());
            return engine;
        }

        // `total` samples, block by block; `before(pos)` runs before each block and gives its transport, `next_cut(pos)`
        // the next sample where something happens (a key): a block ends there, the key lands on its sample, as a host
        // places a MIDI event at its offset in the buffer
        template <class Before, class NextCut>
        models::OfflineTake run(Engine &engine, size_t total, Before before, NextCut next_cut)
        {
            models::OfflineTake take;
            std::vector<float> out(total, 0.0f), pickup(total, 0.0f);
            double busy = 0.0;
            for (size_t pos = 0; pos < total;)
            {
                const models::Transport tr = before(pos);
                const size_t cut = next_cut(pos);
                const int n = (int)std::min({(size_t)BLOCK, total - pos, cut > pos ? cut - pos : (size_t)BLOCK});
                const auto t0 = std::chrono::steady_clock::now();
                engine.render(out.data() + pos, n, tr, pickup.data() + pos);
                busy += std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
                pos += (size_t)n;
            }
            take.force.assign(out.begin(), out.end());
            take.pickup.assign(pickup.begin(), pickup.end());
            take.busy_s = busy;
            return take;
        }
    };
}
