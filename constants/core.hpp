// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
#pragma once
#include <cmath>
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
    constexpr double G_12AX7 = 2.24e-6;
    constexpr double GAMMA_AIR = 1.4;
    constexpr double HAND_MASS = 0.04;
    constexpr double HAND_SIGMA = 0.015;
    constexpr double HAND_TENSION_MAX = 4.0;
    constexpr double K_FLESH = 2.0e3;
    constexpr double M_AIR = 0.028965;
    constexpr double MU_12AX7 = 100.0;
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
    constexpr double R_GRID_LEAK = 1.0e6;
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
    constexpr double V_SAT_TRIODE = 45.0;
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
    constexpr int TENSION_UPDATE = 32;        // samples between two retunings of the modes
    constexpr double TENSION_PULL_HP_HZ = 20.0;

    constexpr double ROOM_BANDS[6] = {125.0, 250.0, 500.0, 1000.0, 2000.0, 4000.0};
    constexpr double C_ROOM = 343.0;

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

    constexpr models::PickupType PICKUP_TYPES[] = {
        {"Single coil (Strat 70s)", 2.2, 6.5e3, 110e-12},
        {"Hot (Lemme average)", 4.1, 12e3, 130e-12}};

    constexpr double PICKUP_LOAD = 1.0 / (1.0 / 250e3 + 1.0 / 1e6);
    constexpr double PICKUP_LEVEL = 1.0, PIEZO_LEVEL = 2.8;
    constexpr double PICKUP_POS_MM[3] = {65.0, 160.0, 255.0};
    constexpr int NUM_PICKUP_TYPES = (int)(sizeof(PICKUP_TYPES) / sizeof(PICKUP_TYPES[0]));

    constexpr double PALM_X = 0.03, PALM_WIDTH = 0.025, PALM_FORCE = 1.0, R_SKIN = 1.0;
    constexpr double PALM_DROP = 0.5, PALM_RECOVER = 0.03, PALM_F_MAX = 2000.0;

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

    constexpr double TIMING_SIGMA = 0.015, ARM_SIGMA = 0.015, AMPLITUDE_SIGMA = 0.07, PLUCK_SIGMA = 0.06,
                     DEPTH_SIGMA = 0.0008, PALM_X_SIGMA = 0.003, PALM_N_SIGMA = 0.12, SWING = 0.10, PICK_DEPTH = 0.003,
                     WRIST_RADIUS = 0.25, STRING_SPACING = 0.0105, FULL_DEPTH = 0.0015, UP_FORCE = 0.8;
    constexpr double ACCENT_DEX[8] = {0.06, -0.04, 0.0, -0.04, 0.03, -0.04, 0.0, -0.04};

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

    constexpr models::StringSpec STRING_SPECS[NUM_STRINGS] = {
        {0.648, 0.0061370673, 70.0, true},
        {0.648, 0.0034443134, 70.0, true},
        {0.648, 0.0019330657, 70.0, true},
        {0.648, 0.0010848655, 70.0, false},
        {0.648, 0.0006834364, 70.0, false},
        {0.648, 0.0003835658, 70.0, false},
    };

    constexpr const char *STRING_NAMES[NUM_STRINGS] = {"E2", "A2", "D3", "G3", "B3", "E4"};

    constexpr models::Tuning TUNINGS[] = {
        {"Standard", {40, 45, 50, 55, 59, 64}},
        {"Half step down", {39, 44, 49, 54, 58, 63}},
        {"Drop D", {38, 45, 50, 55, 59, 64}},
        {"DADGAD", {38, 45, 50, 55, 57, 62}},
        {"Open G", {38, 43, 50, 55, 59, 62}},
        {"Open D", {38, 45, 50, 54, 57, 62}},
    };

    constexpr int NUM_TUNINGS = (int)(sizeof(TUNINGS) / sizeof(TUNINGS[0]));

    inline double midi_hz(int note) { return 440.0 * std::pow(2.0, (note - 69) / 12.0); }

    constexpr double PALM_OFF = 0.005;

    constexpr models::MeasuredLosses LOSSES_NEW[6] = {
        {5e-5, 1.7, 0.3e-2},
        {11e-5, 1.2, 0.4e-2},
        {9e-5, 1.3, 1.1e-2},
        {6e-5, 1.0, 1.1e-2},
        {6e-5, 1.0, 1.1e-2},
        {7e-5, 2.5, 0.7e-2}};

    constexpr models::MeasuredLosses LOSSES_PLAYED[6] = {
        {10e-5, 1.4, 3.8e-2},
        {17e-5, 0.6, 2.9e-2},
        {33e-5, 0.7, 4.1e-2},
        {7e-5, 1.0, 1.1e-2},
        {7e-5, 1.0, 1.1e-2},
        {6e-5, 1.5, 0.7e-2}};

    constexpr double LOSS_SIGMA_DEX = 0.1;

    inline constexpr float OUTPUT_SCALE = 0.018f;

    inline constexpr const char *NOTE_NAMES[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    inline constexpr const char *STRING_LABELS[6] = {"6  E", "5  A", "4  D", "3  G", "2  B", "1  e"};

    inline constexpr const char *GUITAR_PARAMS[] = {
        "tuning", "string_age", "brightness", "core", "stiff", "attack", "pluck", "force", "pick", "palm_mm",
        "palm_intensity", "live_palm", "body_size", "top_mm", "coupling", "braces_g", "body_q", "body_amount", "wood",
        "density_pct", "hollow",
        "sensor", "pickup_mm", "pickup_type", "cable_pf",
        "pickup_bridge_on", "pickup_middle_on", "pickup_neck_on", "pickup_bridge_split",
        "pickup_bridge_type", "pickup_middle_type", "pickup_neck_type",
        "amp_on", "amp_channel_mix",
        "amp_clean_model", "amp_clean_gain", "amp_clean_master", "amp_clean_bass", "amp_clean_middle",
        "amp_clean_treble", "amp_clean_presence", "amp_clean_tight", "amp_clean_ts_boost", "amp_clean_stages",
        "amp_lead_model", "amp_lead_gain", "amp_lead_master", "amp_lead_bass", "amp_lead_middle",
        "amp_lead_treble", "amp_lead_presence", "amp_lead_tight", "amp_lead_ts_boost", "amp_lead_stages",
        "amp_room_mix",
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
        {"plane_pickup", "Along-top pickup", 0.0f, 1.0f, 0.0f, &models::Voicing::h_pickup},
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

    // every parameter a guitar has (its slot is copied, reset or moved as a whole)
    template <class Visit>
    void for_each_guitar_param(Visit &&visit)
    {
        for (const char *id : GUITAR_PARAMS)
            visit(id);
        for (const auto &t : HAND_TUNABLES)
            visit(t.id);
        for (const auto &t : VOICE_TUNABLES)
            visit(t.id);
    }

    inline float dbToGain(float db) { return std::pow(10.0f, db / 20.0f); }

    inline double positive_mod(double x, double m)
    {
        const double r = std::fmod(x, m);
        return r < 0 ? r + m : r;
    }
}