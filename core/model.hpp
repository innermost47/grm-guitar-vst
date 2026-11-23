// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// model.hpp
//
// Everything the GRM networks give the guitar, solved off the audio thread from the settings:
// the strings at every fret, the attack, the palm, the fingers on the frets, the body and the room.
// The audio thread only reads a finished Model (see Engine); a new one replaces it between two blocks.
#pragma once

#include <array>
#include <atomic>
#include <memory>
#include <string>
#include <vector>
#include <cmath>

#include "feedback.hpp"
#include "amp.hpp"
#include "physics.hpp"

namespace grmg
{

	constexpr int NUM_STRINGS = 6;
	constexpr int NUM_FRETS = 20; // 0 .. 19
	constexpr int MAX_MODES = 80;
	constexpr int SYMPATHY_MODES = 40;
	constexpr double Y_REF = 0.003; // m, release height of gain 1

	struct StringSpec
	{
		double L, mu, T;
		bool wound;
	};
	// the six strings of grm_guitar.py, index 0 = string 6 (low E)
	constexpr StringSpec STRING_SPECS[NUM_STRINGS] = {
		{0.648, 0.0061370673, 70.0, true},
		{0.648, 0.0034443134, 70.0, true},
		{0.648, 0.0019330657, 70.0, true},
		{0.648, 0.0010848655, 70.0, false},
		{0.648, 0.0006834364, 70.0, false},
		{0.648, 0.0003835658, 70.0, false},
	};
	constexpr const char* STRING_NAMES[NUM_STRINGS] = { "E2", "A2", "D3", "G3", "B3", "E4" };

	// tunings: MIDI note of each open string, from string 6 to string 1
	struct Tuning
	{
		const char* name;
		int midi[NUM_STRINGS];
	};
	constexpr Tuning TUNINGS[] = {
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

	struct Settings
	{
		// strings
		int tuning = 0;
		double core = 0.45;
		bool stiff = true;
		double string_age = 0.0;
		double brightness = 0.6;
		// attack
		bool pick = true;
		double pluck = 0.22;
		double force = 2.0;
		double pick_mm = 0.71;
		// palm
		double palm_mm = PALM_X * 1000;
		double palm_intensity = 1.0;
		// body
		double body_size = 1.0, top_mm = 2.8, coupling = 0.35, body_amount = 0.85;
		double braces_g = BRACES_KG * 1000;
		double body_q = BODY_Q;
		int wood = 0;
		double density_pct = 0.0;
		double hollow = 1.0;
		// what listens to the guitar
		int sensor = 0;
		double pickup_mm = 100.0;
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
		// room
		bool room_on = true;
		int room_size = 1, room_material = 2;
		double room_distance = 1.0;
		// player
		bool human = true;
		double human_amount = 1.0;
		double swing_cm = SWING * 100;
		bool finger_noises = true;
		double finger_force = 0.6;
		// amplifier, distortion & feedback
		double feedback_amount = 0.0;
		double feedback_distance = 1.0;
		int feedback_target = 0;
		bool amp_on = false;
		double amp_room_mix = 0.2;
		double amp_channel_mix = 1.0;
		AmpSpec amp_clean = amp_preset_clean();
		AmpSpec amp_lead = amp_preset_5150_lead();

		bool is_magnetic_pickup() const { return sensor == 1 || sensor == 3; }
		bool amp_active() const { return amp_on && is_magnetic_pickup(); }
	};

	struct Place
	{
		double f1 = 0, B = 0, T = 0, Leff = 0, c = 0, Zs = 0;
	};

	struct StringLosses
	{
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

	class Model
	{
	public:
		Settings s;
		double sample_rate = 48000.0;
		std::array<double, NUM_STRINGS> tension{};
		std::array<std::array<Place, NUM_FRETS>, NUM_STRINGS> place{};
		std::array<StringLosses, NUM_STRINGS> losses{};
		std::array<std::array<double, NUM_FRETS>, NUM_STRINGS> y0{}, tau{};
		std::array<std::array<std::shared_ptr<const Palm>, NUM_FRETS>, NUM_STRINGS> palm{};
		std::array<Action, NUM_FRETS> action{};
		Body body;
		std::vector<float> body_ir;
		std::array<std::vector<float>, 2> room_ir;
		Room room;
		Settings palm_settings;
		bool palm_ready = false;
		unsigned body_id = 0, room_id = 0;

		AmpModel amp_clean_model;
		AmpModel amp_lead_model;
		bool amp_active = false;

		FeedbackModel feedback;

		static unsigned next_id()
		{
			static std::atomic<unsigned> counter{ 0 };
			return ++counter;
		}

		int open_midi(int i) const { return TUNINGS[s.tuning].midi[i]; }
		double core_fraction(int i) const { return STRING_SPECS[i].wound ? s.core : 1.0; }

		static bool same_strings(const Settings& a, const Settings& b)
		{
			return a.tuning == b.tuning && std::abs(a.core - b.core) < 1e-12 && a.stiff == b.stiff;
		}
		static bool same_attack(const Settings& a, const Settings& b)
		{
			return a.pick == b.pick && std::abs(a.pluck - b.pluck) < 1e-12 &&
				std::abs(a.force - b.force) < 1e-12 && std::abs(a.pick_mm - b.pick_mm) < 1e-12;
		}
		static bool same_palm(const Settings& a, const Settings& b)
		{
			return std::abs(a.palm_mm - b.palm_mm) < 1e-9 && std::abs(a.palm_intensity - b.palm_intensity) < 1e-9;
		}
		static bool same_body(const Settings& a, const Settings& b)
		{
			return std::abs(a.body_size - b.body_size) < 1e-12 && std::abs(a.top_mm - b.top_mm) < 1e-12 &&
				std::abs(a.coupling - b.coupling) < 1e-12 && std::abs(a.braces_g - b.braces_g) < 1e-9 &&
				std::abs(a.body_q - b.body_q) < 1e-9 && a.wood == b.wood &&
				std::abs(a.density_pct - b.density_pct) < 1e-9 && std::abs(a.hollow - b.hollow) < 1e-9;
		}
		static bool same_room(const Settings& a, const Settings& b)
		{
			return a.room_size == b.room_size && a.room_material == b.room_material &&
				std::abs(a.room_distance - b.room_distance) < 1e-9;
		}
		static bool same_pickups(const Settings& a, const Settings& b)
		{
			return a.sensor == b.sensor && std::abs(a.pickup_mm - b.pickup_mm) < 1e-9 &&
				a.pickup_type == b.pickup_type && std::abs(a.cable_pf - b.cable_pf) < 1e-9 &&
				a.pickup_bridge_on == b.pickup_bridge_on && a.pickup_middle_on == b.pickup_middle_on &&
				a.pickup_neck_on == b.pickup_neck_on && a.pickup_bridge_split == b.pickup_bridge_split &&
				a.pickup_bridge_type == b.pickup_bridge_type && a.pickup_middle_type == b.pickup_middle_type &&
				a.pickup_neck_type == b.pickup_neck_type;
		}

		static bool same_amp_dc(const AmpSpec& a, const AmpSpec& b)
		{
			return std::abs(a.Vdd - b.Vdd) < 1e-4 &&
				std::abs(a.Rp - b.Rp) < 1e-4 &&
				std::abs(a.Rk - b.Rk) < 1e-4 &&
				std::abs(a.gain - b.gain) < 1e-4;
		}

		static bool same_amp_params(const AmpSpec& a, const AmpSpec& b)
		{
			return std::abs(a.master - b.master) < 1e-4 &&
				std::abs(a.bass - b.bass) < 1e-4 &&
				std::abs(a.middle - b.middle) < 1e-4 &&
				std::abs(a.treble - b.treble) < 1e-4 &&
				std::abs(a.presence - b.presence) < 1e-4 &&
				std::abs(a.sag_amount - b.sag_amount) < 1e-4 &&
				a.tight == b.tight &&
				a.ts_boost == b.ts_boost &&
				a.stages == b.stages;
		}

		static bool same_feedback(const Settings& a, const Settings& b)
		{
			return std::abs(a.feedback_amount - b.feedback_amount) < 1e-4 &&
				std::abs(a.feedback_distance - b.feedback_distance) < 1e-4 &&
				a.feedback_target == b.feedback_target;
		}

		static std::pair<double, double> palm_snap(double x, double force)
		{
			const double xs = std::max(0.002, std::round(x * 333.3) / 333.3);
			const double fs = std::pow(10.0, std::round(std::log10(std::max(force, 1e-3)) / 0.05) * 0.05);
			return { xs, fs };
		}
		std::pair<double, double> palm_home() const
		{
			return palm_snap(s.palm_mm / 1000.0, PALM_FORCE * s.palm_intensity);
		}

		BodySpec body_spec() const
		{
			BodySpec b;
			b.size = s.body_size;
			b.top_mm = s.top_mm;
			b.coupling = s.coupling;
			b.braces_kg = s.braces_g / 1000.0;
			b.q = s.body_q;
			const auto& w = WOODS[std::min(std::max(s.wood, 0), NUM_WOODS - 1)];
			b.rho = w.rho * (1.0 + s.density_pct / 100.0);
			b.E = w.E;
			b.hollow = s.hollow;
			return b;
		}

		PickupCircuit pickup;
		std::array<PickupCircuit, 3> pickup3{};
		std::array<bool, 3> pickup3_on{};

		void solve_pickup()
		{
			if (s.sensor == 1)
			{
				pickup = pickup_network(s.pickup_type, s.cable_pf);
			}
			else if (s.sensor == 3)
			{
				const int bridge_type = s.pickup_bridge_split ? 0 : s.pickup_bridge_type;
				pickup3[0] = pickup_network(bridge_type, s.cable_pf);
				pickup3[1] = pickup_network(s.pickup_middle_type, s.cable_pf);
				pickup3[2] = pickup_network(s.pickup_neck_type, s.cable_pf);
				pickup3_on = { s.pickup_bridge_on, s.pickup_middle_on, s.pickup_neck_on };
			}
		}

		std::array<double, 3> pickup_xs() const
		{
			if (s.sensor == 1)
				return { s.pickup_mm / 1000.0, 0.0, 0.0 };
			if (s.sensor == 3)
				return { PICKUP_POS_MM[0] / 1000.0,
						PICKUP_POS_MM[1] / 1000.0,
						PICKUP_POS_MM[2] / 1000.0 };
			return { 0.0, 0.0, 0.0 };
		}

		double pickup_x() const { return pickup_xs()[0]; }

		void solve_losses()
		{
			for (int i = 0; i < NUM_STRINGS; ++i)
			{
				const auto r = string_losses_network(i, s.string_age);
				losses[(size_t)i] = { r["etaF"], r["etaA"], r["etaB"], 0.0 };
			}
		}

		void solve(const Settings& settings, double sr, const Model* previous, bool with_palm = true)
		{
			s = settings;
			sample_rate = sr;
			const bool keep_strings = previous && same_strings(previous->s, s);
			if (keep_strings)
			{
				tension = previous->tension;
				place = previous->place;
				action = previous->action;
			}
			else
			{
				for (int i = 0; i < NUM_STRINGS; ++i)
				{
					const auto& sp = STRING_SPECS[i];
					tension[(size_t)i] = tune_string_model(sp.L, sp.mu, midi_hz(open_midi(i)), core_fraction(i))["T"];
					for (int f = 0; f < NUM_FRETS; ++f)
					{
						const auto r = play_string_model(sp.L, sp.mu, tension[(size_t)i], f, core_fraction(i));
						Place& p = place[(size_t)i][(size_t)f];
						p.f1 = r["f1"];
						p.B = s.stiff ? r["B"] : 0.0;
						p.T = tension[(size_t)i];
						p.Leff = r["Leff"];
						p.c = r["c"];
						p.Zs = r["Zs"];
					}
				}
				for (int f = 0; f < NUM_FRETS; ++f)
					action[(size_t)f] = action_network(STRING_SPECS[0].L, f);
			}
			solve_losses();
			solve_pickup();

			if (s.amp_active())
			{
				const bool clean_dc_stable = previous && previous->amp_active && same_amp_dc(previous->s.amp_clean, s.amp_clean);
				const bool clean_spec_stable = previous && previous->amp_active && same_amp_params(previous->s.amp_clean, s.amp_clean);

				if (clean_dc_stable)
				{
					amp_clean_model = previous->amp_clean_model;
					if (!clean_spec_stable)
					{
						amp_clean_model.spec = s.amp_clean;
						amp_clean_model.sag_tau_leak = 0.060 + (1.0 - s.amp_clean.gain / 10.0) * 0.040;
					}
				}
				else
				{
					amp_clean_model = amp_network(s.amp_clean);
				}

				const bool lead_dc_stable = previous && previous->amp_active && same_amp_dc(previous->s.amp_lead, s.amp_lead);
				const bool lead_spec_stable = previous && previous->amp_active && same_amp_params(previous->s.amp_lead, s.amp_lead);

				if (lead_dc_stable)
				{
					amp_lead_model = previous->amp_lead_model;
					if (!lead_spec_stable)
					{
						amp_lead_model.spec = s.amp_lead;
						amp_lead_model.sag_tau_leak = 0.060 + (1.0 - s.amp_lead.gain / 10.0) * 0.040;
					}
				}
				else
				{
					amp_lead_model = amp_network(s.amp_lead);
				}

				amp_active = true;
			}
			else
			{
				amp_active = false;
			}

			if (s.feedback_amount > 1e-4 && s.is_magnetic_pickup())
			{
				if (previous && same_feedback(previous->s, s))
					feedback = previous->feedback;
				else
				{
					FeedbackSpec fbspec;
					fbspec.amount = s.feedback_amount;
					fbspec.distance = s.feedback_distance;
					fbspec.target = s.feedback_target;
					feedback = feedback_network(fbspec);
				}
			}
			else
			{
				feedback = {};
			}

			if (keep_strings && same_attack(previous->s, s))
			{
				y0 = previous->y0;
				tau = previous->tau;
			}
			else
			{
				for (int i = 0; i < NUM_STRINGS; ++i)
					for (int f = 0; f < NUM_FRETS; ++f)
					{
						const auto& sp = STRING_SPECS[i];
						const auto a = pluck_network(sp.mu, sp.L, tension[(size_t)i], f, s.pluck, s.force, s.pick, s.pick_mm);
						y0[(size_t)i][(size_t)f] = a["y0"];
						tau[(size_t)i][(size_t)f] = a["tau"];
					}
			}
			if (previous && same_body(previous->s, s) && std::abs(previous->sample_rate - sr) < 1e-6)
			{
				body = previous->body;
				body_ir = previous->body_ir;
				body_id = previous->body_id;
			}
			else
			{
				body = body_network(body_spec());
				const auto ir = body_impulse(body, sr);
				body_ir.assign(ir.begin(), ir.end());
				body_id = next_id();
			}
			if (previous && same_room(previous->s, s) && std::abs(previous->sample_rate - sr) < 1e-6 && !previous->room_ir[0].empty())
			{
				room = previous->room;
				room_ir = previous->room_ir;
				room_id = previous->room_id;
			}
			else if (with_palm)
			{
				const auto& size = ROOMS[s.room_size];
				room = room_network(size, s.room_material);
				for (int ear = 0; ear < 2; ++ear)
				{
					const auto h = room_impulse(room, size, s.room_distance + 0.09 * ear, sr, 3u + (unsigned)ear);
					room_ir[(size_t)ear].assign(h.begin(), h.end());
				}
				room_id = next_id();
			}
			if (s.palm_intensity < PALM_OFF)
			{
				palm_ready = false;
			}
			else if (previous && previous->palm_ready && same_strings(previous->palm_settings, s) && same_palm(previous->palm_settings, s))
			{
				palm = previous->palm;
				palm_settings = previous->palm_settings;
				palm_ready = true;
			}
			else if (with_palm)
			{
				const auto [x, n] = palm_home();
				for (int i = 0; i < NUM_STRINGS; ++i)
					for (int f = 0; f < NUM_FRETS; ++f)
					{
						const auto& sp = STRING_SPECS[i];
						palm[(size_t)i][(size_t)f] =
							std::make_shared<const Palm>(palm_network(sp.mu, sp.L, f, x, n, tension[(size_t)i]));
					}
				palm_settings = s;
				palm_ready = true;
			}
			else if (previous && previous->palm_ready && previous->s.palm_intensity >= PALM_OFF)
			{
				palm = previous->palm;
				palm_settings = previous->palm_settings;
				palm_ready = true;
			}
		}
	};

} // namespace grmg