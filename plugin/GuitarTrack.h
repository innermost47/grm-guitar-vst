// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// One guitar of the plugin: its settings (parameters with its own prefix), its patterns, its networks and its
// player (solved off the audio thread), its strings (the engine), what listens to it (microphone, pickup,
// piezo) and its mixer strip (gain, level, pan, mute, solo, output). The plugin holds several of them.
// It is the place where another GRM instrument would plug in later: the processor only sees a track that
// renders stereo into a bus.
#pragma once

#include <array>
#include <functional>
#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "../core/engine.hpp"
#include "../core/offline.hpp"

// every pattern of a guitar, played by its player network (built by the solver, read by the audio thread)
struct PerformanceSet {
    std::vector<std::shared_ptr<const grmg::Performance>> list;
    const grmg::Performance* at(int k) const {
        return k >= 0 && k < (int) list.size() ? list[(size_t) k].get() : nullptr;
    }
};

// where the song is, for every guitar of the plugin (each one finds its own place in it: its loop, its pattern)
struct SongClock {
    bool playing = false;
    bool host = false;           // the host's position (ppq) or the plugin's own (Play)
    double beat = 0.0;           // host: its ppq at the block's start; own: beats since `start`
    double start = 0.0;          // own clock: the song beat where Play started
    double beats_per_sample = 0.0;
    bool loop = true;
};

class GuitarTrack {
public:
    static constexpr int MAX_PATTERNS = 24, PATTERN_KEY = 36;    // C1 (middle C = C3) plays pattern 1

    GuitarTrack(juce::AudioProcessorValueTreeState& state, int index);

    const int index;
    // the id of one of this guitar's parameters: guitar 1 keeps the ids of the one-guitar versions (old sessions
    // load as they were); the others are "g2_tuning"...
    static juce::String paramId(int guitar, const juce::String& id) {
        return guitar == 0 ? id : "g" + juce::String(guitar + 1) + "_" + id;
    }
    juce::String paramId(const juce::String& id) const { return paramId(index, id); }
    float param(const char* id) const { return state.getRawParameterValue(paramId(id))->load(); }
    grmg::Settings readSettings() const;
    struct MixerValues {
        float gainDb, level, pan;
        bool mute, solo, ownOutput;
    };
    MixerValues mixerValues() const;

    // ------------------------------------------------------------ patterns (message thread)
    grmg::Song getSong() const;                  // the pattern being edited
    void setSong(const grmg::Song& s);
    std::vector<grmg::Song> getPatterns() const;
    void setPatterns(const std::vector<grmg::Song>& p, int editing);
    int getPatternCount() const;
    int getEditingPattern() const { return editing.load(); }
    void selectPattern(int k);
    void addPattern(bool duplicate);
    void insertPattern(const grmg::Song& s);     // after the one being edited, and edit it
    void deletePattern();
    std::atomic<int> patternVersion {0}, songVersion {1};

    // ------------------------------------------------------------ solver thread
    void solveStep(double sr, unsigned seed, double tempo, bool loop, std::atomic<bool>& solving, std::atomic<int>& stage,
                   const std::function<bool()>& stop);
    void collect() {
        modelBox.collect();
        perfBox.collect();
    }
    std::shared_ptr<const grmg::Model> getShownModel() const {
        std::lock_guard<std::mutex> lock(shownLock);
        return shownModel;
    }

    // ------------------------------------------------------------ audio thread
    void prepare(double sr, int block);
    void ensure(int n) {                          // a host sending more than it announced
        if (dry.getNumSamples() >= n) return;
        dry.setSize(1, n, false, false, true);
        pickupBuf.setSize(1, n, false, false, true);
        out.setSize(2, n, false, false, true);
    }
    void begin(bool patternMode, bool internalPlaying);   // takes the newest networks and performances
    // MIDI for this guitar, at sample `at` of the block (render up to there first with renderTo)
    void midi(const juce::MidiMessage& msg);
    void releaseKeys();                           // the host stopped: patterns held by keys stop too
    void renderTo(int at, const SongClock& clock);
    // body (and the room if ownRoom) or pickup: the stereo `out`, before the mixer. The plugin shares one room
    // among the guitars that have the same one (ownRoom false): it is the heaviest part of the sound.
    void finish(int n, double sr, bool ownRoom);
    // which room this guitar is in (-1: none): guitars with the same key can share one room
    long roomKey() const;
    // a copy of this guitar's room response for the shared room (null once taken, until the next networks)
    juce::AudioBuffer<float>* roomShare();
    juce::AudioBuffer<float> out;                 // this block, stereo, at the output scale
    grmg::Engine engine;
    int playingPattern() const { return playing.load(); }
    double shownBeat() const { return engine.shown_beat.load(); }
    bool patternMode = false, keys = false;

    // interface -> audio thread: strings struck on the neck
    void pluckFromInterface(int string, int fret, float velocity);

private:
    juce::AudioProcessorValueTreeState& state;
    const struct {
        const std::atomic<float> *gainDb, *level, *pan, *mute, *solo, *ownOutput;
    } mixParams;
    const std::atomic<float>* const bodyAmount;
    const std::atomic<float>* const roomOn;

    // patterns, edited on the message thread, read by the solver
    mutable std::mutex songLock;
    std::vector<grmg::Song> patterns {grmg::Song::demo()};
    std::atomic<int> editing {0}, playing {-1};

    // solver state
    struct ModelHolder {
        std::shared_ptr<const grmg::Model> model;
        juce::AudioBuffer<float> body, room, roomShare;
    };
    ModelHolder* holder = nullptr;               // the networks in use (audio thread)
    static ModelHolder* makeHolder(std::shared_ptr<const grmg::Model> m);
    grmg::Mailbox<ModelHolder> modelBox;
    grmg::Mailbox<PerformanceSet> perfBox;
    grmg::Settings solvedFor, compiledPlayer;
    std::shared_ptr<const grmg::Model> model;
    grmg::Compiler compiler;
    int compiledSong = -1;
    unsigned compiledSeed = 0;
    double compiledTempo = 0.0, modelRate = 0.0;
    bool compiledLoop = false;
    const grmg::Model* compiledModel = nullptr;
    std::vector<std::string> compiledTexts;
    std::vector<std::shared_ptr<const grmg::Performance>> compiledPerfs;
    mutable std::mutex shownLock;
    std::shared_ptr<const grmg::Model> shownModel;

    // audio thread
    const PerformanceSet* perfSet = nullptr;
    unsigned loadedBody = 0, loadedRoom = 0;
    juce::dsp::Convolution bodyConv {juce::dsp::Convolution::NonUniform {512}};
    juce::dsp::Convolution roomConv {juce::dsp::Convolution::NonUniform {512}};
    bool roomLoaded = false;
    juce::AudioBuffer<float> dry, pickupBuf;
    juce::SmoothedValue<float> bodyMix;
    grmg::Biquad pickupFilter;
    double filterFr = -1.0, filterQ = -1.0;
    int heldKey = -1, heldPattern = -1, shownPattern = -1;
    double patternBeat = 0.0;
    int pos = 0;

    struct UiPluck { int string, fret; float velocity; };
    juce::AbstractFifo uiFifo {64};
    std::array<UiPluck, 64> uiPlucks {};
};
