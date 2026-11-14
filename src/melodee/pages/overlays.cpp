// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 Kerem Kilic (Ellic Studio)
// Drawn over any page: the tracks while Pad Bank is held (after a second), the Sequence
// sheet while Seq is held (after a moment), Note Repeat's
// rates, the Shift + K cards while Shift is held, and the Exit sheet after a
// short press of Power; and over all of them the place bar.
#include "melodee/engine/arp.h"
#include "melodee/pages/common.h"
#include "melodee/shell/mute_mode.h"
#include "melodee/shell/note_repeat.h"
#include "melodee/shell/pad_bank.h"
#include "melodee/shell/seq_layer.h"
#include "melodee/shell/shift_pad_jobs.h"
#include "melodee/ui/widgets.h"
#include <cstring>

namespace melodee {
using namespace ui;

void draw_overlays(Paint& p, const Snapshot& s) {
    if (s.ui.loading) { draw_loading_sheet(p, s); return; } // it owns the screen and the controls until it is in
    bool tracks = track_overlay_shown(s.ui, s.now); // Pad Bank held a second, or a track copy
    bool sheet = s.ui.name.open() || s.ui.exit_open;
    bool seq = !tracks && seq_sheet_shown(s.ui, s.now); // Seq held a moment: the Sequence sheet
    if (!sheet && !tracks && !seq) draw_sampler_map(p, s); // under the notice and the cards
    if (tracks && !sheet) draw_track_overlay(p, s);
    if (seq && !sheet) draw_seq_sheet(p, s);
    if (!sheet && s.ui.ask != UiState::Ask::None) draw_ask_sheet(p, s); // B1 Cancel / B3 Do it before an erase
    if (!sheet && s.ui.ask == UiState::Ask::None && !tracks && !seq) draw_fader_header(p, s);
    if (s.history_count && !sheet) draw_history(p, s); // LAB: the undo history a moment after an undo or redo
    draw_notice(p, s); // over the fader's line: a copy in hand or what an undo did comes first
    if (s.ui.name.open()) draw_name_sheet(p, s);
    if (s.ui.exit_open) draw_exit_sheet(p, s);
    else if (s.ui.shift && !s.ui.shift_used && !s.ui.name.open()) {
        draw_shift_layer(p, s);
        // INDIGO X DENIM LAB: Shift held a moment with nothing else touched: what Shift + each pad does, as the pads sit
        if (!sheet && !tracks && !seq && s.ui.ask == UiState::Ask::None && s.ui.settings.shift_map_delay && s.ui.shift_at && s.now - s.ui.shift_at >= s.ui.settings.shift_map_ms()) draw_shift_pads(p, s);
    }
    if (!sheet && !tracks && !seq && repeat_keys(s.project, s.ui)) draw_repeat_keys(p, s);
    draw_place_bar(p, s); // over everything: where the music is stays in view
    if (s.ui.settings.system_bar && !s.ui.loading) draw_system_bar(p, s);
}

// INDIGO X DENIM LAB: the Shift + Pad jobs as a 4 x 4 grid laid out as the pads are (13-16 on top, 1-4 at the foot),
// over the page's middle: a toggle that is on lit in the track's colour, a job faint, one not built yet dim.
void draw_shift_pads(Paint& p, const Snapshot& s) {
    const float top = zone::header_y + zone::header_h + 2, bottom = zone::knobs_y - 2;
    const float x0 = zone::pad, w = zone::width - 2 * zone::pad, gap = 3;
    const float cw = (w - 3 * gap) / 4, ch = (bottom - top - 3 * gap) / 4;
    p.fill(0, top - 2, zone::width, bottom - top + 4, colour::bg.withAlpha(0.94f));
    auto accent = colour_of(s);
    for (unsigned pad = 0; pad < 16; ++pad) {
        unsigned col = pad % 4, row = 3 - pad / 4; // pad 1 bottom left
        float x = x0 + col * (cw + gap), y = top + row * (ch + gap);
        const auto& job = shift_pad_job(pad);
        unsigned level = shift_pad_level(s.ui, pad);
        bool on = shift_pad_on(s.ui, pad);
        p.fill(x, y, cw, ch, on ? accent.withAlpha(0.28f) : colour::raised, 4);
        if (on) p.ring(x, y, cw, ch, accent, 4, 1);
        auto ink = level == 0 ? colour::dim : on ? colour::text : colour::sec;
        auto style = plain(ink, type::floor, Weight::Medium);
        auto lines = p.wrap(juce::String(job.name), style, cw - 6, 2);
        float lh = style.size + 1, ty = y + (ch - lh * lines.size()) / 2;
        for (const auto& line : lines) { p.text(line, x + 3, ty, cw - 6, lh, style, juce::Justification::centred); ty += lh; }
    }
}

// INDIGO X DENIM LAB: the undo history a moment after Shift + Minus / Plus: what a redo would bring back, dim, above;
// the state now marked; what an undo takes back below, newest first.
void draw_history(Paint& p, const Snapshot& s) {
    const float w = 190, row = 15, x = zone::width - w - zone::pad, h = row * s.history_count + 22, y = zone::header_y + zone::header_h + 4;
    p.fill(x, y, w, h, colour::panel.withAlpha(0.96f), 6);
    p.ring(x, y, w, h, colour::line, 6, 1);
    p.text("HISTORY", x + 8, y + 3, w - 16, 14, caps_text(colour::mid));
    float ry = y + 19;
    bool marked = false;
    for (unsigned k = 0; k < s.history_count; ++k) {
        const auto& line = s.history[k];
        if (line.done && !marked) { p.fill(x + 6, ry - 1, w - 12, 1, colour_of(s)); marked = true; } // now
        juce::String what(line.what.data());
        auto style = plain(line.done ? colour::text : colour::dim, type::small, line.done && k == 0 ? Weight::SemiBold : Weight::Regular);
        p.text(what, x + 10, ry, w - 20, row, p.fit(what, style, w - 20, type::floor));
        ry += row;
    }
}

// INDIGO X DENIM LAB: Settings > AUDIO > CPU / RAM bar: a thin strip along the screen's foot, the cores' average, the
// audio blocks' share of their time and the memory in use, each a word, a number and a short bar going amber then red.
void draw_system_bar(Paint& p, const Snapshot& s) {
    const auto& m = s.machine;
    unsigned cpu = 0;
    for (unsigned c = 0; c < m.cores && c < m.core_load.size(); ++c) cpu += m.core_load[c];
    if (m.cores) cpu /= m.cores;
    const std::pair<const char*, int> parts[] {{"CPU", int(cpu)}, {"DSP", int(std::lround(m.render_load * 100))}, {"RAM", int(m.ram_used)}};
    const float h = 14, y = zone::height - h;
    p.fill(0, y, zone::width, h, colour::bg.withAlpha(0.88f));
    p.fill(0, y, zone::width, 0.5f, colour::quiet);
    float x = 6, part_w = (zone::width - 12) / 3.0f;
    for (const auto& [name, value] : parts) {
        int v = std::clamp(value, 0, 100);
        auto ink = v >= 85 ? colour::rec : v >= 65 ? colour::amber : colour::sec;
        float tw = p.text(name, x, y + 1, 34, h - 1, caps_text(colour::mid));
        juce::String number = m.cores || name[0] == 'D' ? juce::String(v) + "%" : juce::String("-");
        float nw = p.text(number, x + tw + 3, y + 1, 34, h - 1, plain(ink, type::floor, Weight::Medium));
        float bx = x + tw + nw + 7, bw = part_w - (bx - x) - 8;
        if (bw > 6) {
            p.fill(bx, y + 6, bw, 2, colour::quiet, 1);
            p.fill(bx, y + 6, bw * v / 100.0f, 2, ink, 1);
        }
        x += part_w;
    }
}

// Note Repeat on: B1-B3 are its rate, 1/8, 1/16, 1/32, and with Shift held their triplets; B1 and B3 again 1/4 and
// 1/64 (dots under them, roadmap P1).
void draw_repeat_keys(Paint& p, const Snapshot& s) {
    bool shift = s.ui.shift;
    int active = -1;
    unsigned now = s.track().arp_rate;
    SoftKeys keys {{}, -1, colour_of(s)};
    for (int k = 0; k < 3; ++k) {
        unsigned rate = repeat_shown(now, k, shift);
        keys.labels[size_t(k)] = arp_rate_name(rate);
        if (now == rate) active = k;
        if (repeat_other(k, shift) != repeat_rate(k, shift)) { // B1 and B3 again: 1/4 and 1/64 (roadmap P1)
            keys.dots[size_t(k)] = 2; keys.dot_at[size_t(k)] = uint8_t(now == repeat_other(k, shift) ? 1 : 0);
        }
    }
    keys.active = active;
    p.fill(0, zone::soft_y, zone::width, zone::soft_h, colour::bg);
    soft_keys(p, keys);
}

void draw_shift_layer(Paint& p, const Snapshot& s) {
    // The Shift jobs are printed on the MPC: the page stays as it is, and
    // the knob strip shows the cards Shift turns (Shift + K), or dims when
    // there are none.
    auto cards = cards_of(s);
    bool any = false;
    for (auto& card : cards) { if (card.shift == 2) any = true; else card.state = KnobCard::State::Idle; }
    if (any) knob_strip(p, cards);
    else p.dim_zone(zone::knobs_y, zone::knobs_h);
}

// The fader moved (parity phase 8) and drives a value none of K1-K3 shows
// here (round 3, fader readout B: a card that shows it says so itself,
// cards_of): for a moment the header says what and how much, where the
// notices go. A filled chip once the fader has the value, hollow until it
// reaches it (takeover). The Fader page says it on the page.
void draw_fader_header(Paint& p, const Snapshot& s) {
    if (!fader_moved(s) || fader_card(s) >= 0) return;
    auto shown = fader_shown(s);
    auto c = colour_of(s);
    Header h = header_of(s);
    if (shown.off) {
        h.left.push_back(pill(Icon::Fader, juce::String::fromUTF8("OFF \xc2\xb7 SHIFT + PAD 9")));
    } else if (!shown.value) {
        h.left.push_back(pill(Icon::Fader, shown.what));
    } else {
        h.left.push_back(shown.waiting ? pill(Icon::Fader, shown.what, c) : chip(Icon::Fader, shown.what, c));
        juce::String value = shown.card.value + (shown.card.unit.isNotEmpty() ? " " + shown.card.unit : juce::String());
        h.left.push_back(label(value, shown.waiting ? colour::mid : colour::text));
    }
    h.right.push_back(tag_of(s, s.ui.track));
    p.fill(0, zone::header_y, zone::width, zone::header_h, colour::bg);
    header(p, h);
}

// Over the header: copying a pad (until it is done), or for a moment what
// an undo, redo, delete or copy did.
void draw_notice(Paint& p, const Snapshot& s) {
    juce::String text;
    bool copying = s.ui.copying != UiState::Copying::Off;
    if (!copying && s.ui.captured_at && s.now - s.ui.captured_at < 3000) { // Capture (capture C): what landed, and the undo keys
        p.fill(0, zone::header_y, zone::width, zone::header_h, colour::bg);
        Header h = header_of(s);
        h.left.push_back(chip(Icon::Check, "P" + juce::String(s.ui.captured_pattern + 1), colour_of(s)));
        if (s.ui.captured_tracks > 1) h.left.push_back(pill("+" + juce::String(s.ui.captured_tracks - 1), colour::text)); // roadmap P5: other tracks too
        h.left.push_back(pill(Icon::Undo, juce::String::fromUTF8("SHIFT \xe2\x88\x92")));
        h.right.push_back(tag_of(s, s.ui.track));
        header(p, h);
        return;
    }
    if (s.ui.copying == UiState::Copying::Pads) {
        char label[4]; pad_label(unsigned(std::max<int>(s.ui.copy_from, 0)), label);
        text = s.ui.copy_from < 0 ? juce::String("Copy: pick a pad") : "Copy " + juce::String(label) + ": pick where";
    } else if (s.ui.copying == UiState::Copying::Patterns) {
        text = s.ui.copy_from < 0 ? juce::String("Copy: pick a pattern") : "Copy P" + juce::String(s.ui.copy_from + 1) + ": pick where";
    } else if (s.ui.copying == UiState::Copying::Tracks) {
        juce::String verb = s.ui.moving ? "Move" : "Copy";
        text = s.ui.copy_from < 0 ? verb + ": pick a track" : verb + " track " + juce::String(s.ui.copy_from + 1) + ": pick where";
    } else if (s.ui.notice[0] && s.now - s.ui.notice_at < 1500) {
        text = juce::String::fromUTF8(s.ui.notice); // notices may hold an en dash or a middle dot
    } else if (s.ui.mute_mode() && !tracks_on_pads(s.ui)) { // mute mode: what the pads mute here
        const auto& t = s.track();
        bool kit = mute_on_kit(s.ui, t.engine == EngineKind::Sampler && !t.levels && pad_job(s.ui) == play_pad);
        text = kit ? "Mute: pads" : "Mute: tracks";
        copying = true; // in the track's colour, as a mode
    } else {
        return;
    }
    p.fill(0, zone::header_y, zone::width, zone::header_h, colour::bg);
    Header h = header_of(s);
    h.left.push_back(chip(text.toUpperCase(), copying ? colour_of(s) : colour::text));
    h.right.push_back(tag_of(s, s.ui.track));
    header(p, h);
}

// A project loading (Context::send_reload): its name, and a bar of its samples as they come in.
void draw_loading_sheet(Paint& p, const Snapshot& s) {
    p.fill(0, 0, zone::width, zone::height, colour::bg); // the knob strip too: K1-K3 set nothing while it loads
    Header h = header_of(s);
    h.playing = false; h.recording = false;
    h.left.push_back(pill(Icon::File, "LOADING", colour::amber));
    header(p, h);
    const float x = 24, w = zone::width - 2 * x;
    juce::String name = juce::String::fromUTF8(s.project.name, int(strnlen(s.project.name, sizeof s.project.name)));
    if (name.isEmpty()) name = "NEW PROJECT";
    auto title = p.fit(name, plain(colour::text, type::title, Weight::SemiBold), w, type::body);
    p.text(name, x, zone::picture_y + 28, w, 28, title, juce::Justification::centred);
    unsigned total = s.ui.loading_total, in = total - std::min<unsigned>(s.ui.loading_left, total);
    float fill = total ? float(in) / float(total) : 0.0f;
    gauge(p, x, zone::picture_y + 78, w, fill, colour::amber, 4);
    if (!total) return;
    icon(p, Icon::Wav, x, zone::picture_y + 92, 14, colour::mid);
    p.text(juce::String(in) + " / " + juce::String(total), x, zone::picture_y + 92, w, 14, caps_text(colour::sec),
           juce::Justification::centredRight);
}

void draw_exit_sheet(Paint& p, const Snapshot& s) {
    p.fill(0, 0, zone::width, zone::knobs_y, colour::bg);
    p.dim_zone(zone::knobs_y, zone::knobs_h);
    // Soft keys: STAY under B1, POWER OFF under B2, the MPC app under B3 (Push never quits); a tile under each says it again.
    SoftKeys keys {{"STAY", "POWER OFF", "MPC APP"}, s.ui.exit_off ? 1 : -1, colour::text, {KeyIcon::Back, KeyIcon::None, KeyIcon::None}};
    keys.glyphs = {Icon::None, Icon::Power, Icon::Mpc};
    soft_keys(p, keys);

    // The header says what happens to the project: saved (a check), or no card to save it on.
    bool card = !s.projects.root.empty();
    Header h = header_of(s);
    h.playing = false; h.recording = false;
    if (!card) h.left.push_back(pill(Icon::Card, "NO CARD", colour::amber));
    else if (s.ui.exit_off && !s.ui.exit_saved) h.left.push_back(pill(Icon::Rec, "SAVING", colour::amber));
    else if (!s.ui.exit_off && s.dirty) h.left.push_back(pill(Icon::Rec, "UNSAVED", colour::amber)); // as Project says it: OFF and MPC APP save it first
    else h.left.push_back(pill(Icon::Check, s.project_path.empty() ? "SAVED AS NEW" : "SAVED", colour::play));
    h.right.push_back(label(s.project.name));
    header(p, h);
    if (s.ui.exit_off) { // POWER OFF chosen: the save on its way, then the MPC goes dark
        key_tiles(p, {KeyTile {}, word_tile(Icon::Power, s.ui.exit_saved || !card ? "TURNING OFF" : "SAVING", true), KeyTile {}});
        return;
    }
    key_tiles(p, {word_tile(Icon::Back, "STAY", false, colour::sec), word_tile(Icon::Power, "POWER OFF"), word_tile(Icon::Mpc, "MPC APP", true)});
}

} // namespace melodee
