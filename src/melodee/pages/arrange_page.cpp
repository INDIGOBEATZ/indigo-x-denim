// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 Kerem Kilic (Ellic Studio)
// INDIGO X DENIM LAB: ARRANGE, after MPC 3.10's Arrange mode on the MPC One G2 (standalone): every track's pattern
// on one linear timeline, a lane each, its notes drawn as marks by pitch (a pattern shorter than the view repeats,
// faint), the loop brace over the ruler, each lane's playhead, the cursor.
//
// - The wheel moves the cursor; Shift + wheel draws a selection from where it was (MPC 3.10's Shift + drag).
// - Pads pick the track (its lane comes into view); in MRQ, a pad held and another tapped spans the marquee's lanes.
// - B1 the edit mode: MRQ (the marquee's bars over its lanes), TRK (the selected track only), ARR (every lane).
//   With no selection drawn, an edit takes the loop brace (as MPC 3.10's ARR).
// - B2 EDIT: Copy, Cut, Paste, Paste insert, Duplicate, Delete, Clear, Loop to selection, Select all, Deselect.
// - B3 GRID (or Push): the selected track's notes in Roll at the cursor (MPC's track editor).
// - Shift + B1 / B2 / B3: COPY, PASTE, DUPLICATE straight away. Minus / Plus: a bar back or on.
// - K1 zoom (1 to 128 bars), K2 or the FADER lanes in view (1 to 16), K3 the selected track's pattern length in bars.
#include "melodee/pages/common.h"
#include "melodee/pages/pages.h"
#include "melodee/shell/context.h"
#include "melodee/shell/roll.h"
#include "melodee/ui/widgets.h"
#include <algorithm>
#include <cmath>

namespace melodee {
namespace {
using namespace ui;

juce::String dash() { return juce::String::fromUTF8("\xe2\x80\x93"); }
constexpr unsigned zoom_count = sizeof(arrange_zooms) / sizeof(arrange_zooms[0]);
enum Row : uint8_t { RowCopy, RowCut, RowPaste, RowInsert, RowDuplicate, RowDelete, RowClear, RowLoop, RowAll, RowDeselect, rows };

uint32_t span_of(const UiState& ui, const Project& project) { return arrange_zooms[ui.arr_zoom % zoom_count] * project.bar_ticks(); }
unsigned lanes_of(const UiState& ui) { return std::clamp<unsigned>(ui.arr_lanes, 1, 16); } // LAB: 1..16 (K2, the FADER)
// What one detent moves: a sixteenth of the view, in whole sixteenths, up to bars when zoomed far out.
uint32_t snap_of(const UiState& ui, const Project& project) {
    uint32_t span = span_of(ui, project), bar = project.bar_ticks();
    if (span >= 16 * bar) return std::max(bar, span / 32 / bar * bar);
    return std::max<uint32_t>(sixteenth_ticks, span / 16 / sixteenth_ticks * sixteenth_ticks);
}
juce::String bar_beat(const Project& project, uint32_t tick) {
    uint32_t bar = project.bar_ticks();
    return juce::String(tick / bar + 1) + "." + juce::String(tick % bar / ticks_per_quarter + 1);
}
void lanes_selected(const UiState& ui, unsigned& t0, unsigned& t1) {
    switch (ui.arr_mode % 3) {
    case 0: t0 = std::min(ui.arr_t0, ui.arr_t1); t1 = std::max(ui.arr_t0, ui.arr_t1); break;
    case 1: t0 = t1 = ui.track % track_count; break;
    default: t0 = 0; t1 = track_count - 1; break;
    }
}

void draw(Paint& p, const Snapshot& s) {
    const auto& ui = s.ui;
    const auto& project = s.project;
    auto c = colour_of(s);
    static const char* const modes[] {"MRQ", "TRK", "ARR"};
    Header h = header_of(s);
    h.left.push_back(chip("ARRANGE", colour::text));
    h.left.push_back(pill("BAR " + bar_beat(project, ui.arr_cursor), c));
    if (ui.arr_selected && ui.arr_to > ui.arr_from)
        h.left.push_back(pill(juce::String(modes[ui.arr_mode % 3]) + " " + bar_beat(project, ui.arr_from) + dash() + bar_beat(project, ui.arr_to), colour::amber));
    h.right.push_back(tag_of(s, ui.track));
    SoftKeys keys {{modes[ui.arr_mode % 3], "EDIT", "GRID"}, -1, c};
    keys.dots[0] = 3; keys.dot_at[0] = uint8_t(ui.arr_mode % 3);
    if (ui.shift) keys = SoftKeys {{"COPY", "PASTE", "DUPLICATE"}, -1, c};
    if (ui.arr_sheet) keys = SoftKeys {{"BACK", "", "DO IT"}, 2, c, {KeyIcon::Back, KeyIcon::None, KeyIcon::Check}};
    frame(p, s, keys, h);

    const uint32_t span = span_of(ui, project), view = ui.arr_view, bar = project.bar_ticks();
    const float lx = arrange_lane_x, lw = arrange_lane_w, px = lw / float(std::max<uint32_t>(span, 1));
    auto x_of = [&](uint32_t tick) { return lx + (float(tick) - float(view)) * px; };
    const float ruler = zone::picture_y, top = ruler + 15, bottom = zone::picture_y + zone::picture_h - 2;
    const unsigned shown = lanes_of(ui), first = std::min<unsigned>(ui.arr_top, track_count - shown);
    const float lane_h = (bottom - top) / shown;

    // The ruler: bar numbers, the loop brace (always shown, as on the MPC), the cursor's notch.
    const unsigned bars = std::max(1u, span / bar), every = bars <= 8 ? 1 : bars / 8;
    for (uint32_t b = view / bar; b * bar < view + span; ++b) {
        float bx = x_of(b * bar);
        p.fill(bx, ruler + 11, 0.5f, 3, colour::line);
        if (b % every == 0 && bx + 2 < lx + lw - 14) p.text(juce::String(b + 1), bx + 2, ruler, 30, 12, plain(colour::mid, type::small, Weight::SemiBold));
    }
    if (ui.brace_to > ui.brace_from) {
        float bl = std::max(lx, x_of(ui.brace_from)), br = std::min(lx + lw, x_of(ui.brace_to));
        if (br > bl) {
            p.fill(bl, ruler + 11, br - bl, 3, colour::amber, 1);
            p.fill(bl, top, br - bl, bottom - top, colour::amber.withAlpha(0.05f));
        }
    }
    // The lanes: header (number, name, mute), notes, the pattern's end, its playhead.
    for (unsigned k = 0; k < shown; ++k) {
        unsigned t = first + k;
        float y = top + k * lane_h, inner = lane_h - 1;
        const auto& lane = s.arrange[t];
        const auto& tr = project.tracks[t];
        bool here = t == ui.track % track_count;
        auto tc = track_colour(t);
        p.fill(2, y, lx + lw - 2 + 2, inner, here ? colour::raised : colour::surf, 2);
        p.fill(2, y, 3, inner, lane.used ? tc : colour::line, 1);
        auto label = plain(here ? colour::text : lane.used ? colour::sec : colour::dim, type::small, here ? Weight::SemiBold : Weight::Regular);
        juce::String name = juce::String(t + 1);
        if (lane.used && inner >= 12) {
            juce::String n = juce::String(tr.name).toUpperCase();
            name += " " + n;
        }
        const float name_w = lx - 10 - (tr.mix.mute ? 10 : 0); // the M takes its corner
        while (name.length() > 2 && p.width(name, label) > name_w) name = name.dropLastCharacters(1);
        if (inner >= 11) p.text(name, 7, y, name_w, std::min(inner, 14.0f), label);
        if (tr.mix.mute && inner >= 11) p.text("M", lx - 11, y, 10, std::min(inner, 14.0f), plain(colour::rec, type::small, Weight::Bold));
        else if (tr.mix.mute) p.fill(lx - 6, y + 1, 3, inner - 2, colour::rec, 1); // too small for the letter
        if (!lane.used) continue;
        p.g.saveState();
        p.g.reduceClipRegion(juce::Rectangle<int>(int(lx), int(y), int(lw), int(inner)));
        float mark_h = std::clamp(inner / 7.0f, 1.5f, 3.0f), room = std::max(1.0f, inner - 4 - mark_h);
        for (unsigned m = 0; m < lane.count; ++m) {
            const auto& mk = lane.marks[m];
            float mx = lx + mk.x0, mw = std::max(1.0f, float(mk.x1 - mk.x0) - 0.5f);
            float my = y + 2 + (1.0f - mk.y / 255.0f) * room;
            float a = (mk.ghost ? 0.35f : 1.0f) * (tr.mix.mute ? 0.4f : 1.0f) * (0.55f + 0.45f * mk.velocity / 127.0f);
            p.fill(mx, my, mw, mark_h, tc.withAlpha(a), mark_h >= 2.5f ? 1.0f : 0.0f);
        }
        if (lane.end_x >= 0) p.fill(lx + lane.end_x, y, 1, inner, colour::mid.withAlpha(0.7f));
        if (lane.play_x >= 0) p.fill(lx + lane.play_x, y, 1.2f, inner, colour::text);
        p.g.restoreState();
    }
    // The selection: the marquee's lanes (TRK the selected one, ARR all) over its time.
    if (ui.arr_selected && ui.arr_to > ui.arr_from) {
        unsigned t0, t1;
        lanes_selected(ui, t0, t1);
        float sl = std::max(lx, x_of(ui.arr_from)), sr = std::min(lx + lw, x_of(ui.arr_to));
        int k0 = std::max<int>(int(t0) - int(first), 0), k1 = std::min<int>(int(t1) - int(first), int(shown) - 1);
        if (sr > sl && k1 >= k0) {
            float sy = top + k0 * lane_h, sh = (k1 - k0 + 1) * lane_h - 1;
            p.fill(sl, sy, sr - sl, sh, colour::text.withAlpha(0.13f), 2);
            p.ring(sl, sy, sr - sl, sh, colour::text, 2, 1);
        }
    }
    // The cursor over every lane.
    float cx = x_of(ui.arr_cursor);
    if (cx >= lx && cx <= lx + lw) { p.fill(cx, ruler + 9, 1, bottom - ruler - 9, colour::text.withAlpha(0.8f)); p.fill(cx - 3, ruler + 8, 7, 2, colour::text); }

    if (ui.arr_sheet) {
        p.dim_zone(zone::picture_y, zone::picture_h, 0.7f);
        MenuSheet m;
        m.title = "EDIT";
        m.detail = juce::String(modes[ui.arr_mode % 3]);
        m.accent = c;
        bool sel = ui.arr_selected && ui.arr_to > ui.arr_from, brace = ui.brace_to > ui.brace_from, any = sel || brace;
        bool board = s.ui.arr_board > 0;
        m.rows[RowCopy] = {"Copy", "", false, false, !any};
        m.rows[RowCut] = {"Cut", "", false, false, !any};
        m.rows[RowPaste] = {"Paste", "at cursor", false, false, !board};
        m.rows[RowInsert] = {"Paste insert", "", false, false, !board};
        m.rows[RowDuplicate] = {"Duplicate", "", false, false, !any};
        m.rows[RowDelete] = {"Delete time", "", false, true, !any};
        m.rows[RowClear] = {"Clear", "", false, true, !any};
        m.rows[RowLoop] = {"Loop to selection", "", false, false, !sel};
        m.rows[RowAll] = {"Select all", "", false, false, false};
        m.rows[RowDeselect] = {"Deselect", "", false, false, !sel};
        m.count = rows; m.current = ui.arr_row % rows;
        menu_sheet(p, m);
    }
}

void act(Context& c, unsigned row) {
    c.ui.arr_sheet = false;
    switch (row) {
    case RowCopy: c.arrange_copy(false); break;
    case RowCut: c.arrange_copy(true); break;
    case RowPaste: c.arrange_paste(false); break;
    case RowInsert: c.arrange_paste(true); break;
    case RowDuplicate: c.arrange_duplicate(); break;
    case RowDelete: c.arrange_delete(false); break;
    case RowClear: c.arrange_delete(true); break;
    case RowLoop: c.arrange_loop_to_selection(); break;
    case RowAll: c.arrange_select_all(); break;
    default: c.ui.arr_selected = false; break;
    }
    c.ui.arr_board = uint8_t(std::min<size_t>(c.arrange_board.tracks.size(), 255));
}

void lane_into_view(Context& c) {
    unsigned shown = lanes_of(c.ui), t = c.ui.track % track_count;
    if (t < c.ui.arr_top) c.ui.arr_top = uint8_t(t);
    if (t >= c.ui.arr_top + shown) c.ui.arr_top = uint8_t(t - shown + 1);
    c.ui.arr_top = uint8_t(std::min<unsigned>(c.ui.arr_top, track_count - shown));
}

void pad(Context& c, unsigned pad, uint8_t, bool down) {
    unsigned t = pad % 16;
    if (!down) { c.ui.arr_pads &= uint16_t(~(1u << t)); return; }
    bool another = c.ui.arr_pads != 0;
    c.ui.arr_pads |= uint16_t(1u << t);
    if (c.ui.arr_mode % 3 == 0) { // MRQ: a pad held and another tapped spans the lanes
        if (another) c.ui.arr_t1 = uint8_t(t);
        else c.ui.arr_t0 = c.ui.arr_t1 = uint8_t(t);
    }
    c.select_track(t);
    lane_into_view(c);
}

void wheel(Context& c, int delta) {
    auto& ui = c.ui;
    if (ui.arr_sheet) { ui.arr_row = uint8_t(std::clamp<int>(ui.arr_row + delta, 0, rows - 1)); return; }
    const uint32_t snap = snap_of(ui, c.project), most = c.project.max_length();
    if (ui.shift) { // Shift + wheel: a selection from where the cursor was
        ui.shift_used = true;
        if (!ui.arr_selected) { ui.arr_selected = true; ui.arr_anchor = ui.arr_cursor / snap * snap; if (ui.arr_mode % 3 == 0) ui.arr_t0 = ui.arr_t1 = uint8_t(ui.track); }
    }
    int64_t to = int64_t(ui.arr_cursor / snap * snap) + int64_t(delta) * snap;
    ui.arr_cursor = uint32_t(std::clamp<int64_t>(to, 0, most));
    if (ui.shift) {
        ui.arr_from = std::min(ui.arr_anchor, ui.arr_cursor);
        ui.arr_to = std::max(ui.arr_anchor, ui.arr_cursor);
        if (ui.arr_to == ui.arr_from) ui.arr_to = ui.arr_from + snap;
    }
    c.arrange_show(ui.arr_cursor);
}

void nudge(Context& c, int way) {
    const uint32_t bar = c.project.bar_ticks();
    int64_t to = int64_t(c.ui.arr_cursor / bar * bar) + int64_t(way) * bar;
    c.ui.arr_cursor = uint32_t(std::clamp<int64_t>(to, 0, c.project.max_length()));
    c.arrange_show(c.ui.arr_cursor);
}

void grid(Context& c) { // the selected track's notes at the cursor (MPC's track editor: Roll here)
    if (!c.track().used()) { c.notice("Empty track"); return; }
    const auto& pattern = c.pattern();
    uint32_t length = std::max<uint32_t>(pattern.length, 1), q = std::max(1u, pattern.q());
    c.open(PageId::Roll);
    c.ui.step_cursor = int16_t(std::min<uint32_t>(c.ui.arr_cursor % length / q, 0x7fff)); // Roll's cursor where ARRANGE's was
    roll_follow(pattern, c.ui);
}

void push(Context& c) {
    if (c.ui.arr_sheet) { act(c, c.ui.arr_row % rows); return; }
    grid(c);
}

void soft(Context& c, int key) {
    auto& ui = c.ui;
    if (ui.arr_sheet) { if (key == 0) ui.arr_sheet = false; if (key == 2) act(c, ui.arr_row % rows); return; }
    if (ui.shift) {
        ui.shift_used = true;
        if (key == 0) act(c, RowCopy);
        if (key == 1) act(c, RowPaste);
        if (key == 2) act(c, RowDuplicate);
        return;
    }
    if (key == 0) ui.arr_mode = uint8_t((ui.arr_mode + 1) % 3);
    if (key == 1) { ui.arr_sheet = true; }
    if (key == 2) grid(c);
}
} // namespace

const PageSpec arrange_page {draw, pad, wheel, push, soft, nudge};

} // namespace melodee
