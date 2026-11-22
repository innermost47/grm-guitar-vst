// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <vector>

#include "physics.hpp"
#include "grm.hpp"

namespace grmg
{
	constexpr double MU_12AX7 = 100.0;
	constexpr double G_12AX7 = 2.24e-6;
	constexpr double V_SAT_TRIODE = 45.0;
	constexpr double R_GRID_COND = 1.2e3;
	constexpr double R_GRID_LEAK = 1.0e6;

	struct AmpSpec
	{
		const char *name = "5150 High Gain";
		double Vdd = 320.0;
		double Rp = 100.0e3;
		double Rk = 1.8e3;
		double gain = 6.5;
		double bass = 0.55;
		double middle = 0.45;
		double treble = 0.55;
		double presence = 0.45;
		double master = 0.75;
		double sag_amount = 0.35;
		bool tight = true;
		bool ts_boost = true;
		int stages = 4;
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

	inline AmpModel amp_network(const AmpSpec &spec)
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

		net.add_fn("plate_kcl", {"Vp", "Ip", "Rp", "Vdd"}, [=](const grm::Values &v)
				   { return (v[iVp] + v[iIp] * v[iRp] - v[iVdd]) / 10.0; }, 1e-5);

		net.add_fn("cathode_kcl", {"Vk", "Ip", "Rk"}, [=](const grm::Values &v)
				   { return (v[iVk] - v[iIp] * v[iRk]) * 10.0; }, 1e-5);

		net.add_sum("grid_bias", {{"Vgk", 1.0}, {"Vk", 1.0}}, 0.0, 1e-6);
		net.add_sum("plate_cathode", {{"Vpk", 1.0}, {"Vp", -1.0}, {"Vk", 1.0}}, 0.0, 1e-6);

		net.add_fn("triode_characteristic", {"Ip", "Vgk", "Vpk"}, [=](const grm::Values &v)
				   {
				const double Veff = std::max(v[iVgk] + v[iVpk] / MU_12AX7, 0.0);
				const double Ip_phys = G_12AX7 * std::pow(Veff, 1.5);
				return (v[iIp] - Ip_phys) * 1e4; }, 1e-5);

		net.add_fn("small_signal_gain", {"A0", "Ip", "Vgk", "Vpk", "Rp"}, [=](const grm::Values &v)
				   {
				const double Veff = std::max(v[iVgk] + v[iVpk] / MU_12AX7, 1e-4);
				const double gm = 1.5 * G_12AX7 * std::sqrt(Veff);
				const double rp = std::max(MU_12AX7 / std::max(gm, 1e-9), 10e3);
				const double expected_gain = (MU_12AX7 * v[iRp]) / (v[iRp] + rp);
				return (v[iA0] - expected_gain) / 10.0; }, 1e-4);

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
		s.treble = 0.55;
		s.presence = 0.45;
		s.master = 0.75;
		s.tight = true;
		s.ts_boost = true;
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
		s.ts_boost = true;
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
		s.ts_boost = false;
		s.stages = 2;
		return s;
	}

	struct Halfband2x
	{
		double up_s0 = 0.0, up_s1 = 0.0, up_s2 = 0.0, up_s3 = 0.0;
		double dn_s0 = 0.0, dn_s1 = 0.0;

		inline void up(double in, double &even, double &odd) noexcept
		{
			constexpr double a0 = 0.14134152, a1 = 0.58999482;
			even = in;
			const double y1 = a0 * (in - up_s1) + up_s0;
			up_s0 = in;
			up_s1 = y1;
			const double y2 = a1 * (y1 - up_s3) + up_s2;
			up_s2 = y1;
			up_s3 = y2;
			odd = y2;
		}

		inline double down(double even, double odd) noexcept
		{
			constexpr double a0 = 0.14134152;
			const double y = a0 * (odd - dn_s1) + dn_s0;
			dn_s0 = odd;
			dn_s1 = y;
			return 0.5 * (even + y);
		}

		void reset() noexcept
		{
			up_s0 = up_s1 = up_s2 = up_s3 = 0.0;
			dn_s0 = dn_s1 = 0.0;
		}
	};

	class AmpProcessor
	{
	public:
		void prepare(double sample_rate)
		{
			sr_ = sample_rate;
			os_sr_ = sample_rate * 4.0;
			reset();
			update_coefficients();
		}

		void reset()
		{
			tight_bq_.reset();
			dc_blocker_ = 0.0;
			ts_hp_ = 0.0;
			bias_shift_.fill(0.0);
			miller_state_.fill(0.0);
			interstage_hp_.fill(0.0);

			pre_lp_state_ = 0.0;
			pre_hp_state_ = 0.0;
			os_lp_state_ = 0.0;

			tone_bass_.reset();
			tone_mid_.reset();
			tone_treble_.reset();
			pres_.reset();
			res_.reset();

			cab_res_.reset();
			cab_mid_.reset();
			cab_cone_.reset();
			cab_lp1_.reset();
			cab_lp2_.reset();
			cab_hp_.reset();

			hb_a_.reset();
			hb_b_.reset();
		}

		void set_model(const AmpModel &m)
		{
			model_ = m;
			update_coefficients();
		}

		void process(float *buffer, int num_samples)
		{
			if (!buffer || num_samples <= 0)
				return;

			const double drive = model_.drive_scale;
			const double master = model_.spec.master * 4.8;
			const int stages = std::clamp(model_.spec.stages, 1, 4);
			const double input_boost = 4.2;
			const double inter_stage_drive = 1.15 + (model_.spec.gain / 10.0) * 0.9;
			const bool use_tight = model_.spec.tight;
			const bool use_ts = model_.spec.ts_boost;

			for (int i = 0; i < num_samples; ++i)
			{
				double x = static_cast<double>(buffer[i]);

				if (use_tight)
					x = tight_bq_.process(x);

				if (use_ts)
					x = process_ts_boost(x);

				pre_hp_state_ += pre_hp_coef_ * (x - pre_hp_state_);
				x = x - pre_hp_state_;
				pre_lp_state_ += pre_lp_coef_ * (x - pre_lp_state_);
				x = pre_lp_state_;

				double os[4];
				upsample4x(x, os);

				for (int k = 0; k < 4; ++k)
				{
					double s = os[k] * drive * input_boost;

					for (int st = 0; st < stages; ++st)
					{
						if (st > 0)
							s *= inter_stage_drive;
						s = process_triode_stage(s, st);
					}

					s = final_saturation(s);

					os_lp_state_ += os_lp_coef_ * (s - os_lp_state_);
					os[k] = os_lp_state_;
				}

				double y = downsample4x(os);

				y = process_tone_stack(y);
				y = pres_.process(y);
				y = res_.process(y);
				y = process_cab_sim(y);

				dc_blocker_ += 0.005 * (y - dc_blocker_);
				y = (y - dc_blocker_) * master;

				if (std::abs(y) > 1.0)
					y = std::tanh(y);

				buffer[i] = static_cast<float>(y);
			}
		}

		inline double biased_tanh(double x, double K, double B) noexcept
		{
			const double tb = std::tanh(K * B);
			const double denom = K * (1.0 - tb * tb);
			return (std::tanh(K * (x + B)) - tb) / denom;
		}

	private:
		double sr_ = 48000.0, os_sr_ = 192000.0;
		AmpModel model_{};

		double sag_charge_coef_ = 0.0;
		double sag_leak_coef_ = 0.0;
		double interstage_hp_coef_ = 0.0;
		double ts_hp_a_ = 0.0;

		double pre_hp_coef_ = 0.0;
		double pre_lp_coef_ = 0.0;
		double os_lp_coef_ = 0.0;

		double dc_blocker_ = 0.0;
		double ts_hp_ = 0.0;
		double pre_lp_state_ = 0.0;
		double pre_hp_state_ = 0.0;
		double os_lp_state_ = 0.0;
		std::array<double, 4> bias_shift_{};
		std::array<double, 4> miller_state_{};
		std::array<double, 4> interstage_hp_{};

		Biquad tight_bq_;
		Biquad tone_bass_, tone_mid_, tone_treble_;
		Biquad pres_, res_;
		Biquad cab_res_, cab_mid_, cab_cone_, cab_lp1_, cab_lp2_, cab_hp_;

		Halfband2x hb_a_;
		Halfband2x hb_b_;

		void update_coefficients()
		{
			constexpr double TWOPI = 2.0 * PI;

			tight_bq_.set_highpass(sr_, 110.0, 0.9);

			sag_charge_coef_ = 1.0 - std::exp(-1.0 / (model_.sag_tau_charge * os_sr_));
			sag_leak_coef_ = 1.0 - std::exp(-1.0 / (model_.sag_tau_leak * os_sr_));

			interstage_hp_coef_ = 1.0 - std::exp(-TWOPI * 60.0 / os_sr_);

			const double w_ts = TWOPI * 720.0 / sr_;
			ts_hp_a_ = w_ts / (1.0 + w_ts);

			pre_hp_coef_ = 1.0 - std::exp(-TWOPI * 38.0 / sr_);
			pre_lp_coef_ = 1.0 - std::exp(-TWOPI * 6800.0 / sr_);

			os_lp_coef_ = 1.0 - std::exp(-TWOPI * 9500.0 / os_sr_);

			const double b = model_.spec.bass;
			const double m = model_.spec.middle;
			const double t = model_.spec.treble;
			const double p = model_.spec.presence;

			const double bass_db = (b - 0.5) * 12.0;
			const double mid_db = (m - 0.5) * 12.0 - bass_db * 0.15;

			const double treb_db = (t - 0.5) * 6.5;
			const double pres_db = (p - 0.5) * 4.0 + std::max(0.0, -mid_db) * 0.20;
			const double res_db = (b - 0.5) * 4.0;

			tone_bass_.set_lowshelf(sr_, 120.0, bass_db);
			tone_mid_.set_peaking(sr_, 650.0, 0.9, mid_db);
			tone_treble_.set_highshelf(sr_, 3200.0, treb_db);

			pres_.set_peaking(sr_, 2600.0, 1.0, pres_db);
			res_.set_lowshelf(sr_, 100.0, res_db);

			cab_res_.set_peaking(sr_, 95.0, 2.2, 5.0);
			cab_mid_.set_peaking(sr_, 750.0, 1.1, -4.0);
			cab_cone_.set_peaking(sr_, 2400.0, 1.5, 1.5);

			cab_lp1_.set_lowpass(sr_, 4200.0, 0.80);
			cab_lp2_.set_lowpass(sr_, 5200.0, 0.90);

			cab_hp_.set_highpass(sr_, 75.0, 0.707);
		}

		inline double process_ts_boost(double x) noexcept
		{
			ts_hp_ += ts_hp_a_ * (x - ts_hp_);
			const double x_hp = x - ts_hp_;

			const double K = 0.9;
			const double B = 0.05;

			return biased_tanh(x_hp, K, B) + 0.08 * x_hp;
		}

		inline double process_triode_stage(double in, int st) noexcept
		{
			constexpr double TWOPI = 2.0 * PI;
			constexpr double ASYM = 0.18;

			const double miller_w = TWOPI * 6200.0 / os_sr_;
			const double miller_c = std::min(0.6, miller_w / (1.0 + std::abs(in) * 0.8));
			miller_state_[st] += miller_c * (in - miller_state_[st]);
			in = miller_state_[st];

			constexpr double GRID_KNEE = 0.85;
			if (in > GRID_KNEE)
			{
				const double e = in - GRID_KNEE;
				in = GRID_KNEE + e / (1.0 + e);
			}

			double &bias = bias_shift_[st];
			const double v_grid = in - bias;

			if (v_grid > 0.0)
				bias += v_grid * sag_charge_coef_ * model_.spec.sag_amount;
			else
				bias -= bias * sag_leak_coef_;

			bias = std::clamp(bias, 0.0, 2.0);

			const double x = in - bias;
			const double kp = model_.knee_pos;
			const double kn = model_.knee_neg;

			const double xb = x + ASYM;

			double y;
			if (xb >= 0.0)
				y = kp * (xb / (kp + xb));
			else
				y = -kn * std::tanh(-xb / kn);

			const double y0 = kp * (ASYM / (kp + ASYM));
			y -= y0;

			interstage_hp_[st] += interstage_hp_coef_ * (y - interstage_hp_[st]);
			y -= interstage_hp_[st];

			return y;
		}

		inline double final_saturation(double x) const noexcept
		{
			constexpr double pos = 0.85;
			constexpr double neg = 1.30;
			if (x >= 0.0)
				return pos * std::tanh(x / pos);
			else
				return -neg * std::tanh(-x / neg);
		}

		inline void upsample4x(double in, double out[4]) noexcept
		{
			double a0, a1;
			hb_a_.up(in, a0, a1);
			hb_b_.up(a0, out[0], out[1]);
			hb_b_.up(a1, out[2], out[3]);
		}

		inline double downsample4x(const double in[4]) noexcept
		{
			const double b0 = hb_b_.down(in[0], in[1]);
			const double b1 = hb_b_.down(in[2], in[3]);
			return hb_a_.down(b0, b1);
		}

		inline double process_tone_stack(double x) noexcept
		{
			double y = tone_bass_.process(x);
			y = tone_mid_.process(y);
			y = tone_treble_.process(y);
			return y;
		}

		inline double process_cab_sim(double in) noexcept
		{
			double y = cab_res_.process(in);
			y = cab_mid_.process(y);
			y = cab_cone_.process(y);
			y = cab_lp1_.process(y);
			y = cab_lp2_.process(y);
			y = cab_hp_.process(y);
			return y;
		}
	};

} // namespace grmg