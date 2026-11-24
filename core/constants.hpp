// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// constants.hpp
//
// Global constants and configurations for the GRMGuitar engine.

#pragma once

#include <array>
#include <cmath>
#include <complex>

namespace grmg
{
    constexpr double MU_12AX7 = 100.0;
    constexpr double G_12AX7 = 2.24e-6;
    constexpr double V_SAT_TRIODE = 45.0;
    constexpr double R_GRID_COND = 1.2e3;
    constexpr double R_GRID_LEAK = 1.0e6;

    constexpr const char *ROOT_NAMES[12] = {"C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"};
    constexpr const char *ROOT_LABELS[12] = {"C", "C# / Db", "D", "D# / Eb", "E", "F", "F# / Gb", "G", "G# / Ab", "A", "A# / Bb", "B"};

    constexpr double C_AIR_DEFAULT = 343.0; // speed of sound in air (m/s at 20 deg C)

    constexpr double PI = 3.14159265358979323846;
    constexpr double RHO_STEEL = 7850.0; // kg/m^3
    constexpr double E_STEEL = 2.0e11;   // Pa
    constexpr double SIGMA_L = 0.0005;   // m
    constexpr double SIGMA_MU_REL = 0.01;
    constexpr double SIGMA_T = 0.5;          // N
    constexpr double PICK_E = 2.5e9;         // Pa (hypothesis)
    constexpr double PICK_TIP_WIDTH = 0.005; // m (hypothesis)
    constexpr double PICK_FREE = 0.004;      // m (hypothesis)
    constexpr double PICK_CONTACT = 0.0015;  // m
    constexpr double K_FLESH = 2.0e3;        // N/m (hypothesis)
    constexpr double FINGER_CONTACT = 0.012; // m

    constexpr int NUM_STRINGS = 6;
    constexpr int NUM_FRETS = 20; // 0 .. 19
    constexpr int MAX_MODES = 80;
    constexpr int SYMPATHY_MODES = 40;
    constexpr double Y_REF = 0.003; // m, release height of gain 1

    // how a note stops (s^-1, extra losses at the fundamental): a fretted note dies as the finger relaxes on it, an
    // open string waits for a hand to touch it (softer); a lifted finger leaves the string to the lift pluck
    constexpr double RELEASE_FRETTED = 32.0, RELEASE_OPEN = 14.0, RELEASE_LIFT = 45.0;
    // a hand laid flat on the strings: as fast as a finger leaving its fret (the flesh covers the strings at once)
    constexpr double RELEASE_HAND = 45.0;
    // a dead note: a finger rests on the string without pressing it (no measured value: a damping strong enough that
    // no pitch is left after a few periods of the low strings, and the delay of the pick leaving the string)
    constexpr double RELEASE_DEAD = 400.0, DEAD_TOUCH = 0.002;
    // a chord released: the fingers leave one after the other (s between strings, before the player's own spread)
    constexpr double RELEASE_SPREAD = 0.006;
    // the palm: its edge presses less (share of the pressure on the strings next to the ones asked); how much harder
    // it presses on a harder stroke (exponent of the stroke's force); how fast it stops a string it lands on (s^-1)
    constexpr double PALM_EDGE = 0.45, PALM_FORCE_FOLLOW = 0.3, PALM_LAND_DAMP = 50.0;
    // the damp of a string the palm lands on is given at its fundamental, and the engine adds more for the higher modes:
    // the palm network's fundamental damping times this spread covers the band its modes are damped over
    constexpr double PALM_LAND_SPREAD = 2.0;
    // the sweep: the mass that carries the pick across the strings (kg): the pick and the fingers holding it, the
    // arm behind a supple wrist; each string it pushes aside takes some of its energy
    constexpr double HAND_MASS = 0.04, SWEEP_LOSS_MAX = 0.6;
    // the muscle: recovery time (s), effort of a stroke of force 1 (fatigue units), how much fatigue weakens the stroke
    constexpr double MUSCLE_RECOVER = 2.0, MUSCLE_EFFORT = 0.04, MUSCLE_WEAKEN = 0.3, MUSCLE_LIMITS[2] = {0.75, 1.1};
    // fingers plucking together (a pinch): spread of their timing (s); the thumb leads a little
    constexpr double PINCH_SPREAD = 0.004, THUMB_LEAD = 0.003, PINCH_FORCE_SIGMA = 0.06;

    // ============================================================ body and bridge
    constexpr double GAMMA_AIR = 1.4, R_GAS = 8.314462618, M_AIR = 0.028965, T_AIR = 293.15;
    constexpr double NU_WOOD = 0.3, BETA_MODAL = 0.5, BRACING = 4.0;
    constexpr double BRIDGE_VOLUME = 3.2e-5, RHO_BRIDGE = 850.0, BRIDGE_SHARE = 0.2;
    constexpr double PLATE_RATIOS[5] = {21.26 / 10.21, 34.88 / 10.21, 39.77 / 10.21, 51.04 / 10.21, 60.82 / 10.21};
    constexpr double MODE_GAINS[7] = {0.9, 1.0, 0.55, 0.45, 0.35, 0.25, 0.18};
    constexpr double MODE_QS[7] = {16, 22, 25, 28, 30, 32, 35};

    constexpr double FINGER_BEHIND = 0.010;  // m: a fingertip presses this far behind its fret wire
    constexpr double TIP_SIGMA = 0.003;      // m: how far a fingertip may sit from that place
    constexpr double FINGER_SPREAD = 0.022;  // m: the natural distance between two neighbouring fingertips
    constexpr double HAND_SIGMA = 0.015;     // m: how much the hand gives, per finger gap
    constexpr double HAND_TENSION_MAX = 4.0; // strain (cost of the hand network) the hand can still hold

} // namespace grmg