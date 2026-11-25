// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// A host that plays, stops, lets the user turn a knob, then plays again (Anthony in Bitwig, 25/11/2026: the plugin
// crashed on the second play). Patterns played from the keys: while no key holds a pattern, the engine keeps the
// last performance it played; the knob makes the solver compile new ones and free the old ones. The engine must
// never be left pointing at a freed performance.
#include <chrono>
#include <cstdio>
#include <thread>

#include <juce_audio_processors/juce_audio_processors.h>

#include "../plugin/PluginProcessor.h"

static int checks = 0, failures = 0;

static void check(bool ok, const char *what)
{
    ++checks;
    if (!ok)
    {
        ++failures;
        std::printf("FAIL  %s\n", what);
    }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    std::unique_ptr<juce::AudioProcessor> plugin(createPluginFilter());
    const double rate = 48000.0;
    const int block = 512;
    plugin->setPlayConfigDetails(0, 2, rate, block);
    plugin->prepareToPlay(rate, block);
    auto &processor = dynamic_cast<GRMGuitarProcessor &>(*plugin);
    auto set = [&](const juce::String &id, float value)
    {
        auto *p = processor.params.getParameter(id);
        p->setValueNotifyingHost(p->convertTo0to1(value));
    };
    set("midi_mode", 1.0f); // the keys play the patterns
    juce::AudioBuffer<float> buffer(2, block);
    auto run = [&](int blocks, juce::MidiBuffer midi = {})
    {
        for (int k = 0; k < blocks; ++k)
        {
            buffer.clear();
            plugin->processBlock(buffer, midi);
            midi.clear();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    };
    const int key = GRMGuitarProcessor::PATTERN_KEY; // pattern 1
    run(200);                                        // the networks solved, the patterns compiled

    // play: the key holds pattern 1, then the host stops (the key is released)
    juce::MidiBuffer on, off;
    on.addEvent(juce::MidiMessage::noteOn(1, key, 0.8f), 0);
    run(100, on);
    off.addEvent(juce::MidiMessage::noteOff(1, key), 0);
    run(10, off);
    const auto *played = processor.guitar(0).engine.performance();
    check(played != nullptr, "the pattern played");

    // stopped, the user turns a knob: the sound's networks and the patterns are solved again, the old ones freed
    set("brightness", 0.9f);
    run(200);
    check(processor.guitar(0).engine.performance() != played,
          "stopped, a knob turned: the engine follows the new patterns (not the freed ones)");

    // play again
    run(100, on);
    check(processor.guitar(0).engine.performance() != nullptr, "playing again");
    bool finite = true;
    for (int c = 0; c < 2; ++c)
        for (int t = 0; t < block; ++t)
            finite &= std::isfinite(buffer.getSample(c, t));
    check(finite, "playing again: the sound is finite");

    plugin->releaseResources();
    std::printf("%d checks, %d failed\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
