// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 Kerem Kilic (Ellic Studio)
// Roll's cursor and view (shell/roll.h): pure, on the pattern and the UI state.
#include "melodee/shell/roll.h"
#include "melodee/model/scales.h"
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <vector>

namespace melodee {
namespace {
void put_cursor(UiState& ui, unsigned step) {
    ui.step_cursor = int16_t(std::min<unsigned>(step, 0x7fff));
    ui.steps_bar = uint8_t(std::min<unsigned>(step / bar_steps, 255));
}

// The key of the note starting in `step` nearest to `key`; `key` when none starts there.
int nearest_key(const Pattern& pattern, unsigned step, int key) {
    auto in = step_events(pattern, step);
    int best = key, distance = 1 << 20;
    for (unsigned n = 0; n < in.count; ++n) {
        int k = pattern.events[in.first + n].key, d = std::abs(k - key);
        if (d == 0) return key;
        if (d < distance) { distance = d; best = k; }
    }
    return best;
}
} // namespace

unsigned roll_cursor(const Pattern& pattern, const UiState& ui) {
    return unsigned(std::max<int>(ui.step_cursor, 0)) % pattern.steps();
}

unsigned roll_first(const Pattern& pattern, const UiState& ui) {
    unsigned steps = pattern.steps(), last = steps > roll_columns ? steps - roll_columns : 0;
    return std::min<unsigned>(unsigned(std::max<int>(ui.roll_view, 0)), last);
}

int roll_focus(const Pattern& pattern, const UiState& ui) {
    auto in = step_events(pattern, roll_cursor(pattern, ui));
    for (unsigned n = 0; n < in.count; ++n)
        if (pattern.events[in.first + n].key == ui.roll_key) return int(in.first + n);
    return -1;
}

void roll_follow(const Pattern& pattern, UiState& ui) {
    int cursor = int(roll_cursor(pattern, ui)), first = int(roll_first(pattern, ui));
    int columns = int(roll_columns), margin = int(roll_margin);
    if (cursor < first + margin) first = cursor - margin;
    if (cursor > first + columns - 1 - margin) first = cursor - columns + 1 + margin;
    int last = std::max(0, int(pattern.steps()) - columns);
    ui.roll_view = int16_t(std::clamp(first, 0, last));
    int top = ui.roll_top, key = ui.roll_key, rows = int(roll_rows);
    if (key > top - 1) top = key + 1;              // a row above the cursor's key
    if (key < top - (rows - 2)) top = key + rows - 2; // and one below
    ui.roll_top = uint8_t(std::clamp(top, rows - 1, std::max(127, key + 1))); // a kit's keys run past 127
}

void roll_select(const Pattern& pattern, UiState& ui) {
    int at = roll_focus(pattern, ui);
    auto in = step_events(pattern, roll_cursor(pattern, ui));
    ui.step_event = at >= 0 ? uint8_t(std::min<size_t>(size_t(at) - in.first, 255)) : 0;
}

void roll_open(const Pattern& pattern, UiState& ui) {
    int top = ui.roll_top, low = top - int(roll_rows) + 1, best = -1;
    int64_t at = int64_t(roll_cursor(pattern, ui)) * pattern.q(), distance = INT64_MAX;
    for (const auto& e : pattern.events) {
        if (e.tick >= pattern.length) break;
        if (int(e.key) >= low && int(e.key) <= top) return; // a note in view: the view stays as it was left
        int64_t d = std::llabs(int64_t(e.tick) - at);
        if (d < distance) { distance = d; best = e.key; }
    }
    if (best < 0) return;
    ui.roll_key = uint8_t(best);
    roll_follow(pattern, ui);
    roll_select(pattern, ui);
}

void roll_step(const Pattern& pattern, UiState& ui, int way) {
    int step = std::clamp<int>(int(roll_cursor(pattern, ui)) + way, 0, int(pattern.steps()) - 1);
    put_cursor(ui, unsigned(step));
    ui.roll_key = uint8_t(nearest_key(pattern, unsigned(step), ui.roll_key));
    roll_follow(pattern, ui);
    roll_select(pattern, ui);
}

bool roll_jump(const Pattern& pattern, UiState& ui, int way) {
    // Every note in turn, by step and then pitch: a chord's notes up one by one, then on to the next step's.
    unsigned q = pattern.q();
    auto place = [&](unsigned step, int key) { return int64_t(step) * 256 + key; };
    int64_t here = place(roll_cursor(pattern, ui), ui.roll_key), to = -1;
    for (const auto& e : pattern.events) {
        if (e.tick >= pattern.length) break;
        int64_t at = place(e.tick / q, e.key);
        if (way > 0 && at > here && (to < 0 || at < to)) to = at;
        if (way < 0 && at < here && (to < 0 || at > to)) to = at;
    }
    if (to < 0) return false;
    put_cursor(ui, unsigned(to / 256));
    ui.roll_key = uint8_t(to % 256);
    roll_follow(pattern, ui);
    roll_select(pattern, ui);
    return true;
}

void roll_pitch(const Pattern& pattern, UiState& ui, int way) {
    auto in = step_events(pattern, roll_cursor(pattern, ui));
    int key = ui.roll_key, next = -1;
    for (unsigned n = 0; n < in.count; ++n) { // the chord first
        int k = pattern.events[in.first + n].key;
        if (way > 0 && k > key && (next < 0 || k < next)) next = k;
        if (way < 0 && k < key && (next < 0 || k > next)) next = k;
    }
    ui.roll_key = uint8_t(next >= 0 ? next : std::clamp(key + (way > 0 ? 1 : -1), 0, 127));
    roll_follow(pattern, ui);
    roll_select(pattern, ui);
}

unsigned roll_play_first(const Pattern& pattern, uint64_t position) {
    unsigned steps = pattern.steps(), q = std::max(1u, pattern.q());
    if (!pattern.length) return 0;
    unsigned step = unsigned((position % pattern.length) / q);
    unsigned first = step / roll_columns * roll_columns, last = steps > roll_columns ? steps - roll_columns : 0;
    return std::min(first, last);
}

bool roll_in_scale(int key, unsigned root, unsigned scale) {
    if (key < 0) return false;
    unsigned mask = scales()[scale % scales().size()].mask;
    return (mask >> ((unsigned(key) + 12 - root % 12) % 12)) & 1;
}

namespace {
// The nearest step's start to `tick`, never past the last step's.
uint32_t on_grid(const Pattern& pattern, uint32_t tick) {
    uint32_t q = std::max(1u, pattern.q());
    uint32_t to = (tick + q / 2) / q * q;
    if (pattern.length && to >= pattern.length) to = (pattern.length - 1) / q * q;
    return to;
}
} // namespace

bool roll_quantize(Pattern& pattern, size_t index) {
    if (index >= pattern.events.size()) return false;
    Event e = pattern.events[index];
    if (e.tick >= pattern.length) return false;
    uint32_t to = on_grid(pattern, e.tick);
    if (to == e.tick) return false;
    pattern.events.erase(pattern.events.begin() + long(index));
    e.length = uint16_t(std::clamp<uint32_t>(std::min<uint32_t>(e.length, pattern.length - to), 1, 0xffff));
    e.tick = to;
    pattern.add(e); // one there already on that key is replaced: the two were the same note
    return true;
}

unsigned roll_quantize_all(Pattern& pattern) {
    std::vector<Event> kept, moved;
    unsigned count = 0;
    for (const auto& e : pattern.events) {
        if (e.tick >= pattern.length) { kept.push_back(e); continue; }
        Event m = e;
        uint32_t to = on_grid(pattern, e.tick);
        if (to != e.tick) {
            ++count;
            m.tick = to;
            m.length = uint16_t(std::clamp<uint32_t>(std::min<uint32_t>(m.length, pattern.length - to), 1, 0xffff));
        }
        moved.push_back(m);
    }
    if (!count) return 0;
    pattern.events = std::move(kept);
    pattern.sort();
    for (const auto& e : moved) pattern.add(e);
    return count;
}

namespace {
uint16_t legato_length(const Pattern& pattern, size_t index) {
    const auto& e = pattern.events[index];
    uint32_t next = pattern.length;
    for (size_t n = index + 1; n < pattern.events.size(); ++n) {
        const auto& o = pattern.events[n];
        if (o.tick >= pattern.length) break;
        if (o.tick > e.tick) { next = o.tick; break; }
    }
    return uint16_t(std::clamp<uint32_t>(next > e.tick ? next - e.tick : 1, 1, 0xffff));
}
} // namespace

bool roll_legato(Pattern& pattern, size_t index) {
    if (index >= pattern.events.size() || pattern.events[index].tick >= pattern.length) return false;
    uint16_t length = legato_length(pattern, index);
    if (length == pattern.events[index].length) return false;
    pattern.events[index].length = length;
    return true;
}

unsigned roll_legato_all(Pattern& pattern) {
    unsigned count = 0;
    for (size_t n = 0; n < pattern.events.size(); ++n) count += roll_legato(pattern, n);
    return count;
}

int roll_duplicate(Pattern& pattern, size_t index) {
    if (index >= pattern.events.size()) return -1;
    Event e = pattern.events[index];
    uint32_t q = std::max(1u, pattern.q());
    uint32_t span = std::max<uint32_t>(q, (uint32_t(e.length) + q - 1) / q * q);
    uint64_t to = uint64_t(e.tick) + span;
    if (to >= pattern.length) return -1;
    e.tick = uint32_t(to);
    e.length = uint16_t(std::clamp<uint32_t>(std::min<uint32_t>(e.length, pattern.length - e.tick), 1, 0xffff));
    if (!pattern.add(e)) return -2;
    for (size_t n = pattern.lower(e.tick); n < pattern.events.size() && pattern.events[n].tick == e.tick; ++n)
        if (pattern.events[n].key == e.key && pattern.events[n].pitch == e.pitch) return int(n);
    return -1;
}

std::string roll_position(uint32_t tick, unsigned bar_ticks, unsigned beat_ticks, unsigned q) {
    bar_ticks = std::max(1u, bar_ticks); beat_ticks = std::max(1u, beat_ticks); q = std::max(1u, q);
    unsigned bar = tick / bar_ticks + 1, beat = tick % bar_ticks / beat_ticks + 1;
    std::string s = std::to_string(bar) + "." + std::to_string(beat);
    if (q < beat_ticks) s += "." + std::to_string(tick % beat_ticks / q + 1); // the step within the beat, as on the MPC
    return s;
}

} // namespace melodee
