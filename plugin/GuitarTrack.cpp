// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
#include "GuitarTrack.h"

namespace
{
	constexpr float OUTPUT_SCALE = 0.018f; // newtons on the bridge to the output (the default song peaks near -9 dBFS)

	double positive_mod(double x, double m)
	{
		const double r = std::fmod(x, m);
		return r < 0 ? r + m : r;
	}
} // namespace

GuitarTrack::GuitarTrack(juce::AudioProcessorValueTreeState& s, int i)
	: index(i), state(s),
	mixParams{ s.getRawParameterValue(paramId("mix_gain")), s.getRawParameterValue(paramId("mix_level")),
			  s.getRawParameterValue(paramId("mix_pan")), s.getRawParameterValue(paramId("mix_mute")),
			  s.getRawParameterValue(paramId("mix_solo")), s.getRawParameterValue(paramId("mix_out")) },
	bodyAmount(s.getRawParameterValue(paramId("body_amount"))),
	roomOn(s.getRawParameterValue(paramId("room_on")))
{
	if (i > 0)
	{
		grmg::Song empty;
		empty.bars = 4;
		patterns = { empty };
	}
}

GuitarTrack::MixerValues GuitarTrack::mixerValues() const
{
	return { mixParams.gainDb->load(), mixParams.level->load(), mixParams.pan->load(),
			mixParams.mute->load() > 0.5f, mixParams.solo->load() > 0.5f, mixParams.ownOutput->load() > 0.5f };
}

grmg::Settings GuitarTrack::readSettings() const
{
	auto v = [&](const char* id)
		{ return (double)param(id); };
	auto b = [&](const char* id)
		{ return param(id) > 0.5f; };
	auto c = [&](const char* id)
		{ return (int)std::lround(param(id)); };
	grmg::Settings s;
	s.tuning = juce::jlimit(0, grmg::NUM_TUNINGS - 1, c("tuning"));
	s.string_age = v("string_age");
	s.brightness = v("brightness");
	s.core = v("core");
	s.stiff = b("stiff");
	s.pick = c("attack") == 0;
	s.pluck = v("pluck");
	s.force = v("force");
	s.pick_mm = v("pick");
	s.palm_mm = std::round(v("palm_mm"));
	// steps of 0.05 in log (each one a palm network to solve); below grmg::PALM_OFF the palm is lifted
	s.palm_intensity = v("palm_intensity") < grmg::PALM_OFF
		? 0.0
		: std::pow(10.0, std::round(std::log10(v("palm_intensity")) / 0.05) * 0.05);
	s.body_size = v("body_size");
	s.top_mm = v("top_mm");
	s.coupling = v("coupling");
	s.braces_g = v("braces_g");
	s.body_q = v("body_q");
	s.wood = juce::jlimit(0, grmg::NUM_WOODS - 1, c("wood"));
	s.density_pct = std::round(v("density_pct"));
	s.hollow = std::round(v("hollow") * 50.0) / 50.0; // steps of 0.02 (each a body network to solve)
	s.sensor = juce::jlimit(0, 3, c("sensor"));
	s.pickup_mm = std::round(v("pickup_mm"));
	s.pickup_type = juce::jlimit(0, grmg::NUM_PICKUP_TYPES - 1, c("pickup_type"));
	s.cable_pf = std::round(v("cable_pf") / 10.0) * 10.0;
	s.pickup_bridge_on = b("pickup_bridge_on");
	s.pickup_middle_on = b("pickup_middle_on");
	s.pickup_neck_on = b("pickup_neck_on");
	s.pickup_bridge_split = b("pickup_bridge_split");
	s.pickup_bridge_type = juce::jlimit(0, grmg::NUM_PICKUP_TYPES - 1, c("pickup_bridge_type"));
	s.pickup_middle_type = juce::jlimit(0, grmg::NUM_PICKUP_TYPES - 1, c("pickup_middle_type"));
	s.pickup_neck_type = juce::jlimit(0, grmg::NUM_PICKUP_TYPES - 1, c("pickup_neck_type"));

	s.amp_on = param("amp_on") > 0.5f;

	const int clean_model = juce::jlimit(0, 2, (int)std::lround(param("amp_clean_model")));
	if (clean_model == 0) s.amp_clean = grmg::amp_preset_5150_lead();
	else if (clean_model == 1) s.amp_clean = grmg::amp_preset_jcm800_crunch();
	else s.amp_clean = grmg::amp_preset_clean();

	s.amp_clean.gain = param("amp_clean_gain");
	s.amp_clean.master = param("amp_clean_master");
	s.amp_clean.bass = param("amp_clean_bass");
	s.amp_clean.middle = param("amp_clean_middle");
	s.amp_clean.treble = param("amp_clean_treble");
	s.amp_clean.presence = param("amp_clean_presence");
	s.amp_clean.tight = param("amp_clean_tight") > 0.5f;

	const int lead_model = juce::jlimit(0, 2, (int)std::lround(param("amp_lead_model")));
	if (lead_model == 0) s.amp_lead = grmg::amp_preset_5150_lead();
	else if (lead_model == 1) s.amp_lead = grmg::amp_preset_jcm800_crunch();
	else s.amp_lead = grmg::amp_preset_clean();

	s.amp_lead.gain = param("amp_lead_gain");
	s.amp_lead.master = param("amp_lead_master");
	s.amp_lead.bass = param("amp_lead_bass");
	s.amp_lead.middle = param("amp_lead_middle");
	s.amp_lead.treble = param("amp_lead_treble");
	s.amp_lead.presence = param("amp_lead_presence");
	s.amp_lead.tight = param("amp_lead_tight") > 0.5f;

	s.amp_channel_mix = param("amp_channel_mix");
	s.amp_room_mix = param("amp_room_mix");
	// --------------------------------------

	s.feedback_amount = param("feedback_amount");
	s.feedback_distance = param("feedback_distance");
	s.feedback_target = (int)std::lround(param("feedback_target"));

	s.body_amount = bodyAmount->load();
	s.sympathy = b("sympathy");
	s.room_on = roomOn->load() > 0.5f;
	s.room_size = juce::jlimit(0, 3, c("room_size"));
	s.room_material = juce::jlimit(0, 5, c("room_material"));
	s.room_distance = std::round(v("room_distance") * 10.0) / 10.0;
	s.human = b("human");
	s.human_amount = std::round(v("human_amount") * 20.0) / 20.0;
	s.swing_cm = v("swing_cm");
	s.finger_noises = b("finger_noises");
	s.finger_force = v("finger_force");
	return s;
}

// ================================================================== patterns
grmg::Song GuitarTrack::getSong() const
{
	std::lock_guard<std::mutex> lock(songLock);
	return patterns[(size_t)juce::jlimit(0, (int)patterns.size() - 1, editing.load())];
}

void GuitarTrack::setSong(const grmg::Song& s)
{
	{
		std::lock_guard<std::mutex> lock(songLock);
		patterns[(size_t)juce::jlimit(0, (int)patterns.size() - 1, editing.load())] = s;
	}
	songVersion.fetch_add(1);
}

std::vector<grmg::Song> GuitarTrack::getPatterns() const
{
	std::lock_guard<std::mutex> lock(songLock);
	return patterns;
}

void GuitarTrack::setPatterns(const std::vector<grmg::Song>& p, int k)
{
	if (p.empty())
		return;
	{
		std::lock_guard<std::mutex> lock(songLock);
		patterns = p;
		editing.store(juce::jlimit(0, (int)p.size() - 1, k));
	}
	songVersion.fetch_add(1);
	patternVersion.fetch_add(1);
}

int GuitarTrack::getPatternCount() const
{
	std::lock_guard<std::mutex> lock(songLock);
	return (int)patterns.size();
}

void GuitarTrack::selectPattern(int k)
{
	{
		std::lock_guard<std::mutex> lock(songLock);
		k = juce::jlimit(0, (int)patterns.size() - 1, k);
	}
	if (k == editing.load())
		return;
	editing.store(k);
	patternVersion.fetch_add(1);
}

void GuitarTrack::addPattern(bool duplicate)
{
	grmg::Song fresh;
	{
		std::lock_guard<std::mutex> lock(songLock);
		const int curIdx = juce::jlimit(0, (int)patterns.size() - 1, editing.load());
		const auto& current = patterns[(size_t)curIdx];

		if (duplicate)
		{
			fresh = current;
		}
		else
		{
			fresh.bars = 1;
			fresh.tempo = current.tempo;
			fresh.beats_per_bar = current.beats_per_bar;
		}
		fresh.key = current.key;
		fresh.scale = current.scale;
	}
	insertPattern(fresh);
}

void GuitarTrack::insertPattern(const grmg::Song& s)
{
	int k;
	{
		std::lock_guard<std::mutex> lock(songLock);
		if ((int)patterns.size() >= MAX_PATTERNS)
			return;
		k = editing.load() + 1;
		patterns.insert(patterns.begin() + k, s);
	}
	editing.store(k);
	patternVersion.fetch_add(1);
	songVersion.fetch_add(1);
}

void GuitarTrack::deletePattern()
{
	{
		std::lock_guard<std::mutex> lock(songLock);
		if (patterns.size() <= 1)
			return;
		const int k = editing.load();
		patterns.erase(patterns.begin() + k);
		editing.store(juce::jmin(k, (int)patterns.size() - 1));
	}
	patternVersion.fetch_add(1);
	songVersion.fetch_add(1);
}

// ================================================================== solver thread
GuitarTrack::ModelHolder* GuitarTrack::makeHolder(std::shared_ptr<const grmg::Model> m)
{
	auto* h = new ModelHolder;
	h->model = m;
	if (!m->body_ir.empty())
	{
		h->body.setSize(1, (int)m->body_ir.size());
		std::copy(m->body_ir.begin(), m->body_ir.end(), h->body.getWritePointer(0));
	}
	if (!m->room_ir[0].empty())
	{
		h->room.setSize(2, (int)m->room_ir[0].size());
		for (int ch = 0; ch < 2; ++ch)
			std::copy(m->room_ir[(size_t)ch].begin(), m->room_ir[(size_t)ch].end(), h->room.getWritePointer(ch));
		h->roomShare.makeCopyOf(h->room);
	}
	return h;
}

void GuitarTrack::solveStep(double sr, unsigned seed, double tempo, bool loop, std::atomic<bool>& solving, std::atomic<int>& stage,
	const std::function<bool()>& stop)
{
	using M = grmg::Model;
	auto sameSettings = [](const grmg::Settings& a, const grmg::Settings& b)
		{
			return a.amp_on == b.amp_on &&
				std::abs(a.amp_channel_mix - b.amp_channel_mix) < 1e-4 &&
				std::abs(a.amp_room_mix - b.amp_room_mix) < 1e-4 &&
				M::same_strings(a, b) && M::same_attack(a, b) && M::same_palm(a, b) && M::same_body(a, b) && M::same_room(a, b)
				&& M::same_feedback(a, b)
				&& M::same_amp_dc(a.amp_clean, b.amp_clean) && M::same_amp_params(a.amp_clean, b.amp_clean)
				&& M::same_amp_dc(a.amp_lead, b.amp_lead) && M::same_amp_params(a.amp_lead, b.amp_lead);
		};
	auto samePlayer = [](const grmg::Settings& a, const grmg::Settings& b)
		{
			return a.human == b.human && std::abs(a.human_amount - b.human_amount) < 1e-9 && std::abs(a.swing_cm - b.swing_cm) < 1e-9 && a.finger_noises == b.finger_noises && std::abs(a.finger_force - b.finger_force) < 1e-9;
		};
	collect();
	const auto s = readSettings();
	// 1) the networks of the sound: strings first (instant), then the palm and the room (slower)
	if (!model || !sameSettings(s, solvedFor) || std::abs(sr - modelRate) > 1e-6)
	{
		solving.store(true);
		try
		{
			auto quick = std::make_shared<grmg::Model>();
			quick->solve(s, sr, model ? model.get() : nullptr, false);
			modelBox.post(makeHolder(quick));
			stage.store(1);
			if (stop())
				return;
			auto full = std::make_shared<grmg::Model>();
			full->solve(s, sr, quick.get(), true);
			model = full;
			solvedFor = s;
			modelRate = sr;
			modelBox.post(makeHolder(full));
			{
				std::lock_guard<std::mutex> lock(shownLock);
				shownModel = full;
			}
			stage.store(2);
		}
		catch (const std::exception& e)
		{
			DBG("GRM solver: " << e.what());
			solvedFor = s; // do not loop on a setting the networks cannot solve
			modelRate = sr;
		}
		solving.store(false);
	}
	else if (std::abs(model->s.string_age - s.string_age) > 1e-12 || std::abs(model->s.brightness - s.brightness) > 1e-12 || !M::same_pickups(model->s, s) || model->s.sympathy != s.sympathy)
	{
		auto copy = std::make_shared<grmg::Model>(*model);
		copy->s.string_age = s.string_age;
		copy->solve_losses();
		copy->s.sensor = s.sensor;
		copy->s.pickup_mm = s.pickup_mm;
		copy->s.pickup_type = s.pickup_type;
		copy->s.cable_pf = s.cable_pf;
		copy->s.pickup_bridge_on = s.pickup_bridge_on;
		copy->s.pickup_middle_on = s.pickup_middle_on;
		copy->s.pickup_neck_on = s.pickup_neck_on;
		copy->s.pickup_bridge_split = s.pickup_bridge_split;
		copy->s.pickup_bridge_type = s.pickup_bridge_type;
		copy->s.pickup_middle_type = s.pickup_middle_type;
		copy->s.pickup_neck_type = s.pickup_neck_type;
		copy->solve_pickup();
		copy->s.brightness = s.brightness;
		copy->s.sympathy = s.sympathy;
		model = copy;
		modelBox.post(makeHolder(copy));
		std::lock_guard<std::mutex> lock(shownLock);
		shownModel = copy;
	}
	// 2) the patterns, played by the player network (only the changed ones when nothing else moved)
	const int version = songVersion.load();
	const bool everything = seed != compiledSeed || std::abs(tempo - compiledTempo) > 0.01 || loop != compiledLoop || model.get() != compiledModel || !samePlayer(s, compiledPlayer);
	if (model && (version != compiledSong || everything))
	{
		const auto songs = getPatterns();
		try
		{
			grmg::Settings ps = model->s; // the player settings of now, with the model's networks
			ps.human = s.human;
			ps.human_amount = s.human_amount;
			ps.swing_cm = s.swing_cm;
			ps.finger_noises = s.finger_noises;
			ps.finger_force = s.finger_force;
			auto m = std::make_shared<grmg::Model>(*model);
			m->s = ps;
			grmg::CompileOptions o;
			o.tempo = tempo;
			o.seed = seed + 7919u * (unsigned)index; // each guitarist plays with imprecisions of his own
			o.loop = loop;
			auto* set = new PerformanceSet;
			set->model = m;
			std::vector<std::string> texts;
			for (size_t k = 0; k < songs.size(); ++k)
			{
				texts.push_back(songs[k].to_text());
				if (!everything && k < compiledTexts.size() && compiledTexts[k] == texts.back())
					set->list.push_back(compiledPerfs[k]);
				else
					set->list.push_back(std::shared_ptr<const grmg::Performance>(grmg::compile_song(compiler, songs[k], m, o)));
			}
			compiledTexts = texts;
			compiledPerfs = set->list;
			compiledModelShared = m;
			perfBox.post(set);
		}
		catch (const std::exception& e)
		{
			DBG("GRM compiler: " << e.what());
		}
		compiledSong = version;
		compiledSeed = seed;
		compiledTempo = tempo;
		compiledLoop = loop;
		compiledModel = model.get();
		compiledPlayer = s;
	}
}

// ================================================================== audio thread
void GuitarTrack::prepare(double sr, int block)
{
	engine.prepare(sr);
	bodyConv.prepare({ sr, (juce::uint32)block, 1 });
	roomConv.prepare({ sr, (juce::uint32)block, 2 });
	dry.setSize(1, block);
	pickupBuf.setSize(1, block);
	out.setSize(2, block);
	ampRoomBuf.setSize(2, block, false, false, true);
	bodyMix.reset(sr, 0.05);
	filterFr = filterQ = -1.0;
}

float GuitarTrack::param(const char* k) const
{
	auto* p = state.getRawParameterValue(paramId(k));
	return p != nullptr ? p->load() : 0.0f;
}

void GuitarTrack::begin(bool patternModeNow, bool internalPlaying)
{
	if (auto* h = modelBox.take())
	{
		holder = h;
		engine.set_model(h->model.get());
		// impulse responses built by the solver: loading them is wait-free (juce::dsp::Convolution)
		if (h->model->body_id != loadedBody && h->body.getNumSamples() > 0)
		{
			bodyConv.loadImpulseResponse(std::move(h->body), h->model->sample_rate, juce::dsp::Convolution::Stereo::no,
				juce::dsp::Convolution::Trim::no, juce::dsp::Convolution::Normalise::no);
			loadedBody = h->model->body_id;
		}
		if (h->model->room_id != loadedRoom && h->room.getNumSamples() > 0)
		{
			roomConv.loadImpulseResponse(std::move(h->room), h->model->sample_rate, juce::dsp::Convolution::Stereo::yes,
				juce::dsp::Convolution::Trim::no, juce::dsp::Convolution::Normalise::no);
			loadedRoom = h->model->room_id;
			roomLoaded = true;
		}
	}
	if (auto* set = perfBox.take())
		perfSet = set;
	patternMode = patternModeNow;
	keys = patternMode && !internalPlaying;
	if (!keys)
	{
		const int k = editing.load();
		engine.set_performance(perfSet ? perfSet->at(k) : nullptr, k != shownPattern); // another pattern: carry the endings
		shownPattern = k;
		heldKey = heldPattern = -1;
	}
	else if (heldPattern >= 0)
	{
		engine.set_performance(perfSet ? perfSet->at(heldPattern) : nullptr);
	}
	engine.live_palm = param("live_palm") > 0.5f;
	engine.midi_strings = state.getRawParameterValue("midi_strings")->load() > 0.5f ? grmg::Engine::MidiStrings::channel_per_string
		: grmg::Engine::MidiStrings::automatic;
	// strings struck on the neck
	const auto scope = uiFifo.read(uiFifo.getNumReady());
	auto play = [&](int start, int size)
		{
			for (int k = 0; k < size; ++k)
			{
				const auto& u = uiPlucks[(size_t)(start + k)];
				engine.pluck_string(u.string, u.fret, u.velocity);
			}
		};
	play(scope.startIndex1, scope.blockSize1);
	play(scope.startIndex2, scope.blockSize2);
	pos = 0;
}

void GuitarTrack::pluckFromInterface(int string, int fret, float velocity)
{
	const auto scope = uiFifo.write(1);
	if (scope.blockSize1 > 0)
		uiPlucks[(size_t)scope.startIndex1] = { string, fret, velocity };
	else if (scope.blockSize2 > 0)
		uiPlucks[(size_t)scope.startIndex2] = { string, fret, velocity };
}

bool GuitarTrack::holdsMidiNote(int channel, int pitch) const
{
	if (keys && heldKey >= 0 && heldChannel == channel && (pitch < 0 || heldKey == pitch))
		return true;
	return engine.holds_midi_note(channel, pitch);
}

void GuitarTrack::midi(const juce::MidiMessage& msg)
{
	const int channel = msg.getChannel();
	if (msg.isNoteOff())
	{
		const int pitch = msg.getNoteNumber();
		if (heldChannel == channel && heldKey == pitch)
			heldKey = heldPattern = -1;
		engine.note_off(pitch, channel);
		return;
	}
	if (msg.isAllNotesOff() || msg.isAllSoundOff())
	{
		if (heldChannel == channel)
			releaseKeys();
		engine.note_off_channel(channel);
		return;
	}
	if (keys)
	{
		// a key plays its pattern (its loop) from its start; another key takes over; releasing the key that holds
		// the pattern stops it (the hands stop the strings)
		if (msg.isNoteOn())
		{
			const int k = msg.getNoteNumber() - PATTERN_KEY;
			if (perfSet && perfSet->at(k))
			{
				heldKey = msg.getNoteNumber();
				heldChannel = channel;
				heldPattern = k;
				patternBeat = 0.0;
				engine.set_performance(perfSet->at(k), true); // the old pattern's notes end as they were asked
			}
		}
		return;
	}
	if (msg.isNoteOn())
		engine.note_on(msg.getNoteNumber(), msg.getFloatVelocity(), channel);
}

void GuitarTrack::releaseKeys()
{
	if (heldPattern >= 0 || heldKey >= 0)
		engine.all_off();
	heldKey = heldPattern = -1;
}

void GuitarTrack::renderTo(int at, const SongClock& clock)
{
	if (at <= pos)
		return;
	const int sensor = engine.model() ? engine.model()->s.sensor : 0;
	float* heard = (sensor == 1 || sensor == 3) ? pickupBuf.getWritePointer(0) : nullptr;
	grmg::Transport tr;
	tr.beats_per_sample = clock.beats_per_sample;
	if (keys)
	{
		tr.loop = true; // a pattern loops while its key is held
		tr.playing = heldPattern >= 0;
		tr.beat = patternBeat;
		if (tr.playing)
			patternBeat += (at - pos) * clock.beats_per_sample;
	}
	else
	{
		tr.loop = clock.loop;
		tr.playing = clock.playing;
		const auto* perf = engine.performance();
		const double origin = perf ? perf->origin : 0.0, len = perf ? perf->length_beats : 16.0;
		if (clock.host)
		{
			const double b = clock.beat + pos * clock.beats_per_sample - origin;
			tr.beat = tr.loop && len > 0 ? positive_mod(b, len) : std::max(0.0, b);
		}
		else
		{
			// the plugin's own clock: Play (or a click) before the loop starts at the loop
			const double start = clock.start < origin ? 0.0 : clock.start - origin;
			const double b = start + clock.beat + pos * clock.beats_per_sample;
			tr.beat = tr.loop && len > 0 ? positive_mod(b, len) : b;
		}
	}
	engine.render(dry.getWritePointer(0) + pos, at - pos, tr, heard ? heard + pos : nullptr);
	playing.store(keys ? heldPattern : (clock.playing ? editing.load() : -1));
	pos = at;
}

long GuitarTrack::roomKey() const
{
	const auto* m = engine.model();
	if (!m || roomOn->load() <= 0.5f)
		return -1;

	return 1 + m->s.room_size + 4L * m->s.room_material + 24L * std::lround(m->s.room_distance * 10.0) + 24L * 1000L * std::lround(m->sample_rate);
}

juce::AudioBuffer<float>* GuitarTrack::roomShare()
{
	return holder && holder->roomShare.getNumSamples() > 0 ? &holder->roomShare : nullptr;
}

void GuitarTrack::finish(int n, double sr, bool ownRoom)
{
	// a value that is not a number would silence the convolutions for good: start the strings again instead
	{
		float* d0 = dry.getWritePointer(0);
		bool bad = false;
		for (int t = 0; t < n; ++t)
			bad |= !std::isfinite(d0[t]);
		if (bad)
		{
			dry.clear();
			pickupBuf.clear();
			engine.all_off();
			engine.reset_strings();
			bodyConv.reset();
			roomConv.reset();
			pickupFilter.z1 = pickupFilter.z2 = 0.0;
		}
	}
	const auto* model = engine.model();
	const int sensor = model ? model->s.sensor : 0;
	const float* d = dry.getReadPointer(0);
	out.clear();
	if (sensor != 0)
	{
		if (sensor == 1 && model && (model->pickup.fr != filterFr || model->pickup.Q != filterQ))
		{
			const auto previous = pickupFilter;
			pickupFilter = grmg::pickup_filter(model->pickup, sr);
			pickupFilter.z1 = previous.z1;
			pickupFilter.z2 = previous.z2;
			filterFr = model->pickup.fr;
			filterQ = model->pickup.Q;
		}
		const float* heard = pickupBuf.getReadPointer(0);
		float* w0 = out.getWritePointer(0);
		if (sensor == 1)
		{
			if (model && model->amp_active)
			{
				for (int t = 0; t < n; ++t)
					w0[t] = OUTPUT_SCALE * (float)(grmg::PICKUP_LEVEL * heard[t]);
			}
			else
			{
				for (int t = 0; t < n; ++t)
					w0[t] = OUTPUT_SCALE * (float)(grmg::PICKUP_LEVEL * pickupFilter.process(heard[t]));
			}
		}
		else if (sensor == 3)
		{
			for (int t = 0; t < n; ++t)
				w0[t] = OUTPUT_SCALE * (float)(grmg::PICKUP_LEVEL * heard[t]);
		}
		else
		{
			for (int t = 0; t < n; ++t)
				w0[t] = OUTPUT_SCALE * (float)(grmg::PIEZO_LEVEL * d[t]);
		}

		out.copyFrom(1, 0, out, 0, 0, n);

		if (model && model->s.room_on && roomLoaded && model->s.amp_room_mix > 0.001f)
		{
			ampRoomBuf.copyFrom(0, 0, out, 0, 0, n);
			ampRoomBuf.copyFrom(1, 0, out, 0, 0, n);

			juce::dsp::AudioBlock<float> block(ampRoomBuf.getArrayOfWritePointers(), 2, (size_t)n);
			roomConv.process(juce::dsp::ProcessContextReplacing<float>(block));

			const float wet = (float)model->s.amp_room_mix;
			const float dry = 1.0f - wet;

			out.applyGain(0, 0, n, dry);
			out.applyGain(1, 0, n, dry);

			out.addFrom(0, 0, ampRoomBuf, 0, 0, n, wet);
			out.addFrom(1, 0, ampRoomBuf, 1, 0, n, wet);
		}

		return;
	}

	bodyMix.setTargetValue(bodyAmount->load());
	out.copyFrom(0, 0, dry, 0, 0, n);
	{
		juce::dsp::AudioBlock<float> block(out.getArrayOfWritePointers(), 1, (size_t)n);
		bodyConv.process(juce::dsp::ProcessContextReplacing<float>(block));
	}
	float* w0 = out.getWritePointer(0);
	for (int t = 0; t < n; ++t)
	{
		const float a = bodyMix.getNextValue();
		w0[t] = OUTPUT_SCALE * ((1.0f - a) * d[t] + a * w0[t]);
	}
	out.copyFrom(1, 0, out, 0, 0, n);
	if (ownRoom && roomLoaded)
	{
		juce::dsp::AudioBlock<float> block(out.getArrayOfWritePointers(), 2, (size_t)n);
		roomConv.process(juce::dsp::ProcessContextReplacing<float>(block));
	}
}

grmg::Song GuitarTrack::getPattern(int k) const
{
	std::lock_guard<std::mutex> lock(songLock);
	if (k >= 0 && k < (int)patterns.size())
		return patterns[(size_t)k];
	return {};
}