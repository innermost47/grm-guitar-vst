// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// The plugin as a host sees it, without a host: create it, prepare it, wait for the solver, play the guitar
// roll and some MIDI, save and restore the state. Writes a stereo WAV and checks the output is sound.
// Usage: plugin_host_test [out.wav]
#include <chrono>
#include <functional>
#include <cstdio>
#include <thread>

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include "../plugin/PluginProcessor.h"


int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI init;
    std::unique_ptr<juce::AudioProcessor> plugin(createPluginFilter());
    const double sr = 48000.0;
    const int block = 512;
    plugin->setPlayConfigDetails(0, 2, sr, block);
    plugin->prepareToPlay(sr, block);
    juce::AudioBuffer<float> buf(2, block);
    juce::MidiBuffer midi;
    // wait for the networks (the solver thread), as a host keeps calling processBlock meanwhile
    auto t0 = std::chrono::steady_clock::now();
    for (int k = 0; k < 400; ++k) {
        buf.clear();
        plugin->processBlock(buf, midi);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    std::printf("waited %.1f s for the solver\n",
                std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count());
    // save and restore the state (parameters and guitar roll), as a host does with a project
    auto* p = plugin.get();
    juce::MemoryBlock state;
    p->getStateInformation(state);
    std::printf("state: %zu bytes\n", state.getSize());
    p->setStateInformation(state.getData(), (int) state.getSize());

    const int seconds = 12;
    juce::AudioBuffer<float> out(2, (int) sr * seconds);
    out.clear();
    double busy = 0;
    const int chord[6] = {43, 47, 50, 55, 59, 67};
    for (int pos = 0; pos + block <= out.getNumSamples(); pos += block) {
        midi.clear();
        // a strum from MIDI at 0.5 s, notes off at 3 s, an arpeggio from 3.5 s
        for (int k = 0; k < 6; ++k) {
            const int at = (int) ((0.5 + 0.015 * k) * sr) - pos;
            if (at >= 0 && at < block) midi.addEvent(juce::MidiMessage::noteOn(1, chord[k], (juce::uint8) 100), at);
            const int off = (int) (3.0 * sr) - pos;
            if (off >= 0 && off < block) midi.addEvent(juce::MidiMessage::noteOff(1, chord[k]), off);
        }
        for (int k = 0; k < 16; ++k) {
            const int at = (int) ((3.5 + 0.25 * k) * sr) - pos;
            if (at >= 0 && at < block) midi.addEvent(juce::MidiMessage::noteOn(1, chord[k % 6], (juce::uint8) 90), at);
        }
        buf.clear();
        auto a = std::chrono::steady_clock::now();
        plugin->processBlock(buf, midi);
        busy += std::chrono::duration<double>(std::chrono::steady_clock::now() - a).count();
        for (int ch = 0; ch < 2; ++ch) out.copyFrom(ch, pos, buf, ch, 0, block);
    }
    std::printf("processBlock: %d s of sound in %.2f s (%.1f %% of one core)\n", seconds, busy, 100.0 * busy / seconds);
    float peak = 0;
    bool finite = true;
    for (int ch = 0; ch < 2; ++ch)
        for (int t = 0; t < out.getNumSamples(); ++t) {
            const float v = out.getSample(ch, t);
            finite &= std::isfinite(v);
            peak = std::max(peak, std::abs(v));
        }
    std::printf("peak %.3f, rms %.4f%s\n", peak, out.getRMSLevel(0, 0, out.getNumSamples()), finite ? "" : " NOT FINITE");
    juce::WavAudioFormat wav;
    juce::File file(argc > 1 ? argv[1] : "plugin_host_test.wav");
    file.deleteFile();
    std::unique_ptr<juce::OutputStream> stream(file.createOutputStream());
    if (stream)
        if (auto writer = wav.createWriterFor(stream, juce::AudioFormatWriterOptions {}.withSampleRate(sr).withNumChannels(2).withBitsPerSample(24)))
            writer->writeFromAudioSampleBuffer(out, 0, out.getNumSamples());
    // ---------------------------------------------------------------- pattern mode: the keys play patterns
    int failures = 0;
    auto check = [&](const char* what, bool ok) {
        std::printf("%s %s\n", ok ? "ok  " : "FAIL", what);
        failures += ok ? 0 : 1;
    };
    // Export over an existing file only after the replacement WAV has been written.
    {
        juce::TemporaryFile destination(".wav"), blocked(".wav");
        GRMGuitarProcessor exporter;
        const auto existing = destination.getFile();
        check("create previous export", existing.replaceWithText("previous export"));
        auto waitForExport = [&] {
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
            while (exporter.isExporting() && std::chrono::steady_clock::now() < deadline)
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            return !exporter.isExporting();
        };

        auto* mute = exporter.params.getParameter("mix_mute");
        mute->setValueNotifyingHost(1.0f);
        exporter.exportSong(existing);
        check("muted export finishes and preserves previous bytes",
              waitForExport() && existing.loadFileAsString() == "previous export"
              && exporter.getStatus().startsWith("Export failed"));

        grmg::Song song;
        song.bars = 1;
        song.notes.push_back({0, 0, 0.0, 1.0, 0.8});
        exporter.setSong(song);
        exporter.params.getParameter("sensor")->setValueNotifyingHost(1.0f); // Piezo, no convolution needed.
        mute->setValueNotifyingHost(0.0f);
        exporter.exportSong(existing);
        check("successful export finishes", waitForExport());
        std::unique_ptr<juce::AudioFormatReader> wavReader(wav.createReaderFor(existing.createInputStream().release(), true));
        juce::AudioBuffer<float> exported(2, (int)sr);
        check("successful export replaces the file with a readable stereo WAV",
              wavReader && wavReader->numChannels == 2 && wavReader->bitsPerSample == 24 && wavReader->lengthInSamples >= exported.getNumSamples()
              && wavReader->read(&exported, 0, exported.getNumSamples(), 0, true, true) && exported.getMagnitude(0, exported.getNumSamples()) > 0.1f);

        const auto directory = blocked.getFile();
        const auto marker = directory.getChildFile("keep.txt");
        check("create a destination that cannot be replaced", directory.createDirectory().wasOk() && marker.replaceWithText("keep"));
        exporter.exportSong(directory);
        check("failed replacement preserves the destination",
              waitForExport() && marker.loadFileAsString() == "keep" && exporter.getStatus().startsWith("Export failed"));
    }
    auto* g = dynamic_cast<GRMGuitarProcessor*>(p);
    g->addPattern(false);                              // pattern 2: one low G, four beats
    grmg::Song two;
    two.bars = 1;
    grmg::RollNote note;
    note.string = 0;
    note.fret = 3;
    note.length = 4.0;
    two.notes.push_back(note);
    g->setSong(two);
    check("two patterns, the new one edited", g->getPatternCount() == 2 && g->getEditingPattern() == 1);
    g->params.getParameter("midi_mode")->setValueNotifyingHost(1.0f);
    auto run = [&](double secs, std::function<void(juce::MidiBuffer&, int)> events) {
        double e = 0;
        const int total = (int) (secs * sr);
        for (int pos = 0; pos + block <= total; pos += block) {
            midi.clear();
            events(midi, pos);
            buf.clear();
            plugin->processBlock(buf, midi);
            e += buf.getRMSLevel(0, 0, block) * buf.getRMSLevel(0, 0, block);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return std::sqrt(e / (total / block));
    };
    auto none = [](juce::MidiBuffer&, int) {};
    run(4.0, none);                                    // the solver compiles both patterns, the strings go quiet
    const double quiet = run(1.0, none);
    const double held = run(1.5, [](juce::MidiBuffer& m, int pos) {
        if (pos == 0) m.addEvent(juce::MidiMessage::noteOn(1, GRMGuitarProcessor::PATTERN_KEY + 1, (juce::uint8) 100), 10);
    });
    std::printf("hollow body, microphone: %.3g\n", held);
    check("a key plays its pattern", held > 20 * quiet + 1e-5);
    check("the playing pattern is pattern 2", g->getPlayingPattern() == 1);
    run(0.1, [](juce::MidiBuffer& m, int pos) {
        if (pos == 0) m.addEvent(juce::MidiMessage::noteOff(1, GRMGuitarProcessor::PATTERN_KEY + 1), 0);
    });
    check("releasing the key stops it", g->getPlayingPattern() == -1);
    const double after = run(2.0, none);
    check("the strings are stopped after the key", after < 0.3 * held);
    const double first = run(1.5, [](juce::MidiBuffer& m, int pos) {
        if (pos == 0) m.addEvent(juce::MidiMessage::noteOn(1, GRMGuitarProcessor::PATTERN_KEY, (juce::uint8) 100), 10);
    });
    check("the first key plays pattern 1", g->getPlayingPattern() == 0 && first > 20 * quiet + 1e-5);
    run(0.1, [](juce::MidiBuffer& m, int pos) {
        if (pos == 0) m.addEvent(juce::MidiMessage::noteOff(1, GRMGuitarProcessor::PATTERN_KEY), 0);
    });
    run(3.0, none);
    run(0.5, [](juce::MidiBuffer& m, int pos) {
        if (pos == 0) m.addEvent(juce::MidiMessage::noteOn(1, 72, (juce::uint8) 100), 0);   // no pattern there
    });
    check("a key without a pattern plays nothing", g->getPlayingPattern() == -1);
    // the sensors: a solid body heard by a magnetic pickup, then a piezo
    for (int sensor : {1, 2}) {
        g->params.getParameter("hollow")->setValueNotifyingHost(0.0f);
        g->params.getParameter("sensor")->setValueNotifyingHost(sensor / 2.0f);
        run(3.0, none);                                // the solver: the solid body's network
        const double level = run(1.5, [](juce::MidiBuffer& m, int pos) {
            if (pos == 0) m.addEvent(juce::MidiMessage::noteOn(1, GRMGuitarProcessor::PATTERN_KEY + 1, (juce::uint8) 100), 10);
        });
        run(0.1, [](juce::MidiBuffer& m, int pos) {
            if (pos == 0) m.addEvent(juce::MidiMessage::noteOff(1, GRMGuitarProcessor::PATTERN_KEY + 1), 0);
        });
        run(3.0, none);
        check(sensor == 1 ? "a magnetic pickup on a solid body sounds" : "a piezo sounds", std::isfinite(level) && level > 20 * quiet + 1e-5);
    }
    g->params.getParameter("sensor")->setValueNotifyingHost(0.0f);
    {   // a solid body heard by the microphone (it went silent in a host)
        run(4.0, none);
        const double level = run(1.5, [](juce::MidiBuffer& m, int pos) {
            if (pos == 0) m.addEvent(juce::MidiMessage::noteOn(1, GRMGuitarProcessor::PATTERN_KEY + 1, (juce::uint8) 100), 10);
        });
        run(0.1, [](juce::MidiBuffer& m, int pos) {
            if (pos == 0) m.addEvent(juce::MidiMessage::noteOff(1, GRMGuitarProcessor::PATTERN_KEY + 1), 0);
        });
        std::printf("solid body, microphone: %.3g\n", level);
        check("a solid body heard by the microphone sounds", std::isfinite(level) && level > 20 * quiet + 1e-5);
        run(3.0, none);
    }
    g->params.getParameter("hollow")->setValueNotifyingHost(1.0f);
    // the patterns are saved with the session
    juce::MemoryBlock saved;
    p->getStateInformation(saved);
    std::unique_ptr<juce::AudioProcessor> other(createPluginFilter());
    other->setStateInformation(saved.getData(), (int) saved.getSize());
    auto* o = dynamic_cast<GRMGuitarProcessor*>(other.get());
    check("the patterns come back with the session", o->getPatternCount() == 2 && o->getEditingPattern() == 1
                                                         && o->getSong().notes.size() == 1);
    // ---------------------------------------------------------------- several guitars
    g->params.getParameter("midi_mode")->setValueNotifyingHost(0.0f);
    {
        // a session of the one-guitar versions: its parameters and its patterns, nothing about guitars
        auto old = g->params.copyState().createCopy();
        grmg::Song s1;
        s1.bars = 2;
        grmg::RollNote a;
        a.string = 2;
        a.fret = 2;
        s1.notes.push_back(a);
        old.setProperty("song", juce::String(s1.to_text()), nullptr);
        old.setProperty("pattern_count", 1, nullptr);
        old.setProperty("pattern_0", juce::String(s1.to_text()), nullptr);
        old.setProperty("editing", 0, nullptr);
        for (int k = old.getNumChildren() - 1; k >= 0; --k)       // no parameter of another guitar
            if (old.getChild(k).getProperty("id").toString().startsWith("g")
                && old.getChild(k).getProperty("id").toString().containsChar('_')
                && old.getChild(k).getProperty("id").toString()[1] >= '2' && old.getChild(k).getProperty("id").toString()[1] <= '8')
                old.removeChild(k, nullptr);
        juce::MemoryBlock legacy;
        if (auto xml = old.createXml()) juce::AudioProcessor::copyXmlToBinary(*xml, legacy);
        std::unique_ptr<juce::AudioProcessor> fresh(createPluginFilter());
        fresh->setStateInformation(legacy.getData(), (int) legacy.getSize());
        auto* f = dynamic_cast<GRMGuitarProcessor*>(fresh.get());
        check("a one-guitar session opens as one guitar with its pattern",
              f->getGuitarCount() == 1 && f->getSong().notes.size() == 1 && f->getSong().notes[0].fret == 2);
    }
    // two guitars playing the plugin's own clock, then guitar 1 muted
    g->selectGuitar(0);
    g->addGuitar();
    check("a second guitar, selected", g->getGuitarCount() == 2 && g->getSelectedGuitar() == 1);
    grmg::Song lead;
    lead.bars = 1;
    for (int k = 0; k < 4; ++k) {
        grmg::RollNote n;
        n.string = 4;
        n.fret = 3 + k;
        n.start = k;
        n.length = 1.0;
        lead.notes.push_back(n);
    }
    g->setSong(lead);
    run(4.0, none);                                    // the second guitar's networks
    g->setInternalPlaying(true);
    const double both = run(2.0, none);
    g->params.getParameter("mix_mute")->setValueNotifyingHost(1.0f);
    run(2.0, none);
    const double second = run(2.0, none);
    g->params.getParameter("mix_mute")->setValueNotifyingHost(0.0f);
    g->setInternalPlaying(false);
    check("two guitars sound", both > 20 * quiet + 1e-5);
    check("guitar 1 muted: guitar 2 still sounds", second > 20 * quiet + 1e-5);
    // guitar 2 to its own output
    run(3.0, none);
    plugin->enableAllBuses();
    plugin->prepareToPlay(sr, block);
    g->params.getParameter("g2_mix_out")->setValueNotifyingHost(1.0f);
    run(3.0, none);
    {
        const int channels = plugin->getTotalNumOutputChannels();
        juce::AudioBuffer<float> all(channels, block);
        g->params.getParameter("mix_mute")->setValueNotifyingHost(1.0f);   // only guitar 2 (muted before it plays)
        for (int k = 0; k < 40; ++k) {
            all.clear();
            midi.clear();
            plugin->processBlock(all, midi);
        }
        g->setInternalPlaying(true);
        double mainE = 0, ownE = 0;
        for (int k = 0; k < 200; ++k) {
            all.clear();
            midi.clear();
            g->params.getParameter("mix_mute")->setValueNotifyingHost(1.0f);   // only guitar 2
            plugin->processBlock(all, midi);
            auto mainBus = plugin->getBusBuffer(all, false, 0), own = plugin->getBusBuffer(all, false, 2);
            mainE += mainBus.getRMSLevel(0, 0, block);
            ownE += own.getRMSLevel(0, 0, block);
        }
        g->setInternalPlaying(false);
        g->params.getParameter("mix_mute")->setValueNotifyingHost(0.0f);
        std::printf("own output %.4g, main %.4g, channels %d\n", ownE, mainE, channels);
        check("guitar 2 on its own output, not in the main one", channels == 18 && ownE > 20 * mainE + 1e-6);
    }
    // the guitars and their patterns come back with the session; removing guitar 1 moves guitar 2 down
    juce::MemoryBlock twoGuitars;
    p->getStateInformation(twoGuitars);
    std::unique_ptr<juce::AudioProcessor> again(createPluginFilter());
    again->setStateInformation(twoGuitars.getData(), (int) twoGuitars.getSize());
    auto* a2 = dynamic_cast<GRMGuitarProcessor*>(again.get());
    check("two guitars come back with the session", a2->getGuitarCount() == 2 && a2->guitar(1).getSong().notes.size() == 4
                                                         && a2->params.getRawParameterValue("g2_mix_out")->load() > 0.5f);
    a2->removeGuitar(0);
    check("removing guitar 1: guitar 2 becomes guitar 1", a2->getGuitarCount() == 1 && a2->guitar(0).getSong().notes.size() == 4
                                                              && a2->params.getRawParameterValue("mix_out")->load() > 0.5f);
    plugin->releaseResources();
    return finite && peak > 1e-3 && failures == 0 ? 0 : 1;
}
