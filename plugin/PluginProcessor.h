// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
#pragma once

#include <array>
#include <limits>
#include <atomic>
#include <memory>
#include <mutex>
#include <thread>

#include <JuceHeader.h>

#include "GuitarTrack.h"
#include "SolverThread.h"
#include "../core/offline.hpp"
#include "../models/preset.hpp"

// The plugin: several guitars (GuitarTrack), a mixer (each guitar's gain, level, pan, mute, solo and output),
// a main stereo output and one stereo output per guitar slot (the host enables the ones it wants).
class GRMGuitarProcessor : public juce::AudioProcessor
{
public:
	static constexpr int MAX_GUITARS = 8;
	static constexpr int MAX_PATTERNS = GuitarTrack::MAX_PATTERNS, PATTERN_KEY = GuitarTrack::PATTERN_KEY;

	GRMGuitarProcessor();
	~GRMGuitarProcessor() override;

	void prepareToPlay(double sampleRate, int samplesPerBlock) override;
	void releaseResources() override {}
	bool isBusesLayoutSupported(const BusesLayout &layouts) const override;
	void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;

	juce::AudioProcessorEditor *createEditor() override;
	bool hasEditor() const override { return true; }

	const juce::String getName() const override { return "GRM Guitar"; }
	bool acceptsMidi() const override { return true; }
	bool producesMidi() const override { return false; }
	bool isMidiEffect() const override { return false; }
	// the strings may ring as long as the hands let them, and the plugin plays its own roll: an endless tail tells
	// the host never to put it to sleep (some hosts stop calling an instrument that gets no input)
	double getTailLengthSeconds() const override { return std::numeric_limits<double>::infinity(); }

	int getNumPrograms() override { return 1; }
	int getCurrentProgram() override { return 0; }
	void setCurrentProgram(int) override {}
	const juce::String getProgramName(int) override { return {}; }
	void changeProgramName(int, const juce::String &) override {}

	void getStateInformation(juce::MemoryBlock &destData) override;
	void setStateInformation(const void *data, int sizeInBytes) override;

	juce::AudioProcessorValueTreeState params;

	// ------------------------------------------------------------ the guitars (message thread)
	int getGuitarCount() const { return guitarCount.load(); }
	int getSelectedGuitar() const { return selected.load(); }
	void selectGuitar(int g);
	void addGuitar();		  // a new guitar (the next free slot), selected
	void removeGuitar(int g); // never the last one
	juce::String getGuitarName(int g) const;

	// presets: the sound of guitar g, every parameter but its mixer strip (applied: one the preset does not name goes
	// to its default), and the name of the preset it was last given (empty: none)
	grmg::models::Preset capturePreset(int g, const std::string &name) const;
	void applyPreset(int g, const grmg::models::Preset &preset);
	juce::String getPresetName(int g) const;
	void setPresetName(int g, const juce::String &name);
	int getGuitarVersion() const { return guitarVersion.load(); } // changes when guitars are added, removed, chosen
	GuitarTrack &guitar(int g) { return *tracks[(size_t)juce::jlimit(0, MAX_GUITARS - 1, g)]; }
	const GuitarTrack &guitar(int g) const { return *tracks[(size_t)juce::jlimit(0, MAX_GUITARS - 1, g)]; }
	GuitarTrack &current() { return guitar(selected.load()); }
	const GuitarTrack &current() const { return guitar(selected.load()); }
	juce::String paramId(const juce::String &id) const { return GuitarTrack::paramId(selected.load(), id); }

	// ------------------------------------------------------------ the guitar roll of the selected guitar
	grmg::Song getSong() const { return current().getSong(); }
	void setSong(const grmg::Song &s);
	void newTake();
	unsigned getSeed() const { return seed.load(); }
	int getStateVersion() const { return stateVersion.load(); } // changes when the host loads a state
	int getPatternCount() const { return current().getPatternCount(); }
	int getEditingPattern() const { return current().getEditingPattern(); }
	void selectPattern(int k) { current().selectPattern(k); }
	void addPattern(bool duplicate);
	void deletePattern();
	int getPatternVersion() const { return current().patternVersion.load() + guitarVersion.load() * 1000; }
	int getPlayingPattern() const { return current().playingPattern(); }
	static juce::String patternKeyName(int k) { return juce::MidiMessage::getMidiNoteName(PATTERN_KEY + k, true, true, 3); }
	// a pattern copied from any guitar, pasted into any other (as a new pattern after the one being edited)
	// notes and strums added to another guitar's pattern being edited (copy to guitar)
	void addToGuitar(int g, const grmg::Song &items);
	double getSongBeat() const { return songBeat.load(); } // where the song is (host or Play), -1 stopped
	void copyPattern();
	bool pastePattern();
	bool hasCopiedPattern() const;

	// internal transport (standalone, or when the host is stopped)
	void setInternalPlaying(bool on);
	bool isInternalPlaying() const { return internalPlaying.load(); }
	void setInternalPosition(double beat) { internalSeek.store(beat); }
	bool isHostPlaying() const { return hostPlaying.load(); }
	double getShownBeat() const { return current().shownBeat(); }
	double getTempo() const { return currentTempo.load(); }

	// a string struck from the interface (the neck): the selected guitar
	void pluckFromInterface(int string, int fret, float velocity) { current().pluckFromInterface(string, fret, velocity); }

	// what the interface shows (the selected guitar)
	int getShownFret(int i) const { return current().engine.shown_fret[(size_t)i].load(); }
	float getShownLevel(int i) const { return current().engine.shown_level[(size_t)i].load(); }
	int readScope(float *dest, int count); // the latest samples of the main output
	juce::String getStatus() const;
	int getOpenMidi(int i) const;
	models::Settings readSettings() const { return current().readSettings(); }
	std::shared_ptr<const grmg::Model> getShownModel() const { return current().getShownModel(); }

	// the whole song (every guitar, mixed), rendered offline with bodies and rooms, to a WAV file
	void exportSong(const juce::File &file);
	bool isExporting() const { return exporting.load(); }

	void setPersistentSongKey(int k) { persistentKey.store(k); }
	int getPersistentSongKey() const { return persistentKey.load(); }
	void setPersistentSongScale(int s) { persistentScale.store(s); }
	int getPersistentSongScale() const { return persistentScale.load(); }

	void setPersistentChord(int r, int q)
	{
		persistentChordRoot.store(r);
		persistentChordQuality.store(q);
	}
	int getPersistentChordRoot() const { return persistentChordRoot.load(); }
	int getPersistentChordQuality() const { return persistentChordQuality.load(); }

	std::atomic<int> persistentKey{0};
	std::atomic<int> persistentScale{0};
	std::atomic<int> persistentChordRoot{0};
	std::atomic<int> persistentChordQuality{0};
	std::atomic<int> persistentSnapIndex{4};
	std::atomic<int> persistentStrumPattern{2};
	std::atomic<int> persistentArpPattern{2};
	std::atomic<int> persistentFillCount{1};

private:
	friend class SolverThread;
	static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
	static BusesProperties buses();

	std::array<std::unique_ptr<GuitarTrack>, MAX_GUITARS> tracks;
	std::atomic<int> guitarCount{1}, selected{0}, guitarVersion{0};
	SolverThread solver{*this};

	std::atomic<unsigned> seed{7};
	std::atomic<int> stateVersion{0};
	std::atomic<double> sampleRate{0.0};
	std::atomic<double> currentTempo{90.0};
	std::atomic<bool> solving{false};
	std::atomic<int> solvedStage{0}; // 0 nothing, 1 strings, 2 everything

	std::atomic<bool> exporting{false};
	std::thread exportThread;
	juce::String exportResult;
	mutable std::mutex exportLock;
	mutable std::mutex clipLock;
	juce::String copiedPattern;

	// the shared rooms: one convolution for all the guitars in the same room going to the same output
	struct RoomGroup
	{
		long key = -1;
		int bus = 0, idle = 0;
		bool used = false;
		std::unique_ptr<juce::dsp::Convolution> conv;
		juce::AudioBuffer<float> buf;
	};
	std::array<RoomGroup, MAX_GUITARS> rooms;
	mutable std::mutex presetLock;
	std::array<juce::String, MAX_GUITARS> presetNames;
	std::array<int, MAX_GUITARS> roomOf{}; // per guitar this block: its shared room, -1 none, -2 its own
	// a guitar that changes rooms sends to the old one less and less and to the new one more and more, over
	// ROOM_SWITCH_S (no step in either): its room of the last block, the one it leaves, the samples left of the fade
	std::array<int, MAX_GUITARS> roomBefore{}, roomLeaving{}, roomFadeLeft{};

	// mixer (audio thread)
	const std::atomic<float> *const volumeParam = params.getRawParameterValue("volume");
	juce::SmoothedValue<float> volume;
	std::array<juce::SmoothedValue<float>, MAX_GUITARS> stripGain, stripPan;
	std::array<float, MAX_GUITARS + 1> limiterGain{}; // per output bus

	// transport
	std::atomic<bool> internalPlaying{false}, hostPlaying{false};
	std::atomic<double> internalSeek{-1.0};
	double internalStart = 0.0, internalElapsed = 0.0;
	bool hostWasPlaying = false;
	std::atomic<double> songBeat{-1.0};

	// audio thread -> interface: the oscilloscope
	juce::AbstractFifo scopeFifo{8192};
	std::array<float, 8192> scopeData{};

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GRMGuitarProcessor)
};
