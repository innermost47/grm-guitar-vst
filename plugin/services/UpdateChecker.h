// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// Asks GitHub, once per host process and on its own thread, for the plugin's latest release. A newer one that
// the user has not chosen to skip is handed to the first editor that asks for it (take), which shows it. Shared by
// every instance of the plugin (juce::SharedResourcePointer), so that a project of ten guitars asks once.
#pragma once

#include <atomic>
#include <mutex>
#include <optional>
#include <string>

#include <JuceHeader.h>
#include "../../constants/ui.hpp"
#include "../../models/update.hpp"

class UpdateChecker : private juce::Thread
{
public:
    UpdateChecker();
    ~UpdateChecker() override;

    // the newer release found, once (nothing while the check runs, nothing after it was taken)
    std::optional<grmg::models::Release> take();

    // "don't ask again for this version": remembered in the plugin's own settings, for every project
    void skip(const std::string &version);

    // the plugin's own version
    static std::string currentVersion();
    // is `candidate` ("v0.4.0", "0.4") later than `current`?
    static bool isNewer(const std::string &candidate, const std::string &current);
    // the latest release out of GitHub's answer (its tag_name), if it has one
    static std::optional<grmg::models::Release> parse(const juce::String &json);

private:
    void run() override;
    static juce::PropertiesFile::Options settings();

    std::mutex lock;
    std::optional<grmg::models::Release> found;
    std::atomic<bool> ready{false};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UpdateChecker)
};
