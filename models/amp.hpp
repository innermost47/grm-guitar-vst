// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// amp.hpp
//
// The guitar amplifier: its circuits (component values, from the voicings of constants/core.hpp), the settings of
// the knobs, and what the networks of core/amp/amp_networks.hpp solve from both for the audio thread.
#pragma once

#include <array>
#include <tuple>
#include <vector>

namespace grmg::models
{
    // ------------------------------------------------------------------------------------------------ the circuits

    // one triode gain stage (half a 12AX7) and what feeds its grid
    struct TriodeCircuit
    {
        double Rp;  // plate load (ohm)
        double Rk;  // cathode resistor (ohm)
        double Ck;  // cathode bypass capacitor (F), 0: unbypassed
        double Rin; // what drives the grid: the previous plate (or the guitar) and the grid stopper (ohm)
        double Cc;  // the coupling capacitor into the grid (F)
        double Rg;  // the grid leak after it (ohm)
        double att; // the divider in front of the grid (share of the previous swing it passes)
    };

    // the tone stack, as Yeh & Smith name its parts: R1 the treble pot, R2 the bass pot, R3 the middle pot, R4 the
    // slope resistor; C1 the treble, C2 the bass, C3 the middle capacitor
    struct ToneStackParts
    {
        double R1, R2, R3, R4, C1, C2, C3;
    };

    // the push-pull power stage, its supply and its negative feedback
    struct PowerCircuit
    {
        double B;            // supply (V)
        double bias;         // the grid bias of the power tubes (V, as a positive number)
        double idle;         // the class AB idle conduction, as a share of the bias (0: class B)
        double pi_gain;      // the phase inverter's gain
        double knee;         // how hard the plates saturate: 2 (soft, EL34-like) or 4 (hard, 6L6-like)
        double supply_R;     // rectifier and choke resistance (ohm)
        double supply_C;     // the filter capacitor (F)
        double full_current; // the plate current at full power, both tubes (A)
        double nfb;          // the negative feedback ratio (output fed back to the phase inverter)
        double presence_hz;  // the presence control's corner (feedback removed above it)
        double resonance_hz; // the resonance control's corner (feedback removed below it)
        double lo_hz, hi_hz; // the output transformer's band
    };

    struct AmpVoicing
    {
        const char *name;
        double B;                          // the preamp's supply (V)
        double drop;                       // the decoupling resistor between two supply nodes (ohm)
        std::vector<TriodeCircuit> stages; // in signal order
        int gain_stage;                    // the stage whose grid the gain pot feeds
        int tone_after;                    // the tone stack sits after this stage
        double bright_hz;                  // the bright capacitor across the gain pot (0: none)
        ToneStackParts tone;
        PowerCircuit power;
    };

    // a speaker in its cabinet (Thiele-Small), and what a close microphone hears of it
    struct CabinetSpec
    {
        const char *name;
        int drivers;
        double fs;         // the speaker's free-air resonance (Hz)
        double Qts;        // its total Q
        double Vas;        // its equivalent compliance volume (m^3), per driver
        double Vb;         // the box volume (m^3), 0: open back
        double depth;      // front to back path of an open back (m)
        double breakup_hz; // the cone's first breakup peak
        double breakup_db;
        double notch_hz; // the dip under the breakup
        double notch_db;
        double top_hz; // where the cone stops radiating on axis
    };

    // ------------------------------------------------------------------------------------------------ the knobs

    struct AmpChannelSettings
    {
        int voicing = 0;
        double gain = 5.0; // 0 to 10, an audio taper pot
        double bass = 0.5, middle = 0.5, treble = 0.5;
        double presence = 0.5, resonance = 0.5;
        double master = 0.5;

        auto tied() const { return std::tie(voicing, gain, bass, middle, treble, presence, resonance, master); }
        bool operator==(const AmpChannelSettings &o) const { return tied() == o.tied(); }
    };

    struct OverdriveSettings
    {
        bool on = false;
        double drive = 0.5, tone = 0.5, level = 0.5;

        auto tied() const { return std::tie(on, drive, tone, level); }
        bool operator==(const OverdriveSettings &o) const { return tied() == o.tied(); }
    };

    struct CabinetSettings
    {
        int kind = 2;
        double mic = 0.3; // 0: on the cone's centre, 1: at its edge

        auto tied() const { return std::tie(kind, mic); }
        bool operator==(const CabinetSettings &o) const { return tied() == o.tied(); }
    };

    struct AmpSettings
    {
        bool on = false;
        AmpChannelSettings clean{0, 3.0, 0.5, 0.5, 0.55, 0.5, 0.4, 0.6};
        AmpChannelSettings lead{3, 6.5, 0.6, 0.45, 0.55, 0.55, 0.5, 0.5};
        double mix = 1.0; // 0: clean, 1: lead
        OverdriveSettings drive;
        CabinetSettings cabinet;
        double gate_db = -70.0; // the noise gate's threshold (dB of the guitar's volts); at the bottom: off
        double room_mix = 0.2;

        auto tied() const { return std::tie(on, clean, lead, mix, drive, cabinet, gate_db, room_mix); }
        bool operator==(const AmpSettings &o) const { return tied() == o.tied(); }
    };

    // the parameters of one channel of the plugin: its ids' prefix, its names' prefix, its voicing's id
    struct AmpChannelParams
    {
        const char *prefix, *label, *voicing;
        AmpChannelSettings AmpSettings::*channel;
    };

    // ------------------------------------------------------------------------------------------------ solved

    // one triode stage, as the preamp network solved it
    struct TriodeModel
    {
        double att = 1.0;        // the divider in front of the grid
        double grid_hz = 10.0;   // coupling capacitor and grid leak: a high-pass
        double miller_hz = 2e4;  // what drives the grid against its Miller capacitance: a low-pass
        double shelf_hz = 10.0;  // the cathode bypass capacitor's corner
        double shelf_gain = 1.0; // the gain below it, over the gain above it (unbypassed: 1, the curve has it)
        double grid_on = 0.0;    // the grid voltage where it starts to conduct (V)
        double grid_share = 0.0; // the share of a grid overshoot that stays, conducting: Rgk / (Rin + Rgk)
        double charge_s = 1.0;   // the coupling capacitor charged by the grid current: (Rin + Rgk) Cc (s)
        double leak_s = 1.0;     // and discharged through the grid leak: (Rg + Rin) Cc (s)
        double noise_density = 0.0; // the thermal noise of what drives the grid, sqrt(4 k T R) (V / sqrt(Hz))
        double Vp = 0.0, Ip = 0.0, Vk = 0.0, gain = 0.0; // the operating point and the small-signal gain
        // the plate swing (V, around rest) against the grid voltage (V), from curve_lo to curve_hi
        std::vector<double> curve;
        double curve_lo = 0.0, curve_hi = 0.0;
    };

    // the tone stack's H(s) = (b1 s + b2 s^2 + b3 s^3) / (1 + a1 s + a2 s^2 + a3 s^3)
    struct ToneStackModel
    {
        std::array<double, 4> b{}, a{1.0, 0.0, 0.0, 0.0};
    };

    struct PowerAmpModel
    {
        double input = 0.0;     // the master's share of the preamp's swing reaching the phase inverter
        double drive = 1.0;     // grid swing per volt in, over the bias (normalised: 1 drives a tube to cutoff)
        double idle = 0.1;      // class AB idle conduction
        double knee = 4.0;      // plate saturation hardness
        double sag_depth = 0.0; // the supply's drop at full power (share)
        double sag_s = 0.05;    // the supply's time constant
        double nfb = 0.0;       // feedback ratio (normalised output to normalised input)
        double presence = 0.0, presence_hz = 4000.0;
        double resonance = 0.0, resonance_hz = 100.0;
        double lo_hz = 40.0, hi_hz = 12000.0;
    };

    struct OverdriveModel
    {
        bool on = false;
        double gain = 1.0;      // the op-amp's gain above hp_hz
        double hp_hz = 720.0;   // only that part is amplified and clipped (the mid hump, tight lows)
        double lp_hz = 6000.0;  // the feedback capacitor's corner
        double tone_hz = 2000.0;
        double level = 1.0;
    };

    // a filter of the cabinet: the kinds a Biquad knows
    struct CabinetFilter
    {
        enum class Kind
        {
            highpass,
            lowpass,
            peak
        } kind;
        double hz, q, db;
    };

    struct CabinetModel
    {
        std::vector<CabinetFilter> filters;
        double fc = 0.0, Qtc = 0.0; // the box resonance, as the network solved it
    };

    struct AmpChannelModel
    {
        int voicing = -1;
        std::vector<TriodeModel> stages;
        int gain_stage = 0;      // the stage the gain pot feeds
        double pot = 1.0;        // the gain pot's share of the swing
        double bright_hz = 0.0;  // the bright capacitor around the pot (0: none)
        double bright_gain = 1.0; // how much it lifts the highs at this pot position
        int tone_after = 0;
        ToneStackModel tone;
        PowerAmpModel power;
    };

    struct AmpModel
    {
        AmpChannelModel clean, lead;
        OverdriveModel drive;
        CabinetModel cabinet;
        double mix = 1.0;
        double gate_db = -70.0;
    };
}
