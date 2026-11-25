// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
#include "UpdateChecker.h"

#include <vector>

UpdateChecker::UpdateChecker() : juce::Thread("GRM Guitar update check")
{
    startThread(juce::Thread::Priority::low);
}

UpdateChecker::~UpdateChecker()
{
    stopThread(ui::UPDATE_TIMEOUT_MS + 1000);
}

std::optional<grmg::models::Release> UpdateChecker::take()
{
    if (!ready.load())
        return std::nullopt;
    std::lock_guard<std::mutex> guard(lock);
    ready.store(false);
    auto release = found;
    found.reset();
    return release;
}

void UpdateChecker::skip(const std::string &version)
{
    juce::PropertiesFile file(settings());
    file.setValue(ui::SKIPPED_VERSION_KEY, juce::String(version));
    file.saveIfNeeded();
}

std::string UpdateChecker::currentVersion()
{
    return JucePlugin_VersionString;
}

bool UpdateChecker::isNewer(const std::string &candidate, const std::string &current)
{
    // the numbers of a version, whatever surrounds them ("v0.4.0", "0.4", "0.4.0-beta")
    auto numbers = [](const std::string &version)
    {
        std::vector<int> parts;
        const auto tokens = juce::StringArray::fromTokens(juce::String(version).trimCharactersAtStart("vV"), ".", "");
        for (const auto &token : tokens)
        {
            parts.push_back(token.getIntValue());
            if (!token.containsOnly("0123456789"))
                break; // "0-beta": its leading number, and nothing after it
        }
        return parts;
    };
    const auto a = numbers(candidate), b = numbers(current);
    for (size_t k = 0; k < std::max(a.size(), b.size()); ++k)
    {
        const int x = k < a.size() ? a[k] : 0, y = k < b.size() ? b[k] : 0;
        if (x != y)
            return x > y;
    }
    return false;
}

std::optional<grmg::models::Release> UpdateChecker::parse(const juce::String &json)
{
    const auto answer = juce::JSON::parse(json);
    const auto tag = answer.getProperty("tag_name", {}).toString();
    if (tag.isEmpty())
        return std::nullopt;
    return grmg::models::Release{tag.toStdString(), ui::RELEASES_PAGE};
}

juce::PropertiesFile::Options UpdateChecker::settings()
{
    juce::PropertiesFile::Options options;
    options.applicationName = ui::SETTINGS_NAME;
    options.folderName = ui::SETTINGS_NAME;
    options.filenameSuffix = ".settings";
    options.osxLibrarySubFolder = "Application Support";
    return options;
}

void UpdateChecker::run()
{
    const auto stream = juce::URL(ui::RELEASES_API)
                            .createInputStream(juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                                                   .withConnectionTimeoutMs(ui::UPDATE_TIMEOUT_MS)
                                                   .withExtraHeaders(juce::String("User-Agent: ") + ui::HTTP_USER_AGENT +
                                                                     "\r\nAccept: application/vnd.github+json"));
    if (stream == nullptr || threadShouldExit())
        return; // offline, or no https on this system: say nothing
    const auto release = parse(stream->readEntireStreamAsString());
    if (!release || !isNewer(release->version, currentVersion()))
        return;
    const juce::PropertiesFile file(settings());
    if (file.getValue(ui::SKIPPED_VERSION_KEY) == juce::String(release->version))
        return;
    std::lock_guard<std::mutex> guard(lock);
    found = release;
    ready.store(true);
}
