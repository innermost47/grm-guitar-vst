// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// metrics_table.hpp
//
// The measures of a probe run, one line each (scenario, measure, value, unit): printed as they come, and written as
// a CSV that the next runs can be compared with (before / after a change of the engine).
#pragma once

#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

namespace grmg::timbre
{
    class MetricsTable
    {
    public:
        void add(const std::string &scenario, const std::string &measure, double value, const std::string &unit)
        {
            rows_.push_back({scenario, measure, value, unit});
            std::printf("  %-34s %-28s %12.3f %s\n", scenario.c_str(), measure.c_str(), value, unit.c_str());
        }

        bool write_csv(const std::string &path) const
        {
            std::ofstream f(path);
            if (!f)
                return false;
            f << "scenario,measure,value,unit\n";
            for (const auto &r : rows_)
                f << r.scenario << ',' << r.measure << ',' << r.value << ',' << r.unit << '\n';
            return (bool)f;
        }

        size_t size() const { return rows_.size(); }

    private:
        struct Row
        {
            std::string scenario, measure;
            double value;
            std::string unit;
        };
        std::vector<Row> rows_;
    };
}
