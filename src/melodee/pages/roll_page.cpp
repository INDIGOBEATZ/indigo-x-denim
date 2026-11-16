// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 Kerem Kilic (Ellic Studio)
// ROLL (B2 on a sequence page, or Seq held + B2): a piano roll of the
// selected track's pattern, read from its events (shell/roll.h); on a kit a
// lane per pad, numbered as the pads are (the kit's bank in view). Each
// note sits at its own tick for its own length, over bar lines, any number
// to a step. The cursor is a step of Q (the wheel; Shift + wheel note to
// note) and a pitch (Minus / Plus: the chord in the step, then a semitone);
// the note there is in focus, drawn white, and the cards edit it (K1 length,
// K2 velocity, K3 chance; B2 again the TRIG row: K1 a ratchet's hits, drawn
// along the note, K2 their velocity ramp, Shift + K2 a kit pad's pitch ramp).
// The view scrolls with the cursor a step at a time;
// dots on the top and bottom edges mark notes above and below it. Pads write
// their note at the cursor, NEW NOTE long, or focus it; Pad Bank moves their
// octave (Shift: down). Erase tapped alone deletes the note in focus. Push
// opens its sheet on the likely job, ACCENT (B3 or Push toggles it and
// closes), B2 MORE lists Accent, Slide, Tie and Delete (B3 or Push do the
// row, B1 back); a pad or a pot closes it. With Seq Record on while
// playing, pads play in live.
// INDIGO X DENIM: MORE also lists Quantize (the note onto its nearest step),
// Legato (as long as it takes to reach the next note) and Duplicate (a copy
// straight after it; the cursor goes with each). Push with no note in focus
// opens the VIEW sheet: Follow playhead (while playing the view turns its
// pages with the playhead, the cursor staying put; FOLLOW in the header),
// Scale lanes (the keys outside the project's scale sink), Quantize all and
// Legato all. The two switches are Melodee's settings, kept on the card.
// Shift + Push picks the note in focus (or lets it go): picked notes carry a
// white ring and SEL n shows in the header; VIEW's Select all picks every
// note. Push with a selection opens the SELECTION sheet: move them a step
// earlier or later, a semitone or an octave up or down, duplicate them
// straight after, quantize, delete, or clear the selection (B3 or Push do
// the row; the sheet stays for moves, so a row can be done again).
#include "melodee/engine/engine_host.h"
#include "melodee/model/scales.h"
#include "melodee/pages/common.h"
#include "melodee/pages/pages.h"
#include "melodee/shell/ask.h"
#include "melodee/shell/project_store.h"
#include "melodee/shell/roll.h"
#include "melodee/shell/step_edit.h"
#include "melodee/shell/step_lane.h"
#include "melodee/ui/widgets.h"
#include <algorithm>

namespace melodee {
namespace {
using namespace ui;
constexpr float gutter = 23, ruler = 13, row_h_12 = 10.5f, column_1x = 18.5f;
constexpr unsigned columns_1x = roll_columns, columns_most = roll_columns * 8;
enum SheetRow : uint8_t { RowAccent, RowSlide, RowTie, RowQuantize, RowLegato, RowDuplicate, RowDelete, sheet_rows };
enum ViewRow : uint8_t { ViewFollow, ViewScale, ViewQuantize, ViewLegato, ViewSelectAll, ViewPaste, view_rows }; // LAB: Paste notes
enum DelRow : uint8_t { DelNote, DelLane, DelPicks, del_rows }; // LAB: the DELETE sheet (Shift + B3)
constexpr uint32_t del_window = 2000; // LAB: ms for B3's second tap on a note
// LAB: B3's first tap armed on this very note, and not long ago.
bool roll_armed(const UiState& ui, uint32_t now, const Event* note) {
    return note && ui.roll_del_armed && now - ui.roll_del_armed < del_window && note->tick == ui.roll_del_tick && note->key == ui.roll_del_key;
}
bool kit_lanes(const Track& t) { return t.engine == EngineKind::Sampler || drum_pads(t); }
enum SelRow : uint8_t { SelEarlier, SelLater, SelUp, SelDown, SelOctaveUp, SelOctaveDown, SelDuplicate, SelQuantize, SelCopy, SelDelete, SelClear, sel_rows }; // LAB: Copy notes

bool black(int key) { return (0x54A >> (key % 12)) & 1; } // C#, D#, F#, G#, A#

juce::String where(const Snapshot& s, uint32_t tick) {
    return juce::String(roll_position(tick, s.project.bar_ticks(), s.project.beat_ticks(), s.pattern.q()));
}

void draw(Paint& p, const Snapshot& s) {
    // LAB: the rows in view, 1..16 (a kit's FADER zooms them); twelve fill the picture as before.
    const unsigned rows = s.ui.roll_rows_shown();
    const float row_h = row_h_12 * float(roll_rows) / float(rows);
    const auto& pattern = s.pattern;
    UiState ui = s.ui;
    roll_follow(pattern, ui); // the view onto the cursor, even when it came from another page
    auto c = colour_of(s);
    bool following = ui.settings.roll_follow && s.playing && pattern.length; // Follow playhead: the playhead's page
    // INDIGO X DENIM, the zoom (Shift + K3): `columns` steps in view, each `column` wide, the picture as wide as ever.
    const unsigned span = ui.roll_span(), columns = columns_1x * span;
    const float column = column_1x / float(span);
    unsigned q = pattern.q(), cursor = roll_cursor(pattern, ui), first = following ? roll_play_first(pattern, s.position, span) : roll_first(pattern, ui);
    unsigned bar = s.project.bar_ticks(), beat = s.project.beat_ticks();
    uint32_t from = first * q, to = (first + columns) * q;
    int focus = roll_focus(pattern, ui), top_key = ui.roll_top;
    const Event* focused = focus >= 0 ? &pattern.events[size_t(focus)] : nullptr;

    const auto& track = s.track();
    bool kit = track.engine == EngineKind::Sampler || drum_pads(track); // lanes of pads, not keys (Drums pads too: 36 is A01)
    Header h = header_of(s);
    if (!ui.recording) {
        if (focused && ui.held_note >= 0) h.left.push_back(pill("LOCK", c)); // its pad held: the pots lock it (roadmap P1)
        if (focused) h.left.push_back(chip(Icon::Note, juce::String(key_label(track, focused->key)), c)); // where it sits: the place bar's
        else h.left.push_back(pill("BAR " + juce::String(cursor * q / bar + 1) + " / " + juce::String((pattern.length + bar - 1) / bar)));
        if (ui.roll_sel_count) h.left.push_back(chip("SEL " + juce::String(int(ui.roll_sel_count)), c)); // the selection
        else if (!focused && ui.settings.roll_follow) { auto f = pill("FOLLOW", following ? c : colour::mid); f.spare = true; h.left.push_back(f); } // lit while it turns
    }
    h.right.push_back(tag_of(s, ui.track));
    bool del_sheet = ui.sheet_open && ui.roll_sheet_del; // LAB
    bool sel_sheet = ui.sheet_open && ui.roll_sheet_sel && !del_sheet;
    bool view_sheet = ui.sheet_open && ui.roll_sheet_view && !sel_sheet && !del_sheet;
    bool sheet = ui.sheet_open && !ui.roll_sheet_view && !sel_sheet && !del_sheet && focused;
    const int lane_key = focused ? int(focused->key) : int(ui.roll_key);
    SoftKeys keys {{"STEPS", "ROLL", "MOTION"}, 1, c};
    keys.dots[1] = 2; keys.dot_at[1] = ui.trig_cards ? 1 : 0; // ROLL again: the TRIG row of cards
    if (!ui.sheet_open && focused) { // LAB: B3 on a note: DELETE, twice (the first arms it)
        bool armed = roll_armed(ui, s.now, focused);
        keys.labels[2] = armed ? "DELETE?" : "DELETE";
        keys.tints[2] = colour::rec;
        if (armed) { keys.toggles = 4; keys.lit = 4; keys.leds[2] = colour::rec; }
    }
    if (ui.shift && !ui.sheet_open) keys = SoftKeys {{"STEPS", "ROLL", "DELETE"}, 1, c}; // LAB: Shift + B3, the DELETE sheet
    if (del_sheet && ui.sheet_more) keys = SoftKeys {{"BACK", "", "DO IT"}, 2, c, {KeyIcon::Back, KeyIcon::None, KeyIcon::Check}};
    else if (del_sheet) { keys = SoftKeys {{"BACK", "MORE", "DELETE"}, 2, c, {KeyIcon::Back, KeyIcon::None, KeyIcon::None}}; keys.tints[2] = colour::rec; }
    else if (view_sheet || sel_sheet) keys = SoftKeys {{"BACK", "", "DO IT"}, 2, c, {KeyIcon::Back, KeyIcon::None, KeyIcon::Check}};
    else if (sheet && ui.sheet_more) keys = SoftKeys {{"BACK", "", "DO IT"}, 2, c, {KeyIcon::Back, KeyIcon::None, KeyIcon::Check}};
    else if (sheet) { // the likely job, Accent, under B3; MORE the rest
        keys = SoftKeys {{"BACK", "MORE", "ACCENT"}, 2, c, {KeyIcon::Back, KeyIcon::None, KeyIcon::None}};
        keys.glyphs = {Icon::None, Icon::Layers, Icon::Accent};
    }
    frame(p, s, keys, h);

    float top = zone::picture_y + ruler, height = rows * row_h, right = gutter + columns * column;
    auto x_of = [&](uint32_t tick) { return gutter + (float(tick) - float(from)) / float(q) * column; };
    // The ruler: bar.beat over each beat, where there is room for it.
    float last_label = -100;
    for (unsigned k = 0; k < columns; ++k) {
        uint32_t tick = (first + k) * q;
        if (tick % (span >= 4 ? bar : beat) || gutter + k * column < last_label + 34) continue; // zoomed far out: the bars
        last_label = gutter + k * column;
        p.text(juce::String(tick / bar + 1) + "." + juce::String(tick % bar / beat + 1), last_label + 2, zone::picture_y, 32, 13,
               plain(colour::mid, type::small, Weight::SemiBold));
    }
    for (unsigned r = 0; r < rows; ++r) {
        int key = top_key - int(r);
        bool shade = kit ? (key - int(sampler_first_note)) / 4 % 2 != 0 : key >= 0 && black(key); // a kit: pads in fours
        bool outside = !kit && ui.settings.roll_scale && !roll_in_scale(key, s.project.root, s.project.scale); // Scale lanes
        p.fill(0, top + r * row_h, zone::width, row_h - 0.5f, outside ? colour::bg : shade ? colour::quiet : colour::panel);
    }
    if (kit) { // the pads' numbers in their bank, every other lane so each has room, the cursor's always
        for (unsigned r = 0; r < rows; ++r) {
            int key = top_key - int(r), pad = key - int(sampler_first_note);
            if (pad < 0 || (key - int(ui.roll_key)) % 2) continue;
            p.text(juce::String(pad % 16 + 1), 0, top + r * row_h - 2, gutter - 4, row_h + 3,
                   plain(key == int(ui.roll_key) ? c : colour::dim, type::small, Weight::SemiBold), juce::Justification::centredRight);
        }
    }
    // Names on C, F, G and A and the root: never two side by side, so each has room at the floor size.
    int root = int(s.project.root % 12);
    for (unsigned r = 0; r < rows && !kit; ++r) {
        int key = top_key - int(r);
        if (key < 0) break;
        int pitch = key % 12;
        bool named = pitch == root || ((pitch == 0 || pitch == 5 || pitch == 7 || pitch == 9) && std::abs(pitch - root) > 1 && std::abs(pitch - root) < 11);
        if (black(key) || !named) continue;
        p.text(note_name(key), 0, top + r * row_h - 2, gutter - 4, row_h + 3, plain(pitch == root ? c : colour::dim, type::small, Weight::SemiBold),
               juce::Justification::centredRight);
    }
    p.fill(gutter - 1, top, 1, height, colour::line);
    for (unsigned k = 1; k < columns; ++k) {
        uint32_t tick = (first + k) * q;
        bool on_bar = tick % bar == 0, on_beat = tick % beat == 0;
        if (span > 1 && !on_beat) continue; // zoomed out: the beats and bars alone
        p.fill(gutter + k * column, top, on_beat ? 1 : 0.5f, height, on_bar ? colour::mid.withAlpha(0.45f) : on_beat ? colour::line : colour::bg);
    }
    // The cursor: its step's column and its pitch's row.
    if (cursor >= first && cursor < first + columns) p.fill(gutter + (cursor - first) * column, top, column, height, c.withAlpha(0.12f));
    int cursor_row = top_key - int(ui.roll_key);
    if (cursor_row >= 0 && cursor_row < int(rows)) p.fill(gutter, top + cursor_row * row_h, right - gutter, row_h - 0.5f, c.withAlpha(0.09f));
    // The notes, each at its own tick and for its own length; those above or below the rows are dots on the edge.
    std::array<uint8_t, columns_most> above {}, below {};
    for (size_t n = 0; n < pattern.events.size(); ++n) {
        const auto& e = pattern.events[n];
        if (e.tick >= pattern.length) break;
        uint32_t end = std::min<uint32_t>(e.tick + e.length, pattern.length);
        if (end <= from || e.tick >= to) continue;
        int r = top_key - int(e.key);
        if (r < 0 || r >= int(rows)) {
            if (e.tick >= from && (e.tick - from) / q < columns) (r < 0 ? above : below)[(e.tick - from) / q] = 1;
            continue;
        }
        float x = std::max(x_of(e.tick), gutter) + 1, xe = std::min(x_of(end), right + 1), y = top + r * row_h + 1;
        float w = std::max(xe - x - 1, 2.0f), hgt = row_h - 2.5f;
        bool on = int(n) == focus;
        unsigned hits = e.hits();
        if (hits == 1) {
            p.fill(x, y, w, hgt, on ? colour::text : c.withAlpha(0.4f + e.velocity / 127.0f * 0.6f), 2.5f);
        } else { // a ratchet: its hits along the note, each as strong as it plays (the ramp)
            float each = float(e.length) / float(hits) / float(q) * column;
            for (unsigned k = 0; k < hits; ++k) {
                float hx = x_of(e.tick) + 1 + k * each, hxe = std::min(hx + std::max(each - 1.0f, 1.0f), right + 1);
                if (hxe <= gutter || hx >= right + 1) continue;
                hx = std::max(hx, gutter + 1);
                float v = e.hit_velocity(e.velocity, k) / 127.0f;
                p.fill(hx, y, std::max(hxe - hx, 1.0f), hgt, on ? colour::text : c.withAlpha(0.4f + v * 0.6f), 1.5f);
            }
        }
        if (on) p.ring(x - 1, y - 1, w + 2, hgt + 2, c, 3, 1.2f);
        else if (ui.roll_sel_count && roll_picked(ui, e.tick, e.key)) p.ring(x - 1, y - 1, w + 2, hgt + 2, colour::text, 3, 1.2f); // picked
        bool kit_lane = track.engine == EngineKind::Sampler;
        if (w > 6 && lock_in(pattern, e.tick, e.tick + 1, kit_lane ? int(e.key) - int(sampler_first_note) : -1)) // a Motion lock on it
            p.fill(x + 1, y + 1, 3, 3, on ? c : colour::bg, 0.5f);
        if (e.condition != trig_always && w > 8) // a trig condition: a ring at its end
            p.ring(x + w - 6, y + hgt / 2 - 2.5f, 5, 5, on ? c : colour::bg, 2.5f, 1.2f);
        if ((e.flags & step_accent) && w > 6) p.fill(x + 1.5f, y + 1.5f, 2, hgt - 3, on ? c : colour::bg, 1);
    }
    for (unsigned k = 0; k < columns; ++k) {
        float cx = gutter + k * column + column / 2;
        if (above[k]) p.circle(cx, top + 2, 1.8f, colour::mid);
        if (below[k]) p.circle(cx, top + height - 2.5f, 1.8f, colour::mid);
    }
    if (s.playing && pattern.length) {
        uint32_t at = uint32_t(s.position % pattern.length);
        if (at >= from && at < to) p.fill(x_of(at), top - 3, 1, height + 3, colour::text);
    }
    // Where the view sits: the whole pattern along the bottom with its notes, pitch along the right edge.
    float tw = zone::width - gutter - 3, by = top + height + 3.5f;
    p.fill(gutter, by, tw, 2, colour::line, 1);
    for (const auto& e : pattern.events) {
        if (e.tick >= pattern.length) break;
        p.fill(gutter + tw * float(e.tick) / float(pattern.length), by, 1, 2, colour::dim);
    }
    float steps = float(pattern.steps());
    p.fill(gutter + tw * first / steps, by, tw * std::min<float>(columns, steps) / steps, 2, colour::mid.withAlpha(0.7f), 1);
    float place = std::clamp((top_key - 12) / 115.0f, 0.0f, 1.0f);
    p.fill(zone::width - 3, top, 2, height, colour::line, 1);
    p.fill(zone::width - 3, top + height * (1 - place) * 0.74f, 2, height * 0.26f, colour::mid, 1);

    if (del_sheet && !ui.sheet_more) { // LAB: the likely job, the note in focus (else every note of its lane)
        auto tile = focused ? word_sub_tile(Icon::Note, "DELETE", "NOTE " + juce::String(key_label(track, focused->key)))
                            : word_sub_tile(Icon::Note, "DELETE ALL", juce::String(key_label(track, lane_key)));
        tile.colour = colour::rec;
        likely_sheet(p, tile, del_rows - 1);
    } else if (del_sheet) {
        p.dim_zone(zone::picture_y, zone::picture_h, 0.7f);
        MenuSheet m;
        m.title = "DELETE"; m.detail = juce::String(key_label(track, lane_key)); m.accent = c;
        int lane_notes = 0;
        for (const auto& e : pattern.events) if (e.key == lane_key) ++lane_notes;
        m.rows[DelNote] = {"This note", "", false, true, !focused};
        m.rows[DelLane] = {juce::String(kit ? "All of this pad" : "All of this key"), juce::String(lane_notes) + (lane_notes == 1 ? " note" : " notes"), false, true, lane_notes == 0};
        m.rows[DelPicks] = {"Selected notes", ui.roll_sel_count ? juce::String(int(ui.roll_sel_count)) : juce::String(), false, true, ui.roll_sel_count == 0};
        m.count = del_rows; m.current = ui.sheet_row % del_rows;
        menu_sheet(p, m);
    } else if (sheet && !ui.sheet_more) {
        bool accent = focused->flags & step_accent;
        auto tile = word_sub_tile(Icon::Accent, "ACCENT", accent ? "ON" : "OFF");
        tile.colour = c;
        likely_sheet(p, tile, sheet_rows - 1);
    } else if (sheet) {
        p.dim_zone(zone::picture_y, zone::picture_h, 0.7f);
        MenuSheet m;
        m.title = "NOTE"; m.detail = where(s, focused->tick) + juce::String::fromUTF8(" \xc2\xb7 ") + juce::String(note_name(focused->key)); m.accent = c;
        auto flag = [&](uint8_t f) { return bool(focused->flags & f); };
        m.rows[RowAccent] = {"Accent", flag(step_accent) ? "On" : "Off", flag(step_accent), false, false};
        m.rows[RowSlide] = {"Slide", flag(step_slide) ? "On" : "Off", flag(step_slide), false, false};
        m.rows[RowTie] = {"Tie to next", flag(step_tie) ? "On" : "Off", flag(step_tie), false, false};
        m.rows[RowQuantize] = {"Quantize", "", false, false, focused->tick % q == 0};
        m.rows[RowLegato] = {"Legato", "", false, false, false};
        m.rows[RowDuplicate] = {"Duplicate", "", false, false, false};
        m.rows[RowDelete] = {"Delete note", "", false, true, false};
        m.count = sheet_rows; m.current = ui.sheet_row % sheet_rows;
        menu_sheet(p, m);
    } else if (view_sheet) {
        p.dim_zone(zone::picture_y, zone::picture_h, 0.7f);
        MenuSheet m;
        m.title = "VIEW"; m.detail = "ROLL"; m.accent = c;
        bool follow = ui.settings.roll_follow, lanes = ui.settings.roll_scale, empty = pattern.empty();
        m.rows[ViewFollow] = {"Follow playhead", follow ? "On" : "Off", follow, false, false};
        m.rows[ViewScale] = {"Scale lanes", lanes ? "On" : "Off", lanes, false, kit};
        m.rows[ViewQuantize] = {"Quantize all", "", false, false, empty};
        m.rows[ViewLegato] = {"Legato all", "", false, false, empty};
        m.rows[ViewSelectAll] = {"Select all", "", false, false, empty};
        m.rows[ViewPaste] = {"Paste notes", ui.note_board_count ? juce::String(int(ui.note_board_count)) + " notes" : juce::String(), false, false, !ui.note_board_count}; // LAB
        m.count = view_rows; m.current = ui.sheet_row % view_rows;
        menu_sheet(p, m);
    } else if (sel_sheet) {
        p.dim_zone(zone::picture_y, zone::picture_h, 0.7f);
        MenuSheet m;
        m.title = "SELECTION"; m.detail = juce::String(int(ui.roll_sel_count)) + (ui.roll_sel_count == 1 ? " NOTE" : " NOTES"); m.accent = c;
        m.rows[SelEarlier] = {"A step earlier", "", false, false, false};
        m.rows[SelLater] = {"A step later", "", false, false, false};
        m.rows[SelUp] = {"A semitone up", "", false, false, false};
        m.rows[SelDown] = {"A semitone down", "", false, false, false};
        m.rows[SelOctaveUp] = {"An octave up", "", false, false, false};
        m.rows[SelOctaveDown] = {"An octave down", "", false, false, false};
        m.rows[SelDuplicate] = {"Duplicate after", "", false, false, false};
        m.rows[SelQuantize] = {"Quantize", "", false, false, false};
        m.rows[SelCopy] = {"Copy notes", "", false, false, false}; // LAB: to paste on any track
        m.rows[SelDelete] = {"Delete notes", "", false, true, false};
        m.rows[SelClear] = {"Clear selection", "", false, false, false};
        m.count = sel_rows; m.current = ui.sheet_row % sel_rows;
        menu_sheet(p, m);
    }
}

void pad(Context& c, unsigned pad, uint8_t velocity, bool down) {
    if (down && c.ui.erase) { // Erase + pad: every note of its key, once B3 says so (as on Step Edit)
        c.ui.sheet_open = false;
        int key = pad_key(c.project, c.track(), pad);
        if (c.track().used() && key >= 0) ask(c, UiState::Ask::EraseKeyEvents, key);
        return;
    }
    c.play_pad(pad, velocity, down);
    if (!down && c.ui.held_note == int(pad)) c.ui.held_note = -1; // its cards back from the locks
    if (!down || !c.track().used()) return;
    if (c.ui.sheet_open) c.ui.sheet_open = false; // a pad leaves the sheet and writes as ever
    if (c.ui.recording && c.host.playing()) return; // played in live by Seq Record
    int v = c.track().layout == PadLayout::Levels ? level_velocity(pad) : velocity;
    roll_write(c, pad_key(c.project, c.track(), pad), uint8_t(v));
    if (roll_focus(c.pattern(), c.ui) >= 0) c.ui.held_note = int8_t(pad); // held on its note: the pots lock it (roadmap P1)
}

void wheel(Context& c, int delta) {
    if (c.ui.sheet_open && c.ui.roll_sheet_del) { if (c.ui.sheet_more) c.ui.sheet_row = uint8_t(std::clamp<int>(c.ui.sheet_row + delta, 0, int(del_rows) - 1)); return; } // LAB
    if (c.ui.sheet_open && c.ui.roll_sheet_sel) { c.ui.sheet_row = uint8_t(std::clamp<int>(c.ui.sheet_row + delta, 0, int(sel_rows) - 1)); return; }
    if (c.ui.sheet_open && c.ui.roll_sheet_view) { c.ui.sheet_row = uint8_t(std::clamp<int>(c.ui.sheet_row + delta, 0, int(view_rows) - 1)); return; }
    if (c.ui.sheet_open) {
        if (!c.ui.sheet_more) return; // the likely tile: a turn opens nothing (B2 MORE lists the rest)
        c.ui.sheet_row = uint8_t(std::clamp<int>(c.ui.sheet_row + delta, 0, int(sheet_rows) - 1));
        return;
    }
    if (c.ui.shift) { // note to note, over empty bars
        c.ui.shift_used = true;
        if (!roll_jump(c.pattern(), c.ui, delta)) c.notice(delta > 0 ? "No later note" : "No earlier note");
        return;
    }
    roll_step(c.pattern(), c.ui, delta, kit_lanes(c.track())); // LAB: a kit stays on the pad picked
}

void save_settings(Context& c) { if (c.store) c.store->save_settings(c.ui.settings.text()); }

// The VIEW sheet's row: the switches flip and the sheet stays; the edits close it.
void do_view_row(Context& c) {
    auto& settings = c.ui.settings;
    switch (c.ui.sheet_row % view_rows) {
    case ViewFollow: settings.roll_follow = !settings.roll_follow; save_settings(c); c.notice(settings.roll_follow ? "Follow playhead on" : "Follow playhead off"); break;
    case ViewScale:
        if (c.track().engine == EngineKind::Sampler || drum_pads(c.track())) { c.notice("Keys only"); break; }
        settings.roll_scale = !settings.roll_scale; save_settings(c); c.notice(settings.roll_scale ? "Scale lanes on" : "Scale lanes off"); break;
    case ViewQuantize: c.ui.sheet_open = false; roll_quantize_every(c); break;
    case ViewLegato: c.ui.sheet_open = false; roll_legato_every(c); break;
    case ViewSelectAll: c.ui.sheet_open = false; roll_pick_every(c); break;
    case ViewPaste: c.ui.sheet_open = false; roll_paste(c); break; // LAB
    default: break;
    }
}

// The SELECTION sheet's row: moves keep the sheet (do it again), the rest close it.
void do_sel_row(Context& c) {
    switch (c.ui.sheet_row % sel_rows) {
    case SelEarlier: roll_picks_move(c, -1, 0); break;
    case SelLater: roll_picks_move(c, 1, 0); break;
    case SelUp: roll_picks_move(c, 0, 1); break;
    case SelDown: roll_picks_move(c, 0, -1); break;
    case SelOctaveUp: roll_picks_move(c, 0, 12); break;
    case SelOctaveDown: roll_picks_move(c, 0, -12); break;
    case SelDuplicate: roll_picks_duplicate(c); break;
    case SelQuantize: roll_picks_quantize(c); break;
    case SelCopy: c.ui.sheet_open = false; roll_copy(c); break; // LAB
    case SelDelete: c.ui.sheet_open = false; roll_picks_delete(c); break;
    case SelClear: c.ui.sheet_open = false; roll_picks_clear(c); break;
    default: break;
    }
    if (!c.ui.roll_sel_count) c.ui.sheet_open = false;
}

// LAB: the DELETE sheet: its likely job (B3 before MORE) the note in focus, else every note of the lane.
void do_del_row(Context& c, bool likely) {
    const auto& pattern = c.pattern();
    int focus = roll_focus(pattern, c.ui);
    int key = focus >= 0 ? int(pattern.events[size_t(focus)].key) : int(c.ui.roll_key);
    unsigned row = likely ? (focus >= 0 ? DelNote : DelLane) : c.ui.sheet_row % del_rows;
    c.ui.sheet_open = false; c.ui.roll_sheet_del = false; c.ui.roll_del_armed = 0;
    switch (row) {
    case DelNote: if (focus >= 0) roll_erase(c); else c.notice("No note here"); break;
    case DelLane: erase_key_events(c, key); break;
    case DelPicks: if (c.ui.roll_sel_count) roll_picks_delete(c); else c.notice("Nothing selected"); break;
    default: break;
    }
}

void do_row(Context& c) {
    if (c.ui.roll_sheet_del) { do_del_row(c, !c.ui.sheet_more); return; } // LAB
    if (c.ui.roll_sheet_sel) { do_sel_row(c); return; }
    if (c.ui.roll_sheet_view) { do_view_row(c); return; }
    if (!c.ui.sheet_more) { roll_flag(c, step_accent); c.ui.sheet_open = false; return; } // the likely job: Accent, and done
    switch (c.ui.sheet_row % sheet_rows) {
    case RowAccent: roll_flag(c, step_accent); break;
    case RowSlide: roll_flag(c, step_slide); break;
    case RowTie: roll_flag(c, step_tie); break;
    case RowQuantize: c.ui.sheet_open = false; roll_quantize_note(c); break;
    case RowLegato: c.ui.sheet_open = false; roll_legato_note(c); break;
    case RowDuplicate: c.ui.sheet_open = false; roll_duplicate_note(c); break;
    case RowDelete: c.ui.sheet_open = false; roll_erase(c); break;
    default: break;
    }
}

void push(Context& c) {
    if (c.ui.sheet_open) { do_row(c); return; }
    c.ui.sheet_row = 0;
    c.ui.roll_sheet_sel = false; c.ui.roll_sheet_del = false;
    if (roll_picks_tidy(c.pattern(), c.ui)) { c.ui.sheet_open = true; c.ui.roll_sheet_sel = true; c.ui.sheet_more = true; return; } // SELECTION
    if (roll_focus(c.pattern(), c.ui) < 0) { c.ui.sheet_open = true; c.ui.roll_sheet_view = true; c.ui.sheet_more = true; return; } // VIEW
    c.ui.sheet_open = true; c.ui.sheet_more = false; c.ui.roll_sheet_view = false;
}

void soft(Context& c, int key) {
    if (c.ui.sheet_open && c.ui.roll_sheet_del) { // LAB, DELETE: BACK, MORE, DELETE; on the list BACK, -, DO IT
        if (key == 0) { if (c.ui.sheet_more) c.ui.sheet_more = false; else { c.ui.sheet_open = false; c.ui.roll_sheet_del = false; } }
        if (key == 1 && !c.ui.sheet_more) { c.ui.sheet_more = true; c.ui.sheet_row = roll_focus(c.pattern(), c.ui) >= 0 ? DelNote : DelLane; }
        if (key == 2) do_row(c);
        return;
    }
    if (!c.ui.sheet_open && key == 2 && c.ui.shift && c.track().used()) { // LAB: Shift + B3, the DELETE sheet
        c.ui.shift_used = true;
        c.ui.sheet_open = true; c.ui.roll_sheet_del = true; c.ui.sheet_more = false; c.ui.sheet_row = 0;
        c.ui.roll_sheet_sel = false; c.ui.roll_sheet_view = false; c.ui.roll_del_armed = 0;
        return;
    }
    if (!c.ui.sheet_open && key == 2 && !c.ui.shift) { // LAB: B3 on a note: DELETE, the second tap does it
        const auto& pattern = c.pattern();
        int focus = roll_focus(pattern, c.ui);
        if (focus >= 0) {
            const Event* note = &pattern.events[size_t(focus)];
            if (roll_armed(c.ui, c.now, note)) { c.ui.roll_del_armed = 0; roll_erase(c); return; }
            c.ui.roll_del_armed = std::max<uint32_t>(c.now, 1); c.ui.roll_del_tick = note->tick; c.ui.roll_del_key = note->key;
            c.notice("B3 again: delete note");
            return;
        }
    }
    if (c.ui.sheet_open && (c.ui.roll_sheet_view || c.ui.roll_sheet_sel)) { // VIEW, SELECTION: BACK, -, DO IT
        if (key == 0) c.ui.sheet_open = false;
        if (key == 2) do_row(c);
        return;
    }
    if (c.ui.sheet_open) { // BACK, MORE, ACCENT; on the list BACK, -, DO IT
        if (key == 0) { if (c.ui.sheet_more) c.ui.sheet_more = false; else c.ui.sheet_open = false; }
        if (key == 1 && !c.ui.sheet_more) { c.ui.sheet_more = true; c.ui.sheet_row = 0; }
        if (key == 2) do_row(c);
        return;
    }
    if (key == 1) { c.ui.trig_cards = !c.ui.trig_cards; return; } // ROLL again: the note's TRIG cards, or back
    static const PageId pages[] {PageId::Steps, PageId::Roll, PageId::Motion};
    c.open(pages[key % 3]);
}

void pitch(Context& c, int way) {
    if (c.ui.sheet_open) { wheel(c, way); return; }
    roll_pitch(c.pattern(), c.ui, way);
}
} // namespace

const PageSpec roll_page {draw, pad, wheel, push, soft, pitch};

} // namespace melodee
