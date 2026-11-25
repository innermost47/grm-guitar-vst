// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// guitar_strings.hpp
//
// The six strings a guitar holds: the set chosen (Automatic: nickel under a magnetic pickup, bronze otherwise) on the
// guitar's scale. Each string's mass from its maker's unit weight, the diameter that bends (a plain string: its gauge;
// a wound string: its core), and what a magnetic pickup senses of it (its ferromagnetic share of the section).
#pragma once

#include <array>
#include <cmath>

#include "../constants/core.hpp"

namespace grmg
{
    // the set the menu entry stands for (STRING_SETS index)
    inline int string_set_index(const models::Settings &s)
    {
        if (s.string_set <= STRING_SET_AUTOMATIC || s.string_set > NUM_STRING_SETS)
            return s.is_magnetic_pickup() ? STRING_SET_ELECTRIC : STRING_SET_ACOUSTIC;
        return s.string_set - 1;
    }

    // the diameter of an all-steel string of this mass per length: the old wound-core setting is a share of it
    inline double steel_equivalent_diameter(double mu) { return std::sqrt(4.0 * mu / (PI * RHO_STEEL)); }

    inline std::array<models::GuitarString, NUM_STRINGS> guitar_strings(const models::Settings &s)
    {
        const auto &set = STRING_SETS[string_set_index(s)];
        std::array<models::GuitarString, NUM_STRINGS> out{};
        for (int i = 0; i < NUM_STRINGS; ++i)
        {
            const auto &g = set.strings[i];
            auto &str = out[(size_t)i];
            str.L = s.scale_mm / 1000.0;
            str.mu = g.unit_weight * LB_PER_INCH_KG_M;
            str.gauge = g.inches * INCH_M;
            str.wound = g.wound;
            str.material = &STRING_MATERIALS[g.wound ? set.wound_material : set.plain_material];
            const auto &m = *str.material;
            if (!g.wound)
                str.bending = str.gauge;
            else if (m.core_ratio > 0.0)
                str.bending = m.core_ratio * str.gauge;
            else
                str.bending = s.core * steel_equivalent_diameter(str.mu);
            // a pickup senses the iron in the string: all of it, its core only, or nothing
            const double core_share = str.wound ? std::pow(str.bending / str.gauge, 2.0) : 1.0;
            str.magnetic = m.magnetic_wrap ? 1.0 : (m.magnetic_core ? core_share : 0.0);
        }
        return out;
    }

    // the core's Young's modulus under tension T (nylon stiffens as it is stretched)
    inline double string_modulus(const models::GuitarString &g, double T)
    {
        const double section = PI * g.bending * g.bending / 4.0;
        return g.material->E + g.material->E_per_stress * T / section;
    }

} // namespace grmg
