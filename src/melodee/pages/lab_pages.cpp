// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 Kerem Kilic (Ellic Studio)
// INDIGO X DENIM LAB pages, after Jsen Surreal's SXL (both open from the mixer's TRACKS with Shift):
//
// - TUNER (Shift + B2): the input as it is recorded (Input Config's source and gain), its note, octave and how many
//   cents off, a needle over -50..50, the frequency under it. The pitch is found on the screen thread
//   (shell/shell.cpp, a YIN-style search on the input's last frames).
// - SUB LENS (Shift + B1): two tracks' low end (under 150 Hz) over the last moments, A (the kick, in the brand's
//   magenta) and B (the bass, in blue): over each other, apart, or summed, to see how they sit and how a sidechain
//   ducks one under the other. B1 / B2 pick A and B among the used tracks, B3 the view, the wheel the window, Push
//   freezes it.
#include "melodee/pages/common.h"
#include "melodee/pages/pages.h"
#include "melodee/ui/widgets.h"
#include <algorithm>
#include <cmath>

namespace melodee {
namespace {
using namespace ui;

constexpr unsigned lens_windows[] {100, 250, 500, 1000}; // ms

// ---- Tuner ----
void tuner_draw(Paint& p, const Snapshot& s) {
    Header h = header_of(s);
    h.left.push_back(pill(Icon::Mic, "TUNER"));
    SoftKeys keys {{"", "", ""}, -1, colour_of(s)};
    frame(p, s, keys, h);
    const float top = zone::picture_y + 6, mid = zone::width / 2.0f;
    static const char* names[12] {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    bool heard = s.tuner_hz > 20 && s.tuner_level > 0.003f;
    float cents = 0;
    juce::String note = "-", octave, hz = "No input";
    if (heard) {
        double midi = 69 + 12 * std::log2(s.tuner_hz / 440.0);
        int n = int(std::lround(midi));
        cents = float((midi - n) * 100);
        note = names[((n % 12) + 12) % 12];
        octave = juce::String(n / 12 - 1);
        hz = juce::String(s.tuner_hz, 1) + " Hz";
    }
    bool close = heard && std::fabs(cents) < 5;
    auto ink = !heard ? colour::dim : close ? colour::play : std::fabs(cents) < 20 ? colour::amber : colour::rec;
    float nw = p.width(note, plain(ink, 64, Weight::Bold));
    p.text(note, mid - nw / 2 - 8, top, nw + 4, 70, plain(ink, 64, Weight::Bold));
    if (heard) p.text(octave, mid + nw / 2 - 2, top + 34, 30, 30, plain(colour::sec, 24, Weight::Medium));
    // The scale and the needle.
    const float sy = top + 86, sw = zone::width - 2 * zone::pad - 24, sx = (zone::width - sw) / 2;
    p.fill(sx, sy, sw, 2, colour::quiet, 1);
    for (int c = -50; c <= 50; c += 10) {
        float x = sx + sw * (c + 50) / 100.0f;
        p.fill(x - 0.5f, sy - (c == 0 ? 8 : 4), 1, c == 0 ? 18 : 10, c == 0 ? colour::sec : colour::line);
    }
    if (heard) {
        float x = sx + sw * (std::clamp(cents, -50.0f, 50.0f) + 50) / 100.0f;
        p.fill(x - 2, sy - 12, 4, 26, ink, 2);
    }
    juce::String under = heard ? (cents >= 0 ? "+" : "") + juce::String(int(std::lround(cents))) + " cents" + juce::String::fromUTF8("  \xc2\xb7  ") + hz : hz;
    p.text(under, zone::pad, sy + 20, zone::width - 2 * zone::pad, 16, plain(colour::sec, type::small, Weight::Medium), juce::Justification::centred);
}
void tuner_pad(Context&, unsigned, uint8_t, bool) {}
void tuner_wheel(Context&, int) {}
void tuner_push(Context&) {}
void tuner_soft(Context&, int) {}

// ---- Sub Lens ----
int next_used(const Context& c, int from, int way) {
    for (int k = 1; k <= int(track_count); ++k) {
        int t = ((from < 0 ? 0 : from) + way * k + int(track_count) * 4) % int(track_count);
        if (c.project.tracks[size_t(t)].used()) return t;
    }
    return from;
}

void wave(Paint& p, const std::array<float, Snapshot::lens_points>& v, float x, float y, float w, float h, float scale, juce::Colour c, bool fill) {
    juce::Path path;
    const unsigned n = Snapshot::lens_points;
    for (unsigned i = 0; i < n; ++i) {
        float px = x + w * i / (n - 1), py = y + h / 2 - std::clamp(v[i] * scale, -1.0f, 1.0f) * (h / 2 - 1);
        if (!i) path.startNewSubPath(px, py); else path.lineTo(px, py);
    }
    if (fill) {
        juce::Path area = path;
        area.lineTo(x + w, y + h / 2); area.lineTo(x, y + h / 2); area.closeSubPath();
        p.g.setColour(c.withAlpha(0.18f)); p.g.fillPath(area);
    }
    p.g.setColour(c); p.g.strokePath(path, juce::PathStrokeType(1.4f));
}

void sublens_draw(Paint& p, const Snapshot& s) {
    Header h = header_of(s);
    h.left.push_back(pill("SUB LENS"));
    h.left.push_back(pill(juce::String(lens_windows[s.ui.lens_window % 4]) + " ms"));
    if (s.ui.lens_freeze) h.left.push_back(pill(Icon::Lock, "FROZEN", colour::amber));
    auto label = [&](const char* side, int t) { return juce::String(side) + (t >= 0 ? juce::String(t + 1) : juce::String("-")); };
    static const char* modes[] {"OVERLAY", "SPLIT", "SUM"};
    SoftKeys keys {{label("A: T", s.ui.lens_a), label("B: T", s.ui.lens_b), modes[s.ui.lens_mode % 3]}, -1, colour_of(s)};
    frame(p, s, keys, h);
    const float x = zone::pad, w = zone::width - 2 * zone::pad, top = zone::picture_y + 4, bottom = zone::knobs_y - 4, hh = bottom - top;
    float peak = 0.02f;
    for (unsigned i = 0; i < Snapshot::lens_points; ++i) peak = std::max({peak, std::fabs(s.lens_a[i]), std::fabs(s.lens_b[i]), std::fabs(s.lens_a[i] + s.lens_b[i])});
    float scale = 1.0f / peak;
    auto ca = colour::brand_b, cb = colour::wave;
    p.fill(x, top, w, hh, colour::panel, 4);
    if (s.ui.lens_mode == 1) { // apart: A above, B below
        p.fill(x, top + hh / 4, w, 0.5f, colour::line); p.fill(x, top + hh * 3 / 4, w, 0.5f, colour::line);
        wave(p, s.lens_a, x, top, w, hh / 2, scale, ca, true);
        wave(p, s.lens_b, x, top + hh / 2, w, hh / 2, scale, cb, true);
    } else if (s.ui.lens_mode == 2) { // summed, as the low end is heard
        std::array<float, Snapshot::lens_points> sum {};
        for (unsigned i = 0; i < sum.size(); ++i) sum[i] = s.lens_a[i] + s.lens_b[i];
        p.fill(x, top + hh / 2, w, 0.5f, colour::line);
        wave(p, sum, x, top, w, hh, scale, colour::text, true);
    } else {
        p.fill(x, top + hh / 2, w, 0.5f, colour::line);
        wave(p, s.lens_b, x, top, w, hh, scale, cb, true);
        wave(p, s.lens_a, x, top, w, hh, scale, ca, false);
    }
    if (s.ui.lens_a < 0 && s.ui.lens_b < 0)
        p.text("B1 / B2: the kick's and the bass's tracks", x, top + hh / 2 - 8, w, 16, plain(colour::mid, type::small), juce::Justification::centred);
}
void sublens_pad(Context& c, unsigned pad, uint8_t, bool down) { if (down) c.select_track(pad); }
void sublens_wheel(Context& c, int delta) { c.ui.lens_window = uint8_t(std::clamp(int(c.ui.lens_window) + delta, 0, 3)); }
void sublens_push(Context& c) { c.ui.lens_freeze = !c.ui.lens_freeze; }
void sublens_soft(Context& c, int key) {
    auto& ui = c.ui;
    if (key == 0) ui.lens_a = int8_t(next_used(c, ui.lens_a, 1));
    if (key == 1) ui.lens_b = int8_t(next_used(c, ui.lens_b, 1));
    if (key == 2) ui.lens_mode = uint8_t((ui.lens_mode + 1) % 3);
}
} // namespace

const PageSpec tuner_page {tuner_draw, tuner_pad, tuner_wheel, tuner_push, tuner_soft, tuner_wheel};
const PageSpec sublens_page {sublens_draw, sublens_pad, sublens_wheel, sublens_push, sublens_soft, sublens_wheel};

// The window's length in ms (shell/shell.cpp fills the snapshot after it).
unsigned lens_window_ms(const UiState& ui) { return lens_windows[ui.lens_window % 4]; }

} // namespace melodee
