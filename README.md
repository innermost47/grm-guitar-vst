# GRM Guitar

A guitar instrument (VST3 and standalone, built with JUCE) where every part of the sound and of the playing is a
GRM network: the strings, the attack, the palm, the fingers on the frets, the player's two hands, the body, the
pickups and the room. Several guitars in one plugin, a guitar roll that knows a guitar has six strings, and hands
that refuse what no guitarist can play. Free software under the AGPL-3.0 (JUCE 8 is itself available under the
AGPL-3.0).

## Tracks composed with GRM Guitar

- [A New Path To Trace](https://anthony-charretier.fr/post.php?slug=a-new-path-to-trace)
- [Trembling Hands](https://anthony-charretier.fr/post.php?slug=trembling-hands)
- [His Own Story](https://anthony-charretier.fr/post.php?slug=his-own-story)

## Videos demos

[![Watch the GRM Guitar demo on YouTube](https://img.youtube.com/vi/PHgxrdl3X0Q/maxresdefault.jpg)](https://www.youtube.com/watch?v=PHgxrdl3X0Q)  

[![!With feedback larsen](https://img.youtube.com/vi/dbz1AT30W6M/maxresdefault.jpg)](https://www.youtube.com/watch?v=dbz1AT30W6M)

## The GRM family

- **[grm-core](https://github.com/innermost47/grm-core)**: the Global Relational Model, the research it comes
  from, its tests and the equation found between the data. The guitar was first written there as a Python proof of
  concept (`grm_guitar.py`); this plugin is the product.
- **[pygrm](https://pypi.org/project/pygrm/)** (`pip install pygrm`): the GRM engine as a Python package, to solve
  your own networks. The C++ engine of this plugin (`core/grm.hpp`) is the same algorithm, step by step.

A GRM network is a set of quantities (nodes) tied by relations, some of them measured, with their uncertainties.
The network finds every value that agrees with all the relations at once. Here the measured quantities come from
the literature on guitars (string losses, wood, pickups, hands): see [Sources](#sources).

## The sound

- **Strings.** Each string at each fret is a network: tuning, tension, inharmonicity (the stiffness of the steel),
  wound strings. In the engine, each string is a bank of up to 80 modes pushing on the bridge.
- **How long each partial rings** follows the losses measured on real steel strings (Woodhouse 2012): inside the
  string, in the air, in the steel as it bends. The low partials ring longest, the high ones go first. **String
  age** moves the strings from new to played (played strings lose their highs sooner).
- **Attack.** Pick or fingers, force, pluck point, pick thickness: the attack network gives the shape the string
  leaves the pick with.
- **Palm.** The exact modes of a string under a strip of skin, at the palm's place and pressure. The pressure acts
  progressively from a light touch to a full mute; when the palm lands, the strings ringing under it get the
  palm network's own damping.
- **Body.** The air cavity coupled to the top (Helmholtz), the top's modes, the bridge, the radiation. The top
  carries its braces and the bridge (a top weighs about 200 g with them) and every mode has the measured Q (about
  50). **Wood**: 8 species with their density and stiffness (Wood Handbook), and the piece's density against its
  species' average. **Body: solid to hollow**: from a flat-top acoustic to a solid body (no cavity, a 44.5 mm slab).
- **Sensor.** A **microphone** (body and room), a **magnetic pickup** (the string's velocity where the pickup sits,
  through the coil, the cable and the volume pot: a 1970s single coil or a hotter one, measured values), **three
  magnetic pickups (HSS)** — bridge, middle and neck, each with its own on/off switch, the bridge splittable from
  humbucker to single coil, the cable shared by all three, each through its own coil and load then mixed: every
  classic selector position, from the bridge alone to the neck alone and back through the in-between combinations
  — or a **piezo** under the saddle (the force on the bridge).
- **Room.** Reverberation by band, critical distance, image sources, a tail per ear. Guitars in the same room
  going to the same output share one room (it costs as much as the strings).
- **Sympathetic strings.** The strings nobody plays answer the others through the bridge.
- **Amplifier.** A whole rig behind the magnetic pickups, its circuits solved by GRM networks: an overdrive pedal
  (op-amp gain above its corner, soft diode clipping), clean and lead channels to blend, each a voicing (American
  Clean, British Crunch, British Lead, Modern High Gain) of cascaded 12AX7 stages that share their supply (operating
  points, curves on their load lines, grid conduction and blocking, cathode and coupling corners, thermal noise),
  the exact tone stack of its circuit, a push-pull power stage (sag, negative feedback with presence and resonance),
  a cabinet from its speaker's Thiele-Small parameters and a close microphone, and a noise gate keyed by the guitar.
  Four times oversampled.
- **Acoustic Feedback (Larsen).** Real-time closed-loop acoustic coupling between the virtual speaker and the strings. Sound travels through virtual air ($d/c$), exciting the string modes to create natural, self-sustaining singing feedback that responds dynamically to distance, palm mutes, and selectable harmonic targets.

## The player

- **Notes ring as long as the hands let them.** An open string rings until it is played again; a fretted note
  rings until its string is played again or until the left hand needs that finger elsewhere. The roll draws what
  rings as a fading trail. Each note can instead **stop at its end** (the finger lifts) or be **muted with the
  hand** (a flat hand stops it and the strings beside it). **Hand stops** (Alt+click, on the grid's steps or
  between them) stop every string at that moment.
- **The hands are constraints** (`core/playability.hpp`). The left hand has four fingers (the thumb can take the
  low E); a barre is one finger; the stretch is a GRM network (fingertips behind their frets against the hand's
  natural spread). A pick cannot jump a string; the fingers can, five at most. One hand cannot strum and pick
  single notes at the same moment. The roll refuses an edit no guitarist can play, says why, and **greys the
  places the hand cannot reach** with the fret being written.
- **The right hand.** Notes struck together with a pick are one sweep (about 5 ms between strings, the later ones
  a little softer); single notes are picked down and up; with the fingers, a pinch (the thumb slightly first).
- **The arm** swings, tires on long runs, comes back fresh after a rest. **Human feel** and **Looseness** set how
  far the player strays from the grid (timing, force, pluck point); **New take** plays the same roll again with
  other imprecisions.
- **The fingers on the frets** lift, land and slide on wound strings (finger noises).

## The guitar roll

One lane per string (string 1 on top, as in a tablature) and a lane of strums.

- **Notes**: click a lane to write one (the fret of the "brush", heard as it is written: "Hear the notes written"),
  drag to set its length; a new note stops before the next one on its string. Drag its edges to resize it from the
  right or from the left, **drag its top or bottom edge over other strings** to write the same note on them (a
  barre, a chord shape; then give each its own fret). Wheel, digits or +/- change the fret; X makes a **dead note**
  (the finger only rests on the string); P palm mute.
- **Strums**: a chord shape swept down or up. **Chords**: every root with its sharp and flat names, 16 kinds
  (major, minor, 7, m7, maj7, 6, m6, sus2, sus4, add9, 9, power, dim, aug, m7b5, dim7). The shapes are not written by
  hand: the left-hand network chooses the easiest playable one for the guitar's tuning (the usual open shapes stay
  in standard tuning).
- **Fill strums / Fill arpeggio**: over the selection (any length) or over the bars from the current one. The
  chords already written there are kept: the pattern only gives the rhythm (select held notes and fill strums to
  strum them).
- **Right click**: fret, ending, dynamics, palm, dead note, chord (by root and kind), transpose, **harmonize** (a
  third, fourth, fifth, sixth or octave above, a third or sixth below, in the pattern's **key and scale**: the new
  note goes where the hand can play it), **copy to guitar** (one or all the others), copy, paste, delete.
- **Loop**: two flags in the ruler. Writing past the end does not change the length or the loop; pulling the end
  flag past the end makes the song longer.
- Multiple selection (Ctrl+click, Ctrl or Shift+drag a rectangle, Ctrl+A), copy, cut, paste, duplicate (also
  between two instances), undo, arrows, Ctrl+wheel zoom, Space play. The roll follows the playhead or not
  ("Follow playhead").

## Patterns

Each guitar has up to 24 patterns (New, Duplicate, Delete, Copy, Paste). **MIDI keys** chooses what the keyboard
does:
- **Play notes**: a key plays a note on the guitar (on the string that plays it lowest on the neck, or channel =
  string).
- **Play patterns**: C1 plays pattern 1, C#1 pattern 2... The pattern plays its loop from the key, looped while the
  key is held, on every guitar that has it (a scene), or only on the guitar of the MIDI channel ("MIDI to guitars:
  Channel = guitar"). Write the arrangement with the host's piano roll; the notes of the pattern before end as they
  were written.

## Several guitars

Up to 8 guitars in one plugin, each with its own settings, patterns, player and mixer strip.
- **Mixer**: gain (dB), level, pan, mute, solo, and the output: the main one or the guitar's own (enable
  "Guitar 1" to "Guitar 8" in the host to put each guitar on its own channel with its own effects).
- **The other guitars, faint, behind the roll**, with the name of each note and of each chord where it changes, in
  their colour ("Other guitars"). A shorter pattern is drawn as many times as it repeats under a longer one.
- The header shows where the pattern is and where the whole song is.
- Sessions of 0.2.0 open as one guitar, as they were: guitar 1 keeps every parameter of the one-guitar versions.

## Architecture

- `core/grm.hpp`: the GRM engine in C++ (header only), the algorithm of grm-core and pygrm.
- `core/guitar_networks.hpp`, `core/physics.hpp`: the networks (strings and their losses, attack, palm, body,
  pickup (one, or three for the HSS set), room, fingers, player, strums).
- `core/model.hpp`: everything the networks give for the current settings, solved off the audio thread.
- `core/song.hpp`, `core/chords.hpp`, `core/ringing.hpp`, `core/playability.hpp`: the roll, the chords and scales,
  how long notes ring, the hands.
- `core/performance.hpp`: the roll turned into what the hands do (player network, sweep, pinch, palm, lifts,
  landings, endings, hand stops).
- `core/voice.hpp`, `core/engine.hpp`: the real time part. Its output is the force on the bridge in newtons (and
  the string velocity under the pickup).
- `plugin/GuitarTrack.*`: one guitar (settings, patterns, networks, strings, sensor, body and room); the processor
  holds the guitars, the shared rooms, the mixer and the outputs. A track is the place another GRM instrument
  would plug in.
- Networks are solved in a background thread; the audio thread takes finished results between two blocks, without
  locks or allocation (`grmg::Mailbox`).

## Tests

`ctest` runs the first six:
- `test_core`: 210 checks that the C++ GRM gives the same numbers as the Python one (values to 1e-9).
- `test_physics`: 153 checks of the networks (body, bridge, room, palm, finger, action, player, strum, string
  losses, loaded top) and of one string's sound against the proof of concept.
- `test_play`: 74 checks of the playing (hands, sweep, pinch, muscle, picking, palm, loop flags, ringing, endings,
  hand stops, chords on every tuning, scales, fills, dead notes, endings across a pattern change, legato on one
  string, the string's two planes, the hard attack: frets, pick push and scrape, pitch glide).
- `test_amp`: 38 checks of the amplifier (operating points, tone stacks against their H(s), cabinet, clean and high
  gain, compression, aliasing, hiss and gate, presence, the overdrive's tight lows).
- `test_modal`: 21 checks of the dialogs and the update check (versions compared as numbers, GitHub's answer, the
  equations and the grm-core link on the left of a dialog, closing it).
- `test_icons`: every icon is embedded and drawn, every icon button of the interface has its tooltip and its
  design-system size.
- `plugin_host_test`: the plugin called as a host would (MIDI, patterns, sensors, several guitars, own outputs,
  sessions of 0.2.0, state).
- `plugin_cpu_test [guitars]`: the share of each audio block the plugin takes with several guitars.
- `render_offline [out.wav] [song.txt] [acoustic | clean | crunch | lead | metal]`: the whole path without a host,
  acoustic or through the amplifier.
- `editor_snapshot out.png`: the editor drawn to a picture.

Measured on the build machine (Linux, 48 kHz, 256-sample blocks): one guitar about 5 % of a block on average,
four guitars about 14 %.

## Build

Needs CMake 3.22+ and Git (JUCE is downloaded by CMake); on Windows, Visual Studio 2022 with the C++ workload.

```bat
cmake -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
ctest --test-dir build -C Release

```

The plugin is in `build\GRMGuitar_artefacts\Release\VST3\GRM Guitar.vst3` (copy it to
`C:\Program Files\Common Files\VST3`), the standalone in `build\GRMGuitar_artefacts\Release\Standalone`.

**Linux:** needs the ALSA, X11, Xrandr, Xinerama, Xcursor, freetype and fontconfig development packages.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

```

On `x86_64`, Release builds target `x86-64-v3` (requiring **AVX**, **AVX2**, and **FMA**; check with `lscpu | grep -iE "avx|fma"`). If a prebuilt binary crashes at load with `SIGILL` (signal 4) or if the target CPU lacks AVX2, build with `-DGRMG_PORTABLE_CPU=ON`:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DGRMG_PORTABLE_CPU=ON
cmake --build build

```

**Validation:** [pluginval](https://github.com/Tracktion/pluginval), `pluginval --strictness-level 5 --validate "GRM Guitar.vst3"`.

## Known limits

- The body has seven modes below about 1.4 kHz: its resonances stand out more than on a real guitar, whose modes
  get denser up high (moving the body settings changes the "vowel" more than a real guitar would).
- Some chord shapes chosen by the hand network are unusual (they are playable; edit them in the roll).
- A magnetic pickup listens at one point (the width of its magnetic window is not modelled yet); the HSS set uses
  three fixed positions (65 mm, 160 mm, 255 mm from the saddle) and the three pickups share one cable capacitance.
- On a solid body, a real low E fundamental may ring longer than the measured string losses give here (Zollner's
  measurements on electric guitars): to be checked.
- Live MIDI playing has no finger noises (they are computed for the roll).
- Linux: the same commands with the default generator (needs the ALSA, X11, Xrandr, Xinerama, Xcursor, freetype
and fontconfig development packages). On x86_64, release builds target `x86-64-v3` (requiring **AVX**, **AVX2**,
and **FMA**); if a prebuilt binary fails with `SIGILL` (signal 4) or if the target CPU lacks AVX2, build from source
with `-DGRMG_PORTABLE_CPU=ON` (or pass `-DCMAKE_CXX_FLAGS="-march=x86-64"`). You can check CPU support with
`lscpu | grep -iE "avx|fma"`.

## Next

Slides, hammer-ons, pull-offs and bends between notes; automation lanes under the roll (palm, force...); the guitar
drawn as it is at each moment; presets and custom tunings shared between projects; a strum and arpeggio editor;
a new look (light and dark); modulators; one network holding all the others (the whole hand, the whole guitar);
12-string guitar and bass.

## Sources

- J. Woodhouse, "Perceptual thresholds for acoustical guitar models", Acta Acustica united with Acustica (2012):
  string losses (new and played), inharmonicity, Helmholtz and top ranges.
- J. Woodhouse et al., Acta Acustica (2021), on the banjo compared with the guitar: top mass with braces and
  bridge, mode Q, why a light, mobile bridge sounds like a banjo.
- US Forest Products Laboratory, Wood Handbook, tables 5-3b and 5-5b: wood density and stiffness.
- H. Lemme's pickup measurements (via ozvalveamps.org): pickup inductance, resistance, capacitance.
- M. Zollner, Physics of the Electric Guitar, ch. 7.7: string decay on electric guitars.
- J. Wolfe, UNSW, "How does a guitar work?": the lowest air and top resonances.

## Credits

- Icons: [Phosphor Icons](https://phosphoricons.com) (fill style), MIT license. The strum and arpeggio icons are
  drawn for GRM Guitar on the same grid.
