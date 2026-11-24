// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// feedback.hpp
//
// Acoustic feedback (Larsen) GRM network: speaker-to-string acoustic coupling, air propagation
// delay, phase-constructive harmonic resonance, and non-linear limit cycle stabilization.
#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

#include "../constants/core.hpp"
#include "grm.hpp"

namespace grmg
{
	// ------------------------------------------------------------ GRM network solver
	inline models::FeedbackModel feedback_network(const models::FeedbackSpec &spec, double string_f1 = 0.0)
	{
		grm::Network net;

		const double d = std::clamp(spec.distance, 0.15, 6.0);
		const double c = std::max(spec.c_air, 100.0);
		const double gain = std::clamp(spec.amount, 0.0, 1.0);

		net.add_node("d", d, true, true);
		net.add_node("cair", c, true, true);
		net.add_node("tau", 0.003, true, false);
		net.add_node("att", 1.0, true, false);
		net.add_node("gain", gain, true, true);
		net.add_node("kappa", 0.5, true, false);

		const int itau = net.index("tau"), icair = net.index("cair"), id = net.index("d");
		const int iatt = net.index("att"), igain = net.index("gain"), ikappa = net.index("kappa");

		// Acoustic propagation delay: tau * cair - d = 0
		net.add_fn("propagation_delay", {"tau", "cair", "d"}, [=](const grm::Values &v)
				   { return v[itau] * v[icair] - v[id]; }, 1e-6);

		// Spherical pressure attenuation: att * max(d, 0.3) - 1.0 = 0
		net.add_fn("geometric_attenuation", {"att", "d"}, [=](const grm::Values &v)
				   { return v[iatt] * std::max(v[id], 0.3) - 1.0; }, 1e-4);

		// Effective coupling rate: kappa = gain * att * scale
		net.add_fn("feedback_coupling", {"kappa", "gain", "att"}, [=](const grm::Values &v)
				   {
			const double expected = v[igain] * v[iatt] * 1.5;
			return v[ikappa] - expected; }, 1e-4);

		const auto r = net.solve({}, 25, 1e-3, 1e-12, false);

		models::FeedbackModel m;
		m.spec = spec;
		m.tau = r["tau"];
		m.attenuation = r["att"];
		m.coupling = r["kappa"];

		// Determine target harmonic frequency if requested
		if (spec.target > 0 && string_f1 > 10.0)
		{
			switch (spec.target)
			{
			case 1:
				m.target_freq = string_f1;
				break; // Fundamental
			case 2:
				m.target_freq = 2.0 * string_f1;
				break; // Octave
			case 3:
				m.target_freq = 3.0 * string_f1;
				break; // 12th / 5th
			case 4:
				m.target_freq = 4.0 * string_f1;
				break; // 2nd Octave
			default:
				m.target_freq = 0.0;
				break;
			}
			m.target_q = 3.5; // narrow resonance peak
		}
		else
		{
			m.target_freq = 0.0; // Natural auto-phase selection
		}

		return m;
	}

	// ------------------------------------------------------------ real-time audio DSP
	class FeedbackProcessor
	{
	public:
		void prepare(double sample_rate)
		{
			sr_ = sample_rate;
			// Delay buffer: 4096 samples (~85 ms at 48kHz, up to ~29 metres distance)
			buffer_.assign(4096, 0.0);
			write_pos_ = 0;
			bp_z1_ = bp_z2_ = 0.0;
			update_coefficients();
		}

		void set_model(const models::FeedbackModel &m)
		{
			model_ = m;
			update_coefficients();
		}

		// Writes the amplifier / cabinet output sample into the acoustic air delay line
		inline void push(double amp_output_sample)
		{
			buffer_[(size_t)write_pos_] = amp_output_sample;
			write_pos_ = (write_pos_ + 1) & 4095;
		}

		// Returns the acoustic excitation force striking the strings at the current sample
		inline double get_acoustic_force()
		{
			if (model_.coupling <= 1e-4)
				return 0.0;

			// 1. Fractional delay read (linear interpolation for glitch-free distance modulation)
			const double delay_samples = std::clamp(model_.tau * sr_, 2.0, 4090.0);
			const int idelay = (int)delay_samples;
			const double frac = delay_samples - (double)idelay;

			const int r0 = (write_pos_ - idelay - 1 + 4096) & 4095;
			const int r1 = (write_pos_ - idelay - 2 + 4096) & 4095;
			double sample = buffer_[(size_t)r0] + frac * (buffer_[(size_t)r1] - buffer_[(size_t)r0]);

			// 2. Harmonic targeting filter (if enabled)
			if (model_.target_freq > 20.0)
			{
				// Resonant bandpass filter
				const double y = bp_b0_ * (sample - bp_z2_) - bp_a1_ * bp_z1_ - bp_a2_ * bp_z2_;
				bp_z2_ = bp_z1_;
				bp_z1_ = y;
				sample = y;
			}

			// 3. Loop non-linear limiter (stabilizes the larsen into a steady musical singing tone)
			const double force = std::tanh(sample * model_.coupling * 0.8) * model_.coupling;
			return force;
		}

		// Spatial coupling projection factor for mode n (odd modes take more uniform pressure)
		static inline double mode_coupling(int n)
		{
			// Lower modes couple stronger; decays smoothly as 1 / sqrt(n)
			return 1.0 / std::sqrt((double)n);
		}

		void reset()
		{
			std::fill(buffer_.begin(), buffer_.end(), 0.0);
			write_pos_ = 0;
			bp_z1_ = bp_z2_ = 0.0;
		}

	private:
		double sr_ = 48000.0;
		models::FeedbackModel model_{};
		std::vector<double> buffer_;
		int write_pos_ = 0;

		// Biquad bandpass filter coefficients & states
		double bp_b0_ = 0.0, bp_a1_ = 0.0, bp_a2_ = 0.0;
		double bp_z1_ = 0.0, bp_z2_ = 0.0;

		void update_coefficients()
		{
			if (model_.target_freq <= 20.0 || model_.target_freq >= 0.45 * sr_)
			{
				bp_b0_ = bp_a1_ = bp_a2_ = 0.0;
				return;
			}

			// Standard constant-skirt-gain bandpass
			const double w0 = 2.0 * PI * model_.target_freq / sr_;
			const double alpha = std::sin(w0) / (2.0 * model_.target_q);
			const double a0 = 1.0 + alpha;

			bp_b0_ = alpha / a0;
			bp_a1_ = (-2.0 * std::cos(w0)) / a0;
			bp_a2_ = (1.0 - alpha) / a0;
		}
	};

} // namespace grmg