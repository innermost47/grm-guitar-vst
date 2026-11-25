# Contributing

Read [The GRM in practice](grm.md) and [Architecture](architecture.md) first. This page is the how-to.

## Build and test

```bat
cmake -B build-tests -DGRMG_BUILD_TESTS=ON
cmake --build build-tests --config Release
ctest --test-dir build-tests -C Release
```

`GRMG_BUILD_TESTS=ON` builds the tests and the tools next to the plugin. `ctest` runs the checks; the tools are
run by hand:

| Tool | What for |
| --- | --- |
| `plugin_host_test` | the plugin called as a host would (MIDI, patterns, sensors, several guitars, sessions) |
| `plugin_cpu_test [guitars]` | the share of each audio block the plugin takes |
| `render_offline [out.wav] [song.txt] [acoustic \| clean \| crunch \| lead \| metal]` | the whole sound path without a host |
| `timbre_probe <folder> [take.wav]` | the sound's yardstick: WAV files and `metrics.csv` (decay and Q of each partial, tuning, brightness, spread of a strum, a held chord's motion), and the tonal balance against a real recording |
| `editor_snapshot out.png` | the editor drawn to a picture, with test content (layout checks) |
| `app_screenshot out.png [seconds]` | the documentation's screenshot: the standalone's interface as it opens, playing its demo (`docs/screenshot.png`) |

Before and after any change to the sound, run `timbre_probe` and compare the two `metrics.csv`; after any change
to `StringVoice` or the pickups, measure the CPU (`render_offline ... metal`, quietly: a build running at the
same time distorts the figure).

## Adding a setting

Follow one that exists. `pickup_height_mm` goes through these places:

1. `models/core.hpp`: a field of `Settings`, with its default and a comment saying what it is physically.
2. `plugin/PluginProcessor.cpp` (`createLayout`): its parameter, `real(id, name, min, max, default, centre,
   step)`, `choice(...)` or `flag(...)`. Every guitar gets its own copy (`g2_pickup_height_mm`...).
3. `plugin/GuitarTrack.cpp` (`readSettings`): read into `Settings`, rounded to the steps that make sense (every
   distinct value may cost a network solve).
4. `core/model.hpp`: the right `same_*` comparison, so that changing it solves again what depends on it, and only
   that. A setting that needs no network can patch a copy of the model in `GuitarTrack::solveStep`.
5. The network that uses it (here `pickup_sensing_network`, called by `Model::solve_pickup`).
6. `plugin/PluginEditor.cpp`: its tab (`addTab`), and `showOnlyWhen` if it only matters for some values of another
   setting.

Settings of the player are one list, `Settings::player()`: add the field there and it is compared, copied and
compiled with the others. Sliders that only tune the sound by ear are lines of the `Tunable` tables of
`constants/core.hpp` (`HAND_TUNABLES`, `VOICE_TUNABLES`...): one line makes the parameter, reads it, compares it.

Projects saved before the setting existed open with its default: choose a default that sounds as before.

## Adding a network

1. Write it as one function in the file of its domain (`core/guitar_networks.hpp`, `core/physics.hpp`,
   `core/amp/amp_networks.hpp`), returning a `grm::Result` or a struct of `models/`.
2. Put its data in `constants/core.hpp` with **where it comes from** (paper, table, figure) and its spread. The
   spread is the measurement's sigma: do not tighten it to get the number you want.
3. Name every relation after what it says ("wave speed", "release time"); the name is the documentation and what
   an error message shows.
4. Pin physics that holds exactly with a tiny sigma (`1e-9`); let data and intentions weigh against each other with
   their real spreads.
5. Solve it in `Model::solve` (sound) or in the compiler (`core/performance.hpp`, playing), never in the audio
   thread. If it is slow, keep its result while its inputs do not change (see `same_*`).
6. Test it: its values against a published measurement, within the measurement's uncertainty.

If the network ports one of `grm_guitar.py` (grm-core), add it to `tools/make_golden.py` and check it in
`test_core` or `test_physics` against `tests/golden.hpp`: the C++ must give the Python's numbers.

## Tests

Every change comes with a test that fails without it. For a bug: reproduce it first (a small probe), find its
cause, write the test, then check that the test fails when the fix is removed, and passes when it is back.

| ctest name | File | Checks |
| --- | --- | --- |
| `grm_core_matches_python` | `test_core.cpp` | the C++ GRM against the Python one (1e-9) |
| `guitar_physics_matches_python` | `test_physics.cpp` | the networks against the Python ones and the literature |
| `guitar_playing` | `test_play.cpp` | hands, sweep, pinch, palm, ringing, endings, chords, scales |
| `guitar_amplifier` | `test_amp.cpp` | operating points, tone stacks, cabinet, aliasing, gate, larsen |
| `guitar_magnetic_pickup` | `test_pickup.cpp` | the pickups' window, coils, flux against distance |
| `guitar_string_sets` | `test_strings.cpp` | string sets, tensions against the maker's chart, losses, materials |
| `guitar_live_playing` | `test_live_playing.cpp` | the groove, the strums' timing, Imperfection |
| `guitar_hand_contact` | `test_hand_contact.cpp` | a hand landing on and leaving a ringing string |
| `guitar_feel_lanes` | `test_feel_lanes.cpp` | the Feel lanes, presets (keeping Timing and Palm), clone, variations, the meter |
| `guitar_free_tuning` | `test_tuning.cpp` | custom tunings |
| `host_play_stop_play` | `test_host_resume.cpp` | play, stop, a setting changed, play again (patterns from the keys) |
| `interface_icons` | `test_icons.cpp` | icons, tooltips, sizes, the mixer following the guitars |
| `interface_dialogs` | `test_modal.cpp` | dialogs and the update check |
| `interface_presets` | `test_presets.cpp` | factory and user presets, the session |
| `timbre_probe_runs` | `timbre_probe.cpp` | every rendered sound is finite and audible |

## Code conventions

- **English everywhere** in the code: names, comments, logs.
- **One class per file, one widget per file.** Small, decoupled files; no copied code: share it.
- **Constants** go in `constants/` (`core.hpp` physics and data, `fonts.hpp`, `sizes.hpp`, `theme.hpp` colours,
  `ui.hpp` interface), **structs** in `models/`. No magic numbers in the code.
- **Design system**: colours, sizes and fonts come from the theme (`ui::theme::AppTheme`) and the look and feel
  (`CustomLookAndFeel`); no ad hoc styling. Buttons are SVG icons (`assets/icons/`, Phosphor fill) with a
  tooltip.
- **Refresh** components with `juce::VBlankAttachment`, never a `Timer`.
- **Real time**: nothing in the audio thread allocates, locks or solves a network.
- **Line endings**: some files are CRLF (`constants/`, some of `models/` and `plugin/`), others LF. Keep each
  file's own; `sed -i` under Git Bash turns CRLF into LF.
- C++17: no defaulted `operator==` (compare with `std::tie`).
