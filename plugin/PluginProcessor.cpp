// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
#include "PluginProcessor.h"

#include "PluginEditor.h"
#include "../core/offline.hpp"

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

}  // namespace

// ================================================================== the solver thread
void SolverThread::run() {
    grmg::Settings solvedFor;
    std::shared_ptr<const grmg::Model> model;
    grmg::Compiler compiler;
    int compiledSong = -1;
    unsigned compiledSeed = 0;
    double compiledTempo = 0.0;
    bool compiledLoop = false;
    const grmg::Model* compiledModel = nullptr;
    double modelRate = 0.0;
    auto sameSettings = [](const grmg::Settings& a, const grmg::Settings& b) {
        using M = grmg::Model;
        return M::same_strings(a, b) && M::same_attack(a, b) && M::same_palm(a, b) && M::same_body(a, b)
               && M::same_room(a, b);
    };
    auto samePlayer = [](const grmg::Settings& a, const grmg::Settings& b) {
        return a.human == b.human && std::abs(a.swing_cm - b.swing_cm) < 1e-9 && a.finger_noises == b.finger_noises
               && std::abs(a.finger_force - b.finger_force) < 1e-9;
    };
    grmg::Settings compiledPlayer;

    while (!threadShouldExit()) {
        owner.modelBox.collect();
        owner.perfBox.collect();
        const double sr = owner.sampleRate.load();
        if (sr <= 0) {
            wait(50);
            continue;
        }
        const auto s = owner.readSettings();
        // 1) the networks of the sound: strings first (instant), then the palm and the room (slower)
        if (!model || !sameSettings(s, solvedFor) || std::abs(sr - modelRate) > 1e-6) {
            owner.solving.store(true);
            try {
                auto quick = std::make_shared<grmg::Model>();
                quick->solve(s, sr, model ? model.get() : nullptr, false);
                owner.modelBox.post(owner.makeHolder(quick));
                owner.solvedStage.store(1);
                if (threadShouldExit()) break;
                auto full = std::make_shared<grmg::Model>();
                full->solve(s, sr, quick.get(), true);
                model = full;
                solvedFor = s;
                modelRate = sr;
                owner.modelBox.post(owner.makeHolder(full));
                owner.show(full);
                owner.solvedStage.store(2);
            } catch (const std::exception& e) {
                DBG("GRM solver: " << e.what());
                solvedFor = s;           // do not loop on a setting the networks cannot solve
                modelRate = sr;
            }
            owner.solving.store(false);
        } else if (std::abs(model->s.resonance - s.resonance) > 1e-12 || std::abs(model->s.brightness - s.brightness) > 1e-12
                   || model->s.sympathy != s.sympathy || std::abs(model->s.body_amount - s.body_amount) > 1e-12
                   || model->s.room_on != s.room_on) {
            // settings read when a note starts: a copy of the model carries them
            auto copy = std::make_shared<grmg::Model>(*model);
            copy->s.resonance = s.resonance;
            copy->s.brightness = s.brightness;
            copy->s.sympathy = s.sympathy;
            copy->s.body_amount = s.body_amount;
            copy->s.room_on = s.room_on;
            model = copy;
            owner.modelBox.post(owner.makeHolder(copy));
            owner.show(copy);
        }
        // 2) the guitar roll, played by the player network
        const int version = owner.songVersion.load();
        const unsigned sd = owner.seed.load();
        const double tempo = owner.currentTempo.load();
        const bool loop = owner.params.getRawParameterValue("loop")->load() > 0.5f;
        if (model && (version != compiledSong || sd != compiledSeed || std::abs(tempo - compiledTempo) > 0.01 || loop != compiledLoop
                      || model.get() != compiledModel || !samePlayer(s, compiledPlayer))) {
            grmg::Song song;
            {
                std::lock_guard<std::mutex> lock(owner.songLock);
                song = owner.song;
            }
            try {
                grmg::Settings ps = model->s;          // the player settings of now, with the model's networks
                ps.human = s.human;
                ps.swing_cm = s.swing_cm;
                ps.finger_noises = s.finger_noises;
                ps.finger_force = s.finger_force;
                auto m = std::make_shared<grmg::Model>(*model);
                m->s = ps;
                grmg::CompileOptions o;
                o.tempo = tempo;
                o.seed = sd;
                o.loop = loop;
                auto perf = compiler.compile(song, m, o);
                owner.perfBox.post(perf.release());
            } catch (const std::exception& e) {
                DBG("GRM compiler: " << e.what());
            }
            compiledSong = version;
            compiledSeed = sd;
            compiledTempo = tempo;
            compiledLoop = loop;
            compiledModel = model.get();
            compiledPlayer = s;
        }
        wait(30);
    }
}

// ================================================================== the processor
GRMGuitarProcessor::GRMGuitarProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      params(*this, nullptr, "GRMGuitar", createLayout()) {
    solver.startThread();
}

GRMGuitarProcessor::~GRMGuitarProcessor() {
    solver.stopThread(4000);
    if (exportThread.joinable()) exportThread.join();
}

void GRMGuitarProcessor::exportSong(const juce::File& file) {
    if (exporting.exchange(true)) return;
    if (exportThread.joinable()) exportThread.join();
    const auto settings = readSettings();
    const auto s = getSong();
    const double tempo = currentTempo.load();
    const unsigned sd = seed.load();
    exportThread = std::thread([this, settings, s, tempo, sd, file] {
        juce::String result;
        try {
            const double sr = 48000.0;
            auto model = std::make_shared<grmg::Model>();
            model->solve(settings, sr, nullptr, true);
            grmg::Compiler compiler;
            grmg::CompileOptions o;
            o.tempo = tempo;
            o.seed = sd;
            auto perf = compiler.compile(s, model, o);
            grmg::Engine e;
            e.prepare(sr);
            e.set_model(model.get());
            e.set_performance(perf.get());
            grmg::Transport tr;
            tr.playing = true;
            tr.loop = false;
            tr.beats_per_sample = tempo / 60.0 / sr;
            const size_t total = (size_t) ((s.length_beats() * 60.0 / tempo + 4.0) * sr);
            std::vector<float> out(total, 0.0f);
            for (size_t pos = 0; pos < total; pos += 512) {
                tr.beat = (double) pos * tr.beats_per_sample;
                e.render(out.data() + pos, (int) std::min<size_t>(512, total - pos), tr);
            }
            const auto stereo = grmg::body_and_room(std::vector<double>(out.begin(), out.end()), *model);
            double peak = 1e-12;
            for (const auto& c : stereo)
                for (double v : c) peak = std::max(peak, std::abs(v));
            juce::AudioBuffer<float> buf(2, (int) stereo[0].size());
            for (int ch = 0; ch < 2; ++ch)
                for (int t = 0; t < buf.getNumSamples(); ++t)
                    buf.setSample(ch, t, (float) (stereo[(size_t) ch][(size_t) t] / peak * 0.89));
            juce::WavAudioFormat wav;
            file.deleteFile();
            std::unique_ptr<juce::OutputStream> stream(file.createOutputStream());
            std::unique_ptr<juce::AudioFormatWriter> writer;
            if (stream)
                writer = wav.createWriterFor(stream, juce::AudioFormatWriterOptions {}.withSampleRate(sr).withNumChannels(2).withBitsPerSample(24));
            if (writer) {
                writer->writeFromAudioSampleBuffer(buf, 0, buf.getNumSamples());
                result = "Exported " + file.getFileName();
            } else {
                result = "Could not write " + file.getFullPathName();
            }
        } catch (const std::exception& ex) {
            result = juce::String("Export failed: ") + ex.what();
        }
        {
            std::lock_guard<std::mutex> lock(exportLock);
            exportResult = result;
        }
        exporting.store(false);
    });
}

juce::AudioProcessorValueTreeState::ParameterLayout GRMGuitarProcessor::createLayout() {
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    // centre: the value at the middle of the slider (0: linear)
    auto real = [&](const char* id, const char* name, float lo, float hi, float def, float centre = 0.0f, float step = 0.0f) {
        NormalisableRange<float> r(lo, hi, step);
        if (centre > lo && centre < hi) r.setSkewForCentre(centre);
        const int decimals = hi - lo > 50 ? 0 : hi - lo > 5 ? 1 : 2;
        p.push_back(std::make_unique<AudioParameterFloat>(
            ParameterID {id, 1}, name, r, def,
            AudioParameterFloatAttributes().withStringFromValueFunction([decimals](float v, int) { return String(v, decimals); })));
    };
    auto flag = [&](const char* id, const char* name, bool def) {
        p.push_back(std::make_unique<AudioParameterBool>(ParameterID {id, 1}, name, def));
    };
    auto choice = [&](const char* id, const char* name, const StringArray& c, int def) {
        p.push_back(std::make_unique<AudioParameterChoice>(ParameterID {id, 1}, name, c, def));
    };
    // strings
    choice("tuning", "Tuning", tuningNames(), 0);
    real("resonance", "Resonance (s)", 0.2f, 8.0f, 3.0f, 2.0f);
    real("brightness", "Brightness", 0.0f, 1.0f, 0.6f);
    real("core", "Wound core", 0.2f, 1.0f, 0.45f);
    flag("stiff", "Stiff strings", true);
    // attack
    choice("attack", "Attack", StringArray {"Pick", "Fingers"}, 0);
    real("pluck", "Pluck point", 0.05f, 0.5f, 0.22f);
    real("force", "Pluck force (N)", 0.3f, 6.0f, 2.0f, 1.5f);
    real("pick", "Pick (mm)", 0.4f, 1.5f, 0.71f);
    // palm
    real("palm_mm", "Palm from bridge (mm)", 5.0f, 80.0f, 30.0f);
    real("palm_intensity", "Palm pressure (0: lifted)", 0.0f, 4.0f, 1.0f, 1.0f);
    flag("live_palm", "Palm mute (live)", false);
    // body
    real("body_size", "Body size", 0.8f, 1.25f, 1.0f);
    real("top_mm", "Top (mm)", 2.0f, 4.0f, 2.8f);
    real("coupling", "Air / top coupling", 0.1f, 0.6f, 0.35f);
    real("body_amount", "Body", 0.0f, 1.0f, 0.85f);
    flag("sympathy", "Sympathetic strings", true);
    // room
    flag("room_on", "Room", true);
    choice("room_size", "Room size", roomNames(), 1);
    choice("room_material", "Walls", materialNames(), 2);
    real("room_distance", "Listener (m)", 0.3f, 6.0f, 1.0f, 1.5f);
    // player
    flag("human", "Human feel", true);
    real("swing_cm", "Arm swing (cm)", 4.0f, 20.0f, 10.0f);
    flag("finger_noises", "Finger noises", true);
    real("finger_force", "Finger force (N)", 0.1f, 2.0f, 0.6f, 0.6f);
    // output and MIDI
    real("volume", "Volume", 0.0f, 2.0f, 1.0f, 1.0f);
    choice("midi_strings", "MIDI to strings", StringArray {"Lowest fret", "Channel = string"}, 0);
    flag("loop", "Loop the roll", true);
    flag("follow_host", "Follow the host", true);
    real("tempo", "Tempo (standalone)", 40.0f, 220.0f, 90.0f, 1.0f, 1.0f);
    return {p.begin(), p.end()};
}

grmg::Settings GRMGuitarProcessor::readSettings() const {
    auto v = [&](const char* id) { return (double) params.getRawParameterValue(id)->load(); };
    auto b = [&](const char* id) { return params.getRawParameterValue(id)->load() > 0.5f; };
    auto c = [&](const char* id) { return (int) std::lround(params.getRawParameterValue(id)->load()); };
    grmg::Settings s;
    s.tuning = juce::jlimit(0, grmg::NUM_TUNINGS - 1, c("tuning"));
    s.resonance = v("resonance");
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
    s.body_amount = v("body_amount");
    s.sympathy = b("sympathy");
    s.room_on = b("room_on");
    s.room_size = juce::jlimit(0, 3, c("room_size"));
    s.room_material = juce::jlimit(0, 5, c("room_material"));
    s.room_distance = std::round(v("room_distance") * 10.0) / 10.0;
    s.human = b("human");
    s.swing_cm = v("swing_cm");
    s.finger_noises = b("finger_noises");
    s.finger_force = v("finger_force");
    return s;
}

GRMGuitarProcessor::ModelHolder* GRMGuitarProcessor::makeHolder(std::shared_ptr<const grmg::Model> m) {
    auto* h = new ModelHolder;
    h->model = m;
    if (!m->body_ir.empty()) {
        h->body.setSize(1, (int) m->body_ir.size());
        std::copy(m->body_ir.begin(), m->body_ir.end(), h->body.getWritePointer(0));
    }
    if (!m->room_ir[0].empty()) {
        h->room.setSize(2, (int) m->room_ir[0].size());
        for (int ch = 0; ch < 2; ++ch)
            std::copy(m->room_ir[(size_t) ch].begin(), m->room_ir[(size_t) ch].end(), h->room.getWritePointer(ch));
    }
    return h;
}

void GRMGuitarProcessor::prepareToPlay(double sr, int samplesPerBlock) {
    engine.prepare(sr);
    juce::dsp::ProcessSpec mono {sr, (juce::uint32) samplesPerBlock, 1};
    juce::dsp::ProcessSpec stereo {sr, (juce::uint32) samplesPerBlock, 2};
    bodyConv.prepare(mono);
    roomConv.prepare(stereo);
    dry.setSize(1, samplesPerBlock);
    wet.setSize(2, samplesPerBlock);
    bodyMix.reset(sr, 0.05);
    volume.reset(sr, 0.05);
    sampleRate.store(sr);
    solver.notify();
}

bool GRMGuitarProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void GRMGuitarProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const double sr = getSampleRate();
    if (dry.getNumSamples() < n) {           // a host sending more than it announced
        dry.setSize(1, n, false, false, true);
        wet.setSize(2, n, false, false, true);
    }
    // the newest networks and performance, taken between two blocks
    if (auto* h = modelBox.take()) {
        engine.set_model(h->model.get());
        // impulse responses built by the solver: loading them is wait-free (juce::dsp::Convolution)
        if (h->model->body_id != loadedBody && h->body.getNumSamples() > 0) {
            bodyConv.loadImpulseResponse(std::move(h->body), h->model->sample_rate, juce::dsp::Convolution::Stereo::no,
                                         juce::dsp::Convolution::Trim::no, juce::dsp::Convolution::Normalise::no);
            loadedBody = h->model->body_id;
        }
        if (h->model->room_id != loadedRoom && h->room.getNumSamples() > 0) {
            roomConv.loadImpulseResponse(std::move(h->room), h->model->sample_rate, juce::dsp::Convolution::Stereo::yes,
                                         juce::dsp::Convolution::Trim::no, juce::dsp::Convolution::Normalise::no);
            loadedRoom = h->model->room_id;
            roomLoaded.store(true);
        }
    }
    engine.set_performance(perfBox.take());
    const auto* model = engine.model();
    engine.live_palm = params.getRawParameterValue("live_palm")->load() > 0.5f;
    engine.midi_strings = params.getRawParameterValue("midi_strings")->load() > 0.5f
                              ? grmg::Engine::MidiStrings::channel_per_string
                              : grmg::Engine::MidiStrings::automatic;

    // ---------------------------------------------------------------- where the roll is
    grmg::Transport tr;
    tr.loop = params.getRawParameterValue("loop")->load() > 0.5f;
    double tempo = params.getRawParameterValue("tempo")->load();
    bool host = false;
    if (params.getRawParameterValue("follow_host")->load() > 0.5f)
        if (auto* ph = getPlayHead())
            if (auto pos = ph->getPosition()) {
                if (auto bpm = pos->getBpm()) tempo = *bpm;
                if (pos->getIsPlaying())
                    if (auto ppq = pos->getPpqPosition()) {
                        host = true;
                        tr.playing = true;
                        tr.beat = *ppq;
                    }
            }
    hostPlaying.store(host);
    tempo = juce::jlimit(20.0, 400.0, tempo);
    if (std::abs(tempo - currentTempo.load()) > 0.01) currentTempo.store(tempo);
    tr.beats_per_sample = tempo / 60.0 / sr;
    if (!host) {
        const double seek = internalSeek.exchange(-1.0);
        if (seek >= 0) internalBeat = seek;
        tr.playing = internalPlaying.load();
        tr.beat = internalBeat;
        if (tr.playing) {
            internalBeat += n * tr.beats_per_sample;
            const auto* perf = engine.performance();
            const double len = perf ? perf->length_beats : 16.0;
            if (tr.loop) internalBeat = std::fmod(internalBeat, len);
            else if (internalBeat > len + 4.0 * tempo / 60.0) internalPlaying.store(false);   // the end rings 4 s
        }
    }

    // ---------------------------------------------------------------- strings struck on the neck
    {
        const auto scope = uiFifo.read(uiFifo.getNumReady());
        auto play = [&](int start, int size) {
            for (int k = 0; k < size; ++k) {
                const auto& u = uiPlucks[(size_t) (start + k)];
                engine.pluck_string(u.string, u.fret, u.velocity);
            }
        };
        play(scope.startIndex1, scope.blockSize1);
        play(scope.startIndex2, scope.blockSize2);
    }

    // ---------------------------------------------------------------- the strings, MIDI at its sample
    float* d = dry.getWritePointer(0);
    int pos = 0;
    auto renderTo = [&](int at) {
        if (at <= pos) return;
        grmg::Transport part = tr;
        part.beat = tr.beat + pos * tr.beats_per_sample;
        engine.render(d + pos, at - pos, part);
        pos = at;
    };
    for (const auto meta : midi) {
        const auto msg = meta.getMessage();
        renderTo(juce::jlimit(0, n, meta.samplePosition));
        if (msg.isNoteOn()) engine.note_on(msg.getNoteNumber(), msg.getFloatVelocity(), msg.getChannel());
        else if (msg.isNoteOff()) engine.note_off(msg.getNoteNumber());
        else if (msg.isAllNotesOff() || msg.isAllSoundOff()) engine.all_off();
    }
    renderTo(n);

    // ---------------------------------------------------------------- body, then room
    bodyMix.setTargetValue(model ? (float) model->s.body_amount : 0.85f);
    volume.setTargetValue(params.getRawParameterValue("volume")->load());
    wet.clear();
    wet.copyFrom(0, 0, dry, 0, 0, n);
    {
        juce::dsp::AudioBlock<float> block(wet.getArrayOfWritePointers(), 1, (size_t) n);
        bodyConv.process(juce::dsp::ProcessContextReplacing<float>(block));
    }
    float* w0 = wet.getWritePointer(0);
    for (int t = 0; t < n; ++t) {
        const float a = bodyMix.getNextValue();
        w0[t] = (1.0f - a) * d[t] + a * w0[t];
    }
    wet.copyFrom(1, 0, wet, 0, 0, n);
    const bool room = model && model->s.room_on && roomLoaded.load();
    if (room) {
        juce::dsp::AudioBlock<float> block(wet.getArrayOfWritePointers(), 2, (size_t) n);
        roomConv.process(juce::dsp::ProcessContextReplacing<float>(block));
    }
    // newtons to the output: a fixed scale (the default song peaks around -9 dBFS at volume 1: headroom for
    // harder playing), then a peak limiter that only acts above LIMIT and reduces the gain smoothly instead of
    // bending the wave (no distortion when a strong stroke goes over)
    const int outs = buffer.getNumChannels();
    constexpr float SCALE = 0.018f, LIMIT = 0.89f;
    const float release = std::exp(-1.0f / (0.15f * (float) sr));
    for (int t = 0; t < n; ++t) {
        const float g = volume.getNextValue() * SCALE;
        float l = wet.getSample(0, t) * g, r = wet.getSample(1, t) * g;
        const float peak = std::max(std::abs(l), std::abs(r));
        const float need = peak > LIMIT ? LIMIT / peak : 1.0f;
        limiterGain = need < limiterGain ? need : 1.0f - (1.0f - limiterGain) * release;
        l *= limiterGain;
        r *= limiterGain;
        for (int ch = 0; ch < outs; ++ch) buffer.setSample(ch, t, outs == 1 ? 0.5f * (l + r) : (ch % 2 == 0 ? l : r));
    }
    // the oscilloscope
    const auto scope = scopeFifo.write(std::min(n, scopeFifo.getFreeSpace()));
    const float* o = buffer.getReadPointer(0);
    for (int k = 0; k < scope.blockSize1; ++k) scopeData[(size_t) (scope.startIndex1 + k)] = o[k];
    for (int k = 0; k < scope.blockSize2; ++k) scopeData[(size_t) (scope.startIndex2 + k)] = o[scope.blockSize1 + k];
}

int GRMGuitarProcessor::readScope(float* dest, int count) {
    const auto scope = scopeFifo.read(std::min(count, scopeFifo.getNumReady()));
    for (int k = 0; k < scope.blockSize1; ++k) dest[k] = scopeData[(size_t) (scope.startIndex1 + k)];
    for (int k = 0; k < scope.blockSize2; ++k) dest[scope.blockSize1 + k] = scopeData[(size_t) (scope.startIndex2 + k)];
    return scope.blockSize1 + scope.blockSize2;
}

void GRMGuitarProcessor::pluckFromInterface(int string, int fret, float velocity) {
    const auto scope = uiFifo.write(1);
    if (scope.blockSize1 > 0) uiPlucks[(size_t) scope.startIndex1] = {string, fret, velocity};
    else if (scope.blockSize2 > 0) uiPlucks[(size_t) scope.startIndex2] = {string, fret, velocity};
}

int GRMGuitarProcessor::getOpenMidi(int i) const {
    const int t = juce::jlimit(0, grmg::NUM_TUNINGS - 1, (int) std::lround(params.getRawParameterValue("tuning")->load()));
    return grmg::TUNINGS[t].midi[i];
}

juce::String GRMGuitarProcessor::getStatus() const {
    if (exporting.load()) return "Rendering the roll to a WAV file...";
    {
        std::lock_guard<std::mutex> lock(exportLock);
        if (exportResult.isNotEmpty()) return exportResult;
    }
    if (sampleRate.load() <= 0) return "Waiting for the audio device";
    if (solving.load()) return solvedStage.load() >= 1 ? "Solving the palm and the room networks..." : "Solving the networks...";
    return "Networks solved";
}

grmg::Song GRMGuitarProcessor::getSong() const {
    std::lock_guard<std::mutex> lock(songLock);
    return song;
}

void GRMGuitarProcessor::setSong(const grmg::Song& s) {
    {
        std::lock_guard<std::mutex> lock(songLock);
        song = s;
    }
    songVersion.fetch_add(1);
    solver.notify();
}

void GRMGuitarProcessor::newTake() {
    seed.store((unsigned) juce::Random::getSystemRandom().nextInt(1 << 30) + 1u);
    solver.notify();
}

void GRMGuitarProcessor::setInternalPlaying(bool on) {
    if (on && !internalPlaying.load()) internalSeek.store(0.0);
    internalPlaying.store(on);
}

juce::AudioProcessorEditor* GRMGuitarProcessor::createEditor() { return new GRMGuitarEditor(*this); }

void GRMGuitarProcessor::getStateInformation(juce::MemoryBlock& destData) {
    auto state = params.copyState();
    state.setProperty("song", juce::String(getSong().to_text()), nullptr);
    state.setProperty("seed", (int) seed.load(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary(*xml, destData);
}

void GRMGuitarProcessor::setStateInformation(const void* data, int sizeInBytes) {
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(params.state.getType())) {
            auto tree = juce::ValueTree::fromXml(*xml);
            if (tree.hasProperty("song")) {
                grmg::Song s;
                if (s.from_text(tree.getProperty("song").toString().toStdString())) setSong(s);
                stateVersion.fetch_add(1);
            }
            if (tree.hasProperty("seed")) seed.store((unsigned) (juce::int64) tree.getProperty("seed"));
            params.replaceState(tree);
            solver.notify();
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new GRMGuitarProcessor(); }
