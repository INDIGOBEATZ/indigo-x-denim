// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 Kerem Kilic (Ellic Studio)
// What Roll writes (shell/roll.h). A pad's note is one undo from press to
// release (Context::open_hold); erasing keeps its own (Step Edit's
// erase_event); the sheet's flags are a press on the page (PageEdit).
#include "melodee/shell/roll.h"
#include "melodee/shell/context.h"
#include "melodee/shell/step_edit.h"
#include <algorithm>
#include <string>

namespace melodee {

void roll_write(Context& c, int key, uint8_t velocity) {
    auto& ui = c.ui;
    auto& pattern = c.pattern();
    if (key < 0 || key > 127) return;
    ui.roll_key = uint8_t(key);
    if (roll_focus(pattern, ui) < 0) { // not there yet: written at the start of the cursor's step
        Event e;
        e.tick = roll_cursor(pattern, ui) * pattern.q();
        uint32_t length = ui.roll_new ? ui.roll_new : pattern.q();
        e.length = uint16_t(std::clamp<uint32_t>(std::min(length, pattern.length - e.tick), 1, 0xffff));
        e.key = uint8_t(key);
        e.velocity = uint8_t(std::clamp<int>(velocity, 1, 127));
        if (!pattern.add(e)) { c.notice("The pattern is full"); return; }
        c.sync_pattern(ui.track, c.track().pattern);
    }
    roll_follow(pattern, ui);
    roll_select(pattern, ui);
}

bool roll_erase(Context& c) {
    roll_select(c.pattern(), c.ui);
    if (roll_focus(c.pattern(), c.ui) < 0) return false;
    erase_event(c); // one undo, or part of a take; it says what went
    roll_select(c.pattern(), c.ui);
    return true;
}

void roll_flag(Context& c, uint8_t flag) {
    auto& pattern = c.pattern();
    int at = roll_focus(pattern, c.ui);
    if (at < 0) return;
    pattern.events[size_t(at)].flags ^= flag;
    c.sync_pattern(c.ui.track, c.track().pattern);
}

} // namespace melodee

// INDIGO X DENIM: the note sheet's Quantize, Legato and Duplicate, and the VIEW sheet's Quantize all and Legato all.
namespace melodee {
namespace {
// The cursor onto the note at `index` (a quantized note or a copy moves it), and the track's pattern sent on.
void land(Context& c, int index) {
    auto& pattern = c.pattern();
    if (index >= 0 && size_t(index) < pattern.events.size()) {
        const auto& e = pattern.events[size_t(index)];
        c.ui.step_cursor = int16_t(std::min<unsigned>(e.tick / std::max(1u, pattern.q()), 0x7fff));
        c.ui.steps_bar = uint8_t(std::min<unsigned>(unsigned(c.ui.step_cursor) / bar_steps, 255));
        c.ui.roll_key = e.key;
    }
    roll_follow(pattern, c.ui);
    roll_select(pattern, c.ui);
    c.sync_pattern(c.ui.track, c.track().pattern);
}
// The index of the note on `tick` with `key`, -1 when none.
int find(const Pattern& pattern, uint32_t tick, uint8_t key) {
    for (size_t n = pattern.lower(tick); n < pattern.events.size() && pattern.events[n].tick == tick; ++n)
        if (pattern.events[n].key == key) return int(n);
    return -1;
}
} // namespace

void roll_quantize_note(Context& c) {
    auto& pattern = c.pattern();
    int at = roll_focus(pattern, c.ui);
    if (at < 0) return;
    Event before = pattern.events[size_t(at)];
    if (!roll_quantize(pattern, size_t(at))) { c.notice("On the grid already"); return; }
    uint32_t q = std::max(1u, pattern.q()), to = (before.tick + q / 2) / q * q;
    if (pattern.length && to >= pattern.length) to = (pattern.length - 1) / q * q;
    land(c, find(pattern, to, before.key));
    c.notice("Note quantized");
}

void roll_legato_note(Context& c) {
    auto& pattern = c.pattern();
    int at = roll_focus(pattern, c.ui);
    if (at < 0) return;
    if (!roll_legato(pattern, size_t(at))) { c.notice("Legato already"); return; }
    land(c, at);
    c.notice("Note legato");
}

void roll_duplicate_note(Context& c) {
    auto& pattern = c.pattern();
    int at = roll_focus(pattern, c.ui);
    if (at < 0) return;
    int copy = roll_duplicate(pattern, size_t(at));
    if (copy == -2) { c.notice("The pattern is full"); return; }
    if (copy < 0) { c.notice("No room after it"); return; }
    land(c, copy);
    c.notice("Note duplicated");
}

void roll_quantize_every(Context& c) {
    unsigned moved = roll_quantize_all(c.pattern());
    if (!moved) { c.notice("All on the grid"); return; }
    land(c, -1);
    c.notice("Quantized " + std::to_string(moved) + (moved == 1 ? " note" : " notes"));
}

void roll_legato_every(Context& c) {
    unsigned changed = roll_legato_all(c.pattern());
    if (!changed) { c.notice("Legato already"); return; }
    land(c, -1);
    c.notice("Legato: " + std::to_string(changed) + (changed == 1 ? " note" : " notes"));
}

void roll_pick_note(Context& c) {
    if (roll_focus(c.pattern(), c.ui) < 0) { c.notice("No note here"); return; }
    unsigned before = c.ui.roll_sel_count;
    if (!roll_pick(c.pattern(), c.ui)) { c.notice("64 notes at most"); return; }
    unsigned now = c.ui.roll_sel_count;
    c.notice(now > before ? "Picked: " + std::to_string(now) : now ? "Let go: " + std::to_string(now) + " left" : std::string("Selection empty"));
}

void roll_pick_every(Context& c) {
    unsigned n = roll_pick_all(c.pattern(), c.ui);
    c.notice(n ? "Picked all: " + std::to_string(n) : std::string("No notes"));
}

void roll_picks_move(Context& c, int steps, int keys) {
    auto& pattern = c.pattern();
    if (!roll_move_picks(pattern, c.ui, steps * int(std::max(1u, pattern.q())), keys)) { c.notice(keys ? "No room up there" : "No room there"); return; }
    if (c.ui.roll_sel_count) { // the cursor with the first pick, so the view follows
        const auto& first = c.ui.roll_sel[0];
        c.ui.step_cursor = int16_t(std::min<unsigned>(first.tick / std::max(1u, pattern.q()), 0x7fff));
        c.ui.roll_key = first.key;
    }
    roll_follow(pattern, c.ui);
    roll_select(pattern, c.ui);
    c.sync_pattern(c.ui.track, c.track().pattern);
}

void roll_picks_duplicate(Context& c) {
    int n = roll_duplicate_picks(c.pattern(), c.ui);
    if (n == -2) { c.notice("The pattern is full"); return; }
    if (n < 0) { c.notice("No room after them"); return; }
    roll_picks_move(c, 0, 0); // the cursor and view onto the copies
    c.notice("Duplicated " + std::to_string(n));
}

void roll_picks_quantize(Context& c) {
    unsigned n = roll_quantize_picks(c.pattern(), c.ui);
    c.sync_pattern(c.ui.track, c.track().pattern);
    c.notice(n ? "Quantized " + std::to_string(n) : std::string("On the grid already"));
}

void roll_picks_delete(Context& c) {
    unsigned n = roll_delete_picks(c.pattern(), c.ui);
    roll_select(c.pattern(), c.ui);
    c.sync_pattern(c.ui.track, c.track().pattern);
    c.notice("Deleted " + std::to_string(n) + (n == 1 ? " note" : " notes"));
}

void roll_picks_clear(Context& c) { c.ui.roll_sel_count = 0; c.notice("Selection empty"); }

} // namespace melodee
