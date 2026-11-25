// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// rigs.hpp
//
// The settings of a rig to listen to, by name, shared by the offline tools: an acoustic guitar, a magnetic pickup
// alone (no amplifier: what the jack carries), or a magnetic pickup into the amplifier.
#pragma once

#include <string>

#include "../../models/core.hpp"

namespace grmg::rigs
{
    // acoustic | pickup | clean | crunch | lead | metal (anything else: acoustic)
    inline models::Settings named(const std::string &name)
    {
        models::Settings s;
        if (name.empty() || name == "acoustic")
            return s;
        s.sensor = 1;
        s.hollow = 0.0; // a solid body under a magnetic pickup
        if (name == "pickup")
            return s;
        s.amp.on = true;
        s.amp.gate_db = -60.0;
        if (name == "clean")
        {
            s.amp.mix = 0.0;
            s.amp.cabinet.kind = 0;
        }
        else if (name == "crunch")
        {
            s.amp.lead.voicing = 1;
            s.amp.lead.gain = 6.0;
        }
        else if (name == "lead")
        {
            s.amp.lead.voicing = 2;
            s.amp.lead.gain = 7.0;
        }
        else if (name == "metal") // high gain, the pedal as a tight boost in front, the gate
        {
            s.amp.lead.gain = 6.5;
            s.amp.drive.on = true;
            s.amp.drive.drive = 0.1;
            s.amp.drive.level = 0.9;
            s.amp.gate_db = -50.0;
        }
        return s;
    }
}
