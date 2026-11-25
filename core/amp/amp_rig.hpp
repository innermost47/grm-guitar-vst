// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// amp_rig.hpp
//
// The whole rig behind a magnetic pickup, from the model its networks solved (models::AmpModel):
//   guitar -> overdrive pedal -> clean and lead channels (mixed) -> cabinet -> noise gate (keyed by the guitar)
// The pedal and the channels run four times oversampled; the cabinet and the gate at the host's rate.
// The guitar comes in the engine's pickup units (PICKUP_VOLTS a unit) and leaves in them again (AMP_OUTPUT_UNITS at
// full power), so that the mixer's levels do not move when the amplifier is switched on.
#pragma once

#include <array>
#include <cstdint>

#include "../../models/amp.hpp"
#include "amp_channel.hpp"
#include "cabinet.hpp"
#include "noise_gate.hpp"
#include "overdrive.hpp"
#include "oversampler.hpp"
#include "../smoothed.hpp"

namespace grmg
{
    class AmpRig
    {
    public:
        void prepare(double sr)
        {
            sr_ = sr;
            oversampler_.prepare();
            gate_.prepare(sr);
            mix_.prepare(sr * Oversampler::FACTOR);
            reset();
            if (model_)
                set_model(*model_);
        }

        void reset()
        {
            oversampler_.reset();
            drive_.reset();
            clean_.reset();
            lead_.reset();
            cabinet_.reset();
            gate_.reset();
        }

        // a new model (on the audio thread, between two blocks): pointers and coefficients only, nothing allocated
        void set_model(const models::AmpModel &m)
        {
            const bool first = model_ == nullptr;
            model_ = &m;
            const double os = sr_ * Oversampler::FACTOR;
            drive_.set_model(m.drive, os);
            clean_.set_model(m.clean, os);
            lead_.set_model(m.lead, os);
            cabinet_.set_model(m.cabinet, sr_);
            gate_.set_threshold(m.gate_db);
            if (first)
                mix_.jump(m.mix);
            else
                mix_.set(m.mix);
        }

        // buffer: the pickup (engine units), replaced by the rig's output (engine units). The speaker's drive
        // (1: full power) goes to `speaker` when given: what the larsen hears.
        void process(float *buffer, int samples, float *speaker = nullptr)
        {
            if (!model_)
                return;
            std::array<double, Oversampler::FACTOR> up{};
            for (int t = 0; t < samples; ++t)
            {
                const double guitar = (double)buffer[t] * PICKUP_VOLTS;
                oversampler_.up(guitar, up);
                for (auto &x : up)
                {
                    const double pedal = drive_.process(x);
                    const double noise = this->noise();
                    const double mix = mix_.next();
                    double out = 0.0;
                    if (mix < 1.0 - 1e-6)
                        out += (1.0 - mix) * clean_.process(pedal, noise);
                    if (mix > 1e-6)
                        out += mix * lead_.process(pedal, noise);
                    x = out;
                }
                const double y = gate_.process(guitar, cabinet_.process(oversampler_.down(up)));
                if (speaker)
                    speaker[t] = (float)y;
                buffer[t] = (float)(y * AMP_OUTPUT_UNITS);
            }
        }

    private:
        double sr_ = 48000.0;
        const models::AmpModel *model_ = nullptr;
        Oversampler oversampler_;
        Overdrive drive_;
        AmpChannel clean_, lead_;
        Cabinet cabinet_;
        NoiseGate gate_;
        Smoothed mix_;
        std::uint32_t seed_ = 0x2545F491u;

        // a uniform noise in [-1, 1] (xorshift)
        double noise()
        {
            seed_ ^= seed_ << 13;
            seed_ ^= seed_ >> 17;
            seed_ ^= seed_ << 5;
            return (double)seed_ / 2147483647.5 - 1.0;
        }
    };
}
