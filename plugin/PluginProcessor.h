// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
#pragma once

#include <array>
#include <atomic>
#include <memory>
#include <mutex>
#include <thread>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>

#include "../core/engine.hpp"

class GRMGuitarProcessor;

// Background thread: solves the networks (Model) when a setting that feeds them moves, and turns the guitar
// roll into a Performance when the song, the model, the tempo or the player change. Never the audio thread.
class SolverThread : public juce::Thread {
public:
    explicit SolverThread(GRMGuitarProcessor& p) : juce::Thread("GRM solver"), owner(p) {}
    void run() override;

private:
    GRMGuitarProcessor& owner;
};

class GRMGuitarProcessor : public juce::AudioProcessor {
public:
    GRMGuitarProcessor();
    ~GRMGuitarProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "GRM Guitar"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 6.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState params;

    // ------------------------------------------------------------ the guitar roll (message thread)
    grmg::Song getSong() const;
    void setSong(const grmg::Song& s);       // after every edit
    void newTake();                          // the player plays it again, differently
    unsigned getSeed() const { return seed.load(); }
    int getStateVersion() const { return stateVersion.load(); }   // changes when the host loads a state

    // internal transport (standalone, or when the host is stopped)
    void setInternalPlaying(bool on);
    bool isInternalPlaying() const { return internalPlaying.load(); }
    void setInternalPosition(double beat) { internalSeek.store(beat); }
    bool isHostPlaying() const { return hostPlaying.load(); }
    double getShownBeat() const { return engine.shown_beat.load(); }
    double getTempo() const { return currentTempo.load(); }

    // a string struck from the interface (the neck)
    void pluckFromInterface(int string, int fret, float velocity);

    // what the interface shows
    int getShownFret(int i) const { return engine.shown_fret[(size_t) i].load(); }
    float getShownLevel(int i) const { return engine.shown_level[(size_t) i].load(); }
    int readScope(float* dest, int count);   // the latest samples of the output
    juce::String getStatus() const;
    int getOpenMidi(int i) const;

    grmg::Settings readSettings() const;

    // the latest solved networks, for the interface (any thread)
    std::shared_ptr<const grmg::Model> getShownModel() const {
        std::lock_guard<std::mutex> lock(shownLock);
        return shownModel;
    }

    // the whole roll, rendered offline with the body and the room, to a WAV file (in a thread of its own)
    void exportSong(const juce::File& file);
    bool isExporting() const { return exporting.load(); }

private:
    friend class SolverThread;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    grmg::Engine engine;
    SolverThread solver {*this};

    // the song, edited on the message thread, read by the solver
    mutable std::mutex songLock;
    grmg::Song song = grmg::Song::demo();
    std::atomic<int> songVersion {1};
    std::atomic<unsigned> seed {7};
    std::atomic<int> stateVersion {0};

    // from the solver to the audio thread
    // a model, and the impulse responses to load when they changed (built here, loaded on the audio thread)
    struct ModelHolder {
        std::shared_ptr<const grmg::Model> model;
        juce::AudioBuffer<float> body, room;
    };
    static ModelHolder* makeHolder(std::shared_ptr<const grmg::Model> m);
    unsigned loadedBody = 0, loadedRoom = 0;     // audio thread
    grmg::Mailbox<ModelHolder> modelBox;
    grmg::Mailbox<grmg::Performance> perfBox;
    std::atomic<double> sampleRate {0.0};
    std::atomic<double> currentTempo {90.0};
    std::atomic<bool> solving {false};
    std::atomic<int> solvedStage {0};          // 0 nothing, 1 strings, 2 everything
    mutable std::mutex shownLock;
    std::shared_ptr<const grmg::Model> shownModel;
    void show(std::shared_ptr<const grmg::Model> m) {
        std::lock_guard<std::mutex> lock(shownLock);
        shownModel = std::move(m);
    }

    // body and room: impulse responses from their networks
    juce::dsp::Convolution bodyConv {juce::dsp::Convolution::NonUniform {512}};
    juce::dsp::Convolution roomConv {juce::dsp::Convolution::NonUniform {512}};
    std::atomic<bool> roomLoaded {false};
    std::atomic<bool> exporting {false};
    std::thread exportThread;
    juce::String exportResult;
    mutable std::mutex exportLock;
    juce::AudioBuffer<float> dry, wet;
    juce::SmoothedValue<float> bodyMix, volume;
    float limiterGain = 1.0f;     // output peak limiter (audio thread)

    // transport
    std::atomic<bool> internalPlaying {false}, hostPlaying {false};
    std::atomic<double> internalSeek {-1.0};
    double internalBeat = 0.0;

    // interface -> audio thread: strings struck on the neck
    struct UiPluck { int string, fret; float velocity; };
    juce::AbstractFifo uiFifo {64};
    std::array<UiPluck, 64> uiPlucks {};

    // audio thread -> interface: the oscilloscope
    juce::AbstractFifo scopeFifo {8192};
    std::array<float, 8192> scopeData {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GRMGuitarProcessor)
};
