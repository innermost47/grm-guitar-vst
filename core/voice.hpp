// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// voice.hpp
//
// One string as it sounds: the force it puts on the bridge (newtons), as synthesize_string in grm_guitar.py.
//   - a bank of modes, each a damped complex oscillator, released from the triangle of the pluck;
//   - losses: inside the string and the air (losses network), the energy given to the bridge (body network), the palm;
//   - under the palm, the modes move (palm network) and some no longer ring at all (overdamped: two real
//     exponentials); the palm's pressure drops while the pick goes through and comes back (PALM_RECOVER);
//   - the onset is the release itself, softened by the release time of the attack network, after the pick's push
//     (it slips and scrapes on the string);
//   - a hard attack: the string bounces off the last fret, its tension rises with its vibration (pitch glide).
// And one string answering the others through the bridge (sympathetic strings): each mode a resonator
// driven by the velocity of the bridge, V = Y(f_n) F_others (sympathetic_response in grm_guitar.py).
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstdint>

#include "magnetic_pickup.hpp"
#include "model.hpp"

namespace grmg
{

	// the products of the audio loops, written out: std::complex's operator* checks for infinities and NaN on every
	// call (C99 Annex G), several times slower in the loops that run for every mode at every sample
	inline cplx cmul(cplx a, cplx b)
	{
		return {a.real() * b.real() - a.imag() * b.imag(), a.real() * b.imag() + a.imag() * b.real()};
	}
	inline double cmul_re(cplx a, cplx b) { return a.real() * b.real() - a.imag() * b.imag(); }

	// Fourier amplitudes of a string released from a triangle with its apex at p (share of the length)
	inline double pluck_amplitude(int n, double p)
	{
		p = std::min(std::max(p, 0.02), 0.98);
		return 2.0 * std::sin(n * PI * p) / (n * n * PI * PI * p * (1.0 - p));
	}

	// the same for a string pushed by the pick (apex 1 at xp) onto a fret at xf, where it lies at height hf (a share
	// of the apex): a line with two bends. The sine coefficients of a broken line held at both ends are
	// -2 L / (n pi)^2 sum(change of slope at each bend * sin(n pi x_bend / L))
	inline double pressed_amplitude(int n, double L, double xp, double xf, double hf)
	{
		const bool fret_first = xf < xp;
		const std::array<double, 4> x = {0.0, fret_first ? xf : xp, fret_first ? xp : xf, L};
		const std::array<double, 4> y = {0.0, fret_first ? hf : 1.0, fret_first ? 1.0 : hf, 0.0};
		double sum = 0.0;
		for (size_t k = 1; k < 3; ++k)
		{
			const double before = (y[k] - y[k - 1]) / std::max(x[k] - x[k - 1], 1e-9);
			const double after = (y[k + 1] - y[k]) / std::max(x[k + 1] - x[k], 1e-9);
			sum += (after - before) * std::sin(n * PI * x[k] / L);
		}
		return -2.0 * L / (n * n * PI * PI) * sum;
	}

	inline double sinc(double x) { return std::abs(x) < 1e-12 ? 1.0 : std::sin(PI * x) / (PI * x); }

	// no magnetic pickup under the strings
	inline const std::array<MagneticPickup, 3> &no_pickups()
	{
		static const std::array<MagneticPickup, 3> none{};
		return none;
	}

	class StringVoice
	{
	public:
		bool active = false;
		models::Pluck what;
		int note = -1; // MIDI note that holds it (live playing), -1 otherwise
		int note_channel = 0;

		bool holds_midi_note(int channel, int pitch) const
		{
			return active && note >= 0 && (channel == 0 || note_channel == channel) && (pitch < 0 || note == pitch);
		}

		// pickups: the magnetic pickups under the string (render's `heard` outputs): each coil follows the string's
		// motion over it in both planes, and its voltage is MagneticPickup's (the window, the flux curve, the distance)
		// voicing: how the string answers the attack (models::Voicing).
		//   - Its two planes: each mode n rings twice, across the top, with the losses into the body (body.loss), and
		//     along it, where the bridge hardly gives (h_coupling of those losses), a little detuned (the reactive
		//     part of the same coupling, and the split). The attack's angle shares the pluck between them (sin across,
		//     cos along); what reaches the bridge, the body and the room from the plane along the top is h_radiation
		//     of its force. A magnetic pickup hears the plane across the top towards its poles, the other one only
		//     through the distance it adds (MagneticPickup::voltage).
		//   - The last fret under the plane across the top: the string bounces off it (see collide).
		//   - The pick's push before the release (a strike only): the string is drawn aside along a progressive curve,
		//     slipping on the pick and scraping its edge, then the modes start from the shape it was pushed to.
		//   - Its tension, raised by its own vibration: every mode is retuned by it, and it pulls on the bridge.
		// carry: the vibration the string already has when a finger lands or leaves (lift, land). The string is not
		// struck again, only its vibrating length changes: each mode n of each plane goes on as the same mode of the
		// new length, with the force it puts on the bridge at that instant (the shape near the saddle has not moved
		// yet), and the finger's own small excitation is added to it.
		void start(const models::Pluck &p, const models::Body &body, const models::StringLosses &losses, double brightness, double sr,
				   const std::array<MagneticPickup, 3> &pickups = no_pickups(),
				   const StringVoice *carry = nullptr, const models::Voicing &voicing = models::Voicing::single_plane())
		{
			std::array<std::array<cplx, MAX_MODES>, PLANES> carried{};
			if (carry && carry->active)
				for (int k = 0; k < carry->count_; ++k)
					carried[(size_t)carry->plane_[(size_t)k]][(size_t)carry->n_[(size_t)k] - 1] = carry->sounding(k);
			what = p;
			sr_ = sr;
			count_ = 0;
			over_ = 0;
			t_ = 0;
			fade_ = 1.0;
			fade_step_ = 0.0;
			const models::Place &pl = p.place;
			const bool regain = p.palm && p.kind == models::Kind::strike;
			// the planes: the pluck's share in each, the share of each one's bridge force that is heard (the plane
			// along the top keeps its true force inside: its slots hold the heard force, the pickup gets it back)
			const double angle = (p.up ? voicing.angle_up : voicing.angle_down) * PI / 180.0;
			const double radiated = std::max(voicing.h_radiation, 1e-3);
			const std::array<double, PLANES> share = {std::sin(angle), std::cos(angle)};
			const std::array<double, PLANES> heard = {1.0, radiated};
			const std::array<double, PLANES> coupling = {1.0, voicing.h_coupling};
			const std::array<double, PLANES> motion = {1.0, 1.0 / radiated}; // a slot's true force over the one it holds
			set_coils(pickups, pl.magnetic);
			const double split = std::pow(2.0, -voicing.split_cents / 1200.0);
			// the last fret, its distance from the saddle, and the string's height over it: the string runs straight
			// from the fret it is held on (or the nut) to the saddle, twice the action high there
			const double fret_x = pl.L * std::pow(2.0, -NUM_FRETS / 12.0);
			const bool frets = voicing.action_mm > 0 && pl.Leff > 1.05 * fret_x;
			clearance_ = frets ? 2.0 * voicing.action_mm / 1000.0 * (pl.Leff - fret_x) / pl.Leff : 0.0;
			bounce_ = std::min(std::max(voicing.fret_bounce, 0.0), 1.0);
			click_ = voicing.fret_click;
			click_env_ = 0.0;
			click_decay_ = std::exp(-1.0 / (FRET_CLICK_S * sr_));
			inv_mass_ = 0.0;
			// the pick pushes the plane across the top towards the frets: past the last fret's clearance the string
			// lies on it, and is released from a shape bent there as well (it does not go through the fret)
			const double apex = Y_REF * p.gain * share[0], xp = p.pluck * pl.Leff;
			const double over_fret = fret_x > xp ? (pl.Leff - fret_x) / (pl.Leff - xp) : fret_x / xp;
			const bool pressed = frets && apex * over_fret > clearance_;
			std::array<std::array<double, MAX_MODES>, PLANES> amp{}; // each plane's modes, from a shape of apex 1
			double peak = 0.0;
			int count = 0;
			for (int n = 1; n <= MAX_MODES; ++n)
			{
				const double f = n * pl.f1 * std::sqrt(1.0 + pl.B * n * n);
				if (f >= 0.45 * sr_)
					break;
				double tilt = n * std::pow((double)n, -(1.0 - brightness) * 0.6);
				if (p.tau > 0)
					tilt *= sinc(f * p.tau);
				const double free = pluck_amplitude(n, p.pluck);
				amp[0][(size_t)count] = tilt * (pressed ? pressed_amplitude(n, pl.Leff, xp, fret_x, clearance_ / apex) : free);
				amp[1][(size_t)count] = tilt * free;
				peak = std::max({peak, std::abs(amp[0][(size_t)count]), std::abs(amp[1][(size_t)count])});
				++count;
			}
			// newtons on the bridge: T pi / L * (release height) * (normalised modal amplitudes)
			const double scale = pl.T * PI / pl.Leff * Y_REF * p.gain;
			double first = 0.0;
			across_ = 0;
			push_disp_ = {};
			// the slots plane by plane (all the modes across the top, then along it): the coils follow each plane over
			// a contiguous run of slots (coil_motion)
			for (int plane = 0; plane < PLANES; ++plane)
			{
				if (plane == 1)
					across_ = count_;
				for (int k = 0; k < count; ++k)
				{
					const int n = k + 1;
					const double f0 = n * pl.f1 * std::sqrt(1.0 + pl.B * n * n);
					const double into_body = body.loss(f0, pl.c, pl.Leff, pl.Zs);
					// the plane along the top: what the body's reactance pulls the plane across it away from (it stays put)
					const double detune = (1.0 - voicing.h_coupling) * body.detune(f0, pl.c, pl.Leff, pl.Zs) / (2 * PI);
					// mode n's displacement per newton on the bridge (F = T n pi / L y), where the coils and the fret are
					const double per_newton = pl.Leff / (pl.T * n * PI);
					std::array<double, MAX_COILS> at_coil{};
					for (int c = 0; c < coils_; ++c)
						at_coil[(size_t)c] = per_newton * pickups_[(size_t)coil_pickup_[(size_t)c]].coil_shape(coil_index_[(size_t)c], n, pl.Leff);
					const double at_fret = frets ? per_newton * std::sin(n * PI * fret_x / pl.Leff) : 0.0;
					const double a = amp[(size_t)plane][(size_t)k] * share[(size_t)plane] * heard[(size_t)plane];
					const cplx kept = carried[(size_t)plane][(size_t)k];
					if (std::abs(a) < 1e-4 * peak && kept == cplx(0.0, 0.0))
						continue; // inaudible (a node at the pluck point, a plane not plucked, nothing ringing on it)
					double f = plane == 0 ? f0 : (f0 - detune) * split;
					double sg = losses.sigma(f0, n, pl.B) + coupling[(size_t)plane] * into_body + body.neck_loss(pl, f0, (int)plane);
					const double f_free = f, sg_free = sg;
					double sp = 0.0;
					if (p.palm)
					{
						sp = p.palm->sigma_at((size_t)k);
						sg += sp;
						f *= p.palm->ratio_at((size_t)k);
					}
					const double w = 2 * PI * f;
					first += a;
					for (int c = 0; c < coils_; ++c)
						push_disp_[(size_t)plane][(size_t)c] += (scale * a + kept.real()) * at_coil[(size_t)c] * motion[(size_t)plane];
					if (sg < w)
					{
						// cos(wd t) + (sg / wd) sin(wd t) = Re[(1 - i sg / wd) e^(i wd t)]
						const double wd = std::sqrt(w * w - sg * sg);
						const size_t m = (size_t)count_++;
						n_[m] = n;
						plane_[m] = plane;
						z_[m] = scale * a * cplx(1.0, -sg / wd) + kept;
						step_[m] = std::exp(cplx(-sg, wd) / sr_);
						rate_[m] = cplx(-sg, wd);
						free_w_[m] = 2 * PI * f_free;
						free_sg_[m] = sg_free;
						hand_ratio_[m] = p.palm ? p.palm->ratio_at((size_t)k) : 1.0;
						hand_sigma_[m] = sp;
						extra_[m] = 0.0;
						// each coil follows the string over it: mode n's true displacement there, and its velocity
						for (int c = 0; c < coils_; ++c)
						{
							coil_disp_[(size_t)c][m] = motion[(size_t)plane] * at_coil[(size_t)c];
							coil_vel_[(size_t)c][m] = rate_[m] * coil_disp_[(size_t)c][m];
						}
						// the fret meets the plane across the top: the string's displacement and velocity over it, and
						// what an impulse J there does to the mode: its velocity jumps by 2 / (mu L) sin(n pi x / L) J,
						// so its bridge force's rate by T n pi / L of that, which turns z by -i of it over wd
						const double fret_here = plane == 0 ? at_fret : 0.0;
						at_fret_[m] = fret_here;
						fret_rate_[m] = rate_[m] * fret_here;
						const double shape = std::sin(n * PI * fret_x / pl.Leff);
						const double kick = frets && plane == 0 ? 2.0 * pl.c * pl.c * n * PI / (pl.Leff * pl.Leff) * shape / wd : 0.0;
						kick_[m] = cplx(0.0, -kick);
						if (frets && plane == 0)
							inv_mass_ += 2.0 * pl.c * pl.c / (pl.T * pl.Leff) * shape * shape;
						inv_heard2_[m] = 1.0 / (heard[(size_t)plane] * heard[(size_t)plane]);
						q_[m] = regain ? sp * PALM_DROP * PALM_RECOVER : 0.0;
						if (regain)
							z_[m] *= std::exp(q_[m]); // once the palm is back
					}
					else
					{
						// the palm damps it more than it rings: it only creeps back to rest
						const double r = std::sqrt(sg * sg - w * w), l1 = -sg + r, l2 = -sg - r;
						const double start_force = scale * a + kept.real();
						const size_t m = (size_t)over_++;
						oc_[m] = {start_force * l2 / (l2 - l1), -start_force * l1 / (l2 - l1)};
						od_[m] = {std::exp(l1 / sr_), std::exp(l2 / sr_)};
					}
				}
			}
			if (inv_mass_ <= 0.0)
				clearance_ = 0.0;
			// what the palm was already holding back keeps creeping to rest as it did
			if (carry && carry->active)
				for (int k = 0; k < carry->over_ && over_ < SLOTS; ++k)
				{
					const size_t m = (size_t)over_++;
					oc_[m] = {carry->oc_[(size_t)k][0] * carry->fade_, carry->oc_[(size_t)k][1] * carry->fade_};
					od_[m] = carry->od_[(size_t)k];
				}
			start_push(p, voicing, scale * first);
			// the onset: without a push the string leaves the pick in tau, the force rises from where it was; after a
			// push it is already there
			onset_ = (push_len_ > 0 ? 0.0 : -scale * first) + (carry && carry->active ? carry->onset_ * carry->fade_ : 0.0);
			onset_step_ = std::exp(-1.0 / std::max(p.tau * sr_, 1.0));
			regain_ = regain;
			regain_end_ = (long)(5 * PALM_RECOVER * sr_);
			hand_ = hand_to_ = p.palm ? 1.0 : 0.0;
			moving_ = false;
			start_tension(pl, voicing);
			active = count_ + over_ > 0;
		}

		// a hand lands on the string (a finger relaxing, a flat hand, the palm): skin on a string takes more from the
		// fast modes than from the fundamental, so each mode loses extra * (1 + sqrt(f / f1)) / 2 more per second (at
		// most 4 extra): the note darkens as it dies instead of only getting quieter. With `contact` (the palm network
		// of the hand there), the modes also move to those of the string under it: lower, some barely ringing (the
		// palm's thud). It lands in `land_s`, not at once: the modes glide there.
		void damp(double extra = 40.0, const models::Palm *contact = nullptr, double land_s = HAND_LAND_S)
		{
			const double f1 = std::max(what.place.f1, 1.0);
			begin_move(land_s);
			for (int k = 0; k < count_; ++k)
			{
				const double f = free_w_[(size_t)k] / (2 * PI);
				extra_to_[(size_t)k] += extra * std::min(4.0, 0.5 * (1.0 + std::sqrt(std::max(f, 0.0) / f1)));
				if (contact && hand_ < 0.5)
				{ // a hand where there was none: its modes
					hand_ratio_[(size_t)k] = contact->ratio_at((size_t)n_[(size_t)k] - 1);
					hand_sigma_[(size_t)k] = contact->sigma_at((size_t)n_[(size_t)k] - 1);
				}
			}
			if (contact)
				hand_to_ = 1.0;
			for (int k = 0; k < over_; ++k)
			{
				od_[(size_t)k][0] *= std::exp(-extra / sr_);
				od_[(size_t)k][1] *= std::exp(-extra / sr_);
			}
			note = -1;
		}

		// the palm leaves the string: its modes glide back to the free string's in `lift_s` (what the hand took stays)
		void lift(double lift_s = PALM_LIFT_S)
		{
			if (hand_ <= 0.0 && hand_to_ <= 0.0)
				return;
			begin_move(lift_s);
			hand_to_ = 0.0;
		}

		bool touched() const { return hand_to_ > 0.0; }

		// a new note on the same string: the old one stops in `seconds` (struck again, the string forgets it)
		void fade_out(double seconds = 0.015)
		{
			fade_step_ = 1.0 / std::max(seconds * sr_, 1.0);
			note = -1;
		}

		void render(double *out, int samples,
					const std::array<double *, 3> &heard = std::array<double *, 3>{nullptr, nullptr, nullptr},
					const double *feedback = nullptr)
		{
			if (!active)
				return;
			Block b{out, heard, feedback};
			for (int c = 0; c < coils_; ++c)
				if (heard[(size_t)coil_pickup_[(size_t)c]])
					b.live[(size_t)b.lives++] = c;
			if (feedback)
				for (int m = 0; m < count_; ++m)
					b.fb_drive[(size_t)m] = 0.003 / std::sqrt((double)n_[(size_t)m]);
			int t = 0;
			for (; t < samples && push_t_ < push_len_; ++t)
			{
				std::array<double, 4> o{};
				push_sample(b, o);
				if (!emit(b, t, o))
					return;
			}
			// the fret and the tension only matter while the string swings wide: bounded once for the block (the
			// modes only decay)
			double reach = 0.0, power = 0.0;
			for (int m = 0; m < count_; ++m)
			{
				const double z = std::abs(z_[(size_t)m]);
				reach += z * std::abs(at_fret_[(size_t)m]);
				power += z * z * inv_heard2_[(size_t)m];
			}
			const double added = tension_k_ * power; // the tension the vibration adds, at most
			const bool contact = clearance_ > 0 && reach > clearance_;
			const bool tension = tension_k_ > 0 && (glide_gain_ * added > GLIDE_FLOOR * what.place.T || pull_gain_ * added > PULL_FLOOR);
			while (t < samples)
			{
				// the palm coming back changes the loop: a block ends where it is back; a hand landing or leaving
				// moves the modes every MOVE_STEP samples
				const bool palm = regain_ && t_ < regain_end_;
				int end = palm ? (int)std::min<long>(samples, t + (regain_end_ - t_)) : samples;
				if (moving_)
					end = std::min(end, t + MOVE_STEP);
				const int from = t;
				t = (this->*ring_for(palm, contact, tension))(b, t, end);
				if (t < 0)
					return;
				if (moving_)
					advance_move(t - from);
			}
			if (b.peak < 1e-7 && t_ > (long)(0.05 * sr_))
				active = false;
		}

	private:
		static constexpr int PLANES = 2;			   // across the top, along it
		static constexpr int SLOTS = PLANES * MAX_MODES; // every mode of every plane
		static constexpr int MAX_COILS = 6;			   // three pickups, a humbucker's two coils each
		static constexpr double GLIDE_FLOOR = 1e-4; // a glide below this share of T (0.1 cent) is not worth following
		static constexpr double PULL_FLOOR = 1e-4;	// nor a pull on the bridge below this (newtons)

		double sr_ = 48000.0;
		int count_ = 0, over_ = 0;
		int across_ = 0; // the slots of the plane across the top come first: 0 .. across_ - 1
		long t_ = 0, regain_end_ = 0;
		bool regain_ = false;
		double onset_ = 0, onset_step_ = 0, fade_ = 1, fade_step_ = 0;
		// the mode number and the plane of each ringing slot (the slots skip silent modes)
		std::array<int, SLOTS> n_{}, plane_{};
		std::array<cplx, SLOTS> z_{}, step_{}, rate_{};
		// the coils under the string: their pickup, which coil of it, each slot's true displacement over it per unit of
		// the slot's force, and its velocity there
		std::array<MagneticPickup, 3> pickups_{};
		double magnetic_ = 1.0; // the share of the string a magnetic pickup senses (its iron)
		// each slot as the free string rings it (rad/s, 1/s), the hand on it (its modes' ratio and loss, how much of
		// it is down: hand_), the losses hands added (extra_); a hand landing or leaving moves them from _from to _to
		static constexpr int MOVE_STEP = 32;
		std::array<double, SLOTS> free_w_{}, free_sg_{}, hand_ratio_{}, hand_sigma_{}, extra_{}, extra_from_{}, extra_to_{};
		double hand_ = 0.0, hand_from_ = 0.0, hand_to_ = 0.0;
		long move_t_ = 0, move_len_ = 1;
		bool moving_ = false;

		// a move starts from where the modes are now, towards the targets of any move still under way
		void begin_move(double seconds)
		{
			if (!moving_)
			{
				extra_to_ = extra_;
				hand_to_ = hand_;
			}
			extra_from_ = extra_;
			hand_from_ = hand_;
			move_t_ = 0;
			move_len_ = std::max<long>(1, (long)(seconds * sr_));
			moving_ = true;
		}

		// the hand goes on landing (or leaving): smoothly, the modes follow
		void advance_move(int samples)
		{
			move_t_ += samples;
			const double u = std::min(1.0, (double)move_t_ / (double)move_len_), s = u * u * (3.0 - 2.0 * u);
			hand_ = hand_from_ + s * (hand_to_ - hand_from_);
			for (int k = 0; k < count_; ++k)
			{
				const size_t m = (size_t)k;
				extra_[m] = extra_from_[m] + s * (extra_to_[m] - extra_from_[m]);
				const double w = free_w_[m] * (1.0 + hand_ * (hand_ratio_[m] - 1.0));
				const double sg = free_sg_[m] + hand_ * hand_sigma_[m] + extra_[m];
				const double wd = sg < w ? std::sqrt(w * w - sg * sg) : 1e-3; // past critical: it only creeps to rest
				rate_[m] = cplx(-sg, wd);
				step_[m] = std::exp(cplx(-sg, wd * (1.0 + glide_)) / sr_);
				for (int c = 0; c < coils_; ++c)
					coil_vel_[(size_t)c][m] = rate_[m] * coil_disp_[(size_t)c][m];
				fret_rate_[m] = rate_[m] * at_fret_[m];
			}
			if (u >= 1.0)
				moving_ = false;
		}
		int coils_ = 0;
		std::array<int, MAX_COILS> coil_pickup_{}, coil_index_{};
		std::array<std::array<double, SLOTS>, MAX_COILS> coil_disp_{};
		std::array<std::array<cplx, SLOTS>, MAX_COILS> coil_vel_{};
		std::array<double, SLOTS> q_{}, g_{};
		std::array<std::array<double, 2>, SLOTS> oc_{}, od_{};
		// the fret: each slot's displacement over it per newton, its velocity there, the turn an impulse gives it
		std::array<double, SLOTS> at_fret_{}, inv_heard2_{};
		std::array<cplx, SLOTS> fret_rate_{}, kick_{};
		double clearance_ = 0.0, bounce_ = 0.0, inv_mass_ = 0.0;
		double click_ = 0.0, click_env_ = 0.0, click_decay_ = 0.0, click_last_ = 0.0;
		// the pick's push: its length and progress (samples), the force it ends on, the pickups' displacements then
		long push_len_ = 0, push_t_ = 0;
		double push_force_ = 0.0, push_last_ = 0.0;
		std::array<std::array<double, MAX_COILS>, PLANES> push_disp_{}; // over each coil, in each plane
		double slip_ = 0.0, slip_depth_ = 0.0, slip_chance_ = 0.0, slip_decay_ = 0.0;
		double scrape_ = 0.0, scrape_lp_ = 0.0, scrape_coef_ = 0.0;
		// the tension: T change per squared newton of the modes, its average, the retuning it gives, its pull
		double tension_k_ = 0.0, tension_avg_ = 0.0, tension_coef_ = 0.0, glide_gain_ = 0.0, glide_ = 0.0;
		double pull_gain_ = 0.0, pull_hp_ = 0.0, pull_last_ = 0.0, pull_coef_ = 0.0;
		int retune_count_ = 0;
		std::uint32_t rng_ = 0x9E3779B9u;

		// a noise in [-1, 1] (xorshift: the same notes give the same sound)
		double noise()
		{
			rng_ ^= rng_ << 13;
			rng_ ^= rng_ >> 17;
			rng_ ^= rng_ << 5;
			return (double)rng_ / 2147483647.5 - 1.0;
		}

		// what a render call writes to, and what it keeps for its samples
		struct Block
		{
			double *out;
			std::array<double *, 3> heard;
			const double *feedback;
			std::array<int, MAX_COILS> live{}; // the coils whose pickup is listened to
			int lives = 0;
			std::array<double, SLOTS> fb_drive{};
			double peak = 0.0;
		};

		// the string's displacement and velocity over each coil, in each plane, for one sample
		using CoilMotion = std::array<std::array<double, MAX_COILS>, PLANES>;

		// the coils of the pickups under the string, in order
		void set_coils(const std::array<MagneticPickup, 3> &pickups, double magnetic)
		{
			pickups_ = pickups;
			magnetic_ = magnetic;
			coils_ = 0;
			for (int q = 0; q < 3; ++q)
				for (int c = 0; c < pickups[(size_t)q].coils(); ++c)
				{
					coil_pickup_[(size_t)coils_] = q;
					coil_index_[(size_t)coils_] = c;
					++coils_;
				}
		}

		// the string's displacement and velocity over each live coil, plane by plane (the slots of a plane are
		// contiguous: one tight sum each), as the slots ring now (the palm coming back, g_, when there is one)
		template <bool Palm>
		void coil_motion(const Block &b, CoilMotion &y, CoilMotion &v) const
		{
			for (int k = 0; k < b.lives; ++k)
			{
				const size_t c = (size_t)b.live[(size_t)k];
				const double *disp = coil_disp_[c].data();
				const cplx *vel = coil_vel_[c].data();
				for (size_t plane = 0; plane < PLANES; ++plane)
				{
					const int from = plane == 0 ? 0 : across_, to = plane == 0 ? across_ : count_;
					double sy = 0.0, sv = 0.0;
					for (int m = from; m < to; ++m)
					{
						const double g = Palm ? g_[(size_t)m] : 1.0;
						sy += z_[(size_t)m].real() * g * disp[m];
						sv += cmul_re(z_[(size_t)m], vel[m]) * g;
					}
					y[plane][c] = sy;
					v[plane][c] = sv;
				}
			}
		}

		// each live coil's voltage, added to its pickup's output (o[1 + pickup])
		void hear(const Block &b, const CoilMotion &y, const CoilMotion &v, std::array<double, 4> &o) const
		{
			for (int k = 0; k < b.lives; ++k)
			{
				const size_t c = (size_t)b.live[(size_t)k];
				const int q = coil_pickup_[c];
				o[(size_t)q + 1] += magnetic_ * pickups_[(size_t)q].voltage(y[0][c], v[0][c], y[1][c], v[1][c]);
			}
		}

		// samples from..to of the modes ringing: the force on the bridge, the pickups, the fret and the tension (the
		// true force of a slot is z, times the palm coming back, over the share heard). -1 when the voice ended.
		template <bool Palm, bool Contact, bool Tension>
		int ring(Block &b, int from, int to)
		{
			for (int t = from; t < to; ++t)
			{
				if constexpr (Palm)
					if ((t_ & 15) == 0)
					{
						const double e = std::exp(-(double)t_ / (PALM_RECOVER * sr_));
						for (int k = 0; k < count_; ++k)
							g_[(size_t)k] = std::exp(-q_[(size_t)k] * e);
					}
				std::array<double, 4> o{}; // the bridge force, then the three pickups
				CoilMotion cy{}, cv{};	   // the string over each coil
				coil_motion<Palm>(b, cy, cv);
				double y = 0.0, squares = 0.0;
				const double fb = b.feedback ? b.feedback[t] : 0.0;
				for (int m = 0; m < count_; ++m)
				{
					const double g = Palm ? g_[(size_t)m] : 1.0;
					const cplx z = z_[(size_t)m];
					const double re = z.real() * g;
					o[0] += re;
					if constexpr (Contact)
						y += re * at_fret_[(size_t)m];
					if constexpr (Tension)
						squares += re * re * inv_heard2_[(size_t)m];
					z_[(size_t)m] = cmul(z, step_[(size_t)m]);
					if (b.feedback)
						z_[(size_t)m] += fb * b.fb_drive[(size_t)m];
				}
				if constexpr (Palm)
					if (t_ + 1 == regain_end_)
					{
						const double e5 = std::exp(-5.0);
						for (int k = 0; k < count_; ++k)
							z_[(size_t)k] *= std::exp(-q_[(size_t)k] * e5);
						regain_ = false;
					}
				hear(b, cy, cv, o);
				if constexpr (Contact)
					if (y > clearance_)
						collide<Palm>();
				if constexpr (Tension)
					o[0] += retune(squares);
				for (int m = 0; m < over_; ++m)
				{
					auto &c = oc_[(size_t)m];
					o[0] += c[0] + c[1];
					c[0] *= od_[(size_t)m][0];
					c[1] *= od_[(size_t)m][1];
				}
				o[0] += onset_;
				onset_ *= onset_step_;
				++t_;
				if (!emit(b, t, o))
					return -1;
			}
			return to;
		}

		using RingFn = int (StringVoice::*)(Block &, int, int);
		// ring for the palm coming back, the fret in reach, the tension followed
		static RingFn ring_for(bool palm, bool contact, bool tension)
		{
			static constexpr RingFn rings[2][2][2] = {
				{{&StringVoice::ring<false, false, false>, &StringVoice::ring<false, false, true>},
				 {&StringVoice::ring<false, true, false>, &StringVoice::ring<false, true, true>}},
				{{&StringVoice::ring<true, false, false>, &StringVoice::ring<true, false, true>},
				 {&StringVoice::ring<true, true, false>, &StringVoice::ring<true, true, true>}},
			};
			return rings[palm][contact][tension];
		}

		// sample t written out: the knock of a fret, the fade of a note struck again. False when the voice ended.
		bool emit(Block &b, int t, std::array<double, 4> &o)
		{
			if (click_env_ > 1e-9)
			{
				// metal on metal: broadband, the first difference of a noise
				const double w = noise();
				o[0] += (w - click_last_) * 0.5 * click_env_;
				click_last_ = w;
				click_env_ *= click_decay_;
			}
			if (fade_step_ > 0)
			{
				fade_ -= fade_step_;
				if (fade_ <= 0)
				{
					active = false;
					return false;
				}
				for (auto &x : o)
					x *= fade_;
			}
			b.out[t] += o[0];
			for (int q = 0; q < 3; ++q)
				if (b.heard[(size_t)q])
					b.heard[(size_t)q][t] += o[(size_t)q + 1];
			b.peak = std::max(b.peak, std::abs(o[0]));
			return true;
		}

		// the string over the last fret (positive towards it, the way the pick pushed it) is past the clearance: if
		// it is still coming, the fret stops it. The impulse that sends it back at `bounce` of its speed (or out of
		// the fret within FRET_RELAX_S, pressed onto it) goes into every mode at once, and knocks the fret.
		template <bool Palm>
		void collide()
		{
			double y = 0.0, v = 0.0;
			for (int m = 0; m < count_; ++m)
			{
				const double g = Palm ? g_[(size_t)m] : 1.0;
				y += z_[(size_t)m].real() * g * at_fret_[(size_t)m];
				v += cmul_re(z_[(size_t)m], fret_rate_[(size_t)m]) * g;
			}
			const double into = y - clearance_;
			if (into <= 0.0)
				return;
			const double back = -std::max(bounce_ * std::max(v, 0.0), into / FRET_RELAX_S);
			if (v <= back)
				return;
			const double impulse = (back - v) / inv_mass_;
			for (int m = 0; m < count_; ++m)
				z_[(size_t)m] += kick_[(size_t)m] * (Palm ? impulse / std::max(g_[(size_t)m], 1e-6) : impulse);
			click_env_ += std::abs(impulse) * sr_ * click_;
		}

		// the tension the vibration adds, T' = EA / (4 T^2) sum F_n^2: averaged it retunes every mode by
		// sqrt(1 + T' / T), as it is it pulls on the bridge (its slow part removed: the body does not hear it)
		double retune(double squares)
		{
			const double added = tension_k_ * squares;
			tension_avg_ += (added - tension_avg_) * tension_coef_;
			if (++retune_count_ >= TENSION_UPDATE)
			{
				retune_count_ = 0;
				// at most TENSION_GLIDE_MAX: a string driven without end (a larsen) cannot push its modes past the
				// Nyquist frequency. Each mode turns by th, by (1 + i th / 2) / (1 - i th / 2): of modulus exactly 1
				// (the turn 1 - th^2 / 2 + i th it was grew by a little each time: over a long larsen the modes ran
				// away to infinity), for a division instead of an exponential
				const double glide = std::min(glide_gain_ * tension_avg_ / (2.0 * what.place.T), TENSION_GLIDE_MAX);
				if (std::abs(glide - glide_) > 1e-9)
				{
					const double change = glide - glide_;
					for (int m = 0; m < count_; ++m)
					{
						const double th = rate_[(size_t)m].imag() * change / sr_, q = 0.25 * th * th, inv = 1.0 / (1.0 + q);
						step_[(size_t)m] = cmul(step_[(size_t)m], cplx((1.0 - q) * inv, th * inv));
					}
					glide_ = glide;
				}
			}
			pull_hp_ = pull_coef_ * (pull_hp_ + added - pull_last_);
			pull_last_ = added;
			return pull_gain_ * pull_hp_;
		}

		void start_tension(const models::Place &pl, const models::Voicing &voicing)
		{
			// EA from the stiffness: B = pi^3 E d^4 / (64 T L^2) gives the core's d, so EA = 2 L sqrt(E T B / pi)
			const double ea = 2.0 * pl.Leff * std::sqrt(pl.E * pl.T * std::max(pl.B, 0.0) / PI);
			const bool on = voicing.glide > 0 || voicing.longitudinal > 0;
			tension_k_ = on ? ea / (4.0 * pl.T * pl.T) : 0.0;
			glide_gain_ = voicing.glide;
			pull_gain_ = voicing.longitudinal;
			tension_avg_ = 0.0;
			tension_coef_ = 1.0 - std::exp(-1.0 / (TENSION_SMOOTH_S * sr_));
			glide_ = 0.0;
			pull_hp_ = pull_last_ = 0.0;
			pull_coef_ = std::exp(-2 * PI * TENSION_PULL_HP_HZ / sr_);
			retune_count_ = 0;
		}

		// the pick's push (a strike only): it lasts push_ms at the usual force, shorter when the stroke is harder
		void start_push(const models::Pluck &p, const models::Voicing &voicing, double force)
		{
			push_t_ = 0;
			push_last_ = 0.0;
			slip_ = 0.0;
			scrape_lp_ = 0.0;
			const bool push = p.kind == models::Kind::strike && voicing.push_ms > 0;
			const double seconds = voicing.push_ms / 1000.0 * std::sqrt(PUSH_GAIN_REF / std::max(p.gain, 0.05));
			push_len_ = push ? (long)std::round(std::min(std::max(seconds, PUSH_MIN_S), PUSH_MAX_S) * sr_) : 0;
			push_force_ = force;
			const double depth = p.pick ? 1.0 : 0.0; // a finger neither slips nor scrapes the way a pick does
			slip_depth_ = depth * voicing.slip_stick * SLIP_MAX_DEPTH;
			slip_chance_ = SLIP_RATE / sr_;
			slip_decay_ = std::exp(-1.0 / (SLIP_RECOVER_S * sr_));
			scrape_ = depth * voicing.scrape * SCRAPE_DEPTH * (p.place.wound ? voicing.wound_scrape : 1.0);
			scrape_coef_ = 1.0 - std::exp(-2 * PI * SCRAPE_CUTOFF_HZ / sr_);
		}

		// one sample of the push: the string drawn aside along (t / T)^2 (Zollner: the force grows progressively), the
		// slips in its second half (the string jumps back a little, the pick takes it again), the scrape on the
		// pick's edge. The bridge feels the force, the pickups the string moving under them.
		void push_sample(const Block &b, std::array<double, 4> &o)
		{
			const double x = (double)(++push_t_) / (double)push_len_;
			if (x > 0.5 && slip_depth_ > 0 && 0.5 * (noise() + 1.0) < slip_chance_)
				slip_ += slip_depth_ * 0.5 * (noise() + 1.0) * x * x;
			slip_ *= slip_decay_;
			scrape_lp_ += (noise() - scrape_lp_) * scrape_coef_;
			const double u = x * x - slip_ + scrape_ * scrape_lp_ * x;
			const double speed = (u - push_last_) * sr_;
			push_last_ = u;
			o[0] += push_force_ * u;
			CoilMotion y{}, v{};
			for (int k = 0; k < b.lives; ++k)
			{
				const size_t c = (size_t)b.live[(size_t)k];
				for (size_t plane = 0; plane < PLANES; ++plane)
				{
					y[plane][c] = push_disp_[plane][c] * u;
					v[plane][c] = push_disp_[plane][c] * speed;
				}
			}
			hear(b, y, v, o);
		}

		// the ringing slot k as it sounds now: with the palm still coming back and the fade of a note struck again
		cplx sounding(int k) const
		{
			const double palm = (regain_ && t_ < regain_end_)
									? std::exp(-q_[(size_t)k] * std::exp(-(double)t_ / (PALM_RECOVER * sr_)))
									: 1.0;
			return z_[(size_t)k] * (palm * fade_);
		}
	};

	// A string that nobody plays answering the bridge: F_n = (2 T / L) Re z_n,
	// z_n' = (-sigma_n + i w_n) z_n - Y(f_n) F_others   (sigma_n with the coupling rate kappa of the mode)
	class SympatheticString
	{
	public:
		bool on = false; // free to answer (or fading in or out)
		int fret = 0;
		bool palm = false;

		void set(const models::Place &pl, const models::Palm *palm_net, const models::Body &body, const models::StringLosses &losses, double sr,
				 const std::array<MagneticPickup, 3> &pickups = no_pickups())
		{
			sr_ = sr;
			count_ = 0;
			gain_ = 2 * pl.T / pl.Leff;
			for (int n = 1; n <= SYMPATHY_MODES; ++n)
			{
				const double f = n * pl.f1 * std::sqrt(1.0 + pl.B * n * n);
				if (f >= 0.45 * sr)
					break;
				double sg = losses.sigma(f, n, pl.B) + body.loss(f, pl.c, pl.Leff, pl.Zs) + body.coupling(f, pl.c, pl.Leff, pl.Zs) +
							body.neck_loss(pl, f, 0);
				if (palm_net)
					sg += palm_net->sigma_at((size_t)n - 1);
				const size_t k = (size_t)count_++;
				step_[k] = std::exp(cplx(-sg, 2 * PI * f) / sr);
				drive_[k] = -body.admittance(f) / sr;
				// its force gain_ Re z on the bridge, as the velocity the pickups hear: a string nobody plays moves little,
				// each pickup hears it through its coils at the slope at rest (MagneticPickup::small_signal_shape)
				const cplx base = cplx(-sg, 2 * PI * f) * gain_ * (pl.Leff / (pl.T * n * PI)) * pl.magnetic;
				for (int p = 0; p < 3; ++p)
					pick3_[p][k] = base * pickups[(size_t)p].small_signal_shape(n, pl.Leff);
				z_[k] = 0.0;
			}
			for (int k = count_; k < SYMPATHY_MODES; ++k)
				z_[(size_t)k] = 0.0;
		}
		void free_from_now(double fade_s = 0.005)
		{
			if (!on)
				level_ = 0.0;
			on = true;
			target_ = 1.0;
			fade_step_ = 1.0 / std::max(fade_s * sr_, 1.0);
		}
		void hold(double fade_s = 0.005)
		{ // the string is played: it is left to its own sound
			target_ = 0.0;
			fade_step_ = 1.0 / std::max(fade_s * sr_, 1.0);
		}

		// in: the force of the other strings on the bridge; adds its own force to out
		void render(const double *in, double *out, int samples,
					const std::array<double *, 3> &heard = std::array<double *, 3>{nullptr, nullptr, nullptr})
		{
			if (!on)
				return;
			const bool h0 = heard[0] != nullptr, h1 = heard[1] != nullptr, h2 = heard[2] != nullptr;
			for (int t = 0; t < samples; ++t)
			{
				double sum = 0.0, e0 = 0.0, e1 = 0.0, e2 = 0.0;
				const double x = in[t];
				for (int k = 0; k < count_; ++k)
				{
					z_[k] = cmul(z_[k], step_[k]) + drive_[k] * x;
					sum += z_[k].real();
					if (h0)
						e0 += cmul_re(z_[k], pick3_[0][k]);
					if (h1)
						e1 += cmul_re(z_[k], pick3_[1][k]);
					if (h2)
						e2 += cmul_re(z_[k], pick3_[2][k]);
				}
				if (level_ < target_)
					level_ = std::min(target_, level_ + fade_step_);
				else if (level_ > target_)
					level_ = std::max(target_, level_ - fade_step_);
				out[t] += gain_ * sum * level_;
				if (h0)
					heard[0][t] += e0 * level_;
				if (h1)
					heard[1][t] += e1 * level_;
				if (h2)
					heard[2][t] += e2 * level_;
			}
			if (target_ <= 0.0 && level_ <= 0.0)
				on = false;
		}

	private:
		double sr_ = 48000.0, gain_ = 0.0, level_ = 0.0, target_ = 0.0, fade_step_ = 0.0;
		int count_ = 0;
		std::array<cplx, SYMPATHY_MODES> z_{}, step_{}, drive_{};
		std::array<std::array<cplx, SYMPATHY_MODES>, 3> pick3_{};
	};

} // namespace grmg
