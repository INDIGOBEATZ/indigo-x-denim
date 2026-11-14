// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 Kerem Kilic (Ellic Studio)
// INDIGO X DENIM LAB: ARRANGE's edits (MPC 3.10's Arrange mode). Each track's pattern playing is its lane of one
// timeline; an edit works on a range of ticks over some lanes: the selection drawn (Shift + wheel) or else the loop
// brace, on the marquee's lanes (MRQ), the selected track's (TRK) or every lane (ARR). Each edit is one undo over
// every pattern it changed.
#include "melodee/shell/context.h"
#include <algorithm>

namespace melodee {
namespace {
// The pattern changed, its events in order and none past the longest a pattern may be.
void tidy(Pattern& p, uint32_t most) {
    p.sort();
    p.sort_motion();
    if (p.length > most) p.length = most;
    p.cut(p.length);
}
uint32_t bar_up(uint32_t ticks, uint32_t bar) { return (ticks + bar - 1) / bar * bar; }
} // namespace

bool Context::arrange_range(uint32_t& from, uint32_t& to, unsigned& t0, unsigned& t1) const {
    if (ui.arr_selected && ui.arr_to > ui.arr_from) { from = ui.arr_from; to = ui.arr_to; }
    else if (ui.brace_to > ui.brace_from) { from = ui.brace_from; to = ui.brace_to; }
    else return false;
    switch (ui.arr_mode % 3) {
    case 0: t0 = std::min(ui.arr_t0, ui.arr_t1); t1 = std::max(ui.arr_t0, ui.arr_t1); break;
    case 1: t0 = t1 = ui.track % track_count; break;
    default: t0 = 0; t1 = track_count - 1; break;
    }
    return true;
}

// `change` on each lane's pattern from t0 to t1 (a copy of it), kept as one undo when any changed.
template <class Change> unsigned Context::arrange_each(const char* what, unsigned t0, unsigned t1, Change&& change) {
    Edit edit;
    edit.what = what;
    edit.track = ui.track;
    const uint32_t most = project.max_length();
    for (unsigned t = t0; t <= t1 && t < track_count; ++t) {
        if (!project.tracks[t].used()) continue;
        unsigned pi = project.tracks[t].pattern % pattern_count;
        Pattern p = patterns.at[t][pi];
        change(t, t - t0, p);
        tidy(p, most);
        if (p == patterns.at[t][pi]) continue;
        edit.patterns_before.push_back({uint8_t(t), uint8_t(pi), patterns.at[t][pi]});
        edit.patterns_after.push_back({uint8_t(t), uint8_t(pi), p});
    }
    unsigned changed = unsigned(edit.patterns_after.size());
    for (const auto& p : edit.patterns_after) restore_pattern(p.track, p.pattern, p.value);
    if (changed) { edits.push(std::move(edit)); dirty = true; }
    return changed;
}

void Context::arrange_copy(bool cut) {
    uint32_t from, to;
    unsigned t0, t1;
    if (!arrange_range(from, to, t0, t1)) { notice("Draw a selection first"); return; }
    arrange_board.length = to - from;
    arrange_board.tracks.assign(t1 - t0 + 1, {});
    arrange_board.motion.assign(t1 - t0 + 1, {});
    for (unsigned t = t0; t <= t1; ++t) {
        const auto& p = patterns.at[t][project.tracks[t].pattern % pattern_count];
        for (const auto& e : p.events) if (e.tick >= from && e.tick < to) { Event x = e; x.tick -= from; arrange_board.tracks[t - t0].push_back(x); }
        for (const auto& m : p.motion) if (m.tick >= from && m.tick < to) { MotionPoint x = m; x.tick -= from; arrange_board.motion[t - t0].push_back(x); }
    }
    if (cut) {
        arrange_each("Cut", t0, t1, [&](unsigned, unsigned, Pattern& p) {
            p.events.erase(std::remove_if(p.events.begin(), p.events.end(), [&](const Event& e) { return e.tick >= from && e.tick < to; }), p.events.end());
            p.motion.erase(std::remove_if(p.motion.begin(), p.motion.end(), [&](const MotionPoint& m) { return m.tick >= from && m.tick < to; }), p.motion.end());
        });
    }
    notice(cut ? "Cut" : "Copied");
}

// Paste at the cursor onto the selected track and those under it: merged with what is there, or inserted (what
// was from the cursor on moves later by the clipboard's length). The pattern grows to hold it, in whole bars.
void Context::arrange_paste(bool insert) {
    if (arrange_board.tracks.empty()) { notice("Copy something first"); return; }
    const uint32_t at = ui.arr_cursor, len = arrange_board.length, bar = project.bar_ticks();
    const unsigned t0 = ui.track % track_count, t1 = std::min<unsigned>(track_count - 1, t0 + unsigned(arrange_board.tracks.size()) - 1);
    unsigned changed = arrange_each(insert ? "Paste insert" : "Paste", t0, t1, [&](unsigned, unsigned i, Pattern& p) {
        if (insert) {
            for (auto& e : p.events) if (e.tick >= at) e.tick += len;
            for (auto& m : p.motion) if (m.tick >= at) m.tick += len;
            if (p.length > at) p.length += len;
        }
        for (auto e : arrange_board.tracks[i]) { e.tick += at; p.events.push_back(e); }
        for (auto m : arrange_board.motion[i]) { m.tick += at; p.motion.push_back(m); }
        p.length = std::max(p.length, bar_up(at + len, bar));
    });
    // What was pasted, selected and in view (as MPC 3.10 scrolls to it).
    ui.arr_from = at; ui.arr_to = at + len; ui.arr_selected = true; ui.arr_t0 = uint8_t(t0); ui.arr_t1 = uint8_t(t1);
    arrange_show(at + len);
    notice(changed ? (insert ? "Inserted" : "Pasted") : "Nothing pasted");
}

// Duplicate: the range copied straight after itself (what was there replaced), the selection moving onto the copy.
void Context::arrange_duplicate() {
    uint32_t from, to;
    unsigned t0, t1;
    if (!arrange_range(from, to, t0, t1)) { notice("Draw a selection first"); return; }
    const uint32_t len = to - from, bar = project.bar_ticks();
    arrange_each("Duplicate", t0, t1, [&](unsigned, unsigned, Pattern& p) {
        p.events.erase(std::remove_if(p.events.begin(), p.events.end(), [&](const Event& e) { return e.tick >= to && e.tick < to + len; }), p.events.end());
        p.motion.erase(std::remove_if(p.motion.begin(), p.motion.end(), [&](const MotionPoint& m) { return m.tick >= to && m.tick < to + len; }), p.motion.end());
        std::vector<Event> copies;
        for (const auto& e : p.events) if (e.tick >= from && e.tick < to) { Event x = e; x.tick += len; copies.push_back(x); }
        std::vector<MotionPoint> moves;
        for (const auto& m : p.motion) if (m.tick >= from && m.tick < to) { MotionPoint x = m; x.tick += len; moves.push_back(x); }
        p.events.insert(p.events.end(), copies.begin(), copies.end());
        p.motion.insert(p.motion.end(), moves.begin(), moves.end());
        p.length = std::max(p.length, bar_up(to + len, bar));
    });
    if (ui.arr_selected) { ui.arr_from = to; ui.arr_to = to + len; } else { ui.brace_from = to; ui.brace_to = to + len; }
    ui.arr_cursor = to;
    arrange_show(to + len);
    notice("Duplicated");
}

// Delete takes the time out (what follows moves up); Clear empties it and keeps the time.
void Context::arrange_delete(bool keep_time) {
    uint32_t from, to;
    unsigned t0, t1;
    if (!arrange_range(from, to, t0, t1)) { notice("Draw a selection first"); return; }
    const uint32_t len = to - from, bar = project.bar_ticks();
    unsigned changed = arrange_each(keep_time ? "Clear" : "Delete", t0, t1, [&](unsigned, unsigned, Pattern& p) {
        p.events.erase(std::remove_if(p.events.begin(), p.events.end(), [&](const Event& e) { return e.tick >= from && e.tick < to; }), p.events.end());
        p.motion.erase(std::remove_if(p.motion.begin(), p.motion.end(), [&](const MotionPoint& m) { return m.tick >= from && m.tick < to; }), p.motion.end());
        if (keep_time) return;
        for (auto& e : p.events) if (e.tick >= to) e.tick -= len;
        for (auto& m : p.motion) if (m.tick >= to) m.tick -= len;
        if (p.length > from) p.length = std::max(bar, p.length > to ? p.length - len : bar_up(from, bar));
    });
    if (!keep_time) ui.arr_selected = false;
    notice(changed ? (keep_time ? "Cleared" : "Deleted") : "Nothing there");
}

void Context::arrange_loop_to_selection() {
    if (!ui.arr_selected || ui.arr_to <= ui.arr_from) { notice("Draw a selection first"); return; }
    const uint32_t bar = project.bar_ticks();
    ui.brace_from = ui.arr_from / bar * bar; ui.brace_to = std::max(bar_up(ui.arr_to, bar), ui.brace_from + bar); // whole bars, as MPC 3.10
    ui.arr_selected = false;
    notice("Loop " + std::to_string(ui.brace_from / bar + 1) + "-" + std::to_string(ui.brace_to / bar));
}

void Context::arrange_select_all() {
    uint32_t longest = 0;
    for (unsigned t = 0; t < track_count; ++t)
        if (project.tracks[t].used()) longest = std::max(longest, patterns.at[t][project.tracks[t].pattern % pattern_count].length);
    if (!longest) { notice("Nothing to select"); return; }
    ui.arr_from = 0; ui.arr_to = longest; ui.arr_selected = true; ui.arr_t0 = 0; ui.arr_t1 = track_count - 1; ui.arr_mode = 2;
}

// The view moves so `tick` (and the cursor) are in it.
void Context::arrange_show(uint32_t tick) {
    const uint32_t span = arrange_zooms[ui.arr_zoom % (sizeof(arrange_zooms) / sizeof(arrange_zooms[0]))] * project.bar_ticks();
    const uint32_t c = ui.arr_cursor;
    if (c < ui.arr_view) ui.arr_view = c;
    if (tick > ui.arr_view + span) ui.arr_view = tick - span;
    if (c >= ui.arr_view + span) ui.arr_view = c - span + project.bar_ticks();
    const uint32_t bar = project.bar_ticks();
    ui.arr_view = ui.arr_view / bar * bar;
}

} // namespace melodee
