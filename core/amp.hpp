// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <vector>
#include <cstdint>

#include "../constants/core.hpp"
#include "physics.hpp"
#include "grm.hpp"

namespace grmg::models
{
	constexpr double PI = 3.14159265358979323846;

	inline AmpSpec amp_preset_5150_lead()
	{
		AmpSpec spec;
		spec.name = "5150 High Gain";
		spec.Vdd = 330.0;
		spec.Rp = 100e3;
		spec.Rk = 1.8e3;
		spec.gain = 6.5;
		spec.bass = 0.65;
		spec.middle = 0.35;
		spec.treble = 0.70;
		spec.presence = 0.90;
		spec.master = 0.75;
		spec.tight = true;
		spec.ts_boost = true;
		spec.ensemble = true;
		spec.stages = 4;
		return spec;
	}

	inline AmpSpec amp_preset_jcm800_crunch()
	{
		AmpSpec spec;
		spec.name = "JCM800 Crunch";
		spec.Vdd = 300.0;
		spec.Rp = 100e3;
		spec.Rk = 2.7e3;
		spec.gain = 4.5;
		spec.bass = 0.60;
		spec.middle = 0.65;
		spec.treble = 0.55;
		spec.presence = 0.50;
		spec.master = 0.80;
		spec.tight = true;
		spec.ts_boost = true;
		spec.stages = 2;
		return spec;
	}

	inline AmpSpec amp_preset_clean()
	{
		AmpSpec spec;
		spec.name = "American Clean";
		spec.Vdd = 280.0;
		spec.Rp = 100e3;
		spec.Rk = 1.5e3;
		spec.gain = 2.0;
		spec.bass = 0.50;
		spec.middle = 0.50;
		spec.treble = 0.50;
		spec.presence = 0.40;
		spec.master = 0.90;
		spec.tight = false;
		spec.ts_boost = false;
		spec.stages = 2;
		return spec;
	}
}

namespace grmg
{

	inline models::AmpModel amp_network(const models::AmpSpec &spec)
	{
		grm::Network net;

		net.add_node("Vdd", spec.Vdd, true, true);
		net.add_node("Rp", spec.Rp, true, true);
		net.add_node("Rk", spec.Rk, true, true);
		net.add_node("R_supply", 2.5e3, true, true);
		net.add_node("Ip", 0.0012, true, false);
		net.add_node("Vp", 180.0, true, false);
		net.add_node("Vk", 1.8, true, false);
		net.add_node("Vgk", -1.8, true, false);
		net.add_node("Vpk", 178.0, true, false);
		net.add_node("A0", 55.0, true, false);
		net.add_node("Vdd_sag", spec.Vdd * 0.9, true, false);
		net.add_node("transformer_coupling", 0.85, true, false);

		const int iVdd = net.index("Vdd"), iRp = net.index("Rp"), iRk = net.index("Rk");
		const int iR_supply = net.index("R_supply"), iVdd_sag = net.index("Vdd_sag");
		const int iIp = net.index("Ip"), iVp = net.index("Vp"), iVk = net.index("Vk");
		const int iVgk = net.index("Vgk"), iVpk = net.index("Vpk"), iA0 = net.index("A0");
		const int iTr = net.index("transformer_coupling");

		net.add_fn("plate_kcl", {"Vp", "Ip", "Rp", "Vdd_sag"}, [=](const grm::Values &v)
				   { return (v[iVp] + v[iIp] * v[iRp] - v[iVdd_sag]) / 10.0; }, 1e-5);

		net.add_fn("cathode_kcl", {"Vk", "Ip", "Rk"}, [=](const grm::Values &v)
				   { return (v[iVk] - v[iIp] * v[iRk]) * 10.0; }, 1e-5);

		net.add_fn("power_supply_sag", {"Vdd_sag", "Vdd", "R_supply", "Ip"}, [=](const grm::Values &v)
				   {
				const double total_Ip = v[iIp] * 4.0;
				return (v[iVdd_sag] - (v[iVdd] - v[iR_supply] * total_Ip)) / 10.0; }, 1e-5);

		net.add_fn("transformer_warmth_rel", {"transformer_coupling", "A0"}, [=](const grm::Values &v)
				   {
				const double expected_tr = 0.75 + 0.25 * std::tanh(v[iA0] / 40.0);
				return (v[iTr] - expected_tr) / 10.0; }, 1e-5);

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

		models::AmpModel m;
		m.spec = spec;
		m.Ip0 = r["Ip"];
		m.Vp0 = r["Vp"];
		m.Vk0 = r["Vk"];
		m.Vgk0 = r["Vgk"];
		m.A0 = r["A0"];
		m.Vdd_sag0 = r["Vdd_sag"];
		m.transformer_coupling = r["transformer_coupling"];

		m.output_makeup = 1.6 + static_cast<double>(spec.stages) * 0.25 * std::log10(std::max(m.A0, 10.0));

		const double headroom_cutoff = std::max(m.Vdd_sag0 - m.Vp0, 10.0);
		const double headroom_sat = std::max(m.Vp0 - V_SAT_TRIODE, 10.0);
		m.knee_neg = headroom_cutoff / std::max(m.A0, 1.0);
		m.knee_pos = headroom_sat / std::max(m.A0, 1.0);

		const double drive_factor = std::pow(10.0, (spec.gain - 3.5) * 0.20);
		m.drive_scale = drive_factor * (m.A0 / 30.0);

		m.sag_tau_charge = 0.001;
		m.sag_tau_leak = 0.060 + (1.0 - spec.gain / 10.0) * 0.040;
		return m;
	}

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
			sag_envelope_ = 0.0;
			limiter_env_ = 0.0;

			noise_pink_state_ = 0.0;
			noise_hum_phase_ = 0.0;
			noise_seed_ = 12345;

			pre_lp_state_ = 0.0;
			pre_hp_state_ = 0.0;
			os_lp_state_ = 0.0;
			ts_lp_state_ = 0.0;

			delay_buffer_.fill(0.0);
			delay_write_ = 0;
			lfo_phase_ = 0.0;

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

		void set_model(const models::AmpModel &m)
		{
			model_ = m;
			update_coefficients();
		}

		void set_noise_amount(double amount)
		{
			noise_amount_ = std::clamp(amount, 0.0, 2.0);
		}

		void process(float *buffer, int num_samples)
		{
			if (!buffer || num_samples <= 0)
				return;

			const double drive = model_.drive_scale;
			const double master = model_.spec.master * 4.8;
			const int stages = std::clamp(model_.spec.stages, 1, 4);
			const double input_boost = 1.8;
			const double inter_stage_drive = 1.05 + (model_.spec.gain / 10.0) * 0.4;
			const bool use_tight = model_.spec.tight;
			const bool use_ts = model_.spec.ts_boost;
			const double warmth_factor = model_.transformer_coupling;

			for (int i = 0; i < num_samples; ++i)
			{
				double x = static_cast<double>(buffer[i]);

				x += generate_noise();

				if (model_.spec.ensemble)
				{
					x = apply_ensemble(x);
				}

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

					sag_envelope_ += sag_charge_coef_ * (std::abs(s) - sag_envelope_);
					const double sag_compression = 1.0 - std::min(0.15, sag_envelope_ * model_.spec.sag_amount * 0.05);

					for (int st = 0; st < stages; ++st)
					{
						if (st > 0)
							s *= inter_stage_drive * sag_compression;
						s = process_triode_stage(s, st, sag_compression, warmth_factor);
					}

					s = final_saturation(s, sag_compression, warmth_factor);

					os_lp_state_ += os_lp_coef_ * (s - os_lp_state_);
					os[k] = os_lp_state_;
				}

				double y = downsample4x(os);

				y = process_tone_stack(y);
				y = pres_.process(y);
				y = res_.process(y);
				y = process_cab_sim(y);

				dc_blocker_ += 0.005 * (y - dc_blocker_);
				y = y - dc_blocker_;

				y = y * master * model_.output_makeup;

				const double current_peak = std::abs(y);
				const double attack = 0.02;
				const double release = 0.005;

				if (current_peak > limiter_env_)
				{
					limiter_env_ += attack * (current_peak - limiter_env_);
				}
				else
				{
					limiter_env_ += release * (current_peak - limiter_env_);
				}

				const double threshold = 2.2;
				double gain_reduction = 1.0;

				if (limiter_env_ > threshold)
				{
					gain_reduction = threshold / limiter_env_;
				}

				y = y * gain_reduction;
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
		models::AmpModel model_{};

		double sag_charge_coef_ = 0.0;
		double sag_leak_coef_ = 0.0;
		double interstage_hp_coef_ = 0.0;
		double ts_hp_a_ = 0.0;
		double sag_envelope_ = 0.0;
		double limiter_env_ = 0.0;

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

		models::Biquad tight_bq_;
		models::Biquad tone_bass_, tone_mid_, tone_treble_;
		models::Biquad pres_, res_;
		models::Biquad cab_res_, cab_mid_, cab_cone_, cab_lp1_, cab_lp2_, cab_hp_;

		models::Halfband2x hb_a_;
		models::Halfband2x hb_b_;

		uint32_t noise_seed_ = 12345;
		double noise_pink_state_ = 0.0;
		double noise_hum_phase_ = 0.0;
		double noise_amount_ = 1.0;

		double thermal_noise_coef_ = 0.0;
		double shot_noise_coef_ = 0.0;
		double flicker_noise_coef_ = 0.0;
		double hum_noise_coef_ = 0.0;
		double ts_lp_state_ = 0.0;

		static constexpr int DELAY_BUFFER_SIZE = 512;
		std::array<double, DELAY_BUFFER_SIZE> delay_buffer_{};
		int delay_write_ = 0;
		double lfo_phase_ = 0.0;

		inline double generate_noise() noexcept
		{
			noise_seed_ = noise_seed_ * 1103515245 + 12345;
			const double white = ((noise_seed_ >> 16) & 0x7FFF) / 16383.5 - 1.0;

			noise_pink_state_ += 0.01 * (white - noise_pink_state_);
			const double pink = noise_pink_state_;

			constexpr double TWOPI = 2.0 * PI;
			const double hum_freq = 50.0;
			noise_hum_phase_ += TWOPI * hum_freq / sr_;
			if (noise_hum_phase_ > TWOPI)
				noise_hum_phase_ -= TWOPI;

			const double hum = 0.4 * std::sin(noise_hum_phase_) + 0.15 * std::sin(2.0 * noise_hum_phase_) + 0.05 * std::sin(3.0 * noise_hum_phase_);

			const double total_noise =
				thermal_noise_coef_ * white +
				shot_noise_coef_ * white +
				flicker_noise_coef_ * pink +
				hum_noise_coef_ * hum;

			return total_noise * noise_amount_;
		}

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

			pres_.set_peaking(sr_, 3860.0, 1.0, pres_db);
			res_.set_lowshelf(sr_, 100.0, res_db);

			cab_res_.set_peaking(sr_, 95.0, 2.2, 5.0);
			cab_mid_.set_peaking(sr_, 750.0, 1.1, -4.0);
			cab_cone_.set_peaking(sr_, 2400.0, 1.5, 1.5);

			cab_lp1_.set_lowpass(sr_, 4200.0, 0.80);
			cab_lp2_.set_lowpass(sr_, 5200.0, 0.90);

			cab_hp_.set_highpass(sr_, 75.0, 0.707);

			shot_noise_coef_ = 0.0008 * std::sqrt(std::max(model_.Ip0 * 1000.0, 0.1));

			thermal_noise_coef_ = 0.0003 * model_.spec.stages * (model_.A0 / 50.0);

			flicker_noise_coef_ = 0.0015 * (model_.A0 / 40.0);

			const double sag_factor = 1.0 - (model_.Vdd_sag0 / model_.spec.Vdd);
			hum_noise_coef_ = 0.002 * (1.0 + sag_factor * 3.0) * model_.spec.sag_amount;
		}

		inline double process_ts_boost(double x) noexcept
		{
			ts_hp_ += ts_hp_a_ * (x - ts_hp_);
			const double x_hp = x - ts_hp_;

			const double ts_gain = 8.5;
			const double x_boosted = x_hp * ts_gain;

			const double K = 1.2;
			const double B = 0.15;
			const double clipped = biased_tanh(x_boosted, K, B);

			constexpr double TWOPI = 2.0 * PI;
			const double lp_freq = 3500.0;
			const double lp_coef = 1.0 - std::exp(-TWOPI * lp_freq / sr_);
			ts_lp_state_ += lp_coef * (clipped - ts_lp_state_);

			return 0.85 * ts_lp_state_ + 0.15 * x_hp;
		}

		inline double process_triode_stage(double in, int st, double sag_comp, double warmth) noexcept
		{
			constexpr double TWOPI = 2.0 * PI;
			const double ASYM = 0.15 * warmth;

			const double miller_w = TWOPI * 6200.0 / os_sr_;
			const double miller_c = std::min(0.6, miller_w / (1.0 + std::abs(in) * 0.8));
			miller_state_[st] += miller_c * (in - miller_state_[st]);
			in = miller_state_[st];

			constexpr double GRID_KNEE = 1.2;
			if (in > GRID_KNEE)
			{
				const double e = in - GRID_KNEE;
				in = GRID_KNEE + e / (1.0 + e * 0.5);
			}

			double &bias = bias_shift_[st];
			const double v_grid = in - bias;

			if (v_grid > 0.0)
				bias += v_grid * sag_charge_coef_ * model_.spec.sag_amount * 0.5;
			else
				bias -= bias * sag_leak_coef_;

			bias = std::clamp(bias, 0.0, 1.5);

			const double x = in - bias;

			const double kp = model_.knee_pos * sag_comp * 1.5;
			const double kn = model_.knee_neg * sag_comp * 1.5;

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

		inline double final_saturation(double x, double sag_comp, double warmth) const noexcept
		{
			constexpr double pos = 1.2;
			constexpr double neg = 1.5;

			const double warmed = x + 0.03 * (x * x) * warmth;

			if (warmed >= 0.0)
				return (pos * sag_comp) * std::tanh(warmed / (pos * sag_comp));
			else
				return -(neg * sag_comp) * std::tanh(-warmed / (neg * sag_comp));
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

		inline double apply_ensemble(double in) noexcept
		{
			delay_buffer_[delay_write_] = in;
			delay_write_ = (delay_write_ + 1) & (DELAY_BUFFER_SIZE - 1);

			lfo_phase_ += 2.0 * PI * 0.3 / sr_;
			if (lfo_phase_ > 2.0 * PI)
				lfo_phase_ -= 2.0 * PI;
			const double lfo = 0.5 + 0.5 * std::sin(lfo_phase_);

			const double delay_ms = 1.5 + lfo * 1.0;
			const double delay_samples = delay_ms * sr_ / 1000.0;

			const int read_pos = (delay_write_ - 1 - (int)delay_samples + DELAY_BUFFER_SIZE) & (DELAY_BUFFER_SIZE - 1);
			const int read_pos_next = (read_pos - 1 + DELAY_BUFFER_SIZE) & (DELAY_BUFFER_SIZE - 1);
			const double frac = delay_samples - std::floor(delay_samples);
			const double delayed = delay_buffer_[read_pos] * (1.0 - frac) + delay_buffer_[read_pos_next] * frac;

			return 0.75 * in + 0.25 * delayed;
		}
	};

	class DualChannelAmp
	{
	public:
		void prepare(double sample_rate, int max_block_size = 4096)
		{
			clean_channel.prepare(sample_rate);
			lead_channel.prepare(sample_rate);
			temp_buffer_.resize(max_block_size, 0.0f);
			current_mix_ = target_mix_;
		}

		void reset()
		{
			clean_channel.reset();
			lead_channel.reset();
			current_mix_ = target_mix_;
		}

		void set_clean_model(const models::AmpModel &m)
		{
			clean_channel.set_model(m);
		}

		void set_lead_model(const models::AmpModel &m)
		{
			lead_channel.set_model(m);
		}

		void set_noise_amount(double clean_amt, double lead_amt)
		{
			clean_channel.set_noise_amount(clean_amt);
			lead_channel.set_noise_amount(lead_amt);
		}

		void set_channel_mix(double mix)
		{
			target_mix_ = std::clamp(mix, 0.0, 1.0);
		}

		void process(float *buffer, int num_samples)
		{
			if (!buffer || num_samples <= 0 || num_samples > static_cast<int>(temp_buffer_.size()))
				return;

			std::copy(buffer, buffer + num_samples, temp_buffer_.begin());

			clean_channel.process(buffer, num_samples);

			lead_channel.process(temp_buffer_.data(), num_samples);

			const double crossfade_speed = 0.002;
			for (int i = 0; i < num_samples; ++i)
			{
				current_mix_ += crossfade_speed * (target_mix_ - current_mix_);

				const float clean_gain = 1.0f - static_cast<float>(current_mix_);
				const float lead_gain = static_cast<float>(current_mix_);

				buffer[i] = (clean_gain * buffer[i]) + (lead_gain * temp_buffer_[i]);
			}
		}

	private:
		AmpProcessor clean_channel;
		AmpProcessor lead_channel;

		double target_mix_ = 0.0;
		double current_mix_ = 0.0;

		std::vector<float> temp_buffer_;
	};

} // namespace grmg