// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// The plugin as a host sees it, without a host: create it, prepare it, wait for the solver, play the guitar
// roll and some MIDI, save and restore the state. Writes a stereo WAV and checks the output is sound.
// Usage: plugin_host_test [out.wav]
#include <chrono>
#include <cstdio>
#include <thread>

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_processors/juce_audio_processors.h>


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
    plugin->releaseResources();
    return finite && peak > 1e-3 ? 0 : 1;
}
