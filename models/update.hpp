// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
#pragma once

#include <string>

namespace grmg::models
{
    // a release of the plugin published on GitHub
    struct Release
    {
        std::string version; // its tag, as published ("v0.4.0")
        std::string url;     // where to get it
    };
}
