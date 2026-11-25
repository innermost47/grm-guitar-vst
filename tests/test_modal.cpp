// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// The dialogs and the update check: versions compare as numbers, GitHub's answer is read, the equations are
// embedded and drawn on the left of a dialog with the link to grm-core under them, a dialog closes on a button or
// on Escape and says so once. Usage: test_modal [snapshot.png] (writes the update dialog as it looks).
#include <cstdio>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../plugin/PluginProcessor.h"
#include "../plugin/services/UpdateChecker.h"
#include "../plugin/style/CustomLookAndFeel.h"
#include "../plugin/widgets/Modal.h"

static int failures = 0, checks = 0;

static void check(bool ok, const char *what)
{
    ++checks;
    if (!ok)
    {
        ++failures;
        std::printf("FAIL  %s\n", what);
    }
}

int main(int argc, char **argv)
{
    juce::ScopedJuceInitialiser_GUI init;

    // ---- versions: numbers, not text
    check(UpdateChecker::isNewer("v0.4.0", "0.3.8"), "v0.4.0 is newer than 0.3.8");
    check(UpdateChecker::isNewer("0.3.10", "0.3.9"), "0.3.10 is newer than 0.3.9 (not as text)");
    check(UpdateChecker::isNewer("1.0", "0.9.9"), "1.0 is newer than 0.9.9");
    check(!UpdateChecker::isNewer("v0.3.8", "0.3.8"), "the same version is not newer");
    check(!UpdateChecker::isNewer("0.3", "0.3.0"), "0.3 and 0.3.0 are the same");
    check(!UpdateChecker::isNewer("0.3.7", "0.3.8"), "an older version is not newer");
    check(UpdateChecker::isNewer("0.4.0-beta", "0.3.8"), "a tag with a suffix is read by its numbers");
    check(!UpdateChecker::currentVersion().empty(), "the plugin knows its own version");

    // ---- GitHub's answer
    const auto release = UpdateChecker::parse(R"({"tag_name": "v0.4.0", "html_url": "https://github.com/x/y/releases/tag/v0.4.0"})");
    check(release && release->version == "v0.4.0", "the release's tag is read");
    check(release && release->url == ui::RELEASES_PAGE, "the dialog links to the latest release page");
    check(!UpdateChecker::parse(R"({"message": "Not Found"})"), "an answer without a release gives none");
    check(!UpdateChecker::parse("not json"), "a broken answer gives none");

    // ---- the equations and the link to grm-core, on the left of every dialog
    juce::Component window;
    window.setLookAndFeel(&CustomLookAndFeel::getInstance());
    window.setSize(1000, 700);
    auto modal = std::make_unique<Modal>("A new GRM Guitar is out");
    check(modal->getEquations().hasEquations(), "the equations are embedded and drawn");
    check(modal->getEquations().getLink().getURL().toString(false) == ui::GRM_CORE_PAGE, "the link goes to grm-core");
    modal->setMessage("GRM Guitar v0.4.0 is available (this one is 0.3.8). Its release page tells what is new and has the downloads for every system.");
    auto &link = modal->addLink("Get GRM Guitar v0.4.0", juce::URL(ui::RELEASES_PAGE));
    auto &again = modal->addCheckbox("Don't ask me again for this version");
    auto &close = modal->addButton("Close");
    int dismissed = 0;
    modal->onDismiss = [&] { ++dismissed; };
    modal->show(window);
    check(modal->getBounds() == window.getLocalBounds(), "the dialog covers the window");
    check(link.isVisible() && again.isVisible() && close.isVisible(), "its rows and its button are shown");
    check(link.getBounds().getBottom() <= again.getY(), "its rows are in order");
    check(modal->getEquations().getRight() <= link.getX(), "the equations stay on the left");

    if (argc > 1)
    {
        const auto image = window.createComponentSnapshot(window.getLocalBounds());
        juce::File file(juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]));
        file.deleteFile();
        juce::FileOutputStream out(file);
        juce::PNGImageFormat().writeImageToStream(image, out);
    }

    close.onClick(); // what a click runs (triggerClick would wait for the message loop)
    check(!modal->isVisible() && modal->isDismissed() && dismissed == 1, "a button closes the dialog, once");
    modal->dismiss();
    check(dismissed == 1, "closed twice, it says so once");

    auto other = std::make_unique<Modal>("Escape");
    int escaped = 0;
    other->onDismiss = [&] { ++escaped; };
    other->show(window);
    other->keyPressed(juce::KeyPress(juce::KeyPress::escapeKey));
    check(escaped == 1, "Escape closes the dialog");

    window.setLookAndFeel(nullptr);
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
