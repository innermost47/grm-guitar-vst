# Architecture

GRM Guitar rests on one split: **the GRM networks are solved when something changes, never in the audio
callback.** Their results are plain data; the audio thread only plays them. Everything below follows from that.

```
 settings (parameters)          guitar roll (patterns)
        |                               |
        v                               v
 +--------------- solver thread (SolverThread, one per plugin) ---------------+
 |  Model::solve       the networks of the sound   -> grmg::Model            |
 |  compile_song       the networks of the player  -> grmg::Performance      |
 +-----------------------------------------------------------------------------+
        |  Mailbox::post (no lock)              |  Mailbox::post
        v                                       v
 +--------------------------- audio thread (processBlock) --------------------+
 |  Engine: strings (banks of modes) -> force on the bridge, pickups, amp      |
 |  GuitarTrack::finish: body and room (convolution), listener, mixer strip    |
 +-----------------------------------------------------------------------------+
        |  atomics (levels, playhead)
        v
 editor (message thread, refreshed by VBlankAttachment)
```

## The core, without JUCE

Everything in `core/`, `models/` and `constants/` is plain C++17 with no JUCE: it is tested, rendered offline and
measured without a host.

- `constants/`: the measured data and the tables (`core.hpp`: strings, woods, pickups, rooms, the player's
  sigmas, each with its source; `ui.hpp`, `sizes.hpp`, `theme.hpp`, `fonts.hpp`: the design system).
- `models/`: the structs that travel between the parts (`Settings`, `Place`, `Body`, `Palm`, `Stroke`,
  `AmpModel`...).
- The networks, one function each, returning a `grm::Result` or a struct of `models/`:
  - `core/guitar_networks.hpp`: the string at a fret, its losses, the attack (pick or finger);
  - `core/physics.hpp`: body and bridge, palm, room, fingers on wound strings and frets, the pickups' circuit and
    sensing, the player (`player_network`), the pick crossing the strings;
  - `core/amp/amp_networks.hpp`: the amplifier's circuits (triodes sharing a supply, tone stack, power stage,
    pedal, cabinet);
  - `core/feedback.hpp`: the larsen's loop.
- `core/model.hpp`: **`Model`**, everything the networks give for one set of settings: each string at each of its
  frets, losses, attack, palm, pickups, body and room impulse responses, amplifier, larsen.
- `core/song.hpp`, `chords.hpp`, `playability.hpp`, `ringing.hpp`: the roll as data, chords and scales, what a
  hand can play, how long notes ring.
- `core/meter.hpp`, `feel_curve.hpp`, `feel_generator.hpp`, `player_feel.hpp`: the groove: the bar's accents, the
  Feel lanes, what the player aims at on each stroke.
- `core/performance.hpp`: **`Compiler`**, from the roll to a **`Performance`**: a list of events in beats (strikes
  with their shape and force, palms landing and lifting, fingers, endings), computed by the player, sweep, palm and
  finger networks.
- `core/engine.hpp`, `core/voice.hpp`, `core/magnetic_pickup.hpp`, `core/amp/*`: the real-time part.
- `core/offline_renderer.hpp`: the engine without a host, for tools and tests.

## The solver thread

`plugin/solver/SolverThread.cpp` loops over the guitars, every 30 ms, and calls `GuitarTrack::solveStep`:

1. **The sound.** If the settings that feed a network changed, a new `Model` is solved. It is done twice: first a
   quick one without the palm networks (heard at once), then the full one. `Model::solve` takes the previous model
   and keeps what did not change (`same_strings`, `same_attack`, `same_body`, `same_room`...): moving the room
   does not solve the strings again. Settings that need no network (string age, pickups, sympathy) only patch a
   copy of the model.
2. **The playing.** If a pattern, the tempo, the seed, the player's settings or the model changed, the patterns are
   compiled into `Performance`s (only the changed ones when nothing else moved).

A setting no network can solve throws; the error is logged and the guitar keeps its last model.

## From the solver to the audio thread: `Mailbox`

`grmg::Mailbox<T>` (`core/engine.hpp`) hands a finished object to the audio thread without a lock and without
freeing memory there:

- the solver `post`s a new object (`new`), replacing one the audio thread has not taken yet;
- the audio thread `take`s it between two blocks and puts the old one in a trash slot;
- the solver frees the trash on its next pass (`collect`).

The audio thread never allocates, never frees, never waits. `GuitarTrack` has two mailboxes: `modelBox` (the
model with its body and room impulse responses ready for the convolutions) and `perfBox` (the compiled patterns).

## The audio thread

`GRMGuitarProcessor::processBlock` reads the host's transport and MIDI, then for each guitar:

- `GuitarTrack::begin`: takes a new model or performance from the mailboxes, if any.
- `Engine::render` (`core/engine.hpp`): reads the performance's events at the playhead and runs the strings. Each
  string is a `StringVoice` (`core/voice.hpp`): a bank of up to 80 modes, each a damped complex oscillator, whose
  frequencies and losses come from the model. A hand landing on a string (`damp`) or leaving it (`lift`) moves the
  modes over a few milliseconds. The strings nobody plays answer through the bridge (sympathetic strings). The
  output is the force on the bridge in newtons, and the string's motion over the magnetic pickups
  (`MagneticPickup`: the window of the poles, the coils), through the cable and, if on, the amplifier rig.
- `GuitarTrack::finish`: the sensor's signal (microphone: through the body's impulse response; piezo; magnetic:
  the jack), then the listener's place in the room and the room's reverberation. Guitars in the same room share
  one reverberation (`roomKey`): it costs as much as the strings.
- The mixer strip (gain, level, pan, mute, solo), the guitar's own output or the main one, the main volume, and
  a peak limiter on every output.

Levels for the meters go back to the editor through atomics (`takeStripPeaks`, `takeMasterPeaks`).

## The plugin and its interface

- `plugin/PluginProcessor.*`: the parameters (one set per guitar, prefixed by its index: `paramId`), the guitars,
  the shared rooms, the state (the parameters, and each guitar's patterns as text, Feel lanes included).
- `plugin/GuitarTrack.*`: one guitar: reads its parameters into a `models::Settings` (`readSettings`), owns its
  patterns, its engine and its mailboxes. Another GRM instrument would plug in here.
- `plugin/PluginEditor.*`: the layout. On the left the settings of the selected guitar (`SideTabs`, `ParamPanel`)
  and the networks' figures (`NetworksView`); on top the transport (`TransportBar`), the mixer (`MixerView`,
  `MasterStrip`) and the neck (`NeckView`); below, the roll (`RollView`) with its Feel lanes (`StepLane`).
- `plugin/widgets/`: one widget per file (`IconButton`, `VuMeter`, `StepLane`, `Modal`...).
- `plugin/style/CustomLookAndFeel.*`: how every control is drawn, from the design system of `constants/`.
- Views refresh with a `juce::VBlankAttachment` (never a `Timer`): they read atomics or a snapshot of the model
  (`GuitarTrack::shownModel`).

## Threads at a glance

| Thread | Does | Never does |
| --- | --- | --- |
| audio | plays the strings, the amp, the convolutions | solve a network, allocate, lock |
| solver | solves the networks, compiles the patterns, frees old results | touch the audio buffers |
| message | the interface, the parameters, edits of the roll (under `songLock`) | wait for the solver |
