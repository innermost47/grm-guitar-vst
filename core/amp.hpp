// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// amp.hpp
//
// The amplifier and distortion GRM network: tube preamp operating points, dynamic bias
// shift (grid conduction/sag), interactive tone stack, and real-time oversampled non-linear waveshaping.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <vector>

#include "grm.hpp"

namespace grmg
{

	constexpr double MU_12AX7 = 100.0;     // amplification factor
	constexpr double G_12AX7 = 2.24e-6;    // perveance constant (A / V^1.5)
	constexpr double V_SAT_TRIODE = 45.0;  // minimum plate voltage at saturation (V)
	constexpr double R_GRID_COND = 1.2e3;  // grid-cathode resistance when forward biased (ohms)
	constexpr double R_GRID_LEAK = 1.0e6;  // grid leak resistance (ohms)

	struct AmpSpec
	{
		const char* name = "5150 High Gain";
		double Vdd = 320.0;      // B+ rail plate supply (V)
		double Rp = 100.0e3;     // plate resistor (ohms)
		double Rk = 1.8e3;       // cathode resistor (ohms)
		double gain = 6.5;       // drive/gain knob (0.0 to 10.0)
		double bass = 0.55;      // 0.0 to 1.0
		double middle = 0.45;    // 0.0 to 1.0 (scoop metal)
		double treble = 0.65;    // 0.0 to 1.0
		double presence = 0.60;  // 0.0 to 1.0
		double master = 0.75;    // master volume (0.0 to 1.0)
		double sag_amount = 0.35;// dynamic bias shift
		bool tight = true;       // pre-distortion low-cut (essential for metal chugs)
		int stages = 4;          // 4 cascaded high-gain stages
	};

	struct AmpModel
	{
		double Ip0 = 0.0;
		double Vp0 = 0.0;
		double Vk0 = 0.0;
		double Vgk0 = 0.0;
		double A0 = 0.0;
		double knee_pos = 1.0;
		double knee_neg = 1.0;
		double drive_scale = 1.0;
		double sag_tau_charge = 0.001;
		double sag_tau_leak = 0.070;
		AmpSpec spec;
	};

	inline AmpModel amp_network(const AmpSpec& spec)
	{
		grm::Network net;

		net.add_node("Vdd", spec.Vdd, true, true);
		net.add_node("Rp", spec.Rp, true, true);
		net.add_node("Rk", spec.Rk, true, true);
		net.add_node("Ip", 0.0012, true, false);
		net.add_node("Vp", 180.0, true, false);
		net.add_node("Vk", 1.8, true, false);
		net.add_node("Vgk", -1.8, true, false);
		net.add_node("Vpk", 178.0, true, false);
		net.add_node("A0", 55.0, true, false);

		const int iVdd = net.index("Vdd"), iRp = net.index("Rp"), iRk = net.index("Rk");
		const int iIp = net.index("Ip"), iVp = net.index("Vp"), iVk = net.index("Vk");
		const int iVgk = net.index("Vgk"), iVpk = net.index("Vpk"), iA0 = net.index("A0");

		net.add_fn("plate_kcl", { "Vp", "Ip", "Rp", "Vdd" }, [=](const grm::Values& v) {
			return (v[iVp] + v[iIp] * v[iRp] - v[iVdd]) / 10.0;
			}, 1e-5);

		net.add_fn("cathode_kcl", { "Vk", "Ip", "Rk" }, [=](const grm::Values& v) {
			return (v[iVk] - v[iIp] * v[iRk]) * 10.0;
			}, 1e-5);

		net.add_sum("grid_bias", { {"Vgk", 1.0}, {"Vk", 1.0} }, 0.0, 1e-6);
		net.add_sum("plate_cathode", { {"Vpk", 1.0}, {"Vp", -1.0}, {"Vk", 1.0} }, 0.0, 1e-6);

		net.add_fn("triode_characteristic", { "Ip", "Vgk", "Vpk" }, [=](const grm::Values& v) {
			const double Veff = std::max(v[iVgk] + v[iVpk] / MU_12AX7, 0.0);
			const double Ip_phys = G_12AX7 * std::pow(Veff, 1.5);
			return (v[iIp] - Ip_phys) * 1e4;
			}, 1e-5);

		net.add_fn("small_signal_gain", { "A0", "Ip", "Vgk", "Vpk", "Rp" }, [=](const grm::Values& v) {
			const double Veff = std::max(v[iVgk] + v[iVpk] / MU_12AX7, 1e-4);
			const double gm = 1.5 * G_12AX7 * std::sqrt(Veff);
			const double rp = std::max(MU_12AX7 / std::max(gm, 1e-9), 10e3);
			const double expected_gain = (MU_12AX7 * v[iRp]) / (v[iRp] + rp);
			return (v[iA0] - expected_gain) / 10.0;
			}, 1e-4);

		const auto r = net.solve({}, 40, 1e-3, 1e-12, false);

		AmpModel m;
		m.spec = spec;
		m.Ip0 = r["Ip"];
		m.Vp0 = r["Vp"];
		m.Vk0 = r["Vk"];
		m.Vgk0 = r["Vgk"];
		m.A0 = r["A0"];

		const double headroom_cutoff = std::max(spec.Vdd - m.Vp0, 10.0);
		const double headroom_sat = std::max(m.Vp0 - V_SAT_TRIODE, 10.0);
		m.knee_neg = headroom_cutoff / std::max(m.A0, 1.0);
		m.knee_pos = headroom_sat / std::max(m.A0, 1.0);

		// Course de gain équilibrée et progressive
		const double drive_factor = std::pow(10.0, (spec.gain - 3.5) * 0.35);
		m.drive_scale = drive_factor * (m.A0 / 22.0);

		m.sag_tau_charge = 0.001;
		m.sag_tau_leak = 0.060 + (1.0 - spec.gain / 10.0) * 0.040;
		return m;
	}

	inline AmpSpec amp_preset_5150_lead()
	{
		AmpSpec s;
		s.name = "5150 High Gain";
		s.Vdd = 330.0;
		s.Rp = 100e3;
		s.Rk = 1.8e3;
		s.gain = 6.5;
		s.bass = 0.55;
		s.middle = 0.45;
		s.treble = 0.65;
		s.presence = 0.60;
		s.master = 0.75;
		s.tight = true;
		s.stages = 4;
		return s;
	}

	inline AmpSpec amp_preset_jcm800_crunch()
	{
		AmpSpec s;
		s.name = "JCM800 Crunch";
		s.Vdd = 300.0;
		s.Rp = 100e3;
		s.Rk = 2.7e3;
		s.gain = 4.5;
		s.bass = 0.60;
		s.middle = 0.65;
		s.treble = 0.55;
		s.presence = 0.50;
		s.master = 0.80;
		s.tight = true;
		s.stages = 2;
		return s;
	}

	inline AmpSpec amp_preset_clean()
	{
		AmpSpec s;
		s.name = "American Clean";
		s.Vdd = 280.0;
		s.Rp = 100e3;
		s.Rk = 1.5e3;
		s.gain = 2.0;
		s.bass = 0.50;
		s.middle = 0.50;
		s.treble = 0.50;
		s.presence = 0.40;
		s.master = 0.90;
		s.tight = false;
		s.stages = 2;
		return s;
	}

	class AmpProcessor
	{
	public:
		void prepare(double sample_rate)
		{
			sr_ = sample_rate;
			os_sr_ = sample_rate * 2.0;
			reset();
			update_coefficients();
		}

		void reset()
		{
			tight_hp_ = 0.0;
			dc_blocker_ = 0.0;
			tone_b0_ = tone_b1_ = tone_b2_ = 0.0;
			tone_t0_ = tone_t1_ = 0.0;
			cab_lp1_ = cab_lp2_ = cab_hp_ = 0.0;
			bias_shift_.fill(0.0);
			os_z_.fill(0.0);
		}

		void set_model(const AmpModel& m)
		{
			model_ = m;
			update_coefficients();
		}

		void process(float* buffer, int num_samples)
		{
			if (!buffer || num_samples <= 0)
				return;

			const double drive = model_.drive_scale;
			const double master = model_.spec.master * 5.2;
			const int stages = std::clamp(model_.spec.stages, 1, 4);

			const double input_boost = 8.5;
			const double inter_stage_drive = 1.3 + (model_.spec.gain / 10.0) * 1.5;

			for (int i = 0; i < num_samples; ++i)
			{
				double x = (double)buffer[i];

				if (model_.spec.tight)
				{
					tight_hp_ += tight_alpha_ * (x - tight_hp_);
					x = x - tight_hp_;
				}

				double os_samples[2];
				upsample2x(x, os_samples[0], os_samples[1]);

				for (int os = 0; os < 2; ++os)
				{
					double s = os_samples[os] * drive * input_boost;

					for (int st = 0; st < stages; ++st)
					{
						if (st > 0)
							s *= inter_stage_drive;

						s = process_triode_stage(s, st);
						s = -s;
					}

					s = std::tanh(s * 0.5) * 1.4;
					os_samples[os] = s;
				}

				double y = downsample2x(os_samples[0], os_samples[1]);

				y = process_tone_stack(y);

				y = process_cab_sim(y);

				dc_blocker_ += 0.005 * (y - dc_blocker_);
				y = (y - dc_blocker_) * master;

				buffer[i] = (float)y;
			}
		}

	private:
		double sr_ = 48000.0, os_sr_ = 96000.0;
		AmpModel model_{};

		double tight_alpha_ = 0.0;
		double sag_charge_coef_ = 0.0;
		double sag_leak_coef_ = 0.0;

		double tight_hp_ = 0.0;
		double dc_blocker_ = 0.0;
		std::array<double, 4> bias_shift_{};

		double tone_b0_ = 0, tone_b1_ = 0, tone_b2_ = 0;
		double tone_t0_ = 0, tone_t1_ = 0;

		// Baffle 4x12 filter states
		double cab_lp1_ = 0.0, cab_lp2_ = 0.0, cab_hp_ = 0.0;

		std::array<double, 6> os_z_{};

		void update_coefficients()
		{
			// Tight HP à 200 Hz
			const double w_tight = 2.0 * PI * 200.0 / sr_;
			tight_alpha_ = w_tight / (1.0 + w_tight);

			sag_charge_coef_ = 1.0 - std::exp(-1.0 / (model_.sag_tau_charge * os_sr_));
			sag_leak_coef_ = 1.0 - std::exp(-1.0 / (model_.sag_tau_leak * os_sr_));
		}

		inline double process_triode_stage(double in, int stage_idx)
		{
			double& bias = bias_shift_[(size_t)stage_idx];

			const double v_grid = in - bias;
			if (v_grid > 0.0)
				bias += v_grid * sag_charge_coef_ * model_.spec.sag_amount;
			else
				bias -= bias * sag_leak_coef_;

			const double x = in - bias;
			const double kp = model_.knee_pos;
			const double kn = model_.knee_neg;

			double y = 0.0;
			if (x >= 0.0)
			{
				y = kp * (x / (kp + x));
			}
			else
			{
				const double x_abs = -x;
				y = -kn * std::tanh(x_abs / kn);
			}
			return y;
		}

		inline void upsample2x(double in, double& out0, double& out1)
		{
			out0 = in;
			const double a0 = 0.14134152, a1 = 0.58999482;
			double y1 = a0 * (in - os_z_[1]) + os_z_[0];
			os_z_[0] = in;
			os_z_[1] = y1;
			double y2 = a1 * (y1 - os_z_[3]) + os_z_[2];
			os_z_[2] = y1;
			os_z_[3] = y2;
			out1 = y2;
		}

		inline double downsample2x(double in0, double in1)
		{
			const double a0 = 0.14134152, a1 = 0.58999482;
			double y1 = a0 * (in1 - os_z_[5]) + os_z_[4];
			os_z_[4] = in1;
			os_z_[5] = y1;
			return 0.5 * (in0 + y1);
		}

		inline double process_tone_stack(double x)
		{
			const double b = model_.spec.bass;
			const double m = model_.spec.middle;
			const double t = model_.spec.treble;
			const double p = model_.spec.presence;

			// Low-shelf (~110 Hz)
			const double low_gain = (b - 0.5) * 1.6;
			tone_b0_ += 0.02 * (x - tone_b0_);
			double y = x + low_gain * tone_b0_;

			// Mid-scoop metal (~650 Hz)
			const double mid_gain = (m - 0.5) * 1.5;
			tone_b1_ += 0.07 * (y - tone_b1_);
			tone_b2_ += 0.07 * (tone_b1_ - tone_b2_);
			y = y + mid_gain * (tone_b1_ - tone_b2_);

			// High-shelf & Presence
			const double high_gain = (t - 0.5) * 1.4 + (p - 0.5) * 0.8;
			tone_t0_ += 0.32 * (y - tone_t0_);
			y = y + high_gain * (y - tone_t0_);

			return y;
		}

		inline double process_cab_sim(double in)
		{
			cab_lp1_ += 0.48 * (in - cab_lp1_);
			cab_lp2_ += 0.48 * (cab_lp1_ - cab_lp2_);

			cab_hp_ += 0.015 * (cab_lp2_ - cab_hp_);
			return cab_lp2_ + 0.35 * (cab_lp2_ - cab_hp_);
		}
	};

} // namespace grmg