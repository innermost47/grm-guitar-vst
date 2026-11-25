// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// The pattern of GRM Guitar in one file, without JUCE: a network solved on a worker thread gives the parameters of
// a sound (a stiff string's partials), a lock-free mailbox hands them to the "audio" loop, which only runs a bank
// of damped oscillators. Build: g++ -std=c++17 -O2 -I <path to cppgrm> modal_string.cpp -pthread
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <thread>
#include <vector>

#include "grm.hpp"

constexpr double PI = 3.14159265358979323846;

// what the audio needs, solved off the audio thread
struct StringModel
{
    std::vector<double> freq, decay, amp; // per partial: Hz, 1/s, initial amplitude
};

// the string network: tension, mass, length and the core's stiffness -> pitch and inharmonicity
StringModel solve_string(double L, double mu, double T, double d, double E, int partials)
{
    grm::Network net;
    for (auto [name, guess] : {std::pair{"L", 0.65}, {"mu", 1e-3}, {"T", 70.0}, {"c", 200.0}, {"f1", 100.0},
                               {"d", 5e-4}, {"B", 1e-4}})
        net.add_node(name, guess);
    net.add_log("wave speed", {{"c", 2}, {"mu", 1}, {"T", -1}}, 0.0, 1e-9);
    net.add_log("fundamental", {{"f1", 1}, {"L", 1}, {"c", -1}}, -std::log10(2.0), 1e-9);
    // B = pi^3 E d^4 / (64 T L^2)
    net.add_log("stiffness", {{"B", 1}, {"d", -4}, {"T", 1}, {"L", 2}}, std::log10(PI * PI * PI * E / 64.0), 1e-9);
    const double pct = 0.01 / grm::LN10;
    const auto r = net.solve({{"L", {L, pct / 10}}, {"mu", {mu, pct}}, {"T", {T, pct}}, {"d", {d, pct}}});

    StringModel m;
    for (int n = 1; n <= partials; ++n)
    {
        const double f = n * r["f1"] * std::sqrt(1.0 + r["B"] * n * n); // a stiff string's partials
        m.freq.push_back(f);
        m.decay.push_back(0.8 + 2e-7 * f * f); // a simple loss law (GRM Guitar: a losses network)
        m.amp.push_back(1.0 / (n * n));                // a plucked string's spectrum
    }
    return m;
}

// one object handed from the worker to the audio thread: no lock, no free on the audio side (grmg::Mailbox)
template <class T>
class Mailbox
{
public:
    ~Mailbox() { delete pending_.load(); delete trash_.load(); delete current_; }
    void post(T *p) { collect(); delete pending_.exchange(p); } // worker
    void collect() { delete trash_.exchange(nullptr); }          // worker
    T *take()                                                    // audio
    {
        if (trash_.load() == nullptr)
            if (T *p = pending_.exchange(nullptr))
            {
                trash_.store(current_);
                current_ = p;
            }
        return current_;
    }

private:
    std::atomic<T *> pending_{nullptr}, trash_{nullptr};
    T *current_ = nullptr;
};

int main()
{
    const double sr = 48000.0;
    Mailbox<StringModel> box;

    // the worker: a setting changed (here, once), the network is solved and its result posted. The low E of a 10-46
    // set on a 648 mm scale; d is its steel core (a wound string bends with its core, not its windings)
    std::thread worker([&]
                       { box.post(new StringModel(solve_string(0.648, 0.00682, 77.8, 0.00043, 2.0e11, 40))); });
    worker.join();

    // the audio loop: takes the newest model between two blocks, runs the modes
    const StringModel *model = box.take();
    // each mode's rotation per sample, computed once per model, not per sample
    const size_t modes = model->freq.size();
    std::vector<double> re(modes), im(modes, 0.0), cr(modes), ci(modes);
    for (size_t k = 0; k < modes; ++k)
    {
        const double w = 2 * PI * model->freq[k] / sr, g = std::exp(-model->decay[k] / sr);
        cr[k] = g * std::cos(w);
        ci[k] = g * std::sin(w);
        re[k] = model->amp[k];
    }
    double energy_first = 0, energy_last = 0;
    const int block = 256, blocks = (int)sr / block; // one second
    for (int b = 0; b < blocks; ++b)
        for (int t = 0; t < block; ++t)
        {
            double out = 0.0;
            for (size_t k = 0; k < modes; ++k)
            {
                // a damped complex oscillator: one complex multiplication per sample
                const double nr = re[k] * cr[k] - im[k] * ci[k];
                im[k] = re[k] * ci[k] + im[k] * cr[k];
                re[k] = nr;
                out += re[k];
            }
            (b == 0 ? energy_first : energy_last) += b == 0 || b == blocks - 1 ? out * out : 0.0;
        }
    std::printf("f1 %.2f Hz, partial 20 at %.1f Hz (harmonic: %.1f)\n", model->freq[0], model->freq[19], 20 * model->freq[0]);
    std::printf("level after one second: %.1f dB\n", 10 * std::log10(energy_last / energy_first));
    return 0;
}
