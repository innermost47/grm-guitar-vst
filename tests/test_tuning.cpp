// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// The free tuning: each open string's note is a parameter (Custom). The tuning network gives each string the tension
// its note asks (a string tuned down to B slackens), the strings sound their notes, a MIDI keyboard finds them; in the
// plugin, Custom by hand starts from the tuning chosen before, a session loaded with Custom keeps its own notes, and an
// old session (a tuning of the list) sounds as it did.
#include <cmath>
#include <cstdio>
#include <juce_audio_processors/juce_audio_processors.h>

#include "../core/offline_renderer.hpp"
#include "../plugin/PluginProcessor.h"
#include "support/rigs.hpp"
#include "timbre/partial_tracker.hpp"

using namespace grmg;

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

static const std::array<int, NUM_STRINGS> B_STANDARD = {35, 40, 45, 50, 54, 59};

static void engine()
{
    auto standard = rigs::named("pickup");
    auto low = standard;
    low.open_midi = B_STANDARD;
    Model a, b;
    a.solve(standard, 48000.0, nullptr, false);
    b.solve(low, 48000.0, nullptr, false);
    // the same string tuned down: its tension falls as the square of its pitch
    bool squares = true;
    for (int i = 0; i < NUM_STRINGS; ++i)
    {
        const double ratio = midi_hz(B_STANDARD[(size_t)i]) / midi_hz(standard.open_midi[(size_t)i]);
        squares &= std::abs(b.tension[(size_t)i] / a.tension[(size_t)i] - ratio * ratio) < 1e-6;
    }
    check(squares, "a string tuned down: its tension falls as the square of its pitch");
    check(b.tension[0] < STRING_SLACK_N && a.tension[0] > STRING_SLACK_N, "the low string tuned to B1 is slack, in E2 it is not");
    check(b.open_midi(0) == 35 && b.open_midi(5) == 59, "the model plays the notes it was given");
    check(!Model::same_strings(standard, low), "another tuning: other strings to solve");
    auto one = standard;
    one.open_midi[3] = 54;
    check(!Model::same_strings(standard, one), "one string retuned: the strings are solved again");
    check(Model::same_strings(standard, rigs::named("pickup")), "the same notes: nothing to solve");

    // the low string tuned to B1 sounds B1; a MIDI keyboard's B1 finds it, open
    OfflineRenderer renderer(low, 48000.0);
    const auto take = renderer.render_live({{0.0, 1.5, 35, 0.8, 1}}, 1.5);
    const auto &pl = renderer.model().place[0][0];
    timbre::PartialTracker tracker(48000.0);
    tracker.track(take.pickup, pl.f1, pl.f1, 0.0, 1.5);
    const double cents = timbre::PartialTracker::cents(tracker.median_frequency(0.6, 1.2), midi_hz(35));
    std::printf("  B1 on the low string: %.1f cents from tempered (the attack's glide aside)\n", cents);
    check(std::abs(cents) < 15.0, "the low string tuned to B1 sounds B1");
    Engine engine;
    engine.prepare(48000.0);
    engine.set_model(&renderer.model());
    engine.note_on(35, 0.8);
    check(engine.shown_fret[0].load() == 0, "a MIDI B1 is the low string open");
}

static float plain(GRMGuitarProcessor &p, const juce::String &id)
{
    auto *param = p.params.getParameter(id);
    return param->convertFrom0to1(param->getValue());
}

static void set(GRMGuitarProcessor &p, const juce::String &id, float value, bool gesture)
{
    auto *param = p.params.getParameter(id);
    if (gesture)
        param->beginChangeGesture();
    param->setValueNotifyingHost(param->convertTo0to1(value));
    if (gesture)
        param->endChangeGesture();
}


static void plugin()
{
    GRMGuitarProcessor processor;
    auto &guitar = processor.guitar(0);
    check(guitar.openNotes() == tuning_notes(0), "a new guitar is in standard tuning");
    // a tuning of the list, then Custom by hand: the six notes start from it
    set(processor, "tuning", 2.0f, true); // Drop D
    check(guitar.openNotes() == tuning_notes(2), "Drop D chosen: its notes");
    set(processor, "tuning", (float)CUSTOM_TUNING, true);
    check(guitar.openNotes() == tuning_notes(2), "Custom by hand starts from Drop D");
    // one string retuned: only that one moves
    set(processor, OPEN_NOTE_PARAMS[0], (float)(35 - OPEN_NOTE_LOWEST), true);
    auto expected = tuning_notes(2);
    expected[0] = 35;
    check(guitar.openNotes() == expected, "Custom: the low string retuned to B1, the others stay");
    check(processor.readSettings().open_midi == expected, "the networks are asked for the notes of the Custom tuning");
    check(processor.getOpenMidi(0) == 35, "the interface reads the Custom notes");

    // a session saved with Custom loads with its own notes (nothing copies them over: no gesture)
    juce::MemoryBlock saved;
    processor.getStateInformation(saved);
    GRMGuitarProcessor loaded;
    loaded.setStateInformation(saved.getData(), (int)saved.getSize());
    check(loaded.guitar(0).openNotes() == expected, "a session saved in Custom loads with its notes");
    // a tuning of the list set without a gesture (a host, a session): its notes, Custom's kept for later
    set(loaded, "tuning", 5.0f, false);
    check(loaded.guitar(0).openNotes() == tuning_notes(5), "a tuning of the list set by a host: its notes");
    check((int)std::lround(plain(loaded, OPEN_NOTE_PARAMS[0])) + OPEN_NOTE_LOWEST == 35, "Custom's own notes stay for later");
    // the other guitars have their own
    check(loaded.guitar(1).openNotes() == tuning_notes(0), "each guitar has its own tuning");
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    engine();
    plugin();
    std::printf("%d checks, %d failed\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
