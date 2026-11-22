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
//   - the onset is the release itself, softened by the release time of the attack network.
// And one string answering the others through the bridge (sympathetic strings): each mode a resonator
// driven by the velocity of the bridge, V = Y(f_n) F_others (sympathetic_response in grm_guitar.py).
#pragma once

#include <array>
#include <cmath>
#include <complex>

#include "model.hpp"

namespace grmg
{

	// the products of the audio loops, written out: std::complex's operator* checks for infinities and NaN on every
	// call (C99 Annex G), several times slower in the loops that run for every mode at every sample
	inline cplx cmul(cplx a, cplx b)
	{
		return { a.real() * b.real() - a.imag() * b.imag(), a.real() * b.imag() + a.imag() * b.real() };
	}
	inline double cmul_re(cplx a, cplx b) { return a.real() * b.real() - a.imag() * b.imag(); }

	// Fourier amplitudes of a string released from a triangle with its apex at p (share of the length)
	inline double pluck_amplitude(int n, double p)
	{
		p = std::min(std::max(p, 0.02), 0.98);
		return 2.0 * std::sin(n * PI * p) / (n * n * PI * PI * p * (1.0 - p));
	}

	inline double sinc(double x) { return std::abs(x) < 1e-12 ? 1.0 : std::sin(PI * x) / (PI * x); }

	enum class Kind
	{
		strike,
		lift,
		land
	};

	// what starts a string: the networks' results for this note
	struct Pluck
	{
		int string = 0, fret = 0;
		Place place;
		double gain = 0;     // release height / Y_REF
		double tau = 0;      // s, release time
		double pluck = 0.22; // apex of the triangle, share of the vibrating length
		Kind kind = Kind::strike;
		const Palm* palm = nullptr;
	};

	class StringVoice
	{
	public:
		bool active = false;
		Pluck what;
		int note = -1; // MIDI note that holds it (live playing), -1 otherwise
		int note_channel = 0;

		bool holds_midi_note(int channel, int pitch) const
		{
			return active && note >= 0 && (channel == 0 || note_channel == channel) && (pitch < 0 || note == pitch);
		}

		// pickup_x > 0: a magnetic pickup listens at that distance from the saddle (render's `pickup` output)
		void start(const Pluck& p, const Body& body, const StringLosses& losses, double brightness, double sr,
			const std::array<double, 3>& pickup_xs = std::array<double, 3>{ 0.0, 0.0, 0.0 })
		{
			what = p;
			sr_ = sr;
			count_ = 0;
			over_ = 0;
			t_ = 0;
			fade_ = 1.0;
			fade_step_ = 0.0;
			const Place& pl = p.place;
			const bool regain = p.palm && p.kind == Kind::strike;
			std::array<double, MAX_MODES> amp{};
			double peak = 0.0;
			int count = 0;
			for (int n = 1; n <= MAX_MODES; ++n)
			{
				const double f = n * pl.f1 * std::sqrt(1.0 + pl.B * n * n);
				if (f >= 0.45 * sr_)
					break;
				double a = n * pluck_amplitude(n, p.pluck) * std::pow((double)n, -(1.0 - brightness) * 0.6);
				if (p.tau > 0)
					a *= sinc(f * p.tau);
				amp[(size_t)count++] = a;
				peak = std::max(peak, std::abs(a));
			}
			// newtons on the bridge: T pi / L * (release height) * (normalised modal amplitudes)
			const double scale = pl.T * PI / pl.Leff * Y_REF * p.gain;
			double first = 0.0;
			for (int k = 0; k < count; ++k)
			{
				const int n = k + 1;
				const double a = amp[(size_t)k];
				if (std::abs(a) < 1e-4 * peak)
					continue; // inaudible (a node at the pluck point)
				double f = n * pl.f1 * std::sqrt(1.0 + pl.B * n * n);
				double sg = losses.sigma(f, n, pl.B) + body.loss(f, pl.c, pl.Leff, pl.Zs);
				double sp = 0.0;
				if (p.palm)
				{
					sp = p.palm->sigma_at((size_t)k);
					sg += sp;
					f *= p.palm->ratio_at((size_t)k);
				}
				const double w = 2 * PI * f;
				first += a;
				if (sg < w)
				{
					// cos(wd t) + (sg / wd) sin(wd t) = Re[(1 - i sg / wd) e^(i wd t)]
					const double wd = std::sqrt(w * w - sg * sg);
					const size_t m = (size_t)count_++;
					z_[m] = scale * a * cplx(1.0, -sg / wd);
					step_[m] = std::exp(cplx(-sg, wd) / sr_);
					rate_[m] = cplx(-sg, wd);
					// the pickup hears the string's velocity where it sits: mode n's bridge force T (n pi / L) y_n
					// turned into its displacement there, y_n sin(n pi x / L), and derived in time
					const cplx base = rate_[m] * (pl.Leff / (pl.T * n * PI));
					for (int k = 0; k < 3; ++k)
						pick3_[k][m] = pickup_xs[k] > 0
						? base * std::sin(n * PI * pickup_xs[k] / pl.Leff)
						: cplx(0.0, 0.0);
					q_[m] = regain ? sp * PALM_DROP * PALM_RECOVER : 0.0;
					if (regain)
						z_[m] *= std::exp(q_[m]); // once the palm is back
				}
				else
				{
					// the palm damps it more than it rings: it only creeps back to rest
					const double r = std::sqrt(sg * sg - w * w), l1 = -sg + r, l2 = -sg - r;
					const size_t m = (size_t)over_++;
					oc_[m] = { scale * a * l2 / (l2 - l1), -scale * a * l1 / (l2 - l1) };
					od_[m] = { std::exp(l1 / sr_), std::exp(l2 / sr_) };
				}
			}
			// the onset: the string leaves the pick in tau, the force rises from where it was
			onset_ = -scale * first;
			onset_step_ = std::exp(-1.0 / std::max(p.tau * sr_, 1.0));
			regain_ = regain;
			regain_end_ = (long)(5 * PALM_RECOVER * sr_);
			active = count_ + over_ > 0;
		}

		// the string is damped (the finger relaxes, the hand touches it): skin on a string takes more from the fast
		// modes than from the fundamental, so each mode loses extra * (1 + sqrt(f / f1)) / 2 more per second (at
		// most 4 extra): the note darkens as it dies instead of only getting quieter
		void damp(double extra = 40.0)
		{
			const double f1 = std::max(what.place.f1, 1.0);
			for (int k = 0; k < count_; ++k)
			{
				const double f = rate_[(size_t)k].imag() / (2 * PI);
				const double e = extra * std::min(4.0, 0.5 * (1.0 + std::sqrt(std::max(f, 0.0) / f1)));
				rate_[(size_t)k] = cplx(rate_[(size_t)k].real() - e, rate_[(size_t)k].imag());
				step_[(size_t)k] = std::exp(rate_[(size_t)k] / sr_);
			}
			for (int k = 0; k < over_; ++k)
			{
				od_[(size_t)k][0] *= std::exp(-extra / sr_);
				od_[(size_t)k][1] *= std::exp(-extra / sr_);
			}
			note = -1;
		}

		// a new note on the same string: the old one stops in `seconds` (struck again, the string forgets it)
		void fade_out(double seconds = 0.015)
		{
			fade_step_ = 1.0 / std::max(seconds * sr_, 1.0);
			note = -1;
		}

		void render(double* out, int samples,
			const std::array<double*, 3>& heard = std::array<double*, 3>{ nullptr, nullptr, nullptr },
			const double* feedback = nullptr)
		{
			if (!active)
				return;

			std::array<double, MAX_MODES> fb_drive{};
			if (feedback)
			{
				for (int m = 0; m < count_; ++m)
					fb_drive[(size_t)m] = 0.003 / std::sqrt((double)(m + 1));
			}

			double peak = 0.0;
			const bool h0 = heard[0] != nullptr, h1 = heard[1] != nullptr, h2 = heard[2] != nullptr;
			for (int t = 0; t < samples; ++t)
			{
				double sum = 0.0, e0 = 0.0, e1 = 0.0, e2 = 0.0;
				if (regain_ && t_ < regain_end_)
				{
					if ((t_ & 15) == 0)
					{
						const double e = std::exp(-(double)t_ / (PALM_RECOVER * sr_));
						for (int k = 0; k < count_; ++k)
							g_[(size_t)k] = std::exp(-q_[(size_t)k] * e);
					}
					for (int m = 0; m < count_; ++m)
					{
						const double g = g_[(size_t)m];
						sum += z_[m].real() * g;
						if (h0)
							e0 += cmul_re(z_[m], pick3_[0][m]) * g;
						if (h1)
							e1 += cmul_re(z_[m], pick3_[1][m]) * g;
						if (h2)
							e2 += cmul_re(z_[m], pick3_[2][m]) * g;
						z_[m] = cmul(z_[m], step_[m]);

						if (feedback)
							z_[m] += feedback[t] * fb_drive[(size_t)m];
					}
					if (t_ + 1 == regain_end_)
					{
						const double e5 = std::exp(-5.0);
						for (int k = 0; k < count_; ++k)
							z_[(size_t)k] *= std::exp(-q_[(size_t)k] * e5);
						regain_ = false;
					}
				}
				else
				{
					for (int m = 0; m < count_; ++m)
					{
						sum += z_[m].real();
						if (h0)
							e0 += cmul_re(z_[m], pick3_[0][m]);
						if (h1)
							e1 += cmul_re(z_[m], pick3_[1][m]);
						if (h2)
							e2 += cmul_re(z_[m], pick3_[2][m]);
						z_[m] = cmul(z_[m], step_[m]);

						if (feedback)
							z_[m] += feedback[t] * fb_drive[(size_t)m];
					}
				}
				for (int m = 0; m < over_; ++m)
				{
					auto& c = oc_[(size_t)m];
					sum += c[0] + c[1];
					c[0] *= od_[(size_t)m][0];
					c[1] *= od_[(size_t)m][1];
				}
				sum += onset_;
				onset_ *= onset_step_;
				if (fade_step_ > 0)
				{
					fade_ -= fade_step_;
					if (fade_ <= 0)
					{
						active = false;
						return;
					}
					sum *= fade_;
					e0 *= fade_;
					e1 *= fade_;
					e2 *= fade_;
				}
				out[t] += sum;
				if (h0)
					heard[0][t] += e0;
				if (h1)
					heard[1][t] += e1;
				if (h2)
					heard[2][t] += e2;
				peak = std::max(peak, std::abs(sum));
				++t_;
			}
			if (peak < 1e-7 && t_ >(long)(0.05 * sr_))
				active = false;
		}

	private:
		double sr_ = 48000.0;
		int count_ = 0, over_ = 0;
		long t_ = 0, regain_end_ = 0;
		bool regain_ = false;
		double onset_ = 0, onset_step_ = 0, fade_ = 1, fade_step_ = 0;
		std::array<cplx, MAX_MODES> z_{}, step_{}, rate_{};
		std::array<std::array<cplx, MAX_MODES>, 3> pick3_{};
		std::array<double, MAX_MODES> q_{}, g_{};
		std::array<std::array<double, 2>, MAX_MODES> oc_{}, od_{};
	};

	// A string that nobody plays answering the bridge: F_n = (2 T / L) Re z_n,
	// z_n' = (-sigma_n + i w_n) z_n - Y(f_n) F_others   (sigma_n with the coupling rate kappa of the mode)
	class SympatheticString
	{
	public:
		bool on = false; // free to answer (or fading in or out)
		int fret = 0;
		bool palm = false;

		void set(const Place& pl, const Palm* palm_net, const Body& body, const StringLosses& losses, double sr,
			const std::array<double, 3>& pickup_xs = std::array<double, 3>{ 0.0, 0.0, 0.0 })
		{
			sr_ = sr;
			count_ = 0;
			gain_ = 2 * pl.T / pl.Leff;
			for (int n = 1; n <= SYMPATHY_MODES; ++n)
			{
				const double f = n * pl.f1 * std::sqrt(1.0 + pl.B * n * n);
				if (f >= 0.45 * sr)
					break;
				double sg = losses.sigma(f, n, pl.B) + body.loss(f, pl.c, pl.Leff, pl.Zs) + body.coupling(f, pl.c, pl.Leff, pl.Zs);
				if (palm_net)
					sg += palm_net->sigma_at((size_t)n - 1);
				const size_t k = (size_t)count_++;
				step_[k] = std::exp(cplx(-sg, 2 * PI * f) / sr);
				drive_[k] = -body.admittance(f) / sr;
				// its force gain_ Re z on the bridge, as a velocity where the pickup sits (see StringVoice)
				const cplx base = cplx(-sg, 2 * PI * f) * gain_ * (pl.Leff / (pl.T * n * PI));
				for (int p = 0; p < 3; ++p)
					pick3_[p][k] = pickup_xs[p] > 0
					? base * std::sin(n * PI * pickup_xs[p] / pl.Leff)
					: cplx(0.0, 0.0);
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
		void render(const double* in, double* out, int samples,
			const std::array<double*, 3>& heard = std::array<double*, 3>{ nullptr, nullptr, nullptr })
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
