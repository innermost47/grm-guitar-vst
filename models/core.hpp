#pragma once
#include <cmath>
#include <array>
#include <vector>
#include <string>
#include <random>
#include <complex>

#include "amp.hpp"

namespace grmg::models
{
    using cplx = std::complex<double>;

    struct Material
    {
        const char *name;
        double alpha[6];
    };

    struct RoomSize
    {
        const char *name;
        double L, W, H;
    };

    struct Wood
    {
        const char *name;
        double rho, E;
    };

    // a magnetic pickup as built: its coil(s) as a circuit, and how it senses a string
    struct PickupType
    {
        const char *name;
        double L, R, C;        // the whole pickup's inductance (H), resistance (ohm), capacitance (F)
        int coils;             // 1: a single coil, 2: a humbucker (two coils in series, side by side along the string)
        double coil_spacing_m; // between the two coils' rows of poles
        double pole_radius_m;  // a pole piece's radius
        // the flux through a coil against the string's distance x (mm) to its pole (Novak et al., DAFx 2018, Eq. 6):
        // NL(x) = A ((x + Leq) / cbrt(req^2 + (x + Leq)^2) - x / cbrt(req^2 + x^2)), A in mV s
        double nl_A, nl_Leq_mm, nl_req_mm;
    };

    // one pickup once placed under the strings: where its coils sense them, how wide, how close, how hot
    struct PickupSensing
    {
        int coils = 0;                       // 0: no pickup there
        std::array<double, 2> coil_x{};      // each coil's distance from the saddle (m)
        double window_m = 0.0;               // the width of the string a pole hears (the field's reach, m)
        double height_mm = 3.0;              // the string at rest, above the poles
        double nl_A = 0.0, nl_Leq_mm = 0.0, nl_req_mm = 1.0;
        double gain = 1.0;                   // its slope at rest, against a single coil's at the reference height
    };

    struct PalmMode
    {
        double F, S, cost;
    };

    struct PickupCircuit
    {
        double L = 2.2, R = 6.5e3, C = 610e-12, Rload = 200000.0, fr = 4000.0, Q = 1.0;
    };

    struct Room
    {
        double V = 0, S = 0, rc = 0, fs = 0;
        std::array<double, 6> RT{}, alpha{};
        int size = -1, material = -1; // which one (ROOMS, MATERIALS)
    };

    // where the listener hears a guitar from, per ear: the direct sound's way (s) and the level sent to the room's
    // reverberation (solved from ROOM_REF_DISTANCE), against the direct sound
    struct ListenerTaps
    {
        std::array<double, 2> delay_s{}, send{};
    };

    enum class EventType
    {
        pluck,
        damp,
        squeak,
        palm_on,
        palm_off
    };

    struct CompileOptions
    {
        double tempo = 90.0;
        unsigned seed = 1;
        bool loop = false;
    };

    struct Touch
    {
        double t;
        int string;
        double extra;
    };

    struct Biquad
    {
        static constexpr double PI = 3.14159265358979323846;

        double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
        double z1 = 0.0, z2 = 0.0;

        inline double process(double x) noexcept
        {
            const double y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }

        void reset() noexcept
        {
            z1 = 0.0;
            z2 = 0.0;
        }

        void set_lowpass(double sr, double fc, double q) noexcept
        {
            const double w0 = 2.0 * PI * fc / sr;
            const double cw = std::cos(w0), sw = std::sin(w0);
            const double alpha = sw / (2.0 * q);
            const double a0 = 1.0 + alpha;
            b0 = ((1.0 - cw) * 0.5) / a0;
            b1 = (1.0 - cw) / a0;
            b2 = b0;
            a1 = -2.0 * cw / a0;
            a2 = (1.0 - alpha) / a0;
        }

        void set_highpass(double sr, double fc, double q) noexcept
        {
            const double w0 = 2.0 * PI * fc / sr;
            const double cw = std::cos(w0), sw = std::sin(w0);
            const double alpha = sw / (2.0 * q);
            const double a0 = 1.0 + alpha;
            b0 = ((1.0 + cw) * 0.5) / a0;
            b1 = -(1.0 + cw) / a0;
            b2 = b0;
            a1 = -2.0 * cw / a0;
            a2 = (1.0 - alpha) / a0;
        }

        void set_peaking(double sr, double fc, double q, double gain_db) noexcept
        {
            const double A = std::pow(10.0, gain_db / 40.0);
            const double w0 = 2.0 * PI * fc / sr;
            const double cw = std::cos(w0), sw = std::sin(w0);
            const double alpha = sw / (2.0 * q);
            const double a0 = 1.0 + alpha / A;
            b0 = (1.0 + alpha * A) / a0;
            b1 = -2.0 * cw / a0;
            b2 = (1.0 - alpha * A) / a0;
            a1 = -2.0 * cw / a0;
            a2 = (1.0 - alpha / A) / a0;
        }

        void set_lowshelf(double sr, double fc, double gain_db) noexcept
        {
            const double A = std::pow(10.0, gain_db / 40.0);
            const double w0 = 2.0 * PI * fc / sr;
            const double cw = std::cos(w0), sw = std::sin(w0);
            const double alpha = sw * 0.5 * std::sqrt(2.0);
            const double sqA = std::sqrt(A);
            const double a0 = (A + 1.0) + (A - 1.0) * cw + 2.0 * sqA * alpha;
            b0 = A * ((A + 1.0) - (A - 1.0) * cw + 2.0 * sqA * alpha) / a0;
            b1 = 2.0 * A * ((A - 1.0) - (A + 1.0) * cw) / a0;
            b2 = A * ((A + 1.0) - (A - 1.0) * cw - 2.0 * sqA * alpha) / a0;
            a1 = -2.0 * ((A - 1.0) + (A + 1.0) * cw) / a0;
            a2 = ((A + 1.0) + (A - 1.0) * cw - 2.0 * sqA * alpha) / a0;
        }

        void set_highshelf(double sr, double fc, double gain_db) noexcept
        {
            const double A = std::pow(10.0, gain_db / 40.0);
            const double w0 = 2.0 * PI * fc / sr;
            const double cw = std::cos(w0), sw = std::sin(w0);
            const double alpha = sw * 0.5 * std::sqrt(2.0);
            const double sqA = std::sqrt(A);
            const double a0 = (A + 1.0) - (A - 1.0) * cw + 2.0 * sqA * alpha;
            b0 = A * ((A + 1.0) + (A - 1.0) * cw + 2.0 * sqA * alpha) / a0;
            b1 = -2.0 * A * ((A - 1.0) + (A + 1.0) * cw) / a0;
            b2 = A * ((A + 1.0) - (A - 1.0) * cw - 2.0 * sqA * alpha) / a0;
            a1 = 2.0 * ((A - 1.0) - (A + 1.0) * cw) / a0;
            a2 = ((A + 1.0) - (A - 1.0) * cw - 2.0 * sqA * alpha) / a0;
        }
    };

    struct HandResult
    {
        bool possible = true;
        int fingers = 0;
        double tension = 0.0;
        std::string why;
    };

    struct Finger
    {
        int fret;
        int number;
    };

    struct Problem
    {
        double beat;
        std::string why;
    };

    struct Ringing
    {
        std::vector<double> note_end;
        std::vector<std::array<double, 6>> strum_end;
        // the string of a strum is left when the hand changes shape for a chord that does not play it
        std::vector<std::array<bool, 6>> strum_lifted;
    };

    enum class Kind
    {
        strike,
        lift,
        land
    };

    struct Place
    {
        double f1 = 0, B = 0, T = 0, Leff = 0, c = 0, Zs = 0;
    };

    struct Palm
    {
        double f1 = 0, r = 0, w = 0, x = 0;
        std::vector<double> ratio, sigma;
        double ratio_at(size_t n) const { return n < ratio.size() ? ratio[n] : 1.0; }
        double sigma_at(size_t n) const { return n < sigma.size() ? sigma[n] : (sigma.empty() ? 0.0 : sigma.back()); }
    };

    struct Pluck
    {
        int string = 0, fret = 0;
        Place place;
        double gain = 0;
        double tau = 0;
        double pluck = 0.22;
        Kind kind = Kind::strike;
        const Palm *palm = nullptr;
        bool up = false;  // an upstroke (the pick comes back from the treble side)
        bool pick = true; // struck with a pick (it slips and scrapes on the string), not with a finger
    };

    // How a string answers the attack. Set live from the sliders of the Attack tab.
    //   - its two planes: across the top (it pushes the bridge, which moves easily that way: the energy leaves fast
    //     into the body, the loud start of the note) and along it (the bridge hardly moves: the note rings on). The
    //     attack's angle shares the pluck between them;
    //   - the frets under it: struck hard, the string hits the last fret and bounces off it (Zollner 1.5.3: past
    //     about 2 N the low partials hardly grow while the highs do, and metal hits metal);
    //   - the pick before it lets go: it pushes the string aside for a few milliseconds (the bridge feels it), the
    //     string sticks and slips on it and scrapes its edge (more on a wound string);
    //   - its tension, which a large vibration raises: the note starts sharp and settles (and pulls on the bridge).
    struct Voicing
    {
        double angle_down = 40.0;  // degrees from the top's plane of a downstroke (90: straight into the top)
        double angle_up = 15.0;    // the same for an upstroke
        double h_coupling = 0.15;  // the bridge's admittance along the top, over its admittance across it
        double h_radiation = 0.3;  // the share of the along-the-top force that reaches the body and the room
        double split_cents = 0.0;  // an extra distance between the two planes (beats everywhere: a chorus, by choice)

        double action_mm = 1.8;    // the string's height over the 12th fret (0: no fret under it)
        double fret_bounce = 0.6;  // the share of its speed the string keeps when it bounces off a fret
        double fret_click = 0.15;  // the metal knock of the string on the fret, heard at the bridge
        double push_ms = 2.0;      // how long the pick pushes the string before it lets go, at the usual force
        double slip_stick = 0.4;   // how deep the string slips on the pick while it is pushed
        double scrape = 0.4;       // the noise of the string skipping along the pick's edge
        double wound_scrape = 3.0; // the same on a wound string, times this
        double glide = 1.0;        // the pitch glide of a large vibration (1: the string's own tension change)
        double longitudinal = 0.3; // the pull of that tension change on the bridge, heard through the body

        // one plane only, across the top, nothing but the release: the string as the networks' reference model has it
        static Voicing single_plane()
        {
            Voicing v;
            v.angle_down = v.angle_up = 90.0;
            v.action_mm = 0.0;
            v.push_ms = 0.0;
            v.slip_stick = v.scrape = 0.0;
            v.glide = v.longitudinal = 0.0;
            return v;
        }
    };

    struct Event
    {
        double beat = 0.0;
        EventType type = EventType::pluck;
        int string = 0;
        Pluck pluck;
        int squeak = -1;
        double extra = 40.0;
    };

    struct Stroke
    {
        double t, A, v, force, p, d, palm_x, palm_N;
    };

    struct Played
    {
        double t;
        int string, fret;
        double factor, pluck, length;
        const Palm *palm;
        Stroke stroke;
        bool palm_on;
        int chord_order;
        bool hand_lifts = false; // it ends when the hand leaves for a new chord shape: it stops ahead of that stroke
        bool up = false;         // struck by an upstroke
    };

    struct Body
    {
        static constexpr double PI = 3.14159265358979323846;

        std::array<double, 7> f{}, gain{}, q{};
        double Mm = 0, Zp = 0, mb = 0, fT = 0, frad = 0, fH = 0, f_lo = 0, f_hi = 0, fb = 0;

        cplx admittance(double freq) const
        {
            const double w = 2 * PI * freq;
            cplx Y = 0.0;
            for (int m = 0; m < 7; ++m)
            {
                if (gain[(size_t)m] <= 1e-9)
                    continue;
                const double wm = 2 * PI * f[(size_t)m];
                Y += cplx(0, w) / ((Mm / gain[(size_t)m]) * cplx(wm * wm - w * w, w * wm / q[(size_t)m]));
            }
            Y += freq * freq / (freq * freq + fT * fT) / cplx(Zp, w * mb);
            return Y;
        }

        double loss(double freq, double c, double Leff, double Zs) const
        {
            return (c / Leff) * Zs * std::max(admittance(freq).real(), 0.0);
        }

        double coupling(double freq, double c, double Leff, double Zs) const
        {
            return (c / Leff) * Zs * std::abs(admittance(freq));
        }

        // the shift (rad/s) of a string mode's frequency by the bridge it pushes (the reactive part of the coupling
        // whose resistive part is loss): a bridge that moves like a mass raises it, like a spring lowers it
        double detune(double freq, double c, double Leff, double Zs) const
        {
            return -(c / Leff) * Zs * admittance(freq).imag();
        }
    };

    struct Squeak
    {
        double fsq = 0, P = 0, fL = 0, v = 0, N = 0, L = 0, p = 0, cL = 0;
    };

    struct Action
    {
        double xf, h;
    };

    struct PlayerNoise
    {
        bool exact = false;
        double amount = 1.0;
        std::mt19937 rng{1};

        double normal(double sd)
        {
            if (exact || amount <= 0)
                return 0.0;
            std::normal_distribution<double> d(0.0, sd);
            return amount * d(rng);
        }
    };

    struct Contact
    {
        int string;
        double delay, share;
    };

    struct BodySpec
    {
        double size = 1.0, top_mm = 2.8, coupling = 0.35, braces_kg = -1.0, q = 50.0;
        double rho = 430.0, E = 1.0e10;
        double hollow = 1.0;
    };

    struct RollNote
    {
        int string = 0;
        int fret = 0;
        double start = 0.0, length = 1.0;
        double velocity = 0.8;
        bool palm = false;
        enum End
        {
            ring = 0,
            stop = 1,
            hand = 2
        };
        int end = ring;
        bool dead = false;
        bool selected = false;
    };

    struct RollStrum
    {
        double start = 0.0, length = 2.0;
        std::array<int, 6> frets{{-1, -1, -1, -1, -1, -1}};
        bool up = false;
        double velocity = 0.8;
        bool palm = false;
        std::string chord = "";
        bool selected = false;
    };

    struct ChordShape
    {
        const char *name;
        int frets[6];
    };

    struct StrumPattern
    {
        const char *name;
        const char *steps;
    };

    struct ArpeggioPattern
    {
        const char *name;
        const char *slots[8];
    };

    struct StringSpec
    {
        double L, mu, T;
        bool wound;
    };

    struct Tuning
    {
        const char *name;
        int midi[6];
    };

    struct StringLosses
    {
        static constexpr double PI = 3.14159265358979323846;

        double etaF = 0, etaA = 0, etaB = 0;
        double legacy = 0;

        double sigma(double f, int n, double B) const
        {
            if (legacy > 0)
                return (1.0 + (f / 1800.0) * (f / 1800.0)) / legacy;
            const double w = 2 * PI * f, b = B * n * n;
            return 0.5 * w * (etaF + etaA / w + etaB * b) / (1.0 + b);
        }
    };

    struct MeasuredLosses
    {
        double etaF, etaA, etaB;
    };

    struct FeedbackSpec
    {
        double amount = 0.0;
        double distance = 1.0;
        int target = 0;
        double c_air = 343.0;
    };

    struct FeedbackModel
    {
        double tau = 0.003;
        double attenuation = 1.0;
        double coupling = 0.0;
        double target_freq = 0.0;
        double target_q = 2.0;
        FeedbackSpec spec;
    };

    enum class FeedbackTarget
    {
        Auto = 0,
        Fundamental = 1,
        Octave = 2,
        Fifth = 3,
        DoubleOctave = 4
    };

    struct Transport
    {
        bool playing = false;
        double beat = 0.0;
        double beats_per_sample = 0.0;
        bool loop = true;
    };

    struct ChordQuality
    {
        const char *suffix;
        const char *label;
        std::vector<int> tones;
        std::vector<int> needed;
    };

    struct Chord
    {
        std::string name;
        std::array<int, 6> frets{{-1, -1, -1, -1, -1, -1}};
    };

    struct Scale
    {
        const char *name;
        std::vector<int> steps;
    };

    struct Settings
    {
        int tuning = 0;
        double core = 0.45;
        bool stiff = true;
        double string_age = 0.0;
        double brightness = 0.6;

        bool pick = true;
        double pluck = 0.22;
        double force = 2.0;
        double pick_mm = 0.71;

        double palm_mm = 30.0;
        double palm_intensity = 1.0;

        double body_size = 1.0, top_mm = 2.8, coupling = 0.35, body_amount = 0.85;
        double braces_g = 64.0;
        double body_q = 50.0;
        int wood = 0;
        double density_pct = 0.0;
        double hollow = 1.0;

        int sensor = 0;
        double pickup_mm = 100.0;
        double pickup_height_mm = 3.0; // the strings above the pickups' poles
        int pickup_type = 0;
        double cable_pf = 500.0;
        bool pickup_bridge_on = true;
        bool pickup_middle_on = false;
        bool pickup_neck_on = false;
        bool pickup_bridge_split = false;
        int pickup_bridge_type = 1;
        int pickup_middle_type = 0;
        int pickup_neck_type = 0;
        bool sympathy = true;

        bool room_on = true;
        int room_size = 1, room_material = 2;
        double room_distance = 1.0;

        bool human = true;
        double human_amount = 1.0;
        double swing_cm = 10.0;
        bool finger_noises = true;
        double finger_force = 0.6;
        double hand_mute = 150.0; // 1/s: how fast the hand leaving a chord shape (or travelling) stops the strings

        double feedback_amount = 0.0;
        double feedback_distance = 1.0;
        int feedback_target = 0;
        AmpSettings amp;

        bool is_magnetic_pickup() const { return sensor == 1 || sensor == 3; }
        bool amp_active() const { return amp.on && is_magnetic_pickup(); }
    };

    // A setting exposed as a slider to tune the sound by ear: one line of a table makes its parameter, reads it, compares
    // it and copies it. Its default is its Owner's own (Settings: the roll's compiler; Voicing: the strings, live).
    template <class Owner>
    struct Tunable
    {
        const char *id, *name;
        float lo, hi, centre; // centre: the value at the middle of the slider (0: linear)
        double Owner::*field;
    };

    struct Strike
    {
        double beat;
        int string, fret;
        double velocity, length;
        bool palm, up;
        int strum;
        double ring;
        int end;
        bool dead;
    };

    struct Moment
    {
        double beat;
        std::vector<Strike> strikes;
        int strum = -1;
    };

    struct Pending
    {
        Event e;
        double beats;
    };
}