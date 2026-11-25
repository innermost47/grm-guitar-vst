// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// From the guitar roll to what the hands do (off the audio thread), as the sequencer of grm_guitar.py:
//   1) the player network: one stroke per moment the right hand strikes, over the whole song (the arm keeps
//      its swing; timing, force, pluck point, pick depth and palm wander as a player's do);
//      then the muscle network: every stroke costs an effort the arm recovers from slowly (a tired arm strikes
//      softer and slower, a stroke after a rest is fresh);
//   2) the strokes meet the strings: with a pick, a sweep network carries the hand's energy across the strings
//      (each string pushed aside takes some, the pick slows, the later strings get less), a strum reaching the
//      strings the arc of the wrist allows (strum_contacts); single notes are picked down on the sixteenths of
//      the beat and up between; with the fingers, a pinch (the thumb leads); the palm is one hand, a band of
//      neighbouring strings that also stops the strings ringing under it (palm network for each);
//   3) the left hand: when a string changes fret, the finger leaves the old fret (the string jumps back to
//      open: a soft pluck, the note stops) and lands on the new one (a knock), and slides on a wound string
//      make their noise (finger networks);
//   4) a note that ends is damped; a string played again forgets the note it had.
// The result is a list of events in beats, read by the engine at the host's position.
#pragma once

#include <algorithm>
#include <map>
#include <optional>
#include <memory>
#include <random>
#include <tuple>
#include <utility>
#include <vector>

#include "../constants/core.hpp"
#include "model.hpp"
#include "player_feel.hpp"
#include "ringing.hpp"
#include "song.hpp"
#include "voice.hpp"

namespace grmg
{

	class Performance
	{
	public:
		std::shared_ptr<const Model> model;						// the networks it was compiled with
		std::vector<models::Event> events;						// sorted by beat
		std::vector<std::vector<float>> squeaks;				// force on the bridge (N), at the model's sample rate
		std::vector<std::shared_ptr<const models::Palm>> palms; // the palms its plucks point to
		double length_beats = 16.0;
		double origin = 0.0; // beat of the song where this performance starts (the loop's start flag)
		double tempo = 90.0;

		size_t first_at(double beat) const
		{
			return (size_t)(std::lower_bound(events.begin(), events.end(), beat,
											 [](const models::Event &e, double b)
											 { return e.beat < b; }) -
							events.begin());
		}
	};

	// palms the strokes ask for, solved once (warm-started from the player's chosen palm)
	class PalmCache
	{
	public:
		std::shared_ptr<const models::Palm> get(const Model &m, int i, int fret, double x_stroke, double n_stroke)
		{
			const double home = m.s.palm_mm / 1000.0;
			if (m.s.palm_intensity < PALM_OFF)
				return nullptr; // the palm is lifted
			const auto [x, n] = Model::palm_snap(home + (x_stroke - PALM_X), n_stroke * m.s.palm_intensity);
			const auto key = std::make_tuple(i, fret, std::lround(x * 1e4), std::lround(std::log10(n) * 1e3),
											 std::lround(m.tension[(size_t)i] * 1e3));
			if (!solved_for_ || !Model::same_strings(*solved_for_, m.s))
			{
				cache_.clear();
				solved_for_ = m.s;
			}
			auto it = cache_.find(key);
			if (it != cache_.end())
				return it->second;
			const auto &sp = m.string(i);
			const models::Palm *ref = m.palm_ready ? m.palm[(size_t)i][(size_t)fret].get() : nullptr;
			auto p = std::make_shared<const models::Palm>(palm_network(sp.mu, sp.L, fret, x, n, m.tension[(size_t)i], 4, ref));
			cache_[key] = p;
			return p;
		}

	private:
		std::map<std::tuple<int, int, long, long, long>, std::shared_ptr<const models::Palm>> cache_;
		std::optional<models::Settings> solved_for_; // the strings it was solved for
	};

	class Compiler
	{
	public:
		PalmCache palms;

		// length: the beats it lasts (0: the song's bars)
		std::unique_ptr<Performance> compile(const Song &song, std::shared_ptr<const Model> model_ptr, const models::CompileOptions &o,
											 double length = 0.0)
		{
			const Model &m = *model_ptr;
			const models::Settings &s = m.s;
			auto perf = std::make_unique<Performance>();
			perf->model = model_ptr;
			perf->length_beats = length > 0 ? length : song.length_beats();
			perf->tempo = o.tempo;
			const double spb = 60.0 / o.tempo; // seconds per beat
			const double step = spb / 4.0;	   // a sixteenth: the half swing of the arm
			std::mt19937 rng(o.seed);
			// the strokes' accidents (Imperfection), drawn apart: the rest of the performance stays as it was
			std::mt19937 accidents(o.seed ^ 0x9E3779B9u);
			const Accidents imperfect{s.human ? &accidents : nullptr, s.imperfection};
			std::normal_distribution<double> normal(0.0, 1.0);
			auto jitter = [&](double sd)
			{ return s.human ? s.human_amount * sd * normal(rng) : 0.0; };

			// ---------------------------------------------------------------- 1) the strokes of the right hand
			// how long each note and strum string rings (as long as the hands let it, see ringing.hpp)
			const models::Ringing rings = ringing(song, perf->length_beats, o.loop);

			std::map<long, models::Moment> moments; // by start, to the 1/960 beat
			auto key = [](double beat)
			{ return std::lround(beat * 960.0); };
			for (size_t q = 0; q < song.notes.size(); ++q)
			{
				const auto &n = song.notes[q];
				if (n.start >= perf->length_beats || n.fret < 0 || n.fret >= NUM_FRETS)
					continue;
				auto &mo = moments[key(n.start)];
				mo.beat = n.start;
				mo.strikes.push_back({n.start, n.string, n.dead ? 0 : n.fret, n.velocity, n.length, n.palm, false, -1,
									  rings.note_end[q] - n.start, n.end, n.dead});
			}
			for (size_t k = 0; k < song.strums.size(); ++k)
			{
				const auto &st = song.strums[k];
				if (st.start >= perf->length_beats)
					continue;
				auto &mo = moments[key(st.start)];
				mo.beat = st.start;
				mo.strum = (int)k;
			}
			std::vector<models::Moment> order;
			for (auto &[k, mo] : moments)
				order.push_back(mo);
			std::vector<models::Stroke> strokes = run_player(order, step, spb, song, s, rng);
			const FeelCurve feel = song.feel_curve();
			muscle(strokes, step);

			if (!strokes.empty() && order.front().beat <= 1e-4)
			{
				strokes.front().t = 0.0;
			}

			// ---------------------------------------------------------------- 2) the strokes meet the strings
			std::vector<models::Played> played;
			std::vector<std::pair<double, double>> palm_spans; // seconds
			std::vector<models::Touch> touches;				   // the palm lands on strings that ring
			for (size_t k = 0; k < order.size(); ++k)
			{
				const auto &mo = order[k];
				const models::Stroke &stroke = strokes[k];
				// the palm is one hand: one band of neighbouring strings, around the ones asked, pressing less on its
				// edges, a little harder when the stroke is harder
				int lo = NUM_STRINGS, hi = -1;
				double longest = 0.0;
				for (const auto &st : mo.strikes)
					if (st.palm)
					{
						lo = std::min(lo, st.string);
						hi = std::max(hi, st.string);
						longest = std::max(longest, st.length);
					}
				if (mo.strum >= 0 && song.strums[(size_t)mo.strum].palm)
				{
					const auto &st = song.strums[(size_t)mo.strum];
					for (int i = 0; i < NUM_STRINGS; ++i)
						if (st.frets[(size_t)i] >= 0)
						{
							lo = std::min(lo, i);
							hi = std::max(hi, i);
						}
					longest = std::max(longest, st.length);
				}
				const bool palm_here = hi >= 0;
				const bool palm_down = palm_here && m.s.palm_intensity >= PALM_OFF;
				auto palm_share = [&](int i)
				{
					if (!palm_here || i < lo - 1 || i > hi + 1)
						return 0.0;
					return (i >= lo && i <= hi) ? 1.0 : PALM_EDGE;
				};
				// the Feel's light palm where no note asks a palm mute: its lane, less where the stroke hits harder
				// (Palm coupling), in notches (a few palm networks, not one per stroke), at the palm's home
				const double light = std::round(FEEL_NOTCHES * light_palm(feel, s, mo.beat)) / FEEL_NOTCHES;
				auto palm_for = [&](int i, int fret) -> const models::Palm *
				{
					const double share = palm_share(i);
					if (share <= 0)
					{
						if (light <= 0.0 || m.s.palm_intensity < PALM_OFF)
							return nullptr;
						return keep(*perf, palms.get(m, i, fret, PALM_X, PALM_FORCE * PALM_LANE_SHARE * light));
					}
					const double press = share * std::pow(std::max(stroke.force, 0.2), PALM_FORCE_FOLLOW);
					return keep(*perf, palms.get(m, i, fret, stroke.palm_x, stroke.palm_N * press));
				};
				std::vector<bool> struck(NUM_STRINGS, false);
				// a stroke the player starts before the song's start (no loop to take it from): it starts at 0, its
				// strings keep their spacing in time instead of all landing on 0
				const bool clamp = (k == 0 && mo.beat <= 1e-4) || !o.loop;
				double early = (k == 0 && mo.beat <= 1e-4) ? -stroke.t : ((!o.loop && stroke.t < 0) ? -stroke.t : 0.0);
				// a strum starts before its beat: nothing before the song's start (no loop to take it from)
				auto keep_after_start = [&](const std::vector<models::Contact> &contacts)
				{
					double lead = 0.0;
					for (const auto &c : contacts)
						lead = std::min(lead, c.delay);
					if (clamp && stroke.t + early + lead < 0.0)
						early = -(stroke.t + lead);
				};

				// a dead touch: the finger resting on the string damps it at once, the pick leaves a click
				auto deaden = [&](double t, int i)
				{ touches.push_back({t + DEAD_TOUCH, i, RELEASE_DEAD, nullptr, DEAD_TOUCH}); };

				// single notes struck together: with a pick, the same gesture as a strum of the Strums lane (the arc of
				// the wrist aimed at them, one sweep across them in the arm's direction: down on the sixteenths of the
				// beat, up between); with the fingers, a pinch
				if (!mo.strikes.empty())
				{
					const bool up = s.pick && (std::lround(mo.beat * 4.0) % 2 != 0);
					std::vector<const models::Strike *> by_string;
					for (const auto &st : mo.strikes)
						by_string.push_back(&st);
					std::sort(by_string.begin(), by_string.end(), [&](auto a, auto b)
							  { return up ? a->string > b->string : a->string < b->string; });
					std::vector<int> strings, frets;
					std::vector<double> velocity;
					double aim = 0.0;
					for (auto *st : by_string)
					{
						strings.push_back(st->string);
						frets.push_back(st->fret);
						velocity.push_back(st->velocity);
						aim += st->string / (double)by_string.size();
					}
					const auto contacts = s.pick ? across(m, strings, frets, velocity, stroke, up, aim, imperfect)
												 : pinch(strings, shares(velocity), (s.human && k > 0) ? &rng : nullptr, s.human_amount);
					keep_after_start(contacts);
					for (const auto &c : contacts)
					{
						const auto it = std::find_if(by_string.begin(), by_string.end(), [&](auto *st)
													 { return st->string == c.string; });
						const double t = stroke.t + early + c.delay;
						if (it == by_string.end())
						{ // a string brushed beside the ones asked: the fretting hand mutes it
							played.push_back({t, c.string, 0, stroke.force * c.share, stroke.p, DEAD_TOUCH, nullptr, stroke, false, -1, false, up});
							deaden(t, c.string);
							continue;
						}
						const auto *st = *it;
						played.push_back({t, st->string, st->fret, stroke.force * c.share, stroke.p, st->ring * spb, palm_for(st->string, st->fret),
										  stroke, palm_share(st->string) > 0, contacts.size() > 1 ? (int)(it - by_string.begin()) : -1, false, up});
						struck[(size_t)st->string] = true;
						if (st->dead || c.muted)
							deaden(t, st->string);
						// a flat hand stops the note at its end, and the strings beside it
						if (st->end == models::RollNote::hand)
							for (int j = st->string - 1; j <= st->string + 1; ++j)
								if (j >= 0 && j < NUM_STRINGS)
									touches.push_back({t + st->length * spb, j, RELEASE_HAND});
					}
				}
				if (mo.strum >= 0)
				{
					const auto &st = song.strums[(size_t)mo.strum];
					std::vector<int> crossing, frets;
					for (int i = 0; i < NUM_STRINGS; ++i)
						if (st.frets[(size_t)i] >= 0 && st.frets[(size_t)i] < NUM_FRETS)
							crossing.push_back(i);
					if (st.up)
						std::reverse(crossing.begin(), crossing.end()); // index 0 is string 6: down = from the bass
					if (crossing.empty())
						continue;
					for (int i : crossing)
						frets.push_back(st.frets[(size_t)i]);
					const auto contacts = across(m, crossing, frets, std::vector<double>(crossing.size(), st.velocity), stroke, st.up, -1.0, imperfect);
					keep_after_start(contacts);
					for (size_t q = 0; q < contacts.size(); ++q)
					{
						const auto &c = contacts[q];
						const double t = stroke.t + early + c.delay;
						const auto at = std::find(crossing.begin(), crossing.end(), c.string);
						if (at == crossing.end())
						{ // a string brushed beside the chord: the fretting hand mutes it
							played.push_back({t, c.string, 0, stroke.force * c.share, stroke.p, DEAD_TOUCH, nullptr, stroke, false, -1, false, st.up});
							deaden(t, c.string);
							continue;
						}
						const int i = c.string, fret = frets[(size_t)(at - crossing.begin())];
						const double ring = rings.strum_end[(size_t)mo.strum][(size_t)i] - st.start;
						played.push_back({t, i, fret, stroke.force * c.share, stroke.p, std::max(ring, st.length) * spb, palm_for(i, fret), stroke,
										  palm_share(i) > 0, (int)q, rings.strum_lifted[(size_t)mo.strum][(size_t)i], st.up});
						struck[(size_t)i] = true;
						if (c.muted)
							deaden(t, i);
					}
				}
				if (palm_here)
				{
					palm_spans.push_back({stroke.t - 0.05, stroke.t + longest * spb});
					// strings under the palm, ringing, not struck: the palm lands on them with its own damping (the
					// palm network of that string at that pressure and place), not a fixed one
					for (int i = 0; i < NUM_STRINGS && palm_down; ++i)
						if (!struck[(size_t)i] && palm_share(i) > 0)
							if (const models::Palm *pp = palm_for(i, 0))
								touches.push_back({stroke.t - 0.03, i, std::min(PALM_LAND_DAMP, pp->sigma_at(0) * PALM_LAND_SPREAD), pp});
				}
			}
			std::stable_sort(played.begin(), played.end(), [](const models::Played &a, const models::Played &b)
							 { return a.t < b.t; });

			// ---------------------------------------------------------------- 3) and 4) string by string
			// in a loop the song's end leads into its start: times wrap around the song, and each string starts
			// its cycle with the hand as the end of the song left it
			const double song_s = perf->length_beats * spb;
			const bool wrap = o.loop && song_s > 0;
			auto to_beat = [&](double t)
			{
				if (!wrap)
					return std::max(0.0, t / spb);
				double b = std::fmod(t, song_s);
				if (b < 0)
					b += song_s;
				return b / spb;
			};
			auto under_palm = [&](double t)
			{
				for (auto [a, b] : palm_spans)
					if (a <= t && t < b)
						return true;
				return false;
			};
			// how long before a stroke the whole hand leaves its shape (gap: the time since the string's last stroke)
			auto hand_lead = [&](double gap)
			{ return std::max(0.025, std::min(0.09, 0.35 * gap)) + std::abs(jitter(0.006)); };
			// how a note ends: the finger relaxes (fretted) or a hand touches the string (open); the strings of a
			// chord are left one after the other, a few milliseconds apart. Left for a new chord shape, the hand
			// touches it as it lifts, ahead of that chord's stroke.
			auto release = [&](const models::Played &p, double t)
			{
				const double spread = p.chord_order >= 0 ? p.chord_order * RELEASE_SPREAD + std::abs(jitter(0.004)) : 0.0;
				if (p.hand_lifts)
					return std::make_pair(t - hand_lead(t - p.t) + spread, s.hand_mute);
				return std::make_pair(t + spread, p.fret > 0 ? RELEASE_FRETTED : RELEASE_OPEN);
			};
			for (int i = 0; i < NUM_STRINGS; ++i)
			{
				const auto &sp = m.string(i);
				std::vector<models::Played> mine;
				for (const auto &p : played)
					if (p.string == i)
						mine.push_back(p);
				if (mine.empty())
					continue;
				int held = 0;											// the fret the left hand holds on this string
				double ringing_until = -1.0, last_t = -1e300;			// the note it rings, the last stroke (s; -1e300: none)
				double end_of_note = -1.0, end_extra = RELEASE_FRETTED; // when the note that rings ends, and how
				if (wrap)
				{ // the hand as the end of the song left it
					held = mine.back().fret;
					last_t = mine.back().t - song_s;
					ringing_until = last_t + mine.back().length;
				}
				auto settle = [&](double t_next) { // the next thing on this string comes at t_next
					// a note that ends just before its string plays again is held to it (legato): the player does not
					// damp a string 60 ms before striking it
					if (end_of_note >= 0 && end_of_note < t_next - 0.06)
						add_damp(*perf, i, to_beat(end_of_note), false, end_extra);
					end_of_note = -1.0;
				};
				for (const auto &p : mine)
				{
					if (p.fret != held)
					{
						const double gap = last_t < -1e299 ? 0.25 : p.t - last_t;
						const bool palm_now = under_palm(p.t);
						auto palm_at = [&](int fret)
						{ return palm_now ? keep(*perf, palms.get(m, i, fret, p.stroke.palm_x, p.stroke.palm_N)) : nullptr; };
						// a finger landing or leaving always changes the vibrating length (the engine carries what
						// rings onto it); the tap or the release it makes is heard only with the finger noises
						const double noise = s.finger_noises ? 1.0 : 0.0;
						auto land_on = [&](double t, int fret)
						{
							const models::Action &a = m.action[(size_t)fret];
							add_pluck(*perf, to_beat(t), make_pluck(m, i, fret, noise * LAND_SHARE * a.h / Y_REF, TAU_LAND, 0.03, models::Kind::land, palm_at(fret)));
						};
						auto leave_to = [&](double t, int fret)
						{
							const models::Action &a = m.action[(size_t)held];
							add_pluck(*perf, to_beat(t), make_pluck(m, i, fret, noise * a.h / Y_REF, TAU_LIFT, std::max(a.xf / sp.L, 0.02), models::Kind::lift, palm_at(fret)));
						};
						// a chord whose shape changes, or a jump along the neck: the whole hand moves
						const bool hand_moves = p.chord_order >= 0 || (held > 0 && p.fret > 0 && std::abs(p.fret - held) > HAND_SPAN_FRETS);
						if (!hand_moves)
						{
							// a line of single notes, the new fret under the hand: one finger moves at the last moment
							// before the pick, and the string goes on ringing at its new length until the stroke. Up
							// the neck a finger lands in front of the one holding; down the neck the finger behind was
							// already resting there and the one in front leaves; from or to the open string a single
							// finger moves.
							const double lead = std::clamp(FINGER_LEAD_SHARE * gap, FINGER_LEAD_MIN, FINGER_LEAD_MAX) + std::abs(jitter(FINGER_LEAD_JITTER));
							const double t_change = p.t - lead;
							settle(t_change);
							if (p.fret > held)
								land_on(t_change, p.fret);
							else
								leave_to(t_change, p.fret);
						}
						else
						{
							// the hand lifts (a new chord shape, or a travel along the neck): it leaves the old fret,
							// touches the string on its way (the old note stops: the strum is heard string by string
							// on quiet strings) and lands on the new one, ahead of the stroke
							const double lead = hand_lead(gap);
							const double t_change = p.t - lead;
							const double t_lift = t_change + std::max(0.0, 0.1 * lead + jitter(0.006));
							const double t_land = t_change + std::max(0.3 * lead, 0.65 * lead + jitter(0.006));
							settle(std::min(t_lift, t_land));
							if (held > 0)
							{
								leave_to(t_lift, 0);
								if (sp.wound && s.finger_noises)
									add_squeak(*perf, m, i, held, p.fret, to_beat(t_lift), std::max(0.04, t_land - t_lift), (unsigned)perf->squeaks.size());
							}
							// the moving hand touches the ringing string, fretted or open (under the palm, the palm's modes)
							if (ringing_until > t_lift)
								add_damp(*perf, i, to_beat(t_lift + SHIFT_TOUCH_DELAY), true, s.hand_mute, palm_at(std::max(held, 0)));
							if (p.fret > 0)
								land_on(t_land, p.fret);
						}
						held = p.fret;
					}
					settle(p.t);
					// the stroke itself: attack network at the stroke's force and pluck point
					const double force = s.force * std::max(p.factor, 0.02);
					const auto a = pluck_network(sp.mu, sp.L, m.tension[(size_t)i], p.fret, p.pluck, force, s.pick, s.pick_mm);
					models::Pluck pl = make_pluck(m, i, p.fret, a["y0"] / Y_REF, a["tau"], p.pluck, models::Kind::strike, p.palm);
					pl.up = p.up;
					add_pluck(*perf, to_beat(p.t), pl);
					ringing_until = p.t + p.length;
					last_t = p.t;
					std::tie(end_of_note, end_extra) = release(p, p.t + p.length);
				}
				// the last note: in a loop it ends before the string's first stroke of the next cycle
				settle(wrap ? mine.front().t + song_s - 0.1 : 1e300);
			}
			// the hand stops of the song: a hand laid flat on every string
			for (double t : song.stops)
				if (t < perf->length_beats)
					for (int i = 0; i < NUM_STRINGS; ++i)
						touches.push_back({t * spb, i, RELEASE_HAND});
			for (const auto &t : touches)
				add_damp(*perf, t.string, to_beat(t.t), false, t.extra, t.contact, t.land_s);
			// the palm stays down from one palm-muted stroke to the next: one span while they overlap, it leaves the
			// strings only at the end of the last
			std::sort(palm_spans.begin(), palm_spans.end());
			std::vector<std::pair<double, double>> down;
			for (const auto &span : palm_spans)
				if (!down.empty() && span.first <= down.back().second)
					down.back().second = std::max(down.back().second, span.second);
				else
					down.push_back(span);
			for (auto [a, b] : down)
			{
				models::Event on, off;
				on.type = models::EventType::palm_on;
				on.beat = to_beat(a);
				off.type = models::EventType::palm_off;
				off.beat = to_beat(b);
				perf->events.push_back(on);
				perf->events.push_back(off);
			}
			// a damp written for a note that the string has already left (struck again first) does nothing: the
			// engine only damps the note that rings. Order: at the same beat, a damp comes before a pluck.
			std::stable_sort(perf->events.begin(), perf->events.end(), [](const models::Event &x, const models::Event &y)
							 {
				if (std::abs(x.beat - y.beat) > 1e-12) return x.beat < y.beat;
				return (x.type == models::EventType::damp) && (y.type != models::EventType::damp); });
			return perf;
		}

	public:
		// the pick sweeps across `strings` (in the order it meets them), each asked with its own share of the stroke
		// (`base`). A GRM network carries the energy of the hand along the sweep: pushing string j aside takes the
		// elastic energy k_j d_j^2 / 2 of a string pressed at its pluck point (stiffness T Leff / (x (Leff - x))),
		// so w_{j+1} = (1 - c_j) w_j, with w the share of the energy left and c_j that energy over the hand's. The
		// pick slows (delays grow), the later strings get less: a slow, soft strum fades across the chord, a fast
		// one hardly. The delays start at `first` (the first string the arc of the wrist reaches).
		static std::vector<models::Contact> sweep(const Model &m, const std::vector<int> &strings, const std::vector<double> &base,
												  const std::vector<int> &frets, const models::Stroke &stroke, double first)
		{
			std::vector<models::Contact> out;
			if (strings.empty())
				return out;
			const double v0 = std::max(stroke.v, 0.05);
			const double kinetic = 0.5 * HAND_MASS * v0 * v0;
			grm::Network net;
			std::map<std::string, grm::Measurement> meas;
			for (size_t j = 0; j < strings.size(); ++j)
				net.add_node("w" + std::to_string(j), 1.0, true);
			meas["w0"] = {1.0, 1e-6};
			for (size_t j = 0; j + 1 < strings.size(); ++j)
			{
				const int i = strings[j];
				const double leff = m.string(i).L * std::pow(2.0, -std::max(frets[j], 0) / 12.0);
				const double x = std::min(std::max(stroke.p, 0.05), 0.95);
				const double k = m.tension[(size_t)i] / (leff * x * (1.0 - x));
				const double d = FULL_DEPTH * std::min(1.0, std::max(base[j], 0.0));
				const double c = std::min(SWEEP_LOSS_MAX, 0.5 * k * d * d / kinetic);
				net.add_sum("sweep " + std::to_string(j), {{"w" + std::to_string(j + 1), 1.0}, {"w" + std::to_string(j), -(1.0 - c)}},
							0.0, 1e-6);
			}
			const auto r = net.solve(meas, 20, 1e-3, 1e-14, false);
			double delay = first;
			for (size_t j = 0; j < strings.size(); ++j)
			{
				const double w = std::max(r["w" + std::to_string(j)], 1e-6);
				if (j)
					delay += std::abs(strings[j] - strings[j - 1]) * STRING_SPACING / (v0 * std::sqrt(w));
				out.push_back({strings[j], delay, base[j] * std::sqrt(w)});
			}
			return out;
		}

		// the Feel's light palm at `beat` (0 lifted .. 1): its lane, plus Palm coupling's share of how soft the bar is
		// there (> 0: the softer, the more palm, none on the hardest; < 0: the harder, the more palm)
		static double light_palm(const FeelCurve &feel, const models::Settings &s, double beat)
		{
			const double force = std::clamp(s.feel_depth * feel.force(beat), -1.0, 1.0);
			const double softness = s.palm_coupling >= 0.0 ? 0.5 * (1.0 - force) : 0.5 * (1.0 + force);
			return std::clamp(feel.palm(beat) + std::abs(s.palm_coupling) * softness, 0.0, 1.0);
		}

		// a velocity as the share of the stroke it asks (0.8: the stroke itself)
		static std::vector<double> shares(const std::vector<double> &velocity)
		{
			std::vector<double> out;
			for (double v : velocity)
				out.push_back(v / 0.8);
			return out;
		}

		// what can go wrong in a stroke (Imperfection): the rng to draw from (none: nothing goes wrong), how often
		struct Accidents
		{
			std::mt19937 *rng = nullptr;
			double amount = 0.0;
			bool happens(double chance) const
			{
				return rng && amount > 0 && std::uniform_real_distribution<double>(0.0, 1.0)(*rng) < chance * amount;
			}
			size_t pick(size_t count) const // one of `count` (only drawn when an accident happens)
			{
				return std::uniform_int_distribution<size_t>(0, count - 1)(*rng);
			}
		};

		// the pick across `crossing` (the strings in the order it meets them, each with its fret and velocity): the arc
		// of the wrist aimed at `aim` (a string index; < 0: the whole neck, the arc of a strum) says which it reaches
		// and how deep, then the sweep carries the hand's energy across them (strum_contacts, sweep). An imperfect
		// stroke misses its last string, brushes the next one (muted by the fretting hand), or chokes a string.
		static std::vector<models::Contact> across(const Model &m, const std::vector<int> &crossing, const std::vector<int> &frets,
												   const std::vector<double> &velocity, const models::Stroke &stroke, bool up, double aim,
												   const Accidents &accidents)
		{
			const auto reach = crossing.size() == 1 ? std::vector<models::Contact>{{crossing[0], 0.0, 1.0}}
													: strum_contacts(crossing, stroke, up, aim);
			if (reach.empty())
				return {};
			std::vector<int> strings, reached_frets;
			std::vector<double> base;
			for (const auto &c : reach)
			{
				const size_t at = (size_t)(std::find(crossing.begin(), crossing.end(), c.string) - crossing.begin());
				strings.push_back(c.string);
				reached_frets.push_back(frets[at]);
				base.push_back(c.share * velocity[at] / 0.8);
			}
			auto contacts = sweep(m, strings, base, reached_frets, stroke, reach.front().delay);
			if (contacts.size() < 2)
				return contacts;
			if (contacts.size() >= 3 && accidents.happens(MISS_CHANCE))
				contacts.pop_back();
			if (accidents.happens(BRUSH_CHANCE))
			{
				const auto &last = contacts.back();
				const int next = last.string + (up ? -1 : 1);
				if (next >= 0 && next < NUM_STRINGS && std::find(crossing.begin(), crossing.end(), next) == crossing.end())
					contacts.push_back({next, last.delay + STRING_SPACING / std::max(stroke.v, 0.05), last.share * BRUSH_SHARE});
			}
			// a fretting finger that slips or presses too little: one string of the chord chokes (never the whole stroke)
			if (accidents.happens(CHOKE_CHANCE))
				contacts[accidents.pick(contacts.size())].muted = true;
			// the beat: the player lands it on the strings around B and G, not on the first string met (Freire et al.
			// 2018): a downstroke starts before the beat, an upstroke reaches it at once
			const auto anchor = std::min_element(contacts.begin(), contacts.end(), [](const auto &a, const auto &b)
												 { return std::abs(a.string - BEAT_STRING) < std::abs(b.string - BEAT_STRING); });
			const double on_beat = anchor->delay;
			for (auto &c : contacts)
				c.delay -= on_beat;
			return contacts;
		}

		// fingers plucking together (a pinch): each finger its own force and timing; the thumb (the bass) a little
		// ahead of the others
		template <class Rng>
		static std::vector<models::Contact> pinch(const std::vector<int> &strings, const std::vector<double> &base, Rng *rng,
												  double amount = 1.0)
		{
			std::vector<models::Contact> out;
			if (strings.empty())
				return out;
			const int bass = *std::min_element(strings.begin(), strings.end());
			std::normal_distribution<double> normal(0.0, 1.0);
			for (size_t j = 0; j < strings.size(); ++j)
			{
				const bool thumb = strings.size() > 1 && strings[j] == bass;
				double delay = thumb ? 0.0 : THUMB_LEAD * (strings.size() > 1);
				double share = base[j];
				if (rng)
				{
					delay += std::abs(amount * PINCH_SPREAD * normal(*rng));
					share *= std::pow(10.0, amount * PINCH_FORCE_SIGMA * normal(*rng));
				}
				out.push_back({strings[j], delay, share});
			}
			return out;
		}

		// the muscle: every stroke costs an effort (its force squared) the arm recovers from slowly; a GRM network
		// carries the fatigue F along the strokes, F_k = e^{-dt/RECOVER} F_{k-1} + EFFORT force_k^2, and a tired arm
		// strikes softer and slower. Measured against the arm playing eighths at force 1 (the player's ordinary
		// effort): a stroke after a rest comes out a little stronger, the end of a long run of sixteenths weaker.
		static void muscle(std::vector<models::Stroke> &strokes, double step)
		{
			if (strokes.empty())
				return;
			const double reference = MUSCLE_EFFORT / (1.0 - std::exp(-2.0 * step / MUSCLE_RECOVER));
			double carried = 0.0, t_carried = strokes.front().t - 10.0 * MUSCLE_RECOVER;
			const size_t window = 64;
			for (size_t a = 0; a < strokes.size(); a += window)
			{
				const size_t b = std::min(strokes.size(), a + window);
				grm::Network net;
				std::map<std::string, grm::Measurement> meas;
				net.add_node("F_", carried, true);
				meas["F_"] = {carried, 1e-9};
				double t_prev = t_carried;
				std::string prev = "F_";
				for (size_t k = a; k < b; ++k)
				{
					const std::string n = "F" + std::to_string(k);
					net.add_node(n, carried, true);
					const double decay = std::exp(-std::max(strokes[k].t - t_prev, 0.0) / MUSCLE_RECOVER);
					net.add_sum("effort " + std::to_string(k), {{n, 1.0}, {prev, -decay}}, MUSCLE_EFFORT * strokes[k].force * strokes[k].force,
								1e-9);
					prev = n;
					t_prev = strokes[k].t;
				}
				const auto r = net.solve(meas, 20, 1e-3, 1e-16, false);
				for (size_t k = a; k < b; ++k)
				{
					const double f = r["F" + std::to_string(k)];
					const double factor = std::clamp((1.0 + MUSCLE_WEAKEN * reference) / (1.0 + MUSCLE_WEAKEN * f), MUSCLE_LIMITS[0],
													 MUSCLE_LIMITS[1]);
					strokes[k].force *= factor;
					strokes[k].v *= factor;
				}
				carried = r[prev];
				t_carried = t_prev;
			}
		}

	private:
		static const models::Palm *keep(Performance &perf, std::shared_ptr<const models::Palm> p)
		{
			perf.palms.push_back(p);
			return p.get();
		}

		static models::Pluck make_pluck(const Model &m, int i, int fret, double gain, double tau, double pluck, models::Kind kind, const models::Palm *palm)
		{
			models::Pluck p;
			p.string = i;
			p.fret = fret;
			p.place = m.place[(size_t)i][(size_t)fret];
			p.gain = gain;
			p.tau = tau;
			p.pluck = pluck;
			p.kind = kind;
			p.palm = palm;
			p.pick = m.s.pick;
			return p;
		}

		static void add_pluck(Performance &perf, double beat, const models::Pluck &p)
		{
			models::Event e;
			e.type = models::EventType::pluck;
			e.beat = beat;
			e.string = p.string;
			e.pluck = p;
			perf.events.push_back(e);
		}

		// damp: the end of a note. `left`: the finger leaves the fret (the lift pluck takes over)
		static void add_damp(Performance &perf, int i, double beat, bool left, double extra, const models::Palm *contact = nullptr,
							 double land_s = 0.0)
		{
			models::Event e;
			e.type = models::EventType::damp;
			e.beat = beat;
			e.string = i;
			e.squeak = left ? 1 : 0;
			e.extra = extra;
			e.contact = contact;
			e.land_s = land_s;
			perf.events.push_back(e);
		}

		void add_squeak(Performance &perf, const Model &m, int i, int from, int to, double beat, double dur, unsigned seed)
		{
			const auto &sp = m.string(i);
			const models::Squeak v = squeak_network(sp.mu, sp.L, m.core_fraction(i), from, to, dur, m.s.finger_force);
			std::vector<double> bumps;
			if (from > 0 && to > 0)
			{
				// the finger rides over the fret wires between the two frets (as GRMGuitarApp.squeak)
				auto pos = [&](int k)
				{ return v.L * (1 - std::pow(2.0, -k / 12.0)); };
				const double xa = pos(from) - 0.005, xb = pos(to) - 0.005;
				for (int k = std::min(from, to); k < std::max(from, to); ++k)
				{
					const double c = (pos(k) - xa) / (xb - xa);
					if (c > 0.0 && c < 1.0)
						bumps.push_back(std::acos(1 - 2 * c) / PI);
				}
			}
			const double mid = (from + (to > 0 ? to : from)) / 2.0;
			const auto sound = synthesize_squeak(v.fsq, v.P, dur, v.fL, 1 - std::pow(2.0, -mid / 12.0), v.v, m.sample_rate, seed, bumps, v.N);
			perf.squeaks.emplace_back(sound.begin(), sound.end());
			models::Event e;
			e.type = models::EventType::squeak;
			e.beat = beat;
			e.string = i;
			e.squeak = (int)perf.squeaks.size() - 1;
			perf.events.push_back(e);
		}

		// the player network over the moments, in windows (one network per window; the live player, PlayerFeel, goes
		// on from one window to the next): what the arm does at one stroke depends on the strokes around it
		template <class Moments, class Rng>
		static std::vector<models::Stroke> run_player(const Moments &order, double step, double spb, const Song &song,
													  const models::Settings &s, Rng &rng)
		{
			PlayerFeel player(s, song.meter(), song.feel_curve(), (unsigned)rng());
			std::vector<std::pair<double, int>> grid;
			std::vector<models::StrokeFeel> feel;
			for (const auto &mo : order)
			{
				grid.push_back({mo.beat * spb, (int)std::lround(mo.beat * 4.0)});
				feel.push_back(player.next(grid.back().first, mo.beat));
			}
			std::vector<models::Stroke> out;
			const size_t window = 32;
			for (size_t a = 0; a < grid.size(); a += window)
			{
				const size_t b = std::min(grid.size(), a + window);
				const std::vector<std::pair<double, int>> part(grid.begin() + (long)a, grid.begin() + (long)b);
				const std::vector<models::StrokeFeel> aims(feel.begin() + (long)a, feel.begin() + (long)b);
				auto r = player_network(part, aims, step, s.pluck, s.swing_cm / 100.0);
				out.insert(out.end(), r.begin(), r.end());
			}
			return out;
		}
	};

	// what plays: the whole song, or (in a loop) the part between the loop's flags, its end leading into its start
	inline std::unique_ptr<Performance> compile_song(Compiler &compiler, const Song &song, std::shared_ptr<const Model> model,
													 models::CompileOptions o)
	{
		if (!o.loop)
			return compiler.compile(song, std::move(model), o);
		const auto [a, b] = song.loop_range();
		auto perf = compiler.compile(song.section(a, b), std::move(model), o, b - a);
		perf->origin = a;
		return perf;
	}

} // namespace grmg
