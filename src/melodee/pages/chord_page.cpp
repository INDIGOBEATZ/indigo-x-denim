// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 Kerem Kilic (Ellic Studio)
// INDIGO X DENIM LAB: the chord builder (after SXL's chord progression tool): four or eight slots over the pattern,
// each a degree of the project's key (I..vii) or a rest. Pads 1-7 play a degree's chord and put it in the slot in
// hand (Pad 8 a rest), the cursor then moving on; the wheel picks a slot, Minus / Plus change it, Push hears it.
// B1 a preset progression, B2 four or eight slots, B3 writes it into the selected track's pattern (one undo), the
// chords as the track's chord type, voicing and inversion make them.
#include "melodee/model/chords.h"
#include "melodee/pages/common.h"
#include "melodee/pages/pages.h"
#include "melodee/shell/context.h"
#include "melodee/ui/widgets.h"
#include <algorithm>

namespace melodee {

struct ProgPreset { const char* name; uint8_t degrees[4]; };
constexpr ProgPreset prog_presets[] {
    {"I V vi IV", {1, 5, 6, 4}}, {"vi IV I V", {6, 4, 1, 5}}, {"I vi IV V", {1, 6, 4, 5}}, {"ii V I I", {2, 5, 1, 1}},
    {"i VI III VII", {1, 6, 3, 7}}, {"i iv v i", {1, 4, 5, 1}}, {"i VII VI VII", {1, 7, 6, 7}}, {"I IV vi V", {1, 4, 6, 5}},
};
constexpr unsigned prog_preset_count = sizeof(prog_presets) / sizeof(prog_presets[0]);

// The track as the builder plays it: its chord type, voicing and inversion on the key's scale.
static Track chord_track(const Track& t) {
    Track c = t;
    c.layout = PadLayout::Scale;
    c.chord = true;
    return c;
}

void Context::chord_pad(unsigned pad, uint8_t velocity, bool down) {
    pad %= 16;
    release_pad(pad);
    auto& t = track();
    if (!down) { if (pad < 8) ui.prog_cursor = uint8_t((ui.prog_cursor + 1) % ui.prog_slots); return; }
    if (pad >= 8) return;
    if (t.engine == EngineKind::Sampler || !t.used()) { notice("Chords: pick a keys track"); return; }
    ui.prog[ui.prog_cursor % 8] = uint8_t(pad == 7 ? 0 : pad + 1);
    if (pad == 7) return;
    pad_track[pad] = ui.track;
    std::array<int, notes_per_step> keys {};
    unsigned count = chord_notes(project, chord_track(t), pad, keys);
    for (unsigned n = 0; n < count; ++n) {
        pad_keys[pad][n] = int32_t(std::clamp(keys[n], 0, 127));
        key(pad_keys[pad][n], velocity, true);
    }
}

void Context::chord_write() {
    auto& t = track();
    if (t.engine == EngineKind::Sampler || !t.used()) { notice("Chords: pick a keys track"); return; }
    const unsigned tr = ui.track, pa = t.pattern, slots = ui.prog_slots == 8 ? 8 : 4;
    const Track shape = chord_track(t);
    unsigned written = 0;
    edit_pattern(tr, pa, "Chord progression", [&] {
        auto& p = pattern();
        p.events.clear();
        const uint32_t each = std::max<uint32_t>(p.length / slots, 1);
        for (unsigned k = 0; k < slots; ++k) {
            unsigned degree = ui.prog[k] % 8;
            if (!degree) continue;
            std::array<int, notes_per_step> keys {};
            unsigned count = chord_notes(project, shape, degree - 1, keys);
            for (unsigned n = 0; n < count; ++n) {
                Event e;
                e.tick = k * each; e.length = uint16_t(std::min<uint32_t>(each - 1, 0xffff)); e.key = uint8_t(std::clamp(keys[n], 0, 127)); e.velocity = 96;
                p.add(e);
            }
            ++written;
        }
        sync_pattern(tr, pa);
    });
    notice(std::to_string(written) + (written == 1 ? " chord written" : " chords written"));
}

namespace {
using namespace ui;

const char* roman(unsigned degree, bool minor) {
    static const char* upper[] {"-", "I", "II", "III", "IV", "V", "VI", "VII"};
    static const char* lower[] {"-", "i", "ii", "iii", "iv", "v", "vi", "vii"};
    return (minor ? lower : upper)[degree % 8];
}

void draw(Paint& p, const Snapshot& s) {
    const auto& t = s.track();
    auto c = colour_of(s);
    Header h = header_of(s);
    h.left.push_back(pill(Icon::Note, "CHORDS"));
    h.left.push_back(pill(juce::String(note_letter(s.project.root % 12)) + " " + juce::String(scales()[s.project.scale % scales().size()].name).toUpperCase(), c));
    h.right.push_back(tag_of(s, s.ui.track));
    unsigned slots = s.ui.prog_slots == 8 ? 8 : 4;
    SoftKeys keys {{"PRESET", slots == 8 ? "8 SLOTS" : "4 SLOTS", "WRITE"}, -1, c, {KeyIcon::None, KeyIcon::None, KeyIcon::Check}};
    frame(p, s, keys, h);
    bool kit = t.engine == EngineKind::Sampler;
    const Track shape = chord_track(t);
    const float gap = 4, x0 = zone::pad, w = (zone::width - 2 * zone::pad - 3 * gap) / 4;
    const float rows = slots == 8 ? 2 : 1, top = zone::picture_y + 6, hgt = slots == 8 ? 52 : 80;
    for (unsigned k = 0; k < slots; ++k) {
        float x = x0 + (k % 4) * (w + gap), y = top + (k / 4) * (hgt + gap);
        bool here = k == s.ui.prog_cursor % slots;
        unsigned degree = s.ui.prog[k] % 8;
        p.fill(x, y, w, hgt, here ? colour::raised : colour::surf, 5);
        if (here) p.fill(x, y, w, 2.5f, c, 1);
        if (!degree) { p.text("REST", x, y, w, hgt, caps_text(colour::dim), juce::Justification::centred); continue; }
        juce::String name = kit ? juce::String("-") : juce::String(chord_name(s.project, shape, degree - 1));
        bool minor = name.containsChar('m') && !name.contains("maj");
        auto big = plain(here ? colour::text : colour::sec, slots == 8 ? type::list : type::title, Weight::SemiBold);
        p.text(roman(degree, minor), x, y + 4, w, hgt / 2, big, juce::Justification::centred);
        p.text(name, x + 2, y + hgt / 2, w - 4, hgt / 2 - 4, p.fit(name, plain(here ? c : colour::mid, type::body, Weight::Medium), w - 4, type::floor),
               juce::Justification::centred);
    }
    float y = top + rows * (hgt + gap) + 4;
    juce::String hint = kit ? juce::String("Pick a keys track for chords") : juce::String::fromUTF8("PADS 1-7 \xc2\xb7 I-VII   PAD 8 \xc2\xb7 REST");
    p.text(hint, 0, y, zone::width, 14, caps_text(kit ? colour::amber : colour::mid), juce::Justification::centred);
    p.text(prog_presets[s.ui.prog_preset % prog_preset_count].name, 0, y + 16, zone::width, 14, plain(colour::dim, type::small), juce::Justification::centred);
}

void pad(Context& c, unsigned pad, uint8_t velocity, bool down) { c.chord_pad(pad, velocity, down); }
void wheel(Context& c, int delta) {
    unsigned slots = c.ui.prog_slots == 8 ? 8 : 4;
    c.ui.prog_cursor = uint8_t(std::clamp<int>(c.ui.prog_cursor % slots + delta, 0, int(slots) - 1));
}
void nudge(Context& c, int way) {
    auto& d = c.ui.prog[c.ui.prog_cursor % 8];
    d = uint8_t((int(d) + way + 8) % 8);
}
void push(Context& c) { // the slot's chord, a moment (its release on the next frame's pad up is not coming: Push again ends it)
    unsigned degree = c.ui.prog[c.ui.prog_cursor % 8] % 8;
    c.release_pad(15);
    if (!degree || c.track().engine == EngineKind::Sampler) return;
    std::array<int, notes_per_step> keys {};
    unsigned count = chord_notes(c.project, chord_track(c.track()), degree - 1, keys);
    c.pad_track[15] = c.ui.track;
    for (unsigned n = 0; n < count; ++n) { c.pad_keys[15][n] = int32_t(std::clamp(keys[n], 0, 127)); c.key(c.pad_keys[15][n], 96, true); }
}
void soft(Context& c, int key) {
    auto& ui = c.ui;
    c.release_pad(15);
    if (key == 0) {
        ui.prog_preset = uint8_t((ui.prog_preset + 1) % prog_preset_count);
        const auto& pr = prog_presets[ui.prog_preset];
        for (unsigned k = 0; k < 8; ++k) ui.prog[k] = pr.degrees[k % 4];
        c.notice(pr.name);
    }
    if (key == 1) { ui.prog_slots = ui.prog_slots == 8 ? 4 : 8; ui.prog_cursor = uint8_t(ui.prog_cursor % ui.prog_slots); }
    if (key == 2) c.chord_write();
}
} // namespace

const PageSpec chord_builder_page {draw, pad, wheel, push, soft, nudge};

} // namespace melodee
