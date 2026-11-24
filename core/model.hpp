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

#include "../constants/core.hpp"
#include "feedback.hpp"
#include "amp/amp_networks.hpp"
#include "physics.hpp"

namespace grmg
{

	class Model
	{
	public:
		models::Settings s;
		double sample_rate = 48000.0;
		std::array<double, NUM_STRINGS> tension{};
		std::array<std::array<models::Place, NUM_FRETS>, NUM_STRINGS> place{};
		std::array<models::StringLosses, NUM_STRINGS> losses{};
		std::array<std::array<double, NUM_FRETS>, NUM_STRINGS> y0{}, tau{};
		std::array<std::array<std::shared_ptr<const models::Palm>, NUM_FRETS>, NUM_STRINGS> palm{};
		std::array<models::Action, NUM_FRETS> action{};
		models::Body body;
		std::vector<float> body_ir;
		std::array<std::vector<float>, 2> room_ir;
		models::Room room;
		models::Settings palm_settings;
		bool palm_ready = false;
		unsigned body_id = 0, room_id = 0;

		models::AmpModel amp;
		bool amp_active = false;

		models::FeedbackModel feedback;

		static unsigned next_id()
		{
			static std::atomic<unsigned> counter{0};
			return ++counter;
		}

		int open_midi(int i) const { return TUNINGS[s.tuning].midi[i]; }
		double core_fraction(int i) const { return STRING_SPECS[i].wound ? s.core : 1.0; }

		static bool same_strings(const models::Settings &a, const models::Settings &b)
		{
			return a.tuning == b.tuning && std::abs(a.core - b.core) < 1e-12 && a.stiff == b.stiff;
		}
		static bool same_attack(const models::Settings &a, const models::Settings &b)
		{
			return a.pick == b.pick && std::abs(a.pluck - b.pluck) < 1e-12 &&
				   std::abs(a.force - b.force) < 1e-12 && std::abs(a.pick_mm - b.pick_mm) < 1e-12;
		}
		static bool same_palm(const models::Settings &a, const models::Settings &b)
		{
			return std::abs(a.palm_mm - b.palm_mm) < 1e-9 && std::abs(a.palm_intensity - b.palm_intensity) < 1e-9;
		}
		static bool same_body(const models::Settings &a, const models::Settings &b)
		{
			return std::abs(a.body_size - b.body_size) < 1e-12 && std::abs(a.top_mm - b.top_mm) < 1e-12 &&
				   std::abs(a.coupling - b.coupling) < 1e-12 && std::abs(a.braces_g - b.braces_g) < 1e-9 &&
				   std::abs(a.body_q - b.body_q) < 1e-9 && a.wood == b.wood &&
				   std::abs(a.density_pct - b.density_pct) < 1e-9 && std::abs(a.hollow - b.hollow) < 1e-9;
		}
		static bool same_room(const models::Settings &a, const models::Settings &b)
		{
			return a.room_size == b.room_size && a.room_material == b.room_material &&
				   std::abs(a.room_distance - b.room_distance) < 1e-9;
		}
		static bool same_pickups(const models::Settings &a, const models::Settings &b)
		{
			return a.sensor == b.sensor && std::abs(a.pickup_mm - b.pickup_mm) < 1e-9 &&
				   a.pickup_type == b.pickup_type && std::abs(a.cable_pf - b.cable_pf) < 1e-9 &&
				   a.pickup_bridge_on == b.pickup_bridge_on && a.pickup_middle_on == b.pickup_middle_on &&
				   a.pickup_neck_on == b.pickup_neck_on && a.pickup_bridge_split == b.pickup_bridge_split &&
				   a.pickup_bridge_type == b.pickup_bridge_type && a.pickup_middle_type == b.pickup_middle_type &&
				   a.pickup_neck_type == b.pickup_neck_type;
		}

		static bool same_feedback(const models::Settings &a, const models::Settings &b)
		{
			return std::abs(a.feedback_amount - b.feedback_amount) < 1e-4 &&
				   std::abs(a.feedback_distance - b.feedback_distance) < 1e-4 &&
				   a.feedback_target == b.feedback_target;
		}

		static std::pair<double, double> palm_snap(double x, double force)
		{
			const double xs = std::max(0.002, std::round(x * 333.3) / 333.3);
			const double fs = std::pow(10.0, std::round(std::log10(std::max(force, 1e-3)) / 0.05) * 0.05);
			return {xs, fs};
		}
		std::pair<double, double> palm_home() const
		{
			return palm_snap(s.palm_mm / 1000.0, PALM_FORCE * s.palm_intensity);
		}

		models::BodySpec body_spec() const
		{
			models::BodySpec b;
			b.size = s.body_size;
			b.top_mm = s.top_mm;
			b.coupling = s.coupling;
			b.braces_kg = s.braces_g / 1000.0;
			b.q = s.body_q;
			const auto &w = WOODS[std::min(std::max(s.wood, 0), NUM_WOODS - 1)];
			b.rho = w.rho * (1.0 + s.density_pct / 100.0);
			b.E = w.E;
			b.hollow = s.hollow;
			return b;
		}

		models::PickupCircuit pickup;
		std::array<models::PickupCircuit, 3> pickup3{};
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
				pickup3_on = {s.pickup_bridge_on, s.pickup_middle_on, s.pickup_neck_on};
			}
		}

		std::array<double, 3> pickup_xs() const
		{
			if (s.sensor == 1)
				return {s.pickup_mm / 1000.0, 0.0, 0.0};
			if (s.sensor == 3)
				return {PICKUP_POS_MM[0] / 1000.0,
						PICKUP_POS_MM[1] / 1000.0,
						PICKUP_POS_MM[2] / 1000.0};
			return {0.0, 0.0, 0.0};
		}

		double pickup_x() const { return pickup_xs()[0]; }

		void solve_losses()
		{
			for (int i = 0; i < NUM_STRINGS; ++i)
			{
				const auto r = string_losses_network(i, s.string_age);
				losses[(size_t)i] = {r["etaF"], r["etaA"], r["etaB"], 0.0};
			}
		}

		void solve(const models::Settings &settings, double sr, const Model *previous, bool with_palm = true)
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
					const auto &sp = STRING_SPECS[i];
					tension[(size_t)i] = tune_string_model(sp.L, sp.mu, midi_hz(open_midi(i)), core_fraction(i))["T"];
					for (int f = 0; f < NUM_FRETS; ++f)
					{
						const auto r = play_string_model(sp.L, sp.mu, tension[(size_t)i], f, core_fraction(i));
						models::Place &p = place[(size_t)i][(size_t)f];
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

			// the amplifier: its preamps' curves are kept while their circuit stays
			amp_active = s.amp_active();
			if (amp_active)
				amp = amp_model(s.amp, previous && previous->amp_active ? &previous->amp : nullptr);

			if (s.feedback_amount > 1e-4 && s.is_magnetic_pickup())
			{
				if (previous && same_feedback(previous->s, s))
					feedback = previous->feedback;
				else
				{
					models::FeedbackSpec fbspec;
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
						const auto &sp = STRING_SPECS[i];
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
				const auto &size = ROOMS[s.room_size];
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
						const auto &sp = STRING_SPECS[i];
						palm[(size_t)i][(size_t)f] =
							std::make_shared<const models::Palm>(palm_network(sp.mu, sp.L, f, x, n, tension[(size_t)i]));
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