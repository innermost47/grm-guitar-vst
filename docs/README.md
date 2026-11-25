# GRM Guitar: developer documentation

GRM Guitar is not built like most guitar plugins. There are no samples and no hand-tuned filters: every number
the sound needs (a string's tension, how long each partial rings, where a pickup hears the string, how hard the
player hits the third stroke of a bar) comes out of a **GRM network**, a small system of physical relations solved
against measured data. These pages explain how that works and how the code is organised around it.

## Where to start

| If you want to... | Read |
| --- | --- |
| understand what a GRM network is, with an example that runs | [The GRM in practice](grm.md) |
| see how the plugin turns networks into sound in real time | [Architecture](architecture.md) |
| add a setting, a network, a test or a feature to GRM Guitar | [Contributing](contributing.md) |
| build your own instrument or effect on top of cppgrm | [Your own plugin with cppgrm](your-own-plugin.md) |

If you are just curious, read the first two in order: about twenty minutes.

## The pieces

- **[cppgrm](https://github.com/innermost47/cppgrm)**: the GRM engine, one C++17 header (`grm.hpp`), no
  dependency. CMake fetches it; it ends up in `build/_deps/cppgrm-src/grm.hpp`.
- **[grm-core](https://github.com/innermost47/grm-core)**: the research behind the model, and the Python proof of
  concept of the guitar (`grm_guitar.py`). Many networks here are its ports, checked number for number.
- **[pygrm](https://pypi.org/project/pygrm/)**: the same engine as a Python package, handy to prototype a network
  before writing it in C++.
- **This repository**: the networks of a guitar (`core/`), the real-time engine that plays their results
  (`core/engine.hpp`, `core/voice.hpp`) and the JUCE plugin around them (`plugin/`).

## Examples

- [`examples/string_tension.cpp`](examples/string_tension.cpp): a string's network, solved from three
  measurements. Build it with `g++ -std=c++17 -I build/_deps/cppgrm-src docs/examples/string_tension.cpp` (or `cl
  /std:c++17 /EHsc /I build\_deps\cppgrm-src ...`) after a first CMake configure. It prints a tension of 77.8 N,
  the one D'Addario publishes for that string.
- [`examples/modal_string.cpp`](examples/modal_string.cpp): the plugin's whole pattern in one file, without JUCE:
  a network solved on a worker thread, handed to the audio loop through a lock-free mailbox, played as a bank of
  modes (add `-O2 -pthread`).
