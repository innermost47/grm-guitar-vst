// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
#pragma once

#include <map>
#include <string>

namespace grmg::models
{
    // the sound of one guitar, kept for every project: its parameters (by the first guitar's ids, at their plain
    // values; one it does not name is at its default), not its patterns, not its mixer strip
    struct Preset
    {
        std::string name;
        std::string category; // "Acoustic", "Electric" (the factory's), "User"
        bool factory = false;
        std::map<std::string, float> values;
    };
}
