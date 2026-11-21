// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
#include "PluginProcessor.h"

#include "PluginEditor.h"

namespace {

	juce::StringArray tuningNames() {
		juce::StringArray a;
		for (const auto& t : grmg::TUNINGS) a.add(t.name);
		return a;
	}
	juce::StringArray roomNames() {
		juce::StringArray a;
		for (const auto& r : grmg::ROOMS) a.add(r.name);
		return a;
	}
	juce::StringArray materialNames() {
		juce::StringArray a;
		for (const auto& m : grmg::MATERIALS) a.add(m.name);
		return a;
	}
	juce::StringArray woodNames() {
		juce::StringArray a;
		for (const auto& w : grmg::WOODS) a.add(w.name);
		return a;
	}
	juce::StringArray pickupNames() {
		juce::StringArray a;
		for (const auto& t : grmg::PICKUP_TYPES) a.add(t.name);
		return a;
	}

	// a guitar's parameters, in the order of the layout (copied from slot to slot when a guitar is removed)
	const char* const GUITAR_PARAMS[] = {
		"tuning", "string_age", "brightness", "core", "stiff", "attack", "pluck", "force", "pick", "palm_mm",
		"palm_intensity", "live_palm", "body_size", "top_mm", "coupling", "braces_g", "body_q", "body_amount", "wood",
		"density_pct", "hollow", "sensor", "pickup_mm", "pickup_type", "cable_pf", "sympathy", "room_on", "room_size",
		"room_material", "room_distance", "human", "human_amount", "swing_cm", "finger_noises", "finger_force", "mix_gain", "mix_level",
		"mix_pan", "mix_mute", "mix_solo", "mix_out" };

	float dbToGain(float db) { return std::pow(10.0f, db / 20.0f); }

	// The WAV writer finalizes its header in its destructor and ignores individual header write results.
	// Keep the file stream open so the caller can check those writes and flush after finalization.
	class CheckedWavStream final : public juce::OutputStream {
	public:
		CheckedWavStream(juce::FileOutputStream& stream, bool& failed) : file(stream), writeFailed(failed) {}

		juce::int64 getPosition() override { return file.getPosition(); }
		bool setPosition(juce::int64 position) override { return check(file.setPosition(position)); }
		bool write(const void* data, size_t size) override { return check(file.write(data, size)); }
		void flush() override {
			file.flush();
			check(file.getStatus().wasOk());
		}

	private:
		bool check(bool ok) {
			writeFailed |= !ok;
			return ok;
		}

		juce::FileOutputStream& file;
		bool& writeFailed;
	};

}  // namespace

// ================================================================== the solver thread
void SolverThread::run() {
	while (!threadShouldExit()) {
		const double sr = owner.sampleRate.load();
		if (sr <= 0) {
			wait(50);
			continue;
		}
		const unsigned sd = owner.seed.load();
		const double tempo = owner.currentTempo.load();
		const bool loop = owner.params.getRawParameterValue("loop")->load() > 0.5f;
		// the selected guitar first: it is the one being listened to while its settings move
		const int count = owner.guitarCount.load(), first = owner.selected.load();
		for (int k = 0; k < count && !threadShouldExit(); ++k) {
			const int g = (first + k) % count;
			owner.tracks[(size_t)g]->solveStep(sr, sd, tempo, loop, owner.solving, owner.solvedStage,
				[this] { return threadShouldExit(); });
		}
		for (int g = count; g < GRMGuitarProcessor::MAX_GUITARS; ++g) owner.tracks[(size_t)g]->collect();
		wait(30);
	}
}

// ================================================================== the processor
// the main output, and one output per guitar slot (off until the host enables it)
juce::AudioProcessor::BusesProperties GRMGuitarProcessor::buses() {
	auto b = BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true);
	for (int g = 0; g < MAX_GUITARS; ++g) b = b.withOutput("Guitar " + juce::String(g + 1), juce::AudioChannelSet::stereo(), false);
	return b;
}

GRMGuitarProcessor::GRMGuitarProcessor() : AudioProcessor(buses()), params(*this, nullptr, "GRMGuitar", createLayout()) {
	for (int g = 0; g < MAX_GUITARS; ++g) tracks[(size_t)g] = std::make_unique<GuitarTrack>(params, g);
	limiterGain.fill(1.0f);
	solver.startThread();
}

GRMGuitarProcessor::~GRMGuitarProcessor() {
	solver.stopThread(4000);
	if (exportThread.joinable()) exportThread.join();
}

juce::AudioProcessorValueTreeState::ParameterLayout GRMGuitarProcessor::createLayout() {
	using namespace juce;
	std::vector<std::unique_ptr<RangedAudioParameter>> p;
	String prefix, idPrefix;    // the guitar the next parameters belong to ("G2 ", "g2_"; empty for guitar 1)
	// centre: the value at the middle of the slider (0: linear)
	auto real = [&](const char* id, const char* name, float lo, float hi, float def, float centre = 0.0f, float step = 0.0f) {
		NormalisableRange<float> r(lo, hi, step);
		if (centre > lo && centre < hi) r.setSkewForCentre(centre);
		const int decimals = hi - lo > 50 ? 0 : hi - lo > 5 ? 1 : 2;
		p.push_back(std::make_unique<AudioParameterFloat>(
			ParameterID{ idPrefix + id, 1 }, prefix + name, r, def,
			AudioParameterFloatAttributes().withStringFromValueFunction([decimals](float v, int) { return String(v, decimals); })));
		};
	auto flag = [&](const char* id, const char* name, bool def) {
		p.push_back(std::make_unique<AudioParameterBool>(ParameterID{ idPrefix + id, 1 }, prefix + name, def));
		};
	auto choice = [&](const char* id, const char* name, const StringArray& c, int def) {
		p.push_back(std::make_unique<AudioParameterChoice>(ParameterID{ idPrefix + id, 1 }, prefix + name, c, def));
		};
	for (int g = 0; g < MAX_GUITARS; ++g) {
		prefix = g == 0 ? String() : "G" + String(g + 1) + " ";
		idPrefix = g == 0 ? String() : "g" + String(g + 1) + "_";
		// strings
		choice("tuning", "Tuning", tuningNames(), 0);
		real("string_age", "String age (0 new, 1 played)", 0.0f, 1.0f, 0.0f);
		real("brightness", "Brightness", 0.0f, 1.0f, 0.6f);
		real("core", "Wound core", 0.2f, 1.0f, 0.45f);
		flag("stiff", "Stiff strings", true);
		// attack
		choice("attack", "Attack", StringArray{ "Pick", "Fingers" }, 0);
		real("pluck", "Pluck point", 0.05f, 0.5f, 0.22f);
		real("force", "Pluck force (N)", 0.3f, 6.0f, 2.0f, 1.5f);
		real("pick", "Pick (mm)", 0.4f, 1.5f, 0.71f);
		// palm
		real("palm_mm", "Palm from bridge (mm)", 5.0f, 80.0f, 30.0f);
		real("palm_intensity", "Palm pressure (0: lifted)", 0.0f, 4.0f, 1.0f, 0.3f);
		flag("live_palm", "Palm mute (live)", false);
		// body
		real("body_size", "Body size", 0.8f, 1.25f, 1.0f);
		real("top_mm", "Top (mm)", 2.0f, 4.0f, 2.8f);
		real("coupling", "Air / top coupling", 0.1f, 0.6f, 0.35f);
		real("braces_g", "Braces (g)", 10.0f, 200.0f, 64.0f, 64.0f, 1.0f);
		real("body_q", "Body resonance (Q)", 10.0f, 150.0f, 50.0f, 50.0f, 1.0f);
		real("body_amount", "Body", 0.0f, 1.0f, 0.85f);
		choice("wood", "Wood", woodNames(), 0);
		real("density_pct", "Wood density (% of the species)", -30.0f, 30.0f, 0.0f, 0.0f, 1.0f);
		real("hollow", "Body: solid (0) to hollow (1)", 0.0f, 1.0f, 1.0f);
		// sensor
		choice("sensor", "Sensor", StringArray{ "Microphone (body and room)", "Magnetic pickup", "Piezo under the saddle" }, 0);
		real("pickup_mm", "Pickup from the saddle (mm)", 20.0f, 200.0f, 100.0f, 0.0f, 1.0f);
		choice("pickup_type", "Pickup", pickupNames(), 0);
		real("cable_pf", "Cable (pF)", 100.0f, 1500.0f, 500.0f, 500.0f, 10.0f);
		flag("sympathy", "Sympathetic strings", true);
		// room
		flag("room_on", "Room", true);
		choice("room_size", "Room size", roomNames(), 1);
		choice("room_material", "Walls", materialNames(), 2);
		real("room_distance", "Listener (m)", 0.3f, 6.0f, 1.0f, 1.5f);
		// player
		flag("human", "Human feel", true);
		real("human_amount", "Looseness (1: default)", 0.0f, 3.0f, 1.0f, 1.0f);
		real("swing_cm", "Arm swing (cm)", 4.0f, 20.0f, 10.0f);
		flag("finger_noises", "Finger noises", true);
		real("finger_force", "Finger force (N)", 0.1f, 2.0f, 0.6f, 0.6f);
		// mixer strip
		real("mix_gain", "Gain (dB)", -24.0f, 24.0f, 0.0f, 0.0f, 0.1f);
		real("mix_level", "Level", 0.0f, 2.0f, 1.0f, 1.0f);
		real("mix_pan", "Pan", -1.0f, 1.0f, 0.0f);
		flag("mix_mute", "Mute", false);
		flag("mix_solo", "Solo", false);
		choice("mix_out", "Output", StringArray{ "Main", "Own output" }, 0);
	}
	prefix = idPrefix = String();
	// the plugin
	real("volume", "Volume", 0.0f, 2.0f, 1.0f, 1.0f);
	choice("midi_strings", "MIDI to strings", StringArray{ "Lowest fret", "Channel = string" }, 0);
	choice("midi_route", "MIDI to guitars", StringArray{ "Selected guitar", "Channel = guitar" }, 0);
	flag("loop", "Loop the roll", true);
	flag("follow_host", "Follow the host", true);
	choice("midi_mode", "MIDI keys", StringArray{ "Play notes", "Play patterns" }, 0);
	flag("follow_playhead", "Roll follows the playhead", true);
	flag("ghosts", "Show the other guitars", true);
	flag("audition", "Hear the notes written", true);
	real("tempo", "Tempo (standalone)", 40.0f, 220.0f, 90.0f, 1.0f, 1.0f);
	return { p.begin(), p.end() };
}

// ================================================================== guitars
void GRMGuitarProcessor::selectGuitar(int g) {
	g = juce::jlimit(0, guitarCount.load() - 1, g);
	if (g == selected.load()) return;
	selected.store(g);
	guitarVersion.fetch_add(1);
	solver.notify();
}

void GRMGuitarProcessor::addGuitar() {
	const int count = guitarCount.load();
	if (count >= MAX_GUITARS) return;
	// a fresh slot: its parameters at their defaults, one empty pattern
	for (auto* id : GUITAR_PARAMS)
		if (auto* p = params.getParameter(GuitarTrack::paramId(count, id))) p->setValueNotifyingHost(p->getDefaultValue());
	grmg::Song empty;
	empty.bars = 4;
	tracks[(size_t)count]->setPatterns({ empty }, 0);
	guitarCount.store(count + 1);
	selected.store(count);
	guitarVersion.fetch_add(1);
	solver.notify();
}

void GRMGuitarProcessor::removeGuitar(int g) {
	const int count = guitarCount.load();
	if (count <= 1 || g < 0 || g >= count) return;
	// the guitars after it move down one slot: their settings and patterns with them
	for (int k = g; k + 1 < count; ++k) {
		for (auto* id : GUITAR_PARAMS) {
			auto* to = params.getParameter(GuitarTrack::paramId(k, id));
			auto* from = params.getParameter(GuitarTrack::paramId(k + 1, id));
			if (to && from) to->setValueNotifyingHost(from->getValue());
		}
		tracks[(size_t)k]->setPatterns(tracks[(size_t)k + 1]->getPatterns(), tracks[(size_t)k + 1]->getEditingPattern());
	}
	guitarCount.store(count - 1);
	selected.store(juce::jmin(selected.load(), count - 2));
	guitarVersion.fetch_add(1);
	solver.notify();
}

juce::String GRMGuitarProcessor::getGuitarName(int g) const { return "Guitar " + juce::String(g + 1); }

// ================================================================== the roll of the selected guitar
void GRMGuitarProcessor::setSong(const grmg::Song& s) {
	current().setSong(s);
	solver.notify();
}

void GRMGuitarProcessor::addPattern(bool duplicate) {
	current().addPattern(duplicate);
	solver.notify();
}

void GRMGuitarProcessor::deletePattern() {
	current().deletePattern();
	solver.notify();
}

void GRMGuitarProcessor::addToGuitar(int g, const grmg::Song& items) {
	if (g < 0 || g >= guitarCount.load()) return;
	auto& t = *tracks[(size_t)g];
	auto song = t.getSong();
	for (auto n : items.notes) {
		n.selected = false;
		song.notes.push_back(n);
	}
	for (auto st : items.strums) {
		st.selected = false;
		song.strums.push_back(st);
	}
	song.sort();
	t.setSong(song);
	solver.notify();
}

void GRMGuitarProcessor::copyPattern() {
	const auto text = juce::String(current().getSong().to_text());
	{
		std::lock_guard<std::mutex> lock(clipLock);
		copiedPattern = text;
	}
	juce::SystemClipboard::copyTextToClipboard(text);   // another instance can paste it too
}

bool GRMGuitarProcessor::pastePattern() {
	juce::String text = juce::SystemClipboard::getTextFromClipboard();
	if (!text.startsWith("grm-guitar-roll")) {
		std::lock_guard<std::mutex> lock(clipLock);
		text = copiedPattern;
	}
	grmg::Song s;
	if (text.isEmpty() || !s.from_text(text.toStdString())) return false;
	for (auto& n : s.notes) n.selected = false;
	for (auto& t : s.strums) t.selected = false;
	current().insertPattern(s);
	solver.notify();
	return true;
}

bool GRMGuitarProcessor::hasCopiedPattern() const {
	std::lock_guard<std::mutex> lock(clipLock);
	return copiedPattern.isNotEmpty();
}

void GRMGuitarProcessor::newTake() {
	seed.store((unsigned)juce::Random::getSystemRandom().nextInt(1 << 30) + 1u);
	solver.notify();
}

void GRMGuitarProcessor::setInternalPlaying(bool on) {
	if (on && !internalPlaying.load()) internalSeek.store(0.0);
	internalPlaying.store(on);
}

int GRMGuitarProcessor::getOpenMidi(int i) const {
	const int t = juce::jlimit(0, grmg::NUM_TUNINGS - 1, (int)std::lround(current().param("tuning")));
	return grmg::TUNINGS[t].midi[i];
}

juce::String GRMGuitarProcessor::getStatus() const {
	if (exporting.load()) return "Rendering the song to a WAV file...";
	{
		std::lock_guard<std::mutex> lock(exportLock);
		if (exportResult.isNotEmpty()) return exportResult;
	}
	if (sampleRate.load() <= 0) return "Waiting for the audio device";
	if (solving.load()) return solvedStage.load() >= 1 ? "Solving the palm and the room networks..." : "Solving the networks...";
	return "Networks solved";
}

// ================================================================== export: every guitar, mixed
void GRMGuitarProcessor::exportSong(const juce::File& file) {
	if (exporting.exchange(true)) return;
	if (exportThread.joinable()) exportThread.join();
	struct Part { grmg::Settings settings; grmg::Song song; float gain, pan; int index; };
	std::vector<Part> parts;
	bool solo = false;
	for (int g = 0; g < guitarCount.load(); ++g) solo |= tracks[(size_t)g]->param("mix_solo") > 0.5f;
	for (int g = 0; g < guitarCount.load(); ++g) {
		const auto& t = *tracks[(size_t)g];
		if (t.param("mix_mute") > 0.5f || (solo && t.param("mix_solo") < 0.5f)) continue;
		parts.push_back({ t.readSettings(), t.getSong(), dbToGain(t.param("mix_gain")) * t.param("mix_level"), t.param("mix_pan"), g });
	}
	const double tempo = currentTempo.load();
	const unsigned sd = seed.load();
	exportThread = std::thread([this, parts, tempo, sd, file] {
		juce::String result;
		try {
			if (parts.empty()) throw std::runtime_error("No audible guitars to export");
			const double sr = 48000.0;
			std::array<std::vector<double>, 2> mix;
			for (const auto& part : parts) {
				auto model = std::make_shared<grmg::Model>();
				model->solve(part.settings, sr, nullptr, true);
				grmg::Compiler compiler;
				grmg::CompileOptions o;
				o.tempo = tempo;
				o.seed = sd + 7919u * (unsigned)part.index;
				auto perf = compiler.compile(part.song, model, o);
				grmg::Engine e;
				e.prepare(sr);
				e.set_model(model.get());
				e.set_performance(perf.get());
				grmg::Transport tr;
				tr.playing = true;
				tr.loop = false;
				tr.beats_per_sample = tempo / 60.0 / sr;
				const size_t total = (size_t)((part.song.length_beats() * 60.0 / tempo + 4.0) * sr);
				std::vector<float> out(total, 0.0f), heard(total, 0.0f);
				for (size_t pos = 0; pos < total; pos += 512) {
					tr.beat = (double)pos * tr.beats_per_sample;
					e.render(out.data() + pos, (int)std::min<size_t>(512, total - pos), tr, heard.data() + pos);
				}
				const auto stereo = grmg::sensor_output(std::vector<double>(out.begin(), out.end()),
					std::vector<double>(heard.begin(), heard.end()), *model);
				const double l = std::min(1.0, 1.0 - part.pan) * part.gain, r = std::min(1.0, 1.0 + part.pan) * part.gain;
				for (int ch = 0; ch < 2; ++ch) {
					auto& m = mix[(size_t)ch];
					if (m.size() < stereo[(size_t)ch].size()) m.resize(stereo[(size_t)ch].size(), 0.0);
					for (size_t t = 0; t < stereo[(size_t)ch].size(); ++t) m[t] += (ch ? r : l) * stereo[(size_t)ch][t];
				}
			}
			double peak = 1e-12;
			for (const auto& c : mix)
				for (double v : c) peak = std::max(peak, std::abs(v));
			juce::AudioBuffer<float> buf(2, (int)mix[0].size());
			for (int ch = 0; ch < 2; ++ch)
				for (int t = 0; t < buf.getNumSamples(); ++t) buf.setSample(ch, t, (float)(mix[(size_t)ch][(size_t)t] / peak * 0.89));
			juce::TemporaryFile temporary(file);
			{
				// Unbuffered writes also report short writes while finalizing the header.
				juce::FileOutputStream output(temporary.getFile(), 0);
				if (!output.openedOk()) throw std::runtime_error("Could not open temporary WAV");

				bool writeFailed = false;
				std::unique_ptr<juce::OutputStream> stream = std::make_unique<CheckedWavStream>(output, writeFailed);
				juce::WavAudioFormat wav;
				auto writer = wav.createWriterFor(stream, juce::AudioFormatWriterOptions{}.withSampleRate(sr).withNumChannels(2).withBitsPerSample(24));
				if (!writer || !writer->writeFromAudioSampleBuffer(buf, 0, buf.getNumSamples()))
					throw std::runtime_error("Could not write WAV data");

				writer.reset();
				output.flush();
				if (writeFailed || output.getStatus().failed()) throw std::runtime_error("Could not finalize WAV data");
			}
			if (!temporary.overwriteTargetFileWithTemporary()) throw std::runtime_error("Could not replace destination WAV");
			result = "Exported " + file.getFileName();
		}
		catch (const std::exception& ex) {
			result = juce::String("Export failed: ") + ex.what();
		}
		{
			std::lock_guard<std::mutex> lock(exportLock);
			exportResult = result;
		}
		exporting.store(false);
		});
}

// ================================================================== audio
void GRMGuitarProcessor::prepareToPlay(double sr, int samplesPerBlock) {
	for (auto& t : tracks) t->prepare(sr, samplesPerBlock);
	for (auto& r : rooms) {
		r.conv = std::make_unique<juce::dsp::Convolution>(juce::dsp::Convolution::NonUniform{ 512 });
		r.conv->prepare({ sr, (juce::uint32)samplesPerBlock, 2 });
		r.buf.setSize(2, samplesPerBlock);
		r.key = -1;
	}
	volume.reset(sr, 0.05);
	for (int g = 0; g < MAX_GUITARS; ++g) {
		stripGain[(size_t)g].reset(sr, 0.05);
		stripPan[(size_t)g].reset(sr, 0.05);
	}
	sampleRate.store(sr);
	solver.notify();
}

bool GRMGuitarProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
	const auto main = layouts.getMainOutputChannelSet();
	if (main != juce::AudioChannelSet::mono() && main != juce::AudioChannelSet::stereo()) return false;
	for (int b = 1; b < layouts.outputBuses.size(); ++b) {
		const auto& set = layouts.outputBuses.getReference(b);
		if (!set.isDisabled() && set != juce::AudioChannelSet::stereo()) return false;
	}
	return true;
}

void GRMGuitarProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
	juce::ScopedNoDenormals noDenormals;
	const int n = buffer.getNumSamples();
	const double sr = getSampleRate();
	const int count = guitarCount.load();
	const bool patternMode = params.getRawParameterValue("midi_mode")->load() > 0.5f;
	const bool playingOwn = internalPlaying.load();

	// ---------------------------------------------------------------- where the song is (one clock for every guitar)
	SongClock clock;
	clock.loop = params.getRawParameterValue("loop")->load() > 0.5f;
	double tempo = params.getRawParameterValue("tempo")->load();
	if (params.getRawParameterValue("follow_host")->load() > 0.5f)
		if (auto* ph = getPlayHead())
			if (auto pos = ph->getPosition()) {
				if (auto bpm = pos->getBpm()) tempo = *bpm;
				if (pos->getIsPlaying())
					if (auto ppq = pos->getPpqPosition()) {
						clock.host = true;
						clock.playing = true;
						clock.beat = *ppq;
					}
			}
	if (patternMode && !playingOwn) clock.host = clock.playing = false;   // the keys play the patterns, not the timeline
	hostPlaying.store(clock.host);
	tempo = juce::jlimit(20.0, 400.0, tempo);
	if (std::abs(tempo - currentTempo.load()) > 0.01) currentTempo.store(tempo);
	clock.beats_per_sample = tempo / 60.0 / sr;
	if (!clock.host) {
		const double seek = internalSeek.exchange(-1.0);
		if (seek >= 0) {
			internalStart = seek;
			internalElapsed = 0.0;
		}
		clock.playing = playingOwn;
		clock.start = internalStart;
		clock.beat = internalElapsed;
	}

	// ---------------------------------------------------------------- every guitar: newest networks, MIDI, strings
	for (int g = 0; g < count; ++g) {
		tracks[(size_t)g]->ensure(n);
		tracks[(size_t)g]->begin(patternMode, playingOwn);
	}
	// the host stopped: what MIDI keys were holding stops too (some hosts send no note-off on stop)
	bool hostNow = false;
	if (auto* ph = getPlayHead())
		if (auto pos = ph->getPosition()) hostNow = pos->getIsPlaying();
	if (hostWasPlaying && !hostNow)
		for (int g = 0; g < count; ++g) tracks[(size_t)g]->releaseKeys();
	hostWasPlaying = hostNow;

	// Suivi précis du nombre d'échantillons déjà rendus par guitare dans ce bloc
	std::array<int, MAX_GUITARS> renderedSample{};

	// Calcule la position temporelle exacte (beat) au début précis de chaque sous-tranche
	auto clockForTrack = [&](int g) {
		SongClock c = clock;
		c.beat = clock.beat + (double)renderedSample[(size_t)g] * clock.beats_per_sample;
		return c;
		};

	// where MIDI goes: by channel (channel k: guitar k), or to the selected guitar; in "Play patterns" a key starts
	// its pattern on every guitar that has one (a scene), unless the channels choose the guitar
	const bool byChannel = params.getRawParameterValue("midi_route")->load() > 0.5f;
	const int sel = juce::jlimit(0, count - 1, selected.load());
	for (const auto meta : midi) {
		const auto msg = meta.getMessage();
		const int at = juce::jlimit(0, n, meta.samplePosition);
		if (patternMode && !playingOwn && !byChannel) {
			for (int g = 0; g < count; ++g) {
				const int renderTarget = juce::jmax(renderedSample[(size_t)g], at);
				tracks[(size_t)g]->renderTo(renderTarget, clockForTrack(g));
				renderedSample[(size_t)g] = renderTarget;
				tracks[(size_t)g]->midi(msg);
			}
			continue;
		}
		const int target = byChannel && msg.getChannel() >= 1 && msg.getChannel() <= count ? msg.getChannel() - 1 : sel;
		auto& t = *tracks[(size_t)target];
		const int renderTarget = juce::jmax(renderedSample[(size_t)target], at);
		t.renderTo(renderTarget, clockForTrack(target));
		renderedSample[(size_t)target] = renderTarget;
		t.midi(msg);
	}

	// the rooms: guitars in the same room, going to the same output, share one (it costs as much as the strings)
	for (auto& r : rooms) {
		r.used = false;
		if (r.buf.getNumSamples() < n) r.buf.setSize(2, n, false, false, true);
		r.buf.clear();
	}
	const int tailBlocks = (int)(4.0 * sr / juce::jmax(1, n));       // a room rings on after its guitars stop
	for (int g = 0; g < count; ++g) {
		auto& t = *tracks[(size_t)g];
		roomOf[(size_t)g] = -1;
		const long key = t.roomKey();
		if (key < 0) continue;
		const int bus = t.param("mix_out") > 0.5f ? g + 1 : 0;
		int slot = -1;
		for (int k = 0; k < MAX_GUITARS && slot < 0; ++k)
			if (rooms[(size_t)k].key == key && rooms[(size_t)k].bus == bus) slot = k;
		if (slot < 0)                                // a free room (or one whose tail has died out)
			for (int k = 0; k < MAX_GUITARS && slot < 0; ++k)
				if (rooms[(size_t)k].conv && (rooms[(size_t)k].key < 0 || rooms[(size_t)k].idle > tailBlocks))
					if (auto* share = t.roomShare()) {
						rooms[(size_t)k].conv->loadImpulseResponse(std::move(*share), sr, juce::dsp::Convolution::Stereo::yes,
							juce::dsp::Convolution::Trim::no,
							juce::dsp::Convolution::Normalise::no);
						share->setSize(0, 0);
						rooms[(size_t)k].conv->reset();
						rooms[(size_t)k].key = key;
						rooms[(size_t)k].bus = bus;
						rooms[(size_t)k].idle = 0;
						slot = k;
					}
		roomOf[(size_t)g] = slot >= 0 ? slot : -2;   // no room to share yet: its own
		if (slot >= 0) rooms[(size_t)slot].used = true;
	}

	for (int g = 0; g < count; ++g) {
		tracks[(size_t)g]->renderTo(n, clockForTrack(g));
		renderedSample[(size_t)g] = n;
		tracks[(size_t)g]->finish(n, sr, roomOf[(size_t)g] == -2);
	}
	songBeat.store(clock.playing ? (clock.host ? clock.beat : clock.start + clock.beat) : -1.0);
	if (!clock.host && clock.playing) {
		internalElapsed += n * clock.beats_per_sample;
		if (!clock.loop) {                        // the end of the longest guitar, and 4 s of ringing
			double longest = 0.0;
			for (int g = 0; g < count; ++g)
				if (const auto* p = tracks[(size_t)g]->engine.performance()) longest = std::max(longest, p->length_beats);
			if (internalStart + internalElapsed > longest + 4.0 * tempo / 60.0) internalPlaying.store(false);
		}
	}

	// ---------------------------------------------------------------- the mixer: each guitar to its output
	for (int ch = 0; ch < buffer.getNumChannels(); ++ch) buffer.clear(ch, 0, n);
	bool solo = false;
	for (int g = 0; g < count; ++g) solo |= tracks[(size_t)g]->param("mix_solo") > 0.5f;
	// an output the host enabled and gave channels for (a host may pass fewer channels than the layout says)
	auto usable = [&](int b) {
		auto* bus = getBus(false, b);
		return bus && bus->isEnabled() && bus->getNumberOfChannels() > 0
			&& getChannelIndexInProcessBlockBuffer(false, b, bus->getNumberOfChannels() - 1) < buffer.getNumChannels();
		};
	auto mainBus = getBusBuffer(buffer, false, 0);
	for (int g = 0; g < count; ++g) {
		auto& t = *tracks[(size_t)g];
		const bool silent = t.param("mix_mute") > 0.5f || (solo && t.param("mix_solo") < 0.5f);
		stripGain[(size_t)g].setTargetValue(silent ? 0.0f : dbToGain(t.param("mix_gain")) * t.param("mix_level"));
		stripPan[(size_t)g].setTargetValue(t.param("mix_pan"));
		const bool own = t.param("mix_out") > 0.5f && usable(g + 1);
		const int room = roomOf[(size_t)g];
		juce::AudioBuffer<float> roomBuf;
		if (room >= 0) roomBuf.setDataToReferTo(rooms[(size_t)room].buf.getArrayOfWritePointers(), 2, n);
		auto bus = room >= 0 ? roomBuf : own ? getBusBuffer(buffer, false, g + 1) : mainBus;
		const float* l = t.out.getReadPointer(0);
		const float* r = t.out.getReadPointer(1);
		for (int k = 0; k < n; ++k) {
			const float gain = stripGain[(size_t)g].getNextValue(), pan = stripPan[(size_t)g].getNextValue();
			// a balance: the centre keeps both sides at full level (the one-guitar versions' level)
			const float gl = gain * std::min(1.0f, 1.0f - pan), gr = gain * std::min(1.0f, 1.0f + pan);
			if (bus.getNumChannels() == 1) {
				bus.addSample(0, k, 0.5f * (gl * l[k] + gr * r[k]));
			}
			else if (bus.getNumChannels() >= 2) {
				bus.addSample(0, k, gl * l[k]);
				bus.addSample(1, k, gr * r[k]);
			}
		}
	}
	// the shared rooms, each into its output
	for (auto& r : rooms) {
		if (r.key < 0 || !r.conv) continue;
		r.idle = r.used ? 0 : r.idle + 1;
		if (r.idle > tailBlocks) continue;
		juce::dsp::AudioBlock<float> block(r.buf.getArrayOfWritePointers(), 2, (size_t)n);
		r.conv->process(juce::dsp::ProcessContextReplacing<float>(block));
		auto out = r.bus > 0 && usable(r.bus) ? getBusBuffer(buffer, false, r.bus) : mainBus;
		for (int ch = 0; ch < out.getNumChannels(); ++ch)
			out.addFrom(ch, 0, r.buf, juce::jmin(ch, 1), 0, n, out.getNumChannels() == 1 ? 0.5f : 1.0f);
		if (out.getNumChannels() == 1) out.addFrom(0, 0, r.buf, 1, 0, n, 0.5f);
	}
	// the main output's volume, then a peak limiter on every output that only acts above LIMIT and lowers the gain
	// smoothly instead of bending the wave
	volume.setTargetValue(params.getRawParameterValue("volume")->load());
	for (int k = 0; k < n; ++k) {
		const float v = volume.getNextValue();
		for (int ch = 0; ch < mainBus.getNumChannels(); ++ch) mainBus.setSample(ch, k, mainBus.getSample(ch, k) * v);
	}
	constexpr float LIMIT = 0.89f;
	const float release = std::exp(-1.0f / (0.15f * (float)sr));
	for (int b = 0; b < getBusCount(false) && b <= MAX_GUITARS; ++b) {
		if (b > 0 && !usable(b)) continue;
		auto bus = getBusBuffer(buffer, false, b);
		float& lg = limiterGain[(size_t)b];
		for (int k = 0; k < n; ++k) {
			float peak = 0.0f;
			for (int ch = 0; ch < bus.getNumChannels(); ++ch) peak = std::max(peak, std::abs(bus.getSample(ch, k)));
			const float need = peak > LIMIT ? LIMIT / peak : 1.0f;
			lg = need < lg ? need : 1.0f - (1.0f - lg) * release;
			for (int ch = 0; ch < bus.getNumChannels(); ++ch) bus.setSample(ch, k, bus.getSample(ch, k) * lg);
		}
	}
	// the oscilloscope (main output)
	const auto scope = scopeFifo.write(std::min(n, scopeFifo.getFreeSpace()));
	const float* o = mainBus.getReadPointer(0);
	for (int k = 0; k < scope.blockSize1; ++k) scopeData[(size_t)(scope.startIndex1 + k)] = o[k];
	for (int k = 0; k < scope.blockSize2; ++k) scopeData[(size_t)(scope.startIndex2 + k)] = o[scope.blockSize1 + k];
}

int GRMGuitarProcessor::readScope(float* dest, int count) {
	const auto scope = scopeFifo.read(std::min(count, scopeFifo.getNumReady()));
	for (int k = 0; k < scope.blockSize1; ++k) dest[k] = scopeData[(size_t)(scope.startIndex1 + k)];
	for (int k = 0; k < scope.blockSize2; ++k) dest[scope.blockSize1 + k] = scopeData[(size_t)(scope.startIndex2 + k)];
	return scope.blockSize1 + scope.blockSize2;
}

juce::AudioProcessorEditor* GRMGuitarProcessor::createEditor() { return new GRMGuitarEditor(*this); }

// ================================================================== state
// Guitar 1 keeps the keys of the one-guitar versions ("song", "pattern_count", "pattern_0"...), so that an older
// session opens as it was and an older version still reads guitar 1 of a newer session.
void GRMGuitarProcessor::getStateInformation(juce::MemoryBlock& destData) {
	auto state = params.copyState();
	const int count = guitarCount.load();
	state.setProperty("guitars", count, nullptr);
	state.setProperty("selected", selected.load(), nullptr);
	for (int g = 0; g < count; ++g) {
		const auto patterns = tracks[(size_t)g]->getPatterns();
		auto key = [g](const juce::String& k) { return GuitarTrack::paramId(g, k); };
		if (g == 0) state.setProperty("song", juce::String(patterns[0].to_text()), nullptr);
		state.setProperty(key("pattern_count"), (int)patterns.size(), nullptr);
		for (size_t k = 0; k < patterns.size(); ++k)
			state.setProperty(key("pattern_" + juce::String((int)k)), juce::String(patterns[k].to_text()), nullptr);
		state.setProperty(key("editing"), tracks[(size_t)g]->getEditingPattern(), nullptr);
	}
	state.setProperty("seed", (int)seed.load(), nullptr);
	if (auto xml = state.createXml()) copyXmlToBinary(*xml, destData);
}

void GRMGuitarProcessor::setStateInformation(const void* data, int sizeInBytes) {
	auto xml = getXmlFromBinary(data, sizeInBytes);
	if (!xml || !xml->hasTagName(params.state.getType())) return;
	auto tree = juce::ValueTree::fromXml(*xml);
	const int count = juce::jlimit(1, MAX_GUITARS, (int)tree.getProperty("guitars", 1));
	for (int g = 0; g < count; ++g) {
		auto key = [g](const juce::String& k) { return GuitarTrack::paramId(g, k); };
		std::vector<grmg::Song> loaded;
		const int n = tree.getProperty(key("pattern_count"), 0);
		for (int k = 0; k < juce::jmin(n, MAX_PATTERNS); ++k) {
			grmg::Song s;
			if (s.from_text(tree.getProperty(key("pattern_" + juce::String(k))).toString().toStdString())) loaded.push_back(s);
		}
		if (loaded.empty() && g == 0 && tree.hasProperty("song")) {    // a session of the first versions: one pattern
			grmg::Song s;
			if (s.from_text(tree.getProperty("song").toString().toStdString())) loaded.push_back(s);
		}
		if (loaded.empty()) {
			grmg::Song empty;
			empty.bars = 4;
			loaded.push_back(empty);
		}
		tracks[(size_t)g]->setPatterns(loaded, (int)tree.getProperty(key("editing"), 0));
		tree.removeProperty(key("pattern_count"), nullptr);
		tree.removeProperty(key("editing"), nullptr);
		for (int k = 0; k < n; ++k) tree.removeProperty(key("pattern_" + juce::String(k)), nullptr);
	}
	guitarCount.store(count);
	selected.store(juce::jlimit(0, count - 1, (int)tree.getProperty("selected", 0)));
	if (tree.hasProperty("seed")) seed.store((unsigned)(juce::int64)tree.getProperty("seed"));
	for (auto id : { "song", "guitars", "selected", "seed" }) tree.removeProperty(id, nullptr);
	params.replaceState(tree);
	guitarVersion.fetch_add(1);
	stateVersion.fetch_add(1);
	solver.notify();
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new GRMGuitarProcessor(); }
