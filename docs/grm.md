# The GRM in practice

The Global Relational Model describes a system as **quantities tied by relations**, some of them measured, every
measurement with its uncertainty. Solving the network finds the values that agree best with all the relations and
all the measurements at once, and tells how sure each value is.

This page shows what that means in code, with [cppgrm](https://github.com/innermost47/cppgrm), and why GRM Guitar
is built on it. The theory and the research are in [grm-core](https://github.com/innermost47/grm-core).

## A first network

A guitar string obeys two relations: its wave speed is `c² = T / μ` (tension over mass per length) and its pitch
is `f₁ = c / 2L`. Five quantities, two relations: measure any three and the network gives the other two.

```cpp
#include "grm.hpp"

grm::Network net;
net.add_node("L", 0.65);   // vibrating length (m): the number is only a starting guess
net.add_node("mu", 1e-3);  // mass per length (kg/m)
net.add_node("T", 70.0);   // tension (N)
net.add_node("c", 200.0);  // wave speed (m/s)
net.add_node("f1", 100.0); // fundamental (Hz)

// c^2 mu / T = 1 and f1 L / c = 1/2, written as sums of log10
net.add_log("wave speed", {{"c", 2}, {"mu", 1}, {"T", -1}}, 0.0, 1e-9);
net.add_log("fundamental", {{"f1", 1}, {"L", 1}, {"c", -1}}, -std::log10(2.0), 1e-9);

const double one_percent = 0.01 / grm::LN10; // uncertainties are in dex (log10 units)
const auto r = net.solve({{"L", {0.648, one_percent / 10}},  // a 648 mm scale, to 0.1 %
                          {"mu", {0.00682, one_percent}},    // the low E of a 10-46 set, to 1 %
                          {"f1", {82.41, 1e-9}}});           // tuned to E2, exactly

r["T"];                                  // 77.8 N, the tension the string maker publishes
r.uncertainty[(size_t)net.index("T")];   // 0.010: the tension is known to 1 %, because the mass is
```

The full program is [`examples/string_tension.cpp`](examples/string_tension.cpp).

Nothing in this network says "compute the tension". The same network, measured differently, gives the pitch from
the tension, or the mass a string must have to reach a pitch at a given tension. In GRM Guitar,
`core/guitar_networks.hpp` uses one string network both ways: `tune_string_model` (the tension a tuning asks) and
`play_string_model` (the pitch, stiffness and impedance at each fret under that tension).

## The vocabulary

**Nodes** (`add_node(name, guess, lin = false, fixed = false)`)
- A positive quantity (the default) is solved as `x = log10(value)`. Products, ratios and power laws, which is
  most of physics, become straight lines, and a value can never go negative.
- A signed quantity (`lin = true`) is solved as its value: a time offset, a position that crosses zero.
- The guess only starts the solver. A node nothing determines stays near it: every step is gently held back, very
  weakly (`prior_sigma`).

**Relations**, each with its own `sigma`, how strictly it must hold:
- `add_log(name, {{node, coeff}...}, rhs, sigma)`: `Σ coeff · log10(node) = rhs`. Products, ratios, powers. A
  network made only of these is linear in log space and solves exactly in a few steps.
- `add_sum(name, {{node, coeff}...}, K, sigma, powers)`: `Σ coeff · value^p = K`. Additions: a length split in
  two, energies that add up.
- `add_fn(name, {nodes...}, f, sigma, grad)`: `f(values) = 0`, any equation (an analytic gradient is optional).
  Networks with these are solved by Gauss-Newton, then Levenberg-Marquardt, with continuation for very tight
  relations.

**Measurements** (`solve({{name, {value, sigma_dex}}...})`): what is known, and how well. A sigma of `1e-9` pins a
value; `0.01 / LN10` says "to about 1 %"; a looser one lets the relations move it.

**Results** (`grm::Result`): `r["name"]` or `r.values[index]`; `r.sigma_dex` and `r.uncertainty` (relative for
positive nodes, absolute for signed ones); `r.converged`, `r.iterations`, `r.cost`.

## Relations and measurements are the same thing

The power of the model is that a relation does not need to be exact, and a measurement does not need to be
certain: both are weighed by their sigma. This is how GRM Guitar mixes physics and data.

- **Physics that holds**: the wave speed of a string, sigma `1e-9`. The solver will not bend it.
- **Data from the literature**: the losses of a nickel string, new and played, measured by Woodhouse; a string's
  mass from its maker's chart. Their published spread is their sigma.
- **Intentions that compete**: the player network (`player_network` in `core/physics.hpp`) holds one node per
  stroke for its time, force, pluck point and palm. The player's aim for each stroke is a measurement (where the
  groove wants it, within the motor noise of a real hand); the arm's continuity is a relation between neighbouring
  strokes (a pendulum does not jump). The solution is a stroke sequence where the intention mostly wins and the arm
  smooths what it cannot follow. Nobody wrote that compromise by hand: the sigmas did.

## Why a guitar is built this way

A physical model of a guitar has hundreds of coupled parameters. Set them by hand and each one is a guess that
breaks the others. With networks:

1. **The inputs are things you can look up**: a gauge, a scale length, a wood's density, a pickup's inductance,
   a strum's spread measured on real players. `constants/core.hpp` holds them with their sources.
2. **Everything else follows**: tension, inharmonicity, mode frequencies and losses, the body's resonances, the
   room's reverberation time. Change the string set and every value that depends on it moves together.
3. **Each value carries its uncertainty**: the tests check values within it, and a network that cannot agree with
   its data says so (`converged`, `cost`).
4. **It stays readable**: a relation has a name ("wave speed", "release time", "arm period"), and the
   Networks panel of the plugin shows the values the networks found.

## What the networks do not do

The networks run when a setting changes, not per sample. They give the **parameters** of the sound; the audio
thread then runs ordinary, fast DSP with them (banks of damped oscillators, convolutions, filters). How the two
meet without blocking audio is the subject of [Architecture](architecture.md).

## Same numbers in Python and C++

cppgrm is the same algorithm, step by step, as the Python `grm.py`. `tools/make_golden.py` solves reference
networks with the Python engine and writes `tests/golden.hpp`; `test_core` checks that the C++ gives the same
values to 1e-9. To try an idea quickly, write the network with [pygrm](https://pypi.org/project/pygrm/) first.
