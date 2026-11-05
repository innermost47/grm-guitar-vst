// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// Real-time guitar engine (no JUCE inside, so that it can be tested and rendered offline).
//
// The split that makes a GRM guitar playable in real time:
//   - the GRM networks are solved when something changes, never in the audio callback: their results are a
//     Model (strings at every fret, attack, palm, fingers, body, room) and a Performance (the guitar roll
//     turned into what the hands do);
//   - the audio callback only runs the strings: each string is a bank of modes pushing on the bridge;
//     the strings that nobody plays answer the others through the bridge (sympathetic strings).
// Its output is the force of the strings on the bridge (newtons, mono). The body and the room are impulse
// responses from their networks (Model::body_ir, Model::room_ir), applied by a convolution after it.
#pragma once

#include <array>
#include <atomic>
#include <cmath>
#include <memory>
#include <vector>

#include "performance.hpp"
#include "voice.hpp"

namespace grmg {

// One object handed from a background thread to the audio thread without locks: the worker posts, the
// audio thread takes the newest between two blocks, the worker frees what the audio thread let go.
template <class T>
class Mailbox {
public:
    ~Mailbox() {
        delete pending_.load();
        delete trash_.load();
        delete current_;
    }
    void post(T* p) {                       // worker
        collect();
        delete pending_.exchange(p);
    }
    void collect() { delete trash_.exchange(nullptr); }     // worker
    T* take() {                             // audio thread
        if (trash_.load() == nullptr)
            if (T* p = pending_.exchange(nullptr)) {
                trash_.store(current_);
                current_ = p;
            }
        return current_;
    }
    T* current() const { return current_; } // audio thread

private:
    std::atomic<T*> pending_{nullptr}, trash_{nullptr};
    T* current_ = nullptr;
};

struct Transport {
    bool playing = false;
    double beat = 0.0;               // position of the first sample of the block
    double beats_per_sample = 0.0;
    bool loop = true;                // the song starts again at its end
};

class Engine {
public:
    enum class MidiStrings { automatic, channel_per_string };
    MidiStrings midi_strings = MidiStrings::automatic;
    bool live_palm = false;
    double live_velocity_curve = 1.0;

    // what the interface shows (written by the audio thread)
    std::array<std::atomic<int>, NUM_STRINGS> shown_fret{};
    std::array<std::atomic<float>, NUM_STRINGS> shown_level{};
    std::atomic<double> shown_beat{-1.0};

    void prepare(double sample_rate) {
        sr_ = sample_rate;
        for (auto& b : string_force_) b.assign(BLOCK, 0.0);
        total_.assign(BLOCK, 0.0);
        others_.assign(BLOCK, 0.0);
        mix_.assign(BLOCK, 0.0);
        for (auto& v : voices_) v.active = false;
        for (auto& v : fading_) v.active = false;
        for (auto& s : squeaks_) s.clear();
        sym_set_ = false;
    }

    // between two blocks, on the audio thread
    void set_model(const Model* m) {
        if (m == model_) return;
        model_ = m;
        if (model_ && !sym_set_) {
            // at rest every string is free to answer the others (open strings)
            for (int i = 0; i < NUM_STRINGS; ++i) free_string(i, 0, 0.05);
            sym_set_ = true;
        }
    }
    void set_performance(const Performance* p) {
        if (p == perf_) return;
        // the finger noises sounding are samples of the old performance, which the worker frees soon after:
        // they stop here (a new take, an edit or a player setting while a squeak plays)
        for (auto& s : squeaks_) s.clear();
        perf_ = p;
        relocate_ = true;
    }
    const Model* model() const { return model_; }
    const Performance* performance() const { return perf_; }

    // ------------------------------------------------------------ live playing
    void note_on(int midi, double velocity, int channel = 1) {
        if (!model_) return;
        int best = -1, best_score = 1 << 20;
        if (midi_strings == MidiStrings::channel_per_string && channel >= 1 && channel <= 6) {
            best = 6 - channel;                    // channel 1 = string 1 (high E)
        } else {
            // the string that can play it lowest on the neck, not already holding another key
            for (int i = NUM_STRINGS - 1; i >= 0; --i) {
                const int f = midi - model_->open_midi(i);
                if (f < 0 || f >= NUM_FRETS) continue;
                const bool busy = voices_[(size_t) i].active && voices_[(size_t) i].note >= 0;
                const int score = f + (busy ? 100 : 0);
                if (score < best_score) {
                    best_score = score;
                    best = i;
                }
            }
        }
        if (best < 0) return;
        const int fret = midi - model_->open_midi(best);
        if (fret < 0 || fret >= NUM_FRETS) return;
        pluck_string(best, fret, velocity);
        voices_[(size_t) best].note = midi;
    }
    void note_off(int midi) {
        for (int i = 0; i < NUM_STRINGS; ++i)
            if (voices_[(size_t) i].active && voices_[(size_t) i].note == midi) voices_[(size_t) i].damp();
    }
    void all_off() {
        for (auto& v : voices_)
            if (v.active) v.damp();
    }
    // a string struck by hand (MIDI, a click on the neck): attack network of the model (set force and place)
    void pluck_string(int i, int fret, double velocity) {
        if (!model_) return;
        const Model& m = *model_;
        Pluck p;
        p.string = i;
        p.fret = fret;
        p.place = m.place[(size_t) i][(size_t) fret];
        p.gain = m.y0[(size_t) i][(size_t) fret] / Y_REF * std::pow(std::max(velocity, 0.0) / 0.8, live_velocity_curve);
        p.tau = m.tau[(size_t) i][(size_t) fret];
        p.pluck = m.s.pluck;
        p.kind = Kind::strike;
        p.palm = (live_palm && m.palm_ready) ? m.palm[(size_t) i][(size_t) fret].get() : nullptr;
        start(p);
    }

    // ------------------------------------------------------------ the block
    void render(float* out, int samples, const Transport& tr) {
        if (live_palm != live_palm_was_) {
            // the palm comes down on the strings: what rings is damped at once (the palm takes the high modes
            // first), and the free strings are under it; lifted, the free strings ring open again
            live_palm_was_ = live_palm;
            if (live_palm && model_ && model_->palm_ready)
                for (auto& v : voices_)
                    if (v.active) v.damp(LIVE_PALM_DAMP);
            set_palm(live_palm || song_palm_);
        }
        int pos = 0;
        while (pos < samples) {
            const int n = std::min(samples - pos, BLOCK);
            render_part(out + pos, n, tr, pos);
            pos += n;
        }
        if (was_playing_ && !tr.playing) {             // the transport stops: the hands stop the strings
            all_off();
            song_palm_ = false;
            set_palm(live_palm);
        }
        was_playing_ = tr.playing;
    }

private:
    static constexpr int BLOCK = 256;
    double sr_ = 48000.0;
    const Model* model_ = nullptr;
    const Performance* perf_ = nullptr;
    std::array<StringVoice, NUM_STRINGS> voices_{}, fading_{};
    std::array<SympatheticString, NUM_STRINGS> sym_{};
    struct SqueakPlay { const std::vector<float>* sound = nullptr; size_t at = 0; int string = 0; void clear() { sound = nullptr; } };
    std::array<SqueakPlay, NUM_STRINGS * 2> squeaks_{};
    std::array<std::vector<double>, NUM_STRINGS> string_force_;
    std::vector<double> total_, others_, mix_;
    bool sym_set_ = false, relocate_ = true, was_playing_ = false, palm_down_ = false;
    bool song_palm_ = false, live_palm_was_ = false;
    static constexpr double LIVE_PALM_DAMP = 60.0;   // s^-1: a palm laid on ringing strings
    size_t next_ = 0;
    double expected_beat_ = -1.0;

    void set_palm(bool down) {
        if (down == palm_down_) return;
        palm_down_ = down;
        for (int i = 0; i < NUM_STRINGS; ++i)       // the free strings are under the palm too
            if (sym_[(size_t) i].on) free_string(i, sym_[(size_t) i].fret, 0.005);
    }

    void free_string(int i, int fret, double fade) {
        if (!model_) return;
        const Model& m = *model_;
        const Palm* palm = (palm_down_ && m.palm_ready) ? m.palm[(size_t) i][(size_t) fret].get() : nullptr;
        auto& s = sym_[(size_t) i];
        s.fret = fret;
        s.palm = palm != nullptr;
        s.set(m.place[(size_t) i][(size_t) fret], palm, m.body, m.s.resonance, sr_);
        s.free_from_now(fade);
    }

    void start(const Pluck& p0) {
        const Model& m = *model_;
        Pluck p = p0;
        // the live palm mutes everything struck while it is down, the roll included
        if (live_palm && !p.palm && p.kind == Kind::strike && m.palm_ready)
            p.palm = m.palm[(size_t) p.string][(size_t) p.fret].get();
        auto& v = voices_[(size_t) p.string];
        if (v.active) {                           // struck again: the pick takes the old vibration away (20 ms)
            fading_[(size_t) p.string] = v;
            fading_[(size_t) p.string].fade_out(0.02);
        }
        v.start(p, m.body, m.s.resonance, m.s.brightness, sr_);
        v.note = -1;
        shown_fret[(size_t) p.string].store(p.fret);
        if (!m.s.sympathy) return;
        if (p.kind == Kind::strike) sym_[(size_t) p.string].hold();
        else free_string(p.string, p.fret, 0.005);      // after a lift or a landing the string is free
    }

    void apply(const Event& e) {
        switch (e.type) {
            case EventType::pluck: start(e.pluck); break;
            case EventType::damp: voices_[(size_t) e.string].damp(e.extra); break;
            case EventType::squeak:
                for (auto& s : squeaks_)
                    if (!s.sound) {
                        s.sound = &perf_->squeaks[(size_t) e.squeak];
                        s.at = 0;
                        s.string = e.string;
                        break;
                    }
                break;
            case EventType::palm_on:
            case EventType::palm_off:
                song_palm_ = e.type == EventType::palm_on;
                set_palm(song_palm_ || live_palm);
                break;
        }
    }

    // `n` samples (n <= BLOCK): song events at their sample, strings, bridge
    void render_part(float* out, int n, const Transport& tr, int offset) {
        int pos = 0;
        const bool song = tr.playing && perf_ && model_ && tr.beats_per_sample > 0;
        double beat = tr.beat + offset * tr.beats_per_sample;
        if (song) {
            const double len = perf_->length_beats;
            if (tr.loop && len > 0) beat = std::fmod(std::max(beat, 0.0), len);
            if (relocate_ || std::abs(beat - expected_beat_) > 1e-3) {
                next_ = perf_->first_at(beat);
                relocate_ = false;
            }
            while (pos < n) {
                // the next song event, or the end of the song (loop), inside this part?
                int until = n;
                bool wrap = false;
                if (tr.loop && len > 0) {
                    const double to_end = (len - beat) / tr.beats_per_sample;
                    if (to_end < until - pos) {
                        until = pos + std::max(0, (int) std::ceil(to_end));
                        wrap = true;
                    }
                }
                while (next_ < perf_->events.size()) {
                    const double at = (perf_->events[next_].beat - beat) / tr.beats_per_sample;
                    if (at >= until - pos) break;
                    const int k = pos + std::max(0, (int) std::floor(at));
                    if (k > pos) {
                        synth(out + pos, k - pos);
                        beat += (k - pos) * tr.beats_per_sample;
                        pos = k;
                    }
                    apply(perf_->events[next_++]);
                }
                if (until > pos) {
                    synth(out + pos, until - pos);
                    beat += (until - pos) * tr.beats_per_sample;
                    pos = until;
                }
                if (wrap) {
                    beat = 0.0;
                    next_ = 0;
                    if (until == pos && pos < n && n - pos > 0) continue;
                }
            }
            expected_beat_ = beat;
            shown_beat.store(beat);
        } else {
            synth(out, n);
            shown_beat.store(tr.playing ? beat : -1.0);
        }
    }

    void synth(float* out, int n) {
        if (n <= 0) return;
        for (int t = 0; t < n; ++t) total_[(size_t) t] = 0.0;
        for (int i = 0; i < NUM_STRINGS; ++i) {
            auto& f = string_force_[(size_t) i];
            for (int t = 0; t < n; ++t) f[(size_t) t] = 0.0;
            voices_[(size_t) i].render(f.data(), n);
            fading_[(size_t) i].render(f.data(), n);
        }
        for (auto& s : squeaks_) {
            if (!s.sound) continue;
            // a finger noise pushes on the bridge through its own string (which does not answer itself)
            const auto& snd = *s.sound;
            auto& f = string_force_[(size_t) s.string];
            for (int t = 0; t < n && s.at < snd.size(); ++t) f[(size_t) t] += snd[s.at++];
            if (s.at >= snd.size()) s.clear();
        }
        float level[NUM_STRINGS] = {};
        for (int i = 0; i < NUM_STRINGS; ++i) {
            const auto& f = string_force_[(size_t) i];
            for (int t = 0; t < n; ++t) {
                total_[(size_t) t] += f[(size_t) t];
                level[i] = std::max(level[i], (float) std::abs(f[(size_t) t]));
            }
        }
        for (int t = 0; t < n; ++t) mix_[(size_t) t] = total_[(size_t) t];
        if (model_ && model_->s.sympathy) {
            for (int i = 0; i < NUM_STRINGS; ++i) {
                if (!sym_[(size_t) i].on) continue;
                const auto& f = string_force_[(size_t) i];
                for (int t = 0; t < n; ++t) others_[(size_t) t] = total_[(size_t) t] - f[(size_t) t];
                sym_[(size_t) i].render(others_.data(), mix_.data(), n);
            }
        }
        for (int t = 0; t < n; ++t) out[t] = (float) mix_[(size_t) t];
        for (int i = 0; i < NUM_STRINGS; ++i)
            shown_level[(size_t) i].store(std::max(level[i], shown_level[(size_t) i].load() * 0.97f));
    }
};

}  // namespace grmg
