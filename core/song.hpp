// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Anthony Charretier
//
// The guitar roll: what is written for the guitar, string by string.
// A note belongs to a string and a fret: it rings until its end or until the string is played again (one string
// plays one note at a time, as on a guitar). A strum is a chord shape swept by the pick, down or up.
// Times in beats (quarter notes). Plain data, saved as text in the plugin state.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <sstream>
#include <string>
#include <vector>

namespace grmg {

struct RollNote {
    int string = 0;         // 0 = string 6 (low E) ... 5 = string 1 (high E)
    int fret = 0;
    double start = 0.0, length = 1.0;   // beats
    double velocity = 0.8;
    bool palm = false;
};

struct RollStrum {
    double start = 0.0, length = 2.0;   // beats
    std::array<int, 6> frets{{-1, -1, -1, -1, -1, -1}};   // -1: string not played
    bool up = false;
    double velocity = 0.8;
    bool palm = false;
    std::string chord = "";
};

// chord shapes, from string 6 to string 1; -1 = string not played (CHORDS of grm_guitar.py, and a few more)
struct ChordShape { const char* name; int frets[6]; };
constexpr ChordShape CHORD_SHAPES[] = {
    {"Open", {0, 0, 0, 0, 0, 0}},     {"Em", {0, 2, 2, 0, 0, 0}},     {"E", {0, 2, 2, 1, 0, 0}},
    {"Am", {-1, 0, 2, 2, 1, 0}},      {"A", {-1, 0, 2, 2, 2, 0}},     {"D", {-1, -1, 0, 2, 3, 2}},
    {"C", {-1, 3, 2, 0, 1, 0}},       {"G", {3, 2, 0, 0, 0, 3}},      {"Dm", {-1, -1, 0, 2, 3, 1}},
    {"F", {1, 3, 3, 2, 1, 1}},        {"Bm", {-1, 2, 4, 4, 3, 2}},    {"E7", {0, 2, 0, 1, 0, 0}},
    {"A7", {-1, 0, 2, 0, 2, 0}},      {"D7", {-1, -1, 0, 2, 1, 2}},   {"G7", {3, 2, 0, 0, 0, 1}},
    {"C7", {-1, 3, 2, 3, 1, 0}},      {"Cadd9", {-1, 3, 2, 0, 3, 0}}, {"Fmaj7", {-1, -1, 3, 2, 1, 0}},
    {"Asus2", {-1, 0, 2, 2, 0, 0}},   {"Dsus4", {-1, -1, 0, 2, 3, 3}}, {"E5", {0, 2, 2, -1, -1, -1}},
    {"A5", {-1, 0, 2, 2, -1, -1}},    {"G5", {3, 5, 5, -1, -1, -1}},  {"D5", {-1, 5, 7, 7, -1, -1}},
};
constexpr int NUM_CHORDS = (int) (sizeof(CHORD_SHAPES) / sizeof(CHORD_SHAPES[0]));

// strum patterns of the Python sequencer, one character per sixteenth:
// d = down, u = up, x = palm-muted down, . = let the strings ring
struct StrumPattern { const char* name; const char* steps; };
constexpr StrumPattern STRUM_PATTERNS[] = {
    {"All down", "d.d.d.d.d.d.d.d."},          {"Folk  d du ud u", "d.du.ud.d.du.udu"},
    {"Eighths du", "dudududududududu"},       {"Reggae (off-beat)", ".x.d.x.d.x.d.x.d"},
    {"Palm-muted rock", "xxxdxxxdxxxdxxdu"},  {"Quarter notes", "d...d...d...d..."},
};
constexpr int NUM_STRUM_PATTERNS = (int) (sizeof(STRUM_PATTERNS) / sizeof(STRUM_PATTERNS[0]));

// arpeggio slots, one per eighth: B = bass (lowest string of the chord), B2 = next bass string,
// 3 / 2 / 1 = treble strings (G, B, E); several = played together (a pinch)  (ARPEGGIOS of grm_guitar.py)
struct ArpeggioPattern { const char* name; const char* slots[8]; };
constexpr ArpeggioPattern ARPEGGIO_PATTERNS[] = {
    {"Up and down", {"B", "3", "2", "1", "2", "3", "2", "1"}},
    {"Travis (alternating bass)", {"B", "2", "B2", "1", "B", "2", "B2", "3"}},
    {"Bass + chord", {"B", "321", "B2", "321", "B", "321", "B2", "321"}},
    {"Ballad", {"B", "3", "2", "3", "1", "3", "2", "3"}},
    {"Pinch (bass + treble)", {"B1", "3", "2", "3", "B21", "3", "2", "3"}},
};
constexpr int NUM_ARPEGGIO_PATTERNS = (int) (sizeof(ARPEGGIO_PATTERNS) / sizeof(ARPEGGIO_PATTERNS[0]));

class Song {
public:
    int bars = 4;
    int beats_per_bar = 4;
    double tempo = 90.0;              // used when the host gives none (standalone)
    std::vector<RollNote> notes;
    std::vector<RollStrum> strums;

    double length_beats() const { return (double) bars * beats_per_bar; }

    // the strings of a chord an arpeggio slot picks (arpeggio_slot of grm_guitar.py)
    static std::vector<int> arpeggio_slot(const int* frets, const std::string& slot) {
        std::vector<int> played, basses, chosen;
        for (int i = 0; i < 6; ++i)
            if (frets[i] >= 0) played.push_back(i);
        for (int i : played)
            if (i <= 2) basses.push_back(i);
        if (basses.empty() && !played.empty()) basses.push_back(played[0]);
        std::string token = slot;
        if (token.rfind("B2", 0) == 0) {
            if (basses.size() > 1) chosen.push_back(basses[1]);
            token = token.substr(2);
        } else if (token.rfind("B", 0) == 0) {
            if (!basses.empty()) chosen.push_back(basses[0]);
            token = token.substr(1);
        }
        for (char ch : token) {
            const int i = 6 - (ch - '0');
            if (i >= 0 && i < 6 && frets[i] >= 0) chosen.push_back(i);
        }
        return chosen;
    }

    void remove_range(double from, double to) {
        notes.erase(std::remove_if(notes.begin(), notes.end(), [&](const RollNote& n) { return n.start >= from - 1e-9 && n.start < to - 1e-9; }), notes.end());
        strums.erase(std::remove_if(strums.begin(), strums.end(), [&](const RollStrum& s) { return s.start >= from - 1e-9 && s.start < to - 1e-9; }), strums.end());
    }

    // a strum pattern over `count` bars from bar `bar`, with one chord shape
    void fill_strums(int bar, int count, int pattern, int chord, double ring_beats) {
        const double from = bar * beats_per_bar, to = (bar + count) * beats_per_bar;
        remove_range(from, to);
        const char* steps = STRUM_PATTERNS[pattern].steps;
        const size_t n = std::char_traits<char>::length(steps);
        for (double b = from; b < to - 1e-9; b += 4.0) {
            for (size_t k = 0; k < n; ++k) {
                const char c = steps[k];
                if (c == '.') continue;
                RollStrum s;
                s.start = b + 0.25 * (double) k;
                if (s.start >= to - 1e-9) break;
                // a strum rings until the next one (or `ring_beats`)
                size_t next = k + 1;
                while (next < n && steps[next] == '.') ++next;
                s.length = std::min(ring_beats, 0.25 * (double) (next - k));
                if (next >= n) s.length = std::min(ring_beats, 0.25 * (double) (n - k));
                s.up = c == 'u';
                s.palm = c == 'x';
                s.velocity = 0.8;
                for (int i = 0; i < 6; ++i) s.frets[(size_t) i] = CHORD_SHAPES[chord].frets[i];
                s.chord = CHORD_SHAPES[chord].name;
                strums.push_back(s);
            }
        }
        sort();
    }

    // an arpeggio over `count` bars: one slot per eighth; every note rings until its string plays again
    void fill_arpeggio(int bar, int count, int pattern, int chord, double ring_beats) {
        const double from = bar * beats_per_bar, to = (bar + count) * beats_per_bar;
        remove_range(from, to);
        const int* frets = CHORD_SHAPES[chord].frets;
        int k = 0;
        for (double b = from; b < to - 1e-9; b += 0.5, ++k) {
            for (int i : arpeggio_slot(frets, ARPEGGIO_PATTERNS[pattern].slots[k % 8])) {
                RollNote n;
                n.string = i;
                n.fret = frets[i];
                n.start = b;
                n.length = ring_beats;
                n.velocity = i <= 2 ? 0.9 : 0.7;     // the thumb on the bass is heavier
                notes.push_back(n);
            }
        }
        sort();
    }

    void sort() {
        std::sort(notes.begin(), notes.end(), [](const RollNote& a, const RollNote& b) {
            return a.start < b.start - 1e-9 || (std::abs(a.start - b.start) <= 1e-9 && a.string < b.string);
        });
        std::sort(strums.begin(), strums.end(), [](const RollStrum& a, const RollStrum& b) { return a.start < b.start; });
    }

    // ------------------------------------------------------------ text form (plugin state)
    std::string to_text() const {
        std::ostringstream o;
        o.precision(10);
        o << "grm-guitar-roll 1\n";
        o << "bars " << bars << " " << beats_per_bar << " " << tempo << "\n";
        for (const auto& n : notes)
            o << "note " << n.string << " " << n.fret << " " << n.start << " " << n.length << " " << n.velocity << " "
              << (n.palm ? 1 : 0) << "\n";
        for (const auto& s : strums) {
            o << "strum " << s.start << " " << s.length << " " << (s.up ? 1 : 0) << " " << s.velocity << " "
              << (s.palm ? 1 : 0);
            for (int f : s.frets) o << " " << f;
            o << " " << (s.chord.empty() ? "-" : s.chord) << "\n";
        }
        return o.str();
    }

    bool from_text(const std::string& text) {
        std::istringstream in(text);
        std::string word;
        int version = 0;
        if (!(in >> word >> version) || word != "grm-guitar-roll") return false;
        Song s;
        while (in >> word) {
            if (word == "bars") {
                in >> s.bars >> s.beats_per_bar >> s.tempo;
            } else if (word == "note") {
                RollNote n;
                int palm = 0;
                in >> n.string >> n.fret >> n.start >> n.length >> n.velocity >> palm;
                n.palm = palm != 0;
                if (n.string >= 0 && n.string < 6) s.notes.push_back(n);
            } else if (word == "strum") {
                RollStrum t;
                int up = 0, palm = 0;
                in >> t.start >> t.length >> up >> t.velocity >> palm;
                for (auto& f : t.frets) in >> f;
                in >> t.chord;
                if (t.chord == "-") t.chord.clear();
                t.up = up != 0;
                t.palm = palm != 0;
                s.strums.push_back(t);
            } else {
                std::string rest;
                std::getline(in, rest);
            }
        }
        if (in.bad()) return false;
        s.bars = std::max(1, std::min(s.bars, 256));
        s.beats_per_bar = std::max(1, std::min(s.beats_per_bar, 12));
        *this = s;
        sort();
        return true;
    }

    // a first song so the roll is not empty: the progression of the Python demo, strummed then picked
    static Song demo() {
        Song s;
        s.bars = 8;
        s.fill_strums(0, 1, 1, 7, 2.0);      // G, folk
        s.fill_strums(1, 1, 1, 6, 2.0);      // C
        s.fill_strums(2, 1, 1, 5, 2.0);      // D
        s.fill_strums(3, 1, 4, 1, 2.0);      // Em, palm-muted rock
        s.fill_arpeggio(4, 1, 1, 7, 2.0);    // G, Travis
        s.fill_arpeggio(5, 1, 1, 6, 2.0);    // C
        s.fill_arpeggio(6, 1, 0, 3, 2.0);    // Am, up and down
        s.fill_arpeggio(7, 1, 4, 1, 4.0);    // Em, pinch
        return s;
    }
};

}  // namespace grmg
