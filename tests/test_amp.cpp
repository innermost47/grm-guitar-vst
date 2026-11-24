// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// The amplifier: its networks (operating points, tone stack, cabinet) and the rig as it plays (clean stays clean,
// high gain compresses, little folds back, the gate closes, presence lifts the highs, the pedal tightens the lows).
#include <chrono>
#include <cmath>
#include <complex>
#include <cstdio>
#include <vector>

#include "../core/amp/amp_networks.hpp"
#include "../core/amp/amp_rig.hpp"

using namespace grmg;

static int failures = 0, checks = 0;
static void ok(const char *name, bool pass)
{
    ++checks;
    if (!pass)
    {
        ++failures;
        std::printf("FAIL %s\n", name);
    }
}

static const double SR = 48000.0;

// the level of the component at f (Goertzel), over the whole buffer from `from`
static double tone_at(const std::vector<double> &y, double f, size_t from)
{
    const double w = 2 * PI * f / SR;
    double re = 0, im = 0;
    for (size_t i = from; i < y.size(); ++i)
    {
        re += y[i] * std::cos(w * (double)i);
        im += y[i] * std::sin(w * (double)i);
    }
    return 2.0 * std::sqrt(re * re + im * im) / (double)(y.size() - from);
}

static double rms(const std::vector<double> &y, size_t from, size_t to)
{
    double e = 0;
    for (size_t i = from; i < to; ++i)
        e += y[i] * y[i];
    return std::sqrt(e / (double)(to - from));
}

static double db(double x) { return 20.0 * std::log10(std::max(x, 1e-30)); }

// the rig playing a sine of `volts` at f for `seconds` (then `silence` seconds of nothing)
static std::vector<double> play(const models::AmpModel &model, double f, double volts, double seconds, double silence = 0.0)
{
    AmpRig rig;
    rig.prepare(SR);
    rig.set_model(model);
    const size_t n = (size_t)(seconds * SR), quiet = (size_t)(silence * SR);
    std::vector<float> buffer(n + quiet, 0.0f);
    for (size_t i = 0; i < n; ++i)
        buffer[i] = (float)(volts / PICKUP_VOLTS * std::sin(2 * PI * f * (double)i / SR));
    for (size_t p = 0; p < buffer.size(); p += 256)
        rig.process(buffer.data() + p, (int)std::min<size_t>(256, buffer.size() - p));
    return std::vector<double>(buffer.begin(), buffer.end());
}

// the harmonics' share of the fundamental (from 0.5 s on: the amplifier settled)
static double distortion(const std::vector<double> &y, double f)
{
    const size_t from = (size_t)(0.5 * SR);
    double h = 0;
    for (int k = 2; k * f < 0.45 * SR; ++k)
        h += std::pow(tone_at(y, k * f, from), 2);
    return std::sqrt(h) / std::max(tone_at(y, f, from), 1e-30);
}

int main()
{
    // ---- the preamp network: every voicing has its triodes at a sane operating point, its supply shared
    for (const auto &v : AMP_VOICINGS)
    {
        const auto t0 = std::chrono::steady_clock::now();
        const auto stages = preamp_network(v);
        const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        bool sane = stages.size() == v.stages.size();
        for (const auto &s : stages)
            std::printf("       stage: Vp %.0f V, Ip %.3f mA, Vk %.2f V, gain %.1f, curve %.1f .. %.1f V\n", s.Vp, s.Ip * 1e3, s.Vk, s.gain, s.curve.front(), s.curve.back());
        for (const auto &s : stages)
            sane = sane && s.Ip > 0.03e-3 && s.Ip < 4e-3 && s.Vp > 0.25 * v.B && s.Vp < 0.95 * v.B && s.gain > 1.5 && s.curve.size() > 2;
        std::printf("     %s: %zu stages, first at %.0f V %.2f mA gain %.1f, solved in %.0f ms\n", v.name, stages.size(),
                    stages[0].Vp, stages[0].Ip * 1e3, stages[0].gain, ms);
        ok("preamp: every triode at a sane operating point", sane);
        // the curve is asymmetric: a triode makes even harmonics
        const auto &s = stages[0];
        const double down = std::abs(s.curve.front()), up = std::abs(s.curve.back());
        ok("preamp: the triode's curve is asymmetric", std::abs(down - up) > 0.1 * std::max(down, up));
    }

    // ---- the tone stack: its knobs do what they say, its digital response is its H(s)
    for (const auto &v : AMP_VOICINGS)
    {
        auto response = [&](double bass, double middle, double treble, double f)
        {
            const auto m = tone_stack_network(v.tone, bass, middle, treble);
            const std::complex<double> s(0.0, 2 * PI * f);
            const auto num = m.b[1] * s + m.b[2] * s * s + m.b[3] * s * s * s;
            const auto den = 1.0 + m.a[1] * s + m.a[2] * s * s + m.a[3] * s * s * s;
            return std::abs(num / den);
        };
        ok("tone stack: bass lifts the lows", response(1, 0.5, 0.5, 80) > 1.5 * response(0, 0.5, 0.5, 80));
        ok("tone stack: middle lifts the mids", response(0.5, 1, 0.5, 500) > 1.5 * response(0.5, 0, 0.5, 500));
        ok("tone stack: treble lifts the highs", response(0.5, 0.5, 1, 5000) > 1.5 * response(0.5, 0.5, 0, 5000));
        const auto m = tone_stack_network(v.tone, 0.5, 0.5, 0.5);
        ToneStack ts;
        const double os = SR * AMP_OVERSAMPLE;
        ts.set_model(m, os);
        bool matches = true;
        for (double f : {100.0, 1000.0, 5000.0})
        {
            std::vector<double> y((size_t)(0.25 * os));
            for (size_t i = 0; i < y.size(); ++i)
                y[i] = ts.process(std::sin(2 * PI * f * (double)i / os));
            double peak = 0;
            for (size_t i = y.size() / 2; i < y.size(); ++i)
                peak = std::max(peak, std::abs(y[i]));
            matches = matches && std::abs(db(peak) - db(response(0.5, 0.5, 0.5, f))) < 0.5;
        }
        ok("tone stack: the digital filter is the circuit's H(s)", matches);
    }
    {
        const auto fender = tone_stack_network(AMP_VOICINGS[0].tone, 0.5, 0.5, 0.5);
        auto at = [&](double f)
        {
            const std::complex<double> s(0.0, 2 * PI * f);
            return std::abs((fender.b[1] * s + fender.b[2] * s * s + fender.b[3] * s * s * s) /
                            (1.0 + fender.a[1] * s + fender.a[2] * s * s + fender.a[3] * s * s * s));
        };
        std::printf("     American Clean tone stack at noon: 100 Hz %.1f dB, 500 Hz %.1f dB, 5 kHz %.1f dB\n", db(at(100)), db(at(500)), db(at(5000)));
        ok("tone stack: a Fender stack scoops the mids", at(500) < at(100) && at(500) < at(5000));
    }

    // ---- the cabinet: the closed box resonates above the speaker alone, the top falls steeply
    {
        const auto cab = cabinet_network(CABINETS[2], 0.3);
        std::printf("     4x12: box resonance %.0f Hz, Q %.2f\n", cab.fc, cab.Qtc);
        ok("cabinet: the closed box raises the resonance", cab.fc > CABINETS[2].fs && cab.Qtc > CABINETS[2].Qts);
        Cabinet c;
        c.set_model(cab, SR);
        auto level = [&](double f)
        {
            c.reset();
            double peak = 0;
            for (int i = 0; i < 24000; ++i)
            {
                const double y = c.process(std::sin(2 * PI * f * i / SR));
                if (i > 12000)
                    peak = std::max(peak, std::abs(y));
            }
            return peak;
        };
        const double fall = db(level(2000)) - db(level(10000));
        std::printf("     4x12: 2 kHz to 10 kHz falls %.1f dB\n", fall);
        ok("cabinet: the speaker takes the fizz off (steep fall above 5 kHz)", fall > 24.0);
    }

    // ---- the rig as it plays
    models::AmpSettings clean;
    clean.on = true;
    clean.mix = 0.0;
    clean.gate_db = GATE_OFF_DB;
    models::AmpSettings lead = clean;
    lead.mix = 1.0;
    lead.lead.gain = 7.0;
    const auto clean_model = amp_model(clean, nullptr), lead_model = amp_model(lead, nullptr);
    {
        const double thd_clean = distortion(play(clean_model, 220.0, 0.1, 1.0), 220.0);
        const double thd_lead = distortion(play(lead_model, 220.0, 0.1, 1.0), 220.0);
        std::printf("     rig: distortion of a 0.1 V note: clean %.1f %%, lead %.1f %%\n", 100 * thd_clean, 100 * thd_lead);
        ok("rig: the clean channel stays clean at a soft touch", thd_clean < 0.05);
        ok("rig: the high gain channel distorts", thd_lead > 0.3);
        const double soft = rms(play(lead_model, 220.0, 0.1, 1.0), (size_t)(0.5 * SR), (size_t)SR);
        const double hard = rms(play(lead_model, 220.0, 0.4, 1.0), (size_t)(0.5 * SR), (size_t)SR);
        std::printf("     rig: lead, 12 dB more guitar gives %.1f dB more\n", db(hard) - db(soft));
        ok("rig: the high gain channel compresses", db(hard) - db(soft) < 4.0);
        bool finite = true;
        double mean = 0;
        const auto y = play(lead_model, 110.0, 0.5, 2.0);
        for (double v : y)
        {
            finite = finite && std::isfinite(v);
            mean += v;
        }
        ok("rig: finite", finite);
        ok("rig: no DC", std::abs(mean / (double)y.size()) < 0.01 * rms(y, 0, y.size()));
    }
    {
        // little folds back: a high note through the high gain channel, its harmonics against everything else
        const double f = 3500.0;
        const auto y = play(lead_model, f, 0.2, 1.5);
        const size_t from = (size_t)(0.5 * SR);
        double harmonics = 0;
        for (int k = 1; k * f < 0.5 * SR; ++k)
            harmonics += std::pow(tone_at(y, k * f, from), 2) / 2.0;
        const double total = std::pow(rms(y, from, y.size()), 2);
        const double folded = std::max(total - harmonics, 0.0) / total;
        std::printf("     rig: aliasing of a 3.5 kHz note at high gain: %.2f %% of the energy\n", 100 * folded);
        ok("rig: oversampled, little folds back", folded < 0.02);
    }
    {
        // the gate: the amplifier hisses at high gain; the gate keyed by the guitar silences it between the notes
        models::AmpSettings hiss = lead;
        hiss.lead.gain = 10.0;
        const auto open = play(amp_model(hiss, nullptr), 110.0, 0.2, 0.5, 0.5);
        hiss.gate_db = -50.0;
        const auto gated = play(amp_model(hiss, nullptr), 110.0, 0.2, 0.5, 0.5);
        const size_t a = (size_t)(0.8 * SR), b = (size_t)(1.0 * SR);
        std::printf("     gate: hiss after the note %.1f dB open, %.1f dB gated (note %.1f dB)\n", db(rms(open, a, b)),
                    db(rms(gated, a, b)), db(rms(open, (size_t)(0.2 * SR), (size_t)(0.4 * SR))));
        ok("gate: the amplifier hisses at high gain", rms(open, a, b) > 1e-4 * rms(open, (size_t)(0.2 * SR), (size_t)(0.4 * SR)));
        ok("gate: closed between the notes", rms(gated, a, b) < 0.01 * rms(open, a, b));
        ok("gate: open on the note", rms(gated, (size_t)(0.2 * SR), (size_t)(0.4 * SR)) > 0.9 * rms(open, (size_t)(0.2 * SR), (size_t)(0.4 * SR)));
    }
    {
        // presence: less feedback above its corner, more highs
        auto highs_over_lows = [&](double presence)
        {
            models::AmpSettings s = clean;
            s.clean.presence = presence;
            const auto m = amp_model(s, nullptr);
            return tone_at(play(m, 4000.0, 0.02, 1.0), 4000.0, (size_t)(0.5 * SR)) /
                   tone_at(play(m, 200.0, 0.02, 1.0), 200.0, (size_t)(0.5 * SR));
        };
        const double lift = db(highs_over_lows(1.0)) - db(highs_over_lows(0.0));
        std::printf("     power stage: presence lifts 4 kHz by %.1f dB\n", lift);
        ok("power stage: presence lifts the highs", lift > 1.5);
    }
    {
        // the pedal: it clips what lies above its corner, the lows pass it nearly clean (tight, for metal)
        models::OverdriveSettings on;
        on.on = true;
        on.drive = 0.7;
        on.level = 0.5;
        const auto m = overdrive_network(on);
        // a soft note through it: its gain at f (the fundamental out over in)
        auto pedal = [&](double f)
        {
            Overdrive od;
            const double os = SR * AMP_OVERSAMPLE;
            od.set_model(m, os);
            const double in = 0.005;
            const size_t n = (size_t)os, from = n / 2;
            const double w = 2 * PI * f / os;
            double re = 0, im = 0;
            for (size_t i = 0; i < n; ++i)
            {
                const double y = od.process(in * std::sin(w * (double)i));
                if (i >= from)
                {
                    re += y * std::cos(w * (double)i);
                    im += y * std::sin(w * (double)i);
                }
            }
            return 2.0 * std::sqrt(re * re + im * im) / (double)(n - from) / in;
        };
        std::printf("     overdrive: gain %.0f above %.0f Hz; a soft note comes out x%.2f at 80 Hz, x%.2f at 1 kHz\n", m.gain, m.hp_hz, pedal(80), pedal(1000));
        ok("overdrive: it pushes the mids, the lows pass (a tight low end)", pedal(1000) > 2.0 * pedal(80));
    }

    std::printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
