// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <vector>
#include "../models/core.hpp"

namespace grmg
{
    constexpr double BETA_MODAL = 0.5;
    constexpr double BRACING = 4.0;
    constexpr double BRIDGE_SHARE = 0.2;
    constexpr double BRIDGE_VOLUME = 3.2e-5;
    constexpr double C_AIR_DEFAULT = 343.0;
    constexpr double DEAD_TOUCH = 0.002;
    constexpr double E_STEEL = 2.0e11;
    constexpr double FINGER_BEHIND = 0.010;
    constexpr double FINGER_CONTACT = 0.012;
    constexpr double FINGER_SPREAD = 0.022;
    constexpr double GAMMA_AIR = 1.4;
    constexpr double HAND_MASS = 0.04;
    constexpr double HAND_SIGMA = 0.015;
    constexpr double HAND_TENSION_MAX = 4.0;
    constexpr double K_FLESH = 2.0e3;
    constexpr double M_AIR = 0.028965;
    constexpr double MUSCLE_EFFORT = 0.04;
    constexpr double MUSCLE_RECOVER = 2.0;
    constexpr double MUSCLE_WEAKEN = 0.3;
    constexpr double NU_WOOD = 0.3;
    constexpr double PALM_EDGE = 0.45;
    constexpr double PALM_FORCE_FOLLOW = 0.3;
    constexpr double PALM_LAND_DAMP = 50.0;
    constexpr double PALM_LAND_SPREAD = 2.0;
    constexpr double PICK_CONTACT = 0.0015;
    constexpr double PICK_E = 2.5e9;
    constexpr double PICK_FREE = 0.004;
    constexpr double PICK_TIP_WIDTH = 0.005;
    constexpr double PI = 3.14159265358979323846;
    constexpr double PINCH_FORCE_SIGMA = 0.06;
    constexpr double PINCH_SPREAD = 0.004;
    constexpr double R_GAS = 8.314462618;
    constexpr double R_GRID_COND = 1.2e3;
    constexpr double RELEASE_DEAD = 400.0;
    constexpr double RELEASE_FRETTED = 32.0;
    constexpr double RELEASE_HAND = 45.0;
    constexpr double RELEASE_OPEN = 14.0;
    constexpr double RELEASE_SPREAD = 0.006;
    constexpr double RHO_BRIDGE = 850.0;
    constexpr double RHO_STEEL = 7850.0;
    constexpr double SIGMA_L = 0.0005;
    constexpr double SIGMA_MU_REL = 0.01;
    constexpr double SIGMA_T = 0.5;
    constexpr double SWEEP_LOSS_MAX = 0.6;
    constexpr double T_AIR = 293.15;
    constexpr double THUMB_LEAD = 0.003;
    constexpr double TIP_SIGMA = 0.003;
    constexpr double Y_REF = 0.003;

    constexpr double MODE_GAINS[7] = {0.9, 1.0, 0.55, 0.45, 0.35, 0.25, 0.18};
    constexpr double MUSCLE_LIMITS[2] = {0.75, 1.1};
    constexpr double PLATE_RATIOS[5] = {21.26 / 10.21, 34.88 / 10.21, 39.77 / 10.21, 51.04 / 10.21, 60.82 / 10.21};

    constexpr int MAX_MODES = 80;
    constexpr int NUM_FRETS = 20;
    constexpr int NUM_FRETS_UI = 20;
    constexpr int NUM_STRINGS = 6;
    constexpr int SYMPATHY_MODES = 40;

    // ---- the hard attack (Voicing): frets, pick contact, tension
    constexpr double FRET_RELAX_S = 0.0002;   // a string pressed onto a fret leaves it in about this (Zollner: 0.2 ms)
    constexpr double FRET_CLICK_S = 0.0003;   // the metal knock of a string on a fret dies in this
    constexpr double PUSH_GAIN_REF = 0.5;     // the release height (over Y_REF) the pick push time is given at
    constexpr double PUSH_MIN_S = 0.0002, PUSH_MAX_S = 0.02;
    constexpr double SLIP_RATE = 3000.0;      // slips per second while the string is pushed (several per millisecond)
    constexpr double SLIP_MAX_DEPTH = 0.15;   // the deepest slip, as a share of the displacement
    constexpr double SLIP_RECOVER_S = 0.0002; // the pick takes the string back in this
    constexpr double SCRAPE_DEPTH = 0.1;      // the scrape noise at full slider, as a share of the displacement
    constexpr double SCRAPE_CUTOFF_HZ = 6000.0;
    constexpr double TENSION_SMOOTH_S = 0.005; // the pitch follows the tension averaged over this
    constexpr double TENSION_GLIDE_MAX = 0.12; // the vibration raises the pitch by two semitones at most
    constexpr int TENSION_UPDATE = 32;        // samples between two retunings of the modes
    constexpr double TENSION_PULL_HP_HZ = 20.0;

    constexpr double ROOM_BANDS[6] = {125.0, 250.0, 500.0, 1000.0, 2000.0, 4000.0};
    constexpr double C_ROOM = 343.0;
    // the listener: the room's reverberation is solved once, heard from ROOM_REF_DISTANCE; moving the listener only
    // moves the direct sound (its way to the ears) and the level of the reverberation against it, which glide over
    // LISTENER_GLIDE_S (nothing to solve again, nothing to reload: no click)
    constexpr double ROOM_REF_DISTANCE = 1.0;
    constexpr double LISTENER_HEIGHT = 0.2;     // the ears above the guitar
    constexpr double EAR_SPACING = 0.09;        // the right ear, this much farther
    constexpr double LISTENER_REACH = 0.55;     // the listener stays within this share of the room's length
    constexpr double LISTENER_GLIDE_S = 0.05;
    constexpr double LISTENER_MAX_DELAY_S = 0.06; // the longest way to the ears (a church, its far end)
    constexpr double ROOM_SWITCH_S = 0.02;        // a guitar moved to another room fades from one to the other

    constexpr int MODE_QS[7] = {16, 22, 25, 28, 30, 32, 35};

    constexpr const char *ROOT_LABELS[12] = {"C", "C# / Db", "D", "D# / Eb", "E", "F", "F# / Gb", "G", "G# / Ab", "A", "A# / Bb", "B"};
    constexpr const char *ROOT_NAMES[12] = {"C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"};

    constexpr models::Material MATERIALS[6] = {
        {"Painted brick", {0.01, 0.01, 0.02, 0.02, 0.02, 0.03}},
        {"Plaster", {0.013, 0.015, 0.02, 0.03, 0.04, 0.05}},
        {"Wood panels", {0.28, 0.22, 0.17, 0.09, 0.10, 0.11}},
        {"Glass", {0.35, 0.25, 0.18, 0.12, 0.07, 0.04}},
        {"Curtains", {0.14, 0.35, 0.55, 0.72, 0.70, 0.65}},
        {"Carpet", {0.02, 0.06, 0.14, 0.37, 0.60, 0.65}}};

    constexpr models::RoomSize ROOMS[4] = {
        {"Small room", 3.5, 3.0, 2.5},
        {"Living room", 6.0, 4.5, 2.6},
        {"Studio", 9.0, 7.0, 3.5},
        {"Church", 30.0, 15.0, 14.0}};

    constexpr double BRACES_KG = 0.064, BODY_Q = 50.0;

    constexpr models::Wood WOODS[] = {
        {"Sitka spruce", 0.40 * 1120, 1.57 * 6.894757e9},
        {"Engelmann spruce", 0.35 * 1120, 1.30 * 6.894757e9},
        {"Western red cedar", 0.32 * 1120, 1.11 * 6.894757e9},
        {"Mahogany", 0.45 * 1120, 1.50 * 6.894757e9},
        {"Sugar maple", 0.63 * 1120, 1.83 * 6.894757e9},
        {"Red alder", 0.41 * 1120, 1.38 * 6.894757e9},
        {"Basswood", 0.37 * 1120, 1.46 * 6.894757e9},
        {"Black walnut", 0.55 * 1120, 1.68 * 6.894757e9}};

    constexpr int NUM_WOODS = (int)(sizeof(WOODS) / sizeof(WOODS[0]));
    constexpr double SLAB_M = 0.0445;

    // the circuits: a 70s Strat single coil, a hot humbucker (Lemme's average). How they sense the string: their poles
    // (about 5 mm across), a humbucker's two rows of poles about 18 mm apart, and the flux curves Novak et al. measured
    // (DAFx 2018, Fig. 9) on a Seymour Duncan SSL-5 single coil and an SH-2N humbucker
    constexpr models::PickupType PICKUP_TYPES[] = {
        {"Single coil (Strat 70s)", 2.2, 6.5e3, 110e-12, 1, 0.0, 2.5e-3, 21.51e-3, 12.98, 2.77},
        {"Humbucker, hot (Lemme average)", 4.1, 12e3, 130e-12, 2, 18e-3, 2.4e-3, 40.67e-3, 9.86, 1.34}};
    // the height a single coil's level is the engine's unit at (Novak et al. measured from 3 to 10 mm)
    constexpr double PICKUP_HEIGHT_REF_MM = 3.0;
    constexpr double PICKUP_HEIGHT_MIN_MM = 1.0; // the string never comes closer to a pole than this
    // the flux curve's slope is read from a table in the audio loop: its points from PICKUP_HEIGHT_MIN_MM on, this far
    constexpr int PICKUP_SLOPE_POINTS = 256;
    constexpr double PICKUP_SLOPE_SPAN_MM = 16.0;

    constexpr double PICKUP_LOAD = 1.0 / (1.0 / 250e3 + 1.0 / 1e6);
    constexpr double PICKUP_LEVEL = 1.0, PIEZO_LEVEL = 2.8;
    constexpr double PICKUP_POS_MM[3] = {65.0, 160.0, 255.0};
    constexpr int NUM_PICKUP_TYPES = (int)(sizeof(PICKUP_TYPES) / sizeof(PICKUP_TYPES[0]));

    constexpr double PALM_X = 0.03, PALM_WIDTH = 0.025, PALM_FORCE = 1.0, R_SKIN = 1.0;
    constexpr double PALM_DROP = 0.5, PALM_RECOVER = 0.03, PALM_F_MAX = 2000.0;
    // a hand landing on a ringing string (a finger relaxing, a flat hand, the palm) presses its skin in over this, a
    // palm leaving it over the time it takes to come back after the pick (PALM_RECOVER): the modes glide, no switch
    constexpr double HAND_LAND_S = 0.015, PALM_LIFT_S = PALM_RECOVER;

    constexpr double WIND_FILL = 0.8, MU_FRICTION = 0.3, LIFT_DX = 0.02;
    constexpr double H_NUT = 0.0005, H_12 = 0.0022, TAU_LIFT = 0.003, TAU_LAND = 0.001, LAND_SHARE = 0.4;
    // a line of single notes: the finger moves on the ringing string at the last moment before the pick (share of the
    // time since the last stroke, within [MIN, MAX] s). A chord whose shape changes, or a jump beyond HAND_SPAN_FRETS:
    // the whole hand lifts, touches the strings on its way (SHIFT_TOUCH_DELAY after leaving) and lands
    constexpr int HAND_SPAN_FRETS = 4;
    constexpr double FINGER_LEAD_SHARE = 0.03, FINGER_LEAD_MIN = 0.004, FINGER_LEAD_MAX = 0.012, FINGER_LEAD_JITTER = 0.002;
    constexpr double SHIFT_TOUCH_DELAY = 0.002;
    constexpr double LONG_COUPLING = 0.3, LONG_TAU = 0.04, V_REF = 0.3, PAD_EDGE = 0.001, FRET_CROWN = 0.002, FRET_BUMP = 0.3;
    constexpr double SENSOR_SCALE[3] = {1.0, PICKUP_LEVEL, PIEZO_LEVEL};

    // ---- the live player (PlayerFeel): a groove is first what repeats, then what wanders slowly, then the gesture's
    // own error (Rasanen et al. 2015, Porcaro's hi-hat: intervals 8.7 ms apart at 96 bpm, -0.48 from one interval to
    // the next, accents repeated at 88-93 % from bar to bar, timing at about 50 %; Hennig et al. 2011: listeners
    // prefer slow correlated wandering to white noise). Sizes at Looseness 1 (a good player); Anthony's DI: level
    // +-2.4 dB and brightness +-27 % from one stroke to the next.
    // the weight of each level of the bar (downbeat, half bar or group, beat, eighth, sixteenth) and the accent's
    // span over it (dex of force at Feel depth 1)
    constexpr double METER_WEIGHTS[5] = {1.0, 0.75, 0.55, 0.3, 0.1};
    // The Feel lanes (FeelCurve): a force step of 1 is this much force (dex) at Feel depth 1 (0.105: the Natural preset
    // gives the bar's accents as before the lanes), a timing step of 1 this late (s), a palm step of 1 this share of
    // the full palm's pressure (a light palm: the note's Palm mute is the full one); edited in FEEL_NOTCHES notches
    constexpr double FEEL_LANE_DEX = 0.105, FEEL_LANE_TIMING_S = 0.03, PALM_LANE_SHARE = 0.35;
    constexpr int FEEL_NOTCHES = 16;
    // the Feel presets: the weight of each place of the bar, as the force step it gives is (weight - mean) / span
    constexpr double FEEL_WEIGHT_SPAN = 0.7;
    enum FeelPreset
    {
        FEEL_NATURAL,
        FEEL_BACKBEAT,
        FEEL_OFFBEAT,
        FEEL_GALLOP,
        FEEL_WALTZ,
        FEEL_FLAT,
        NUM_FEEL_PRESETS
    };
    inline constexpr const char *FEEL_PRESET_NAMES[NUM_FEEL_PRESETS] = {"Natural", "Backbeat", "Offbeat", "Gallop", "Waltz", "Flat"};
    // the gesture (white: each onset on its own, which makes successive intervals anticorrelated by -0.5)
    constexpr double MOTOR_TIME_S = 0.005, MOTOR_FORCE_DEX = 0.07, MOTOR_PLUCK = 0.08, MOTOR_DEPTH_M = 0.0006;
    // the player's own way of playing each place of the bar, kept from bar to bar, renewed slowly over phrases
    constexpr double SIGNATURE_TIME_S = 0.003, SIGNATURE_FORCE_DEX = 0.05, SIGNATURE_PLUCK = 0.04, SIGNATURE_DEPTH_M = 0.0004;
    constexpr double SIGNATURE_KEEP_TIME = 0.5, SIGNATURE_KEEP_FORCE = 0.9; // correlation from one phrase to the next
    constexpr int PHRASE_BARS = 2;
    // the slow wandering: tempo, energy, the hand along the string (three time scales, equal shares: close to 1/f)
    constexpr double DRIFT_TAU_S[3] = {2.0, 8.0, 32.0};
    constexpr double DRIFT_TIME_S = 0.006, DRIFT_FORCE_DEX = 0.10, DRIFT_PLUCK = 0.12, DRIFT_DEPTH_M = 0.0005;
    // Imperfection 1: how often a stroke brushes the next string (muted by the fretting hand), misses its last
    // string, or chokes one of its strings (a fretting finger that slips)
    // The strum (Freire et al., SMC 2018, hexaphonic pickups on a nylon guitar, a pick): 6-note strums spread 37-46 ms
    // at 64 bpm (8-11 ms from string to string), 26-62 ms at 86 bpm (5-12.5 ms); the beat lands on strings 2 and 3
    // (B and G, string index 3.5 here), not on the first string the pick meets.
    constexpr double BEAT_STRING = 3.5;
    constexpr double BRUSH_CHANCE = 0.35, MISS_CHANCE = 0.25, CHOKE_CHANCE = 0.06, BRUSH_SHARE = 0.35;
    constexpr double TIMING_SIGMA = 0.015, ARM_SIGMA = 0.015, AMPLITUDE_SIGMA = 0.07, PLUCK_SIGMA = 0.06,
                     DEPTH_SIGMA = 0.0008, PALM_X_SIGMA = 0.003, PALM_N_SIGMA = 0.12, SWING = 0.10, PICK_DEPTH = 0.003,
                     WRIST_RADIUS = 0.25, STRING_SPACING = 0.0105, FULL_DEPTH = 0.0015, UP_FORCE = 0.8;

    constexpr models::ChordShape CHORD_SHAPES[] = {
        {"Open", {0, 0, 0, 0, 0, 0}},
        {"Em", {0, 2, 2, 0, 0, 0}},
        {"E", {0, 2, 2, 1, 0, 0}},
        {"Am", {-1, 0, 2, 2, 1, 0}},
        {"A", {-1, 0, 2, 2, 2, 0}},
        {"D", {-1, -1, 0, 2, 3, 2}},
        {"C", {-1, 3, 2, 0, 1, 0}},
        {"G", {3, 2, 0, 0, 0, 3}},
        {"Dm", {-1, -1, 0, 2, 3, 1}},
        {"F", {1, 3, 3, 2, 1, 1}},
        {"Bm", {-1, 2, 4, 4, 3, 2}},
        {"E7", {0, 2, 0, 1, 0, 0}},
        {"A7", {-1, 0, 2, 0, 2, 0}},
        {"D7", {-1, -1, 0, 2, 1, 2}},
        {"G7", {3, 2, 0, 0, 0, 1}},
        {"C7", {-1, 3, 2, 3, 1, 0}},
        {"Cadd9", {-1, 3, 2, 0, 3, 0}},
        {"Fmaj7", {-1, -1, 3, 2, 1, 0}},
        {"Asus2", {-1, 0, 2, 2, 0, 0}},
        {"Dsus4", {-1, -1, 0, 2, 3, 3}},
        {"E5", {0, 2, 2, -1, -1, -1}},
        {"A5", {-1, 0, 2, 2, -1, -1}},
        {"G5", {3, 5, 5, -1, -1, -1}},
        {"D5", {-1, 5, 7, 7, -1, -1}}};

    constexpr int NUM_CHORDS = (int)(sizeof(CHORD_SHAPES) / sizeof(CHORD_SHAPES[0]));

    constexpr models::StrumPattern STRUM_PATTERNS[] = {
        {"All down", "d.d.d.d.d.d.d.d."},
        {"Folk  d du ud u", "d.du.ud.d.du.udu"},
        {"Eighths du", "dudududududududu"},
        {"Reggae (off-beat)", ".x.d.x.d.x.d.x.d"},
        {"Palm-muted rock", "xxxdxxxdxxxdxxdu"},
        {"Quarter notes", "d...d...d...d..."},
    };

    constexpr int NUM_STRUM_PATTERNS = (int)(sizeof(STRUM_PATTERNS) / sizeof(STRUM_PATTERNS[0]));

    constexpr models::ArpeggioPattern ARPEGGIO_PATTERNS[] = {
        {"Up and down", {"B", "3", "2", "1", "2", "3", "2", "1"}},
        {"Travis (alternating bass)", {"B", "2", "B2", "1", "B", "2", "B2", "3"}},
        {"Bass + chord", {"B", "321", "B2", "321", "B", "321", "B2", "321"}},
        {"Ballad", {"B", "3", "2", "3", "1", "3", "2", "3"}},
        {"Pinch (bass + treble)", {"B1", "3", "2", "3", "B21", "3", "2", "3"}},
    };

    constexpr int NUM_ARPEGGIO_PATTERNS = (int)(sizeof(ARPEGGIO_PATTERNS) / sizeof(ARPEGGIO_PATTERNS[0]));

    // ---- the strings: materials and makers' sets
    constexpr double INCH_M = 0.0254;
    constexpr double LB_PER_INCH_KG_M = 17.857967; // a unit weight in lb/in, in kg/m
    // the air around a string (Pate et al. 2014, after Valette)
    constexpr double AIR_VISCOSITY = 1.8e-5; // Pa s
    constexpr double AIR_DENSITY = 1.2;      // kg/m^3
    // The internal losses (Woodhouse's eta_F, eta_B; the air's are computed from each string's size):
    //   - steel: eta_F = 1 / Q_disl, Q_disl = 5500 fitted by Pate et al. (JASA 2014) on an EXL110 G3 between rigid
    //     ends (with the air above, the model gives that string's measured Q, 2100 to 3400 below 1 kHz); eta_B from
    //     Woodhouse 2012 table I (Martin 80/20 set: plain strings 0.7 and 1.1e-2, wound 0.3 to 1.8e-2, geometric means);
    //   - played wound strings (dirt between the windings, Woodhouse 2012): eta_F x 2.05, eta_B x 6.0 (geometric means
    //     of his played / new ratios over the four wound strings); plain strings do not change. Nickel wound strings
    //     are taken as his bronze ones (the same windings: no published measurement of their own);
    //   - nylon (Woodhouse 2012 table I, Pro Arte Composites hard tension): eta_B 2.2e-2 for every string; eta_F 1e-4
    //     on the trebles, so that with the air it gives his E4 and B3 losses at their fundamentals, 4.5e-5 on the
    //     basses (his values). Its modulus stiffens with stress: E = 4.5 GPa + 39 x stress (Lynch-Aird & Woodhouse,
    //     nylon harp strings), which gives his trebles' bending stiffness. The basses' multifilament core: 0.7 of the
    //     gauge, 5 GPa (his basses' stiffness, within a factor 2).
    constexpr double Q_DISLOCATION = 5500.0;
    // The neck end of a string (the fret or the nut, the neck behind it): its conductance (s/kg), the same on every
    // guitar. Pate et al. 2014 measured a G3 between rigid ends (Q 2100 to 3400) and on a solid body (1200 to 2300,
    // the neck taking more than the bridge): this is what takes the string from the one to the other's middle (Q 1750
    // at 196 Hz). Until the neck's own modes (its dead spots) replace it, its average.
    constexpr double NECK_CONDUCTANCE = 5.7e-4;
    // The neck's own modes, where its dead spots come from (Pate et al. 2014, Les Paul Junior replica: their Fig. 7-8
    // place a mode across the top near 385 Hz, strongest about the 12th fret, and one along it near 330 Hz about the
    // 9th; their Fig. 11 measures the G3's Q there: 390 at the 12th fret across the top, 450 at the 9th along it).
    // Each mode's shape along the neck: sin(pi x / (2 antinode)) from the nut; its peak conductance is the one that
    // gives those measured Q in the engine (their printed conductances disagree with their own Q by a factor 2).
    constexpr models::Body::NeckMode NECK_MODES[2] = {
        {0, 388.0, 40.0, 0.019, 0.5},
        {1, 330.0, 40.0, 0.0092, 0.405},
    };
    enum StringMaterialId
    {
        STEEL_PLAIN,
        NICKEL_WOUND,
        BRONZE_WOUND,
        NYLON_PLAIN,
        NYLON_WOUND,
        NUM_STRING_MATERIALS
    };
    constexpr models::StringMaterial STRING_MATERIALS[NUM_STRING_MATERIALS] = {
        {"Plain steel", 2.0e11, 0.0, 0.0, true, true, 1.0 / Q_DISLOCATION, 0.88e-2, 1.0 / Q_DISLOCATION, 0.88e-2},
        {"Nickel-plated steel, round wound", 2.0e11, 0.0, 0.0, true, true, 1.0 / Q_DISLOCATION, 0.70e-2, 2.05 / Q_DISLOCATION, 4.2e-2},
        {"Bronze, round wound", 2.0e11, 0.0, 0.0, true, false, 1.0 / Q_DISLOCATION, 0.70e-2, 2.05 / Q_DISLOCATION, 4.2e-2},
        {"Nylon", 4.5e9, 39.0, 0.0, false, false, 1.0e-4, 2.2e-2, 1.0e-4, 2.2e-2},
        {"Silver-plated copper on nylon", 5.0e9, 0.0, 0.7, false, false, 4.5e-5, 2.2e-2, 9.2e-5, 13.2e-2},
    };

    // Sets of six strings, low to high: gauges and unit weights from D'Addario's tension chart (catalog supplement,
    // lb/in at 25.5"). Plain steel PL, nickel wound NW, phosphor bronze PB, 80/20 bronze BW, nylon J45xx / J46xx.
    constexpr models::StringSet STRING_SETS[] = {
        {"Nickel 9-42", STEEL_PLAIN, NICKEL_WOUND,
         {{0.042, 0.00032279, true}, {0.032, 0.00019347, true}, {0.024, 0.00010857, true}, {0.016, 0.00005671, false}, {0.011, 0.00002680, false}, {0.009, 0.00001794, false}}},
        {"Nickel 10-46", STEEL_PLAIN, NICKEL_WOUND,
         {{0.046, 0.00038216, true}, {0.036, 0.00023964, true}, {0.026, 0.00012671, true}, {0.017, 0.00006402, false}, {0.013, 0.00003744, false}, {0.010, 0.00002215, false}}},
        {"Nickel 11-49", STEEL_PLAIN, NICKEL_WOUND,
         {{0.049, 0.00043014, true}, {0.038, 0.00026471, true}, {0.028, 0.00014666, true}, {0.018, 0.00007177, false}, {0.014, 0.00004342, false}, {0.011, 0.00002680, false}}},
        {"Phosphor bronze 11-52", STEEL_PLAIN, BRONZE_WOUND,
         {{0.052, 0.00055223, true}, {0.042, 0.00036722, true}, {0.030, 0.00018660, true}, {0.022, 0.00009876, true}, {0.015, 0.00004984, false}, {0.011, 0.00002680, false}}},
        {"Phosphor bronze 12-53", STEEL_PLAIN, BRONZE_WOUND,
         {{0.053, 0.00056962, true}, {0.042, 0.00036722, true}, {0.032, 0.00020184, true}, {0.024, 0.00011682, true}, {0.016, 0.00005671, false}, {0.012, 0.00003190, false}}},
        {"80/20 bronze 12-53", STEEL_PLAIN, BRONZE_WOUND,
         {{0.053, 0.00054852, true}, {0.042, 0.00034808, true}, {0.032, 0.00020352, true}, {0.024, 0.00011353, true}, {0.016, 0.00005671, false}, {0.012, 0.00003190, false}}},
        {"Nylon, normal tension", NYLON_PLAIN, NYLON_WOUND,
         {{0.043, 0.00030632, true}, {0.035, 0.00018416, true}, {0.029, 0.00010754, true}, {0.0403, 0.00004679, false}, {0.0322, 0.00002827, false}, {0.0280, 0.00002092, false}}},
        {"Nylon, hard tension", NYLON_PLAIN, NYLON_WOUND,
         {{0.044, 0.00031726, true}, {0.036, 0.00019521, true}, {0.030, 0.00011237, true}, {0.0410, 0.00004795, false}, {0.0327, 0.00002924, false}, {0.0285, 0.00002161, false}}},
    };
    constexpr int NUM_STRING_SETS = (int)(sizeof(STRING_SETS) / sizeof(STRING_SETS[0]));
    // the string set menu: Automatic first (nickel under a magnetic pickup, bronze otherwise), then STRING_SETS
    constexpr int STRING_SET_AUTOMATIC = 0;
    constexpr int STRING_SET_ELECTRIC = 1, STRING_SET_ACOUSTIC = 4; // Automatic's choices, in STRING_SETS
    // the scale (nut to saddle): 648 mm (25.5") Fender and most flat-tops, 628 mm Gibson, 650 mm classical
    constexpr double SCALE_MM_DEFAULT = 648.0, SCALE_MM_MIN = 550.0, SCALE_MM_MAX = 770.0;
    constexpr double SCALE_DEFAULT_M = SCALE_MM_DEFAULT / 1000.0;


    constexpr models::Tuning TUNINGS[] = {
        {"Standard", {40, 45, 50, 55, 59, 64}},
        {"Half step down", {39, 44, 49, 54, 58, 63}},
        {"Drop D", {38, 45, 50, 55, 59, 64}},
        {"DADGAD", {38, 45, 50, 55, 57, 62}},
        {"Open G", {38, 43, 50, 55, 59, 62}},
        {"Open D", {38, 45, 50, 54, 57, 62}},
    };

    constexpr int NUM_TUNINGS = (int)(sizeof(TUNINGS) / sizeof(TUNINGS[0]));
    constexpr int CUSTOM_TUNING = NUM_TUNINGS; // the tuning menu's last entry: each open string's note chosen freely
    // the notes an open string may be tuned to (C1 to E5: from the deepest baritone drop to a high open tuning)
    constexpr int OPEN_NOTE_LOWEST = 24, OPEN_NOTE_HIGHEST = 76;
    // an open string under about 10 lb (45 N) feels floppy under the pick and buzzes on the frets (players' rule of
    // thumb, string makers' tension charts sit at 13 to 20 lb)
    constexpr double STRING_SLACK_N = 45.0;
    // the parameters of the open strings' notes in a Custom tuning, low string to high
    inline constexpr const char *OPEN_NOTE_PARAMS[NUM_STRINGS] = {"open_note_0", "open_note_1", "open_note_2",
                                                                  "open_note_3", "open_note_4", "open_note_5"};

    // a tuning of the list as the open strings' notes, low to high
    inline std::array<int, NUM_STRINGS> tuning_notes(int tuning)
    {
        const auto &t = TUNINGS[std::min(std::max(tuning, 0), NUM_TUNINGS - 1)];
        std::array<int, NUM_STRINGS> notes{};
        for (int i = 0; i < NUM_STRINGS; ++i)
            notes[(size_t)i] = t.midi[i];
        return notes;
    }

    inline double midi_hz(int note) { return 440.0 * std::pow(2.0, (note - 69) / 12.0); }

    constexpr double PALM_OFF = 0.005;

    constexpr double LOSS_SIGMA_DEX = 0.1;

    inline constexpr float OUTPUT_SCALE = 0.018f;

    // ---- the amplifier (core/amp)
    // the 12AX7 after Koren: Ip = (E1^EX / KG1) (1 + sgn E1), E1 = Vpk / KP ln(1 + exp(KP (1 / MU + Vgk / sqrt(KVB + Vpk^2))))
    constexpr double KOREN_MU = 100.0, KOREN_EX = 1.4, KOREN_KG1 = 1060.0, KOREN_KP = 600.0, KOREN_KVB = 300.0;
    constexpr double TRIODE_CGP = 1.7e-12, TRIODE_CGK = 1.6e-12; // grid to plate, grid to cathode (F)
    constexpr double GRID_ONSET_V = -0.3;                        // the grid starts to conduct this far below the cathode
    constexpr double BOLTZMANN = 1.380649e-23;
    constexpr int MAX_TRIODES = 6;
    constexpr int TRIODE_CURVE_POINTS = 1024;
    constexpr int AMP_OVERSAMPLE = 4;              // two halfband stages
    constexpr int HALFBAND_TAPS[2] = {63, 23};     // the first stage (sharp, at the original Nyquist), the second
    constexpr double HALFBAND_KAISER_BETA = 9.0;   // about -90 dB of stopband
    constexpr double TRIODE_LAST_LOAD = 1.0e6;     // what loads the last preamp stage (tone stack, phase inverter)
    constexpr double FULL_POWER_VOLTS = 20.0;      // the speaker's peak volts at full power
    // the cabinet heard by a close microphone, moved from the cone's centre to its edge: the top falls, the peak too
    constexpr double CAB_MIC_TOP_DROP = 0.45, CAB_MIC_PEAK_DROP = 0.6;
    constexpr double CAB_NOTCH_Q = 1.4, CAB_BREAKUP_Q = 1.8, CAB_TOP_Q = 0.9, CAB_TOP2_RATIO = 1.3, CAB_TOP2_Q = 0.6;
    constexpr double PICKUP_VOLTS = 0.02;          // a magnetic pickup's volts per unit of the engine's pickup signal
    constexpr double AMP_OUTPUT_UNITS = 30.0;      // the speaker at full power, in units of the engine's pickup signal
    constexpr double AUDIO_TAPER_DECADES = 2.0;    // an audio pot: (10^(2 p) - 1) / 99 of its track at position p
    constexpr double TONE_BASS_TAPER = 3.4;        // the bass pot of a tone stack: exp((p - 1) 3.4) (Yeh)
    constexpr double BRIGHT_MAX = 4.0;             // the bright capacitor lifts the highs at most this much
    constexpr double SAG_ATTACK_S = 0.01;          // the supply drops this fast when the power tubes draw more
    constexpr double LEVEL_SMOOTH_S = 0.02;        // gains moved by a knob glide over this
    // the overdrive pedal (a Tube Screamer's circuit): gain 1 + (R_DRIVE p + R_FIXED) / R_GAIN above 1 / (2 pi R_GAIN C_GAIN)
    constexpr double OD_R_GAIN = 4.7e3, OD_C_GAIN = 0.047e-6, OD_R_FIXED = 51e3, OD_R_DRIVE = 500e3;
    constexpr double OD_C_FEEDBACK = 51e-12;       // across the diodes: the clipped part's low-pass
    constexpr double OD_DIODE_V = 0.3;             // the knee of the silicon diodes in the feedback loop
    constexpr double OD_TONE_LO_HZ = 700.0, OD_TONE_HI_HZ = 7000.0;
    constexpr double OD_LEVEL_MAX = 2.0;
    // the noise gate (keyed by the guitar)
    constexpr double GATE_OFF_DB = -90.0;          // at this threshold the gate is off
    constexpr double GATE_DETECT_S = 0.004;        // the envelope's release
    constexpr double GATE_ATTACK_S = 0.0005, GATE_HOLD_S = 0.015, GATE_RELEASE_S = 0.04;
    constexpr double GATE_HYSTERESIS = 0.5;        // it closes below this share of the threshold

    inline const std::vector<models::AmpVoicing> AMP_VOICINGS = {
        // a blackface-style clean: the tone stack right after the first stage, the volume pot before the second
        {"American Clean", 300.0, 10e3,
         {{100e3, 1.5e3, 25e-6, 34e3, 1e-6, 1e6, 1.0},
          {100e3, 1.5e3, 22e-6, 100e3, 0.02e-6, 1e6, 1.0}},
         1, 0, 0.0,
         {250e3, 250e3, 10e3, 100e3, 250e-12, 0.1e-6, 0.047e-6},
         {460.0, 52.0, 0.15, 25.0, 4.0, 60.0, 70e-6, 0.35, 0.12, 4000.0, 120.0, 35.0, 12000.0}},
        // a plexi-to-800 crunch: partly bypassed first stage, the gain pot with its bright cap, a cold second stage
        {"British Crunch", 300.0, 10e3,
         {{100e3, 2.7e3, 0.68e-6, 68e3, 1e-6, 1e6, 1.0},
          {100e3, 10e3, 0.0, 470e3, 0.022e-6, 1e6, 1.0}},
         1, 1, 400.0,
         {220e3, 1e6, 25e3, 33e3, 470e-12, 0.022e-6, 0.022e-6},
         {460.0, 38.0, 0.1, 25.0, 2.0, 150.0, 50e-6, 0.4, 0.1, 5000.0, 100.0, 40.0, 11000.0}},
        // the same, hot-rodded: one more stage cascaded before the tone stack
        {"British Lead", 300.0, 10e3,
         {{100e3, 2.7e3, 0.68e-6, 68e3, 1e-6, 1e6, 1.0},
          {100e3, 10e3, 0.0, 470e3, 0.022e-6, 1e6, 1.0},
          {100e3, 1.5e3, 0.68e-6, 470e3, 0.022e-6, 1e6, 0.4}},
         1, 2, 400.0,
         {220e3, 1e6, 25e3, 33e3, 470e-12, 0.022e-6, 0.022e-6},
         {460.0, 38.0, 0.1, 25.0, 2.0, 150.0, 50e-6, 0.4, 0.1, 5000.0, 100.0, 40.0, 11000.0}},
        // a modern high gain: five cascaded stages, small coupling caps between them (tight lows), a cold clipper
        {"Modern High Gain", 330.0, 10e3,
         {{100e3, 1.8e3, 1e-6, 34e3, 1e-6, 1e6, 1.0},
          {100e3, 2.7e3, 1e-6, 470e3, 0.0022e-6, 1e6, 1.0},
          {100e3, 39e3, 0.0, 100e3, 0.022e-6, 1e6, 0.5},
          {100e3, 1.5e3, 1e-6, 470e3, 0.001e-6, 1e6, 0.6},
          {100e3, 10e3, 0.0, 470e3, 0.022e-6, 470e3, 0.5}},
         1, 4, 0.0,
         {250e3, 1e6, 25e3, 47e3, 470e-12, 0.022e-6, 0.022e-6},
         {470.0, 50.0, 0.15, 25.0, 4.0, 100.0, 100e-6, 0.4, 0.15, 3500.0, 90.0, 30.0, 10000.0}},
    };

    inline constexpr models::CabinetSpec CABINETS[] = {
        {"1x12 open back", 1, 75.0, 0.6, 0.05, 0.0, 0.25, 2500.0, 4.0, 1300.0, -3.0, 5500.0},
        {"2x12 closed", 2, 75.0, 0.5, 0.05, 0.07, 0.0, 2700.0, 5.0, 1400.0, -4.0, 5200.0},
        {"4x12 closed", 4, 75.0, 0.45, 0.05, 0.14, 0.0, 2600.0, 6.0, 1500.0, -5.0, 5000.0},
    };

    inline constexpr const char *NOTE_NAMES[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

    // a MIDI note's name with its octave ("E2", "C#3")
    inline std::string note_name(int midi)
    {
        return std::string(NOTE_NAMES[((midi % 12) + 12) % 12]) + std::to_string(midi / 12 - 1);
    }

    inline constexpr const char *GUITAR_PARAMS[] = {
        "tuning", "string_age", "brightness", "core", "stiff", "attack", "pluck", "force", "pick", "palm_mm",
        "palm_intensity", "live_palm", "body_size", "top_mm", "coupling", "braces_g", "body_q", "body_amount", "wood",
        "density_pct", "hollow",
        "sensor", "pickup_mm", "pickup_height_mm", "pickup_type", "cable_pf",
        "pickup_bridge_on", "pickup_middle_on", "pickup_neck_on", "pickup_bridge_split",
        "pickup_bridge_type", "pickup_middle_type", "pickup_neck_type",
        "amp_on", "amp_clean_model", "amp_lead_model", "amp_od_on", "amp_cab",
        "feedback_amount", "feedback_distance", "feedback_target",
        "sympathy", "room_on", "room_size", "room_material", "room_distance",
        "human", "human_amount", "swing_cm", "finger_noises", "finger_force",
        "mix_gain", "mix_level", "mix_pan", "mix_mute", "mix_solo", "mix_out"};

    // the sliders of the hand (Player tab): they change how the roll is played, not the networks of the sound
    inline constexpr models::Tunable<models::Settings> HAND_TUNABLES[] = {
        {"hand_mute", "Hand mute (1/s)", 20.0f, 600.0f, 150.0f, &models::Settings::hand_mute},
    };

    // the sliders of the strings' two planes of vibration (Attack tab): live, nothing to solve or compile again
    inline constexpr models::Tunable<models::Voicing> VOICE_TUNABLES[] = {
        {"attack_angle_down", "Downstroke angle (deg)", 0.0f, 90.0f, 0.0f, &models::Voicing::angle_down},
        {"attack_angle_up", "Upstroke angle (deg)", 0.0f, 90.0f, 0.0f, &models::Voicing::angle_up},
        {"plane_coupling", "Along-top bridge coupling", 0.0f, 1.0f, 0.0f, &models::Voicing::h_coupling},
        {"plane_radiation", "Along-top radiation", 0.0f, 1.0f, 0.0f, &models::Voicing::h_radiation},
        {"plane_split", "Planes split (cents)", 0.0f, 20.0f, 3.0f, &models::Voicing::split_cents},
        {"fret_action", "Action at 12th fret (mm)", 0.0f, 4.0f, 0.0f, &models::Voicing::action_mm},
        {"fret_bounce", "Fret bounce", 0.0f, 1.0f, 0.0f, &models::Voicing::fret_bounce},
        {"fret_click", "Fret click", 0.0f, 1.0f, 0.2f, &models::Voicing::fret_click},
        {"pick_push", "Pick push (ms)", 0.0f, 10.0f, 2.0f, &models::Voicing::push_ms},
        {"pick_slip", "Pick slip-stick", 0.0f, 1.0f, 0.0f, &models::Voicing::slip_stick},
        {"pick_scrape", "Pick scrape", 0.0f, 1.0f, 0.0f, &models::Voicing::scrape},
        {"wound_scrape", "Wound string scrape (x)", 1.0f, 10.0f, 3.0f, &models::Voicing::wound_scrape},
        {"tension_glide", "Pitch glide", 0.0f, 4.0f, 1.0f, &models::Voicing::glide},
        {"tension_pull", "Tension pull on bridge", 0.0f, 2.0f, 0.5f, &models::Voicing::longitudinal},
    };

    // the knobs of the amplifier (Amp tab): each channel has the same, its id and name prefixed (amp_clean_gain)
    inline constexpr models::Tunable<models::AmpChannelSettings> AMP_CHANNEL_TUNABLES[] = {
        {"gain", "Gain", 0.0f, 10.0f, 0.0f, &models::AmpChannelSettings::gain},
        {"bass", "Bass", 0.0f, 1.0f, 0.0f, &models::AmpChannelSettings::bass},
        {"middle", "Middle", 0.0f, 1.0f, 0.0f, &models::AmpChannelSettings::middle},
        {"treble", "Treble", 0.0f, 1.0f, 0.0f, &models::AmpChannelSettings::treble},
        {"presence", "Presence", 0.0f, 1.0f, 0.0f, &models::AmpChannelSettings::presence},
        {"resonance", "Resonance", 0.0f, 1.0f, 0.0f, &models::AmpChannelSettings::resonance},
        {"master", "Master", 0.0f, 1.0f, 0.0f, &models::AmpChannelSettings::master},
    };
    inline constexpr models::AmpChannelParams AMP_CHANNELS[] = {
        {"amp_clean_", "Clean ", "amp_clean_model", &models::AmpSettings::clean},
        {"amp_lead_", "Lead ", "amp_lead_model", &models::AmpSettings::lead},
    };
    inline constexpr models::Tunable<models::OverdriveSettings> OVERDRIVE_TUNABLES[] = {
        {"amp_od_drive", "Overdrive drive", 0.0f, 1.0f, 0.0f, &models::OverdriveSettings::drive},
        {"amp_od_tone", "Overdrive tone", 0.0f, 1.0f, 0.0f, &models::OverdriveSettings::tone},
        {"amp_od_level", "Overdrive level", 0.0f, 1.0f, 0.0f, &models::OverdriveSettings::level},
    };
    inline constexpr models::Tunable<models::CabinetSettings> CABINET_TUNABLES[] = {
        {"amp_cab_mic", "Mic: centre to edge", 0.0f, 1.0f, 0.0f, &models::CabinetSettings::mic},
    };
    inline constexpr models::Tunable<models::AmpSettings> AMP_TUNABLES[] = {
        {"amp_channel_mix", "Channel mix (0: clean, 1: lead)", 0.0f, 1.0f, 0.0f, &models::AmpSettings::mix},
        {"amp_gate", "Noise gate (dB, bottom: off)", (float)GATE_OFF_DB, -20.0f, 0.0f, &models::AmpSettings::gate_db},
        {"amp_room_mix", "Amp room mix", 0.0f, 1.0f, 0.0f, &models::AmpSettings::room_mix},
    };

    // the id of a channel's knob
    inline std::string amp_channel_id(const models::AmpChannelParams &channel, const char *knob) { return std::string(channel.prefix) + knob; }

    // a guitar's mixer strip (mix_gain...): where it plays in the mix, not its sound (a preset leaves it)
    inline bool is_mixer_param(const std::string &id) { return id.rfind("mix_", 0) == 0; }

    // every parameter a guitar has (its slot is copied, reset or moved as a whole)
    template <class Visit>
    void for_each_guitar_param(Visit &&visit)
    {
        for (const char *id : GUITAR_PARAMS)
            visit(std::string(id));
        for (const auto &t : HAND_TUNABLES)
            visit(std::string(t.id));
        for (const auto &t : VOICE_TUNABLES)
            visit(std::string(t.id));
        for (const auto &channel : AMP_CHANNELS)
            for (const auto &t : AMP_CHANNEL_TUNABLES)
                visit(amp_channel_id(channel, t.id));
        for (const auto &t : OVERDRIVE_TUNABLES)
            visit(std::string(t.id));
        for (const auto &t : CABINET_TUNABLES)
            visit(std::string(t.id));
        for (const auto &t : AMP_TUNABLES)
            visit(std::string(t.id));
    }

    inline float dbToGain(float db) { return std::pow(10.0f, db / 20.0f); }

    inline double positive_mod(double x, double m)
    {
        const double r = std::fmod(x, m);
        return r < 0 ? r + m : r;
    }
}