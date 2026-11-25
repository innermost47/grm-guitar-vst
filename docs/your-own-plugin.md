# Your own plugin with cppgrm

GRM Guitar is one instrument built on [cppgrm](https://github.com/innermost47/cppgrm). The same recipe works for
any physical model (a drum, a bowed string, a room, an amplifier, a vocal tract) and for things that are not sound
at all. This page gives the recipe; the guitar's code is the worked example.

## 1. Fetch the engine

cppgrm is one header with no dependency (C++17):

```cmake
include(FetchContent)
FetchContent_Declare(cppgrm
    GIT_REPOSITORY https://github.com/innermost47/cppgrm.git
    GIT_TAG        main # or a fixed commit, for reproducible builds
    GIT_SHALLOW    ON
)
FetchContent_MakeAvailable(cppgrm)

target_link_libraries(MyPlugin PRIVATE cppgrm)
```

## 2. Write the physics as networks

For each part of the model, list its quantities, the relations between them, and what you can measure. Write one
function per network that takes measured values and returns what the sound needs (see
[The GRM in practice](grm.md)). Prototyping it first in Python with [pygrm](https://pypi.org/project/pygrm/) is
often quicker; the C++ gives the same numbers.

Rules that served GRM Guitar well:

- **Measure the inputs, derive the rest.** A user setting should be something physical (a gauge, a length, a
  material) or musical (a groove), not a filter coefficient.
- **Keep the sources.** Every constant with its paper or table, and its spread as the sigma.
- **Return plain structs.** The audio side should not know a network exists.

## 3. Split the work between threads

The networks take from microseconds to tens of milliseconds: never solve them in the audio callback.

```
message thread     parameters change
      |
worker thread      solve the networks -> new Model     (allocates freely)
      |  Mailbox::post
audio thread       Mailbox::take between two blocks -> run the DSP with the Model
```

- **A `Model` struct** holds everything the DSP needs, already computed (frequencies, decays, impulse responses,
  filter coefficients). GRM Guitar's is `core/model.hpp`.
- **A worker thread** watches the settings, solves a new model when they change, and keeps the parts whose inputs
  did not change (`Model::solve(settings, rate, previous)`). GRM Guitar's is `plugin/solver/SolverThread.cpp` with
  `GuitarTrack::solveStep`.
- **A lock-free mailbox** hands the new model over: the worker posts, the audio thread takes it between two blocks,
  and the old one is freed later by the worker, never on the audio thread. GRM Guitar's is `grmg::Mailbox` in
  `core/engine.hpp` (25 lines).
- **The DSP** is ordinary and fast: banks of damped oscillators, convolutions, filters, with the model's values.
  When a model replaces another, glide what would click (`core/smoothed.hpp`).

[`examples/modal_string.cpp`](examples/modal_string.cpp) is this whole pattern in one file, without JUCE: a stiff
string's network solved on a worker thread, posted through a mailbox, played as a bank of 40 modes.

```
g++ -std=c++17 -O2 -I build/_deps/cppgrm-src docs/examples/modal_string.cpp -pthread
f1 82.42 Hz, partial 20 at 1681.3 Hz (harmonic: 1648.3)
```

## 4. Keep the core without the framework

Put the networks, the model and the DSP in headers that do not include JUCE (GRM Guitar's `core/`, `models/`,
`constants/`). Then:

- tests link only cppgrm and run in a second (`test_core`, `test_physics`, `test_play`...);
- an offline renderer plays the whole sound path to a WAV (`core/offline_renderer.hpp`, `tests/render_offline.cpp`);
- the plugin layer (`plugin/`) stays thin: parameters into settings, settings into the worker, the engine into
  `processBlock`.

## 5. Test against the world

A physical model is only as good as its comparison with reality. Check the networks' values against published
measurements within their uncertainty, render sounds and measure them (decay of each partial, brightness, timing;
GRM Guitar's `timbre_probe`), and when you can, compare with a real recording.

## Licence

cppgrm and GRM Guitar are free software under the AGPL-3.0: a plugin built on them, distributed, shares its source
under the same licence.
