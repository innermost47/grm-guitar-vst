// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// timbre_probe: the yardstick of the guitar's sound. Fixed scenarios rendered offline (WAV) and measured (CSV), so
// that a change of the engine can be heard and compared, before and after:
//   - single notes under a magnetic pickup alone (no amplifier): the decay of their partials, their pitch (the
//     glide of the attack, the tuning of a fretted note), their brightness through time; an acoustic for reference;
//   - a power chord struck again and again into a high-gain amplifier, from the guitar roll (with and without the
//     player's human noise) and from a MIDI keyboard: how much each stroke differs from the others;
//   - what the model holds before any sound: the fretted notes' tuning, the beats of a power chord's partials, the
//     bumps of the cabinets' responses.
// Usage: timbre_probe [out_dir]   (default: timbre_probe_out). Not in ctest: run it by hand.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "../core/amp/amp_networks.hpp"
#include "../core/amp/amp_rig.hpp"
#include "../core/amp/cabinet.hpp"
#include "../core/offline.hpp"
#include "../core/offline_renderer.hpp"
#include "support/rigs.hpp"
#include "support/wav.hpp"
#include "timbre/metrics_table.hpp"
#include "timbre/partial_tracker.hpp"
#include "timbre/spectrum_probe.hpp"
#include "timbre/sound_check.hpp"
#include "timbre/stroke_spread.hpp"
#include "timbre/sustain_movement.hpp"

namespace
{
    using namespace grmg;

    constexpr int SR = 48000;
    constexpr double TEMPO = 120.0;           // two beats a second
    constexpr int DROP_D = 2;                 // TUNINGS
    constexpr double PICKUP_BRIDGE_MM = 60.0; // from the saddle
    constexpr double PICKUP_NECK_MM = 160.0;
    constexpr int PARTIALS[] = {1, 2, 3, 5, 8};
    constexpr double NOTE_S = 3.0;  // a single note rings this long
    constexpr int STROKES = 8;      // the power chord, struck this many times
    constexpr double STROKE_BEATS = 1.0;
    constexpr double HELD_S = 2.5;          // a held chord rings this long
    constexpr double MOVEMENT_FROM_S = 0.3; // its movement is measured past its attack

    Song single_note(int string, int fret)
    {
        Song s;
        s.bars = 2;
        s.tempo = TEMPO;
        models::RollNote n;
        n.string = string;
        n.fret = fret;
        n.start = 0.0;
        n.length = NOTE_S * TEMPO / 60.0;
        s.notes.push_back(n);
        return s;
    }

    // the D5 of a drop D tuning (the three lowest strings, open or at `fret`), struck on every beat
    Song power_chord(int fret, bool palm)
    {
        Song s;
        s.bars = 2;
        s.tempo = TEMPO;
        for (int k = 0; k < STROKES; ++k)
            for (int string = 0; string < 3; ++string)
            {
                models::RollNote n;
                n.string = string;
                n.fret = fret;
                n.start = k * STROKE_BEATS;
                n.length = 0.9 * STROKE_BEATS;
                n.palm = palm;
                s.notes.push_back(n);
            }
        return s;
    }

    // the same chord struck once and held
    Song held_chord(int fret)
    {
        Song s;
        s.bars = 2;
        s.tempo = TEMPO;
        for (int string = 0; string < 3; ++string)
        {
            models::RollNote n;
            n.string = string;
            n.fret = fret;
            n.start = 0.0;
            n.length = HELD_S * TEMPO / 60.0;
            s.notes.push_back(n);
        }
        return s;
    }

    std::vector<models::LiveNote> power_chord_keys(const Model &m, int fret)
    {
        std::vector<models::LiveNote> keys;
        const double beat_s = 60.0 / TEMPO;
        for (int k = 0; k < STROKES; ++k)
            for (int string = 0; string < 3; ++string)
                keys.push_back({k * STROKE_BEATS * beat_s, (k + 0.9) * STROKE_BEATS * beat_s, m.open_midi(string) + fret, 0.8, 1});
        return keys;
    }

    std::vector<double> stroke_times()
    {
        std::vector<double> t;
        for (int k = 0; k < STROKES; ++k)
            t.push_back(k * STROKE_BEATS * 60.0 / TEMPO);
        return t;
    }

    std::string note_name(int string, int fret) { return "s" + std::to_string(string) + "f" + std::to_string(fret); }

    // one note's partials, pitch and brightness, in the signal a listener gets (the pickup, or the force on the bridge)
    void measure_note(timbre::MetricsTable &table, const std::string &scenario, const std::vector<double> &x, const Model &m,
                      int string, int fret)
    {
        const auto &pl = m.place[(size_t)string][(size_t)fret];
        const double tempered = midi_hz(m.open_midi(string) + fret);
        timbre::PartialTracker tracker(SR);
        for (int n : PARTIALS)
        {
            const double f = n * pl.f1 * std::sqrt(1.0 + pl.B * n * n);
            if (f > 0.45 * SR)
                continue;
            tracker.track(x, f, pl.f1, 0.0, NOTE_S);
            const double t60 = tracker.t60();
            table.add(scenario, "T60 partial " + std::to_string(n), t60, "s");
            // its quality factor, as Paté et al. 2014 give them: Q = pi f T60 / ln(1000)
            table.add(scenario, "Q partial " + std::to_string(n), PI * f * t60 / std::log(1000.0), "");
            if (n == 1)
            {
                const double settled = tracker.median_frequency(0.4, 1.0);
                table.add(scenario, "pitch vs tempered", timbre::PartialTracker::cents(settled, tempered), "cents");
                table.add(scenario, "attack glide", timbre::PartialTracker::cents(tracker.median_frequency(0.02, 0.06), settled), "cents");
            }
        }
        const timbre::SpectrumProbe spectrum(SR);
        table.add(scenario, "centroid attack", spectrum.centroid(x, 0), "Hz");
        table.add(scenario, "centroid 100 ms", spectrum.centroid(x, (size_t)(0.1 * SR)), "Hz");
        table.add(scenario, "centroid 1 s", spectrum.centroid(x, (size_t)(1.0 * SR)), "Hz");
        table.add(scenario, "95 % of the energy below", spectrum.energy_below_hz(x, 0.95), "Hz");
    }

    void single_notes(timbre::MetricsTable &table, timbre::SoundCheck &checked, const std::filesystem::path &out)
    {
        std::printf("\n== single notes\n");
        auto pickup_at = [](double mm)
        {
            auto s = rigs::named("pickup");
            s.pickup_mm = mm;
            s.human = false;
            return s;
        };
        auto acoustic = rigs::named("acoustic");
        acoustic.human = false;
        acoustic.room_on = false;
        // name, settings, whether the listener gets the pickup (else the force on the bridge, through the body)
        const std::tuple<std::string, models::Settings, bool> rigs_[] = {{"bridge pickup", pickup_at(PICKUP_BRIDGE_MM), true},
                                                                         {"neck pickup", pickup_at(PICKUP_NECK_MM), true},
                                                                         {"acoustic", acoustic, false}};
        const std::pair<int, int> notes[] = {{0, 0}, {0, 5}, {0, 12}, {3, 0}, {3, 5}, {5, 0}, {5, 12}};
        for (const auto &[name, settings, pickup] : rigs_)
        {
            OfflineRenderer renderer(settings, SR);
            for (const auto &[string, fret] : notes)
            {
                // the neck pickup and the acoustic: the open low string only, for comparison
                if (name != "bridge pickup" && !(string == 0 && fret == 0) && !(string == 5 && fret == 0))
                    continue;
                const auto take = renderer.render_song(single_note(string, fret), 7, 0.5);
                const std::string scenario = name + " " + note_name(string, fret);
                const auto heard = pickup ? take.pickup : body_and_room(take.force, renderer.model())[0];
                checked.check(scenario, heard);
                measure_note(table, scenario, heard, renderer.model(), string, fret);
                std::string file = scenario;
                std::replace(file.begin(), file.end(), ' ', '_');
                wav::write_mono((out / (file + ".wav")).string(), heard, SR);
            }
        }
    }

    // the strokes of one take, at the onsets given (found on the raw pickup: see StrokeSpread)
    void stroke_spread(timbre::MetricsTable &table, const std::string &scenario, const std::vector<double> &x,
                       const std::vector<double> &onsets, bool timing)
    {
        timbre::StrokeSpread spread(SR);
        spread.measure(x, stroke_times(), onsets);
        table.add(scenario, "strokes", (double)spread.strokes(), "");
        if (timing)
            table.add(scenario, "onset spread", spread.onset_sd_ms(), "ms");
        table.add(scenario, "level spread", spread.level_sd_db(), "dB");
        table.add(scenario, "brightness spread", spread.centroid_sd_pct(), "%");
        table.add(scenario, "spectral spread", spread.spectral_spread_db(), "dB");
        table.add(scenario, "brightness", spread.centroid_mean_hz(), "Hz");
    }

    // the high-gain amplifier after a pickup's take (the engine runs the same rig on the same signal)
    std::vector<double> through_amp(const models::Settings &metal, const std::vector<double> &pickup)
    {
        const auto model = amp_model(metal.amp, nullptr);
        AmpRig rig;
        rig.prepare(SR);
        rig.set_model(model);
        std::vector<float> buffer(pickup.begin(), pickup.end());
        for (size_t pos = 0; pos < buffer.size(); pos += OfflineRenderer::BLOCK)
            rig.process(buffer.data() + pos, (int)std::min<size_t>(OfflineRenderer::BLOCK, buffer.size() - pos));
        return std::vector<double>(buffer.begin(), buffer.end());
    }

    void power_chords(timbre::MetricsTable &table, timbre::SoundCheck &checked, const std::filesystem::path &out)
    {
        std::printf("\n== power chord D5 (drop D), struck %d times: the pickup, then the metal amplifier\n", STROKES);
        const auto metal = rigs::named("metal");
        for (bool human : {true, false})
        {
            auto s = rigs::named("pickup");
            s.tuning = DROP_D;
            s.pickup_mm = PICKUP_BRIDGE_MM;
            s.human = human;
            OfflineRenderer renderer(s, SR);
            const std::string base = human ? "human" : "exact";
            std::vector<std::pair<std::string, models::OfflineTake>> variants; // its name, what it gave
            variants.push_back({base + " roll", renderer.render_song(power_chord(0, false), 7, 1.0)});
            if (human)
            {
                variants.push_back({base + " roll palm", renderer.render_song(power_chord(0, true), 7, 1.0)});
                variants.push_back({base + " roll one plane", renderer.render_song(power_chord(0, false), 7, 1.0, models::Voicing::single_plane())});
            }
            else
            {
                variants.push_back({"keyboard", renderer.render_live(power_chord_keys(renderer.model(), 0), STROKES * 0.5 + 1.0)});
            }
            for (const auto &[name, take] : variants)
            {
                const auto onsets = timbre::StrokeSpread(SR).find_onsets(take.pickup, stroke_times());
                const auto amplified = through_amp(metal, take.pickup);
                checked.check(name, take.pickup);
                checked.check(name + " metal", amplified);
                stroke_spread(table, "pickup " + name, take.pickup, onsets, true);
                stroke_spread(table, "metal " + name, amplified, onsets, false);
                std::string file = "power_chord " + name;
                std::replace(file.begin(), file.end(), ' ', '_');
                wav::write_mono((out / (file + "_pickup.wav")).string(), take.pickup, SR);
                wav::write_mono((out / (file + "_metal.wav")).string(), amplified, SR);
            }
        }
    }

    // the same strokes under each type of pickup (PICKUP_TYPES), at the bridge: their tonal balance, to compare with
    // a real guitar's DI
    void pickup_types(timbre::MetricsTable &table, timbre::SoundCheck &checked, const std::filesystem::path &out)
    {
        std::printf("\n== power chord D5 under each pickup type\n");
        const timbre::SpectrumProbe spectrum(SR);
        for (int type = 0; type < NUM_PICKUP_TYPES; ++type)
        {
            auto s = rigs::named("pickup");
            s.tuning = DROP_D;
            s.pickup_mm = PICKUP_BRIDGE_MM;
            s.pickup_type = type;
            OfflineRenderer renderer(s, SR);
            const auto take = renderer.render_song(power_chord(0, false), 7, 1.0);
            const std::string name = std::string("pickup type ") + std::to_string(type);
            checked.check(name, take.pickup);
            table.add(name + " roll", "95 % of the energy below", spectrum.energy_below_hz(take.pickup, 0.95), "Hz");
            wav::write_mono((out / ("power_chord_pickup_type_" + std::to_string(type) + ".wav")).string(), take.pickup, SR);
        }
    }

    void held_chords(timbre::MetricsTable &table, timbre::SoundCheck &checked, const std::filesystem::path &out)
    {
        std::printf("\n== power chord held %.1f s: how much it moves inside\n", HELD_S);
        auto s = rigs::named("pickup");
        s.tuning = DROP_D;
        s.pickup_mm = PICKUP_BRIDGE_MM;
        OfflineRenderer renderer(s, SR);
        const timbre::SustainMovement movement(SR);
        for (int fret : {0, 3})
        {
            const auto take = renderer.render_song(held_chord(fret), 7, 0.5);
            const auto amplified = through_amp(rigs::named("metal"), take.pickup);
            const std::string name = "held D5 fret " + std::to_string(fret);
            checked.check(name, amplified);
            table.add("pickup " + name, "movement", movement.measure(take.pickup, MOVEMENT_FROM_S, HELD_S - 0.2), "dB");
            table.add("metal " + name, "movement", movement.measure(amplified, MOVEMENT_FROM_S, HELD_S - 0.2), "dB");
            std::string file = name;
            std::replace(file.begin(), file.end(), ' ', '_');
            wav::write_mono((out / (file + "_metal.wav")).string(), amplified, SR);
        }
    }

    // what the model holds before a sound: are its fretted notes tempered exactly, how fast do a power chord's
    // coinciding partials beat
    void model_statics(timbre::MetricsTable &table)
    {
        std::printf("\n== the model\n");
        auto s = rigs::named("pickup");
        s.tuning = DROP_D;
        Model m;
        m.solve(s, SR, nullptr, false);
        for (int fret : {1, 5, 12, 19})
        {
            const auto &pl = m.place[0][(size_t)fret];
            table.add("model s0f" + std::to_string(fret), "f1 vs tempered",
                      timbre::PartialTracker::cents(pl.f1, midi_hz(m.open_midi(0) + fret)), "cents");
        }
        auto partial = [&](int string, int n)
        {
            const auto &pl = m.place[(size_t)string][0];
            return n * pl.f1 * std::sqrt(1.0 + pl.B * n * n);
        };
        // D2 (string 0), A2 (string 1), D3 (string 2): the partials they share
        table.add("model D5 open", "beat root 3 / fifth 2", std::abs(partial(0, 3) - partial(1, 2)), "Hz");
        table.add("model D5 open", "beat root 2 / octave 1", std::abs(partial(0, 2) - partial(2, 1)), "Hz");
        table.add("model D5 open", "beat fifth 4 / octave 3", std::abs(partial(1, 4) - partial(2, 3)), "Hz");
    }

    void cabinets(timbre::MetricsTable &table, const std::filesystem::path &out)
    {
        std::printf("\n== cabinets (close microphone)\n");
        const timbre::SpectrumProbe spectrum(SR);
        const int count = (int)(sizeof(CABINETS) / sizeof(CABINETS[0]));
        for (int k = 0; k < count; ++k)
        {
            Cabinet cabinet;
            cabinet.set_model(cabinet_network(CABINETS[k], models::CabinetSettings{}.mic), SR);
            std::vector<double> impulse(16384, 0.0);
            for (size_t t = 0; t < impulse.size(); ++t)
                impulse[t] = cabinet.process(t == 0 ? 1.0 : 0.0);
            const std::string scenario = std::string("cab ") + CABINETS[k].name;
            const auto mid = spectrum.response_db(impulse, 1000.0, 6000.0, 240);
            table.add(scenario, "bumps > 1 dB, 1-6 kHz", timbre::SpectrumProbe::bumps(mid, 1.0), "");
            const auto wide = spectrum.response_db(impulse, 60.0, 10000.0, 400);
            table.add(scenario, "bumps > 1 dB, 60 Hz-10 kHz", timbre::SpectrumProbe::bumps(wide, 1.0), "");
            std::string file = scenario;
            std::replace(file.begin(), file.end(), ' ', '_');
            wav::write_mono((out / (file + ".wav")).string(), impulse, SR);
        }
    }
}

int main(int argc, char **argv)
{
    const std::filesystem::path out = argc > 1 ? argv[1] : "timbre_probe_out";
    std::filesystem::create_directories(out);
    grmg::timbre::MetricsTable table;
    grmg::timbre::SoundCheck checked;
    model_statics(table);
    cabinets(table, out);
    single_notes(table, checked, out);
    power_chords(table, checked, out);
    held_chords(table, checked, out);
    pickup_types(table, checked, out);
    const auto csv = (out / "metrics.csv").string();
    if (!table.write_csv(csv))
    {
        std::printf("FAILED: could not write %s\n", csv.c_str());
        return 1;
    }
    std::printf("\n%zu measures written to %s\n%s\n", table.size(), csv.c_str(), checked.ok() ? "all sounds finite and audible" : "SOME SOUNDS FAILED");
    return checked.ok() ? 0 : 1;
}
