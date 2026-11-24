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
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "../constants/core.hpp"

namespace grmg
{

    class Song
    {
    public:
        int bars = 4;
        int beats_per_bar = 4;
        double tempo = 90.0; // used when the host gives none (standalone)
        std::vector<models::RollNote> notes;
        std::vector<models::RollStrum> strums;
        // hand stops: at these beats a hand is laid flat on every string, whatever rings stops there
        std::vector<double> stops;

        // the loop: between two flags of the ruler (beats); loop_end < 0: the end of the song
        double loop_start = 0.0, loop_end = -1.0;
        int key = 0, scale = 0; // the key and scale of the song (chords.hpp): harmonizing follows them
        static constexpr int MAX_BARS = 256;

        double length_beats() const { return (double)bars * beats_per_bar; }
        double max_beats() const { return (double)MAX_BARS * beats_per_bar; }

        // where the loop is, inside the song (the whole song if the flags are not set or out of it)
        std::pair<double, double> loop_range() const
        {
            const double len = length_beats();
            const double a = std::min(std::max(loop_start, 0.0), len);
            const double b = loop_end < 0 ? len : std::min(std::max(loop_end, 0.0), len);
            if (b <= a + 1e-9)
                return {0.0, len};
            return {a, b};
        }

        // the notes and strums that start in [a, b), moved so that a is their 0
        Song section(double a, double b) const
        {
            Song out = *this;
            out.notes.clear();
            out.strums.clear();
            for (auto n : notes)
                if (n.start >= a - 1e-9 && n.start < b - 1e-9)
                {
                    n.start -= a;
                    out.notes.push_back(n);
                }
            for (auto t : strums)
                if (t.start >= a - 1e-9 && t.start < b - 1e-9)
                {
                    t.start -= a;
                    out.strums.push_back(t);
                }
            out.stops.clear();
            for (double t : stops)
                if (t >= a - 1e-9 && t < b - 1e-9)
                    out.stops.push_back(t - a);
            out.bars = std::max(1, (int)std::ceil((b - a) / beats_per_bar - 1e-9));
            out.loop_start = 0.0;
            out.loop_end = -1.0;
            return out;
        }

        // the strings of a chord an arpeggio slot picks (arpeggio_slot of grm_guitar.py)
        static std::vector<int> arpeggio_slot(const int *frets, const std::string &slot)
        {
            std::vector<int> played, basses, chosen;
            for (int i = 0; i < 6; ++i)
                if (frets[i] >= 0)
                    played.push_back(i);
            for (int i : played)
                if (i <= 2)
                    basses.push_back(i);
            if (basses.empty() && !played.empty())
                basses.push_back(played[0]);
            std::string token = slot;
            if (token.rfind("B2", 0) == 0)
            {
                if (basses.size() > 1)
                    chosen.push_back(basses[1]);
                token = token.substr(2);
            }
            else if (token.rfind("B", 0) == 0)
            {
                if (!basses.empty())
                    chosen.push_back(basses[0]);
                token = token.substr(1);
            }
            for (char ch : token)
            {
                const int i = 6 - (ch - '0');
                if (i >= 0 && i < 6 && frets[i] >= 0)
                    chosen.push_back(i);
            }
            return chosen;
        }

        void remove_range(double from, double to)
        {
            notes.erase(std::remove_if(notes.begin(), notes.end(), [&](const models::RollNote &n)
                                       { return n.start >= from - 1e-9 && n.start < to - 1e-9; }),
                        notes.end());
            strums.erase(std::remove_if(strums.begin(), strums.end(), [&](const models::RollStrum &s)
                                        { return s.start >= from - 1e-9 && s.start < to - 1e-9; }),
                         strums.end());
        }

        // the chord shape sounding at `beat` (frets per string, -1 not played) and its name: the strum or the notes
        // there; nothing there: all -1
        std::pair<std::array<int, 6>, std::string> shape_at(double beat) const
        {
            std::array<int, 6> f{{-1, -1, -1, -1, -1, -1}};
            std::string name;
            double since = -1e300;
            for (const auto &t : strums)
                if (t.start <= beat + 1e-9 && beat < t.start + std::max(t.length, 0.25) - 1e-9 && t.start > since)
                {
                    f = t.frets;
                    name = t.chord;
                    since = t.start;
                }
            if (since > -1e299)
                return {f, name};
            for (const auto &n : notes)
                if (n.start <= beat + 1e-9 && beat < n.start + n.length - 1e-9 && !n.dead)
                    f[(size_t)n.string] = n.fret;
            return {f, name};
        }

        // A strum pattern from `from` to `to` (beats, any length). Where a chord is already written (a strum, or notes
        // held together) the strums keep it: the pattern only gives the rhythm; elsewhere they take `fallback`.
        // What was there is replaced by the strums (one way of playing at a time).
        void fill_strums_range(double from, double to, int pattern, const std::array<int, 6> &fallback, const std::string &fallback_name,
                               double ring_beats)
        {
            std::vector<std::pair<std::array<int, 6>, std::string>> shapes;
            const char *steps = STRUM_PATTERNS[pattern].steps;
            const size_t n = std::char_traits<char>::length(steps);
            std::vector<models::RollStrum> made;
            for (double b = from; b < to - 1e-9; b += 4.0)
            {
                for (size_t k = 0; k < n; ++k)
                {
                    const char c = steps[k];
                    if (c == '.')
                        continue;
                    models::RollStrum s;
                    s.start = b + 0.25 * (double)k;
                    if (s.start >= to - 1e-9)
                        break;
                    size_t next = k + 1;
                    while (next < n && steps[next] == '.')
                        ++next;
                    s.length = std::min({ring_beats, 0.25 * (double)((next >= n ? n : next) - k), to - s.start});
                    s.up = c == 'u';
                    s.palm = c == 'x';
                    s.velocity = 0.8;
                    auto here = shape_at(s.start);
                    bool any = false;
                    for (int f : here.first)
                        any |= f >= 0;
                    s.frets = any ? here.first : fallback;
                    s.chord = any ? here.second : fallback_name;
                    made.push_back(s);
                }
            }
            remove_range(from, to);
            // what still rings into the range from before it stops where the strums start
            for (auto &note : notes)
                if (note.start < from && note.start + note.length > from)
                    note.length = from - note.start;
            for (auto &m : made)
                strums.push_back(m);
            sort();
        }

        // an arpeggio from `from` to `to`: one slot per eighth, on the chord written there (or `fallback`)
        void fill_arpeggio_range(double from, double to, int pattern, const std::array<int, 6> &fallback, double ring_beats)
        {
            std::vector<models::RollNote> made;
            int k = 0;
            for (double b = from; b < to - 1e-9; b += 0.5, ++k)
            {
                auto here = shape_at(b);
                bool any = false;
                for (int f : here.first)
                    any |= f >= 0;
                const auto frets = any ? here.first : fallback;
                for (int i : arpeggio_slot(frets.data(), ARPEGGIO_PATTERNS[pattern].slots[k % 8]))
                {
                    models::RollNote nn;
                    nn.string = i;
                    nn.fret = frets[(size_t)i];
                    nn.start = b;
                    nn.length = std::min(ring_beats, to - b);
                    nn.velocity = i <= 2 ? 0.9 : 0.7; // the thumb on the bass is heavier
                    made.push_back(nn);
                }
            }
            remove_range(from, to);
            for (auto &m : made)
                notes.push_back(m);
            sort();
        }

        // a strum pattern over `count` bars from bar `bar`, with one chord shape
        void fill_strums(int bar, int count, int pattern, int chord, double ring_beats)
        {
            const double from = bar * beats_per_bar, to = (bar + count) * beats_per_bar;
            remove_range(from, to);
            const char *steps = STRUM_PATTERNS[pattern].steps;
            const size_t n = std::char_traits<char>::length(steps);
            for (double b = from; b < to - 1e-9; b += 4.0)
            {
                for (size_t k = 0; k < n; ++k)
                {
                    const char c = steps[k];
                    if (c == '.')
                        continue;
                    models::RollStrum s;
                    s.start = b + 0.25 * (double)k;
                    if (s.start >= to - 1e-9)
                        break;
                    // a strum rings until the next one (or `ring_beats`)
                    size_t next = k + 1;
                    while (next < n && steps[next] == '.')
                        ++next;
                    s.length = std::min(ring_beats, 0.25 * (double)(next - k));
                    if (next >= n)
                        s.length = std::min(ring_beats, 0.25 * (double)(n - k));
                    s.up = c == 'u';
                    s.palm = c == 'x';
                    s.velocity = 0.8;
                    for (int i = 0; i < 6; ++i)
                        s.frets[(size_t)i] = CHORD_SHAPES[chord].frets[i];
                    s.chord = CHORD_SHAPES[chord].name;
                    strums.push_back(s);
                }
            }
            sort();
        }

        // an arpeggio over `count` bars: one slot per eighth; every note rings until its string plays again
        void fill_arpeggio(int bar, int count, int pattern, int chord, double ring_beats)
        {
            const double from = bar * beats_per_bar, to = (bar + count) * beats_per_bar;
            remove_range(from, to);
            const int *frets = CHORD_SHAPES[chord].frets;
            int k = 0;
            for (double b = from; b < to - 1e-9; b += 0.5, ++k)
            {
                for (int i : arpeggio_slot(frets, ARPEGGIO_PATTERNS[pattern].slots[k % 8]))
                {
                    models::RollNote n;
                    n.string = i;
                    n.fret = frets[i];
                    n.start = b;
                    n.length = ring_beats;
                    n.velocity = i <= 2 ? 0.9 : 0.7; // the thumb on the bass is heavier
                    notes.push_back(n);
                }
            }
            sort();
        }

        void sort()
        {
            std::sort(notes.begin(), notes.end(), [](const models::RollNote &a, const models::RollNote &b)
                      { return a.start < b.start - 1e-9 || (std::abs(a.start - b.start) <= 1e-9 && a.string < b.string); });
            std::sort(strums.begin(), strums.end(), [](const models::RollStrum &a, const models::RollStrum &b)
                      { return a.start < b.start; });
            std::sort(stops.begin(), stops.end());
            stops.erase(std::unique(stops.begin(), stops.end(), [](double a, double b)
                                    { return std::abs(a - b) < 1e-6; }),
                        stops.end());
        }

        // ------------------------------------------------------------ text form (plugin state)
        std::string to_text() const
        {
            std::ostringstream o;
            o.precision(10);
            o << "grm-guitar-roll 1\n";
            o << "bars " << bars << " " << beats_per_bar << " " << tempo << "\n";
            if (loop_start > 0 || loop_end >= 0)
                o << "loop " << loop_start << " " << loop_end << "\n";
            if (key != 0 || scale != 0)
                o << "key " << key << " " << scale << "\n";
            for (double t : stops)
                o << "stop " << t << "\n";
            for (const auto &n : notes)
                o << "note " << n.string << " " << n.fret << " " << n.start << " " << n.length << " " << n.velocity << " "
                  << (n.palm ? 1 : 0) << " " << n.end << " " << (n.dead ? 1 : 0) << "\n";
            for (const auto &s : strums)
            {
                o << "strum " << s.start << " " << s.length << " " << (s.up ? 1 : 0) << " " << s.velocity << " "
                  << (s.palm ? 1 : 0);
                for (int f : s.frets)
                    o << " " << f;
                o << " " << (s.chord.empty() ? "-" : s.chord) << "\n";
            }
            return o.str();
        }

        bool from_text(const std::string &text)
        {
            std::istringstream in(text);
            std::string word;
            int version = 0;
            if (!(in >> word >> version) || word != "grm-guitar-roll")
                return false;
            Song s;
            while (in >> word)
            {
                if (word == "bars")
                {
                    in >> s.bars >> s.beats_per_bar >> s.tempo;
                }
                else if (word == "loop")
                {
                    in >> s.loop_start >> s.loop_end;
                }
                else if (word == "key")
                {
                    in >> s.key >> s.scale;
                }
                else if (word == "stop")
                {
                    double t = 0;
                    if (in >> t)
                        s.stops.push_back(t);
                }
                else if (word == "note")
                {
                    models::RollNote n;
                    int palm = 0;
                    in >> n.string >> n.fret >> n.start >> n.length >> n.velocity >> palm;
                    n.palm = palm != 0;
                    int end = 0;
                    if (in >> end)
                    {
                        n.end = std::min(std::max(end, 0), 2); // older songs: no ending written (it rings)
                        int dead = 0;
                        if (in >> dead)
                            n.dead = dead != 0;
                        else
                            in.clear();
                    }
                    else
                    {
                        in.clear();
                    }
                    if (n.string >= 0 && n.string < 6)
                        s.notes.push_back(n);
                }
                else if (word == "strum")
                {
                    models::RollStrum t;
                    int up = 0, palm = 0;
                    in >> t.start >> t.length >> up >> t.velocity >> palm;
                    for (auto &f : t.frets)
                        in >> f;
                    in >> t.chord;
                    if (t.chord == "-")
                        t.chord.clear();
                    t.up = up != 0;
                    t.palm = palm != 0;
                    s.strums.push_back(t);
                }
                else
                {
                    std::string rest;
                    std::getline(in, rest);
                }
            }
            if (in.bad())
                return false;
            s.bars = std::max(1, std::min(s.bars, MAX_BARS));
            s.beats_per_bar = std::max(1, std::min(s.beats_per_bar, 12));
            *this = s;
            sort();
            return true;
        }

        // a first song so the roll is not empty: the progression of the Python demo, strummed then picked
        static Song demo()
        {
            Song s;
            s.bars = 8;
            s.fill_strums(0, 1, 1, 7, 2.0);   // G, folk
            s.fill_strums(1, 1, 1, 6, 2.0);   // C
            s.fill_strums(2, 1, 1, 5, 2.0);   // D
            s.fill_strums(3, 1, 4, 1, 2.0);   // Em, palm-muted rock
            s.fill_arpeggio(4, 1, 1, 7, 2.0); // G, Travis
            s.fill_arpeggio(5, 1, 1, 6, 2.0); // C
            s.fill_arpeggio(6, 1, 0, 3, 2.0); // Am, up and down
            s.fill_arpeggio(7, 1, 4, 1, 4.0); // Em, pinch
            return s;
        }
    };

} // namespace grmg
