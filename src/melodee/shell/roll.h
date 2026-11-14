// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 Kerem Kilic (Ellic Studio)
#pragma once
#include "melodee/model/pattern.h"
#include "melodee/shell/ui_state.h"
#include <cstddef>
#include <cstdint>
#include <string>

// Roll (pages/roll_page.cpp), read from the pattern's events. The cursor is
// a step of Q (UiState::step_cursor, shared with Steps and Step Edit) and a
// pitch (UiState::roll_key); the event starting in that step on that key is
// in focus. The view scrolls with the cursor a step at a time, never by the
// page. The functions here are pure (tests/melodee.cpp); the ones that write
// the pattern take the Context.
namespace melodee {

class Context;

constexpr unsigned roll_columns = 16, roll_rows = 12;
constexpr unsigned roll_margin = 2; // steps kept in view beside the cursor

// The cursor's step, inside the pattern.
unsigned roll_cursor(const Pattern& pattern, const UiState& ui);
// The first step in view, inside the pattern.
unsigned roll_first(const Pattern& pattern, const UiState& ui);
// The note in focus: its index in pattern.events, or -1 when none starts in the cursor's step on roll_key.
int roll_focus(const Pattern& pattern, const UiState& ui);
// The view onto the cursor: its step roll_margin steps from either edge, its key a row from the top or bottom.
void roll_follow(const Pattern& pattern, UiState& ui);
// Step Edit's selection (step_event) onto the note in focus, so its card, fader and erase functions act on it.
void roll_select(const Pattern& pattern, UiState& ui);
// Roll opening on a pattern whose notes all lie outside the rows in view (a bass line in octave 2 under a cursor left
// on C4): the cursor's pitch goes to the note nearest its step in time, so the page opens on the notes. Left as it
// was when one is in view, or the pattern is empty.
void roll_open(const Pattern& pattern, UiState& ui);
// The wheel: the cursor a step of Q on; where notes start, the pitch goes to the nearest of them.
void roll_step(const Pattern& pattern, UiState& ui, int way);
// Shift + wheel: to the next (or previous) note, by step and then pitch, so a chord's notes come one by one; false when none.
bool roll_jump(const Pattern& pattern, UiState& ui, int way);
// Minus / Plus: to the next note up (or down) starting in the cursor's step; past the last, a semitone.
void roll_pitch(const Pattern& pattern, UiState& ui, int way);
// INDIGO X DENIM, Follow playhead: the first step in view while playing at `position` (ticks from the pattern's
// start, any number of passes): the page of roll_columns steps the playhead is in, so the view turns a page at a time.
unsigned roll_play_first(const Pattern& pattern, uint64_t position, unsigned span = 1); // span: bars in view (the zoom)
// INDIGO X DENIM, Scale lanes: whether `key` is in `scale` (scales()'s index) from `root` (0 C .. 11 B).
bool roll_in_scale(int key, unsigned root, unsigned scale);
// INDIGO X DENIM, the note sheet's and the VIEW sheet's edits, pure on the pattern (tests/melodee.cpp).
// Quantize: note `index` onto its nearest step of Q (the last step's start at most); false when it was on it.
bool roll_quantize(Pattern& pattern, size_t index);
// Every note before the pattern's end onto its nearest step; how many moved.
unsigned roll_quantize_all(Pattern& pattern);
// Legato: note `index` as long as it takes to reach the next note starting after it (any key), or the pattern's end;
// false when it already was.
bool roll_legato(Pattern& pattern, size_t index);
// Every note so; how many changed.
unsigned roll_legato_all(Pattern& pattern);
// Duplicate: a copy of note `index` straight after it, a whole number of steps of Q on (at least one); the copy's index,
// -1 when it would start past the pattern's end, -2 when the pattern is full.
int roll_duplicate(Pattern& pattern, size_t index);

// Where a tick sits as the MPC counts it: bar.beat.step of Q within the beat ("2.2.3"), 1-based.
std::string roll_position(uint32_t tick, unsigned bar_ticks, unsigned beat_ticks, unsigned q);

// A pad: `key` at the cursor, NEW NOTE long, or the note there focused if it is already written.
void roll_write(Context& c, int key, uint8_t velocity);
// Erase tapped alone, Delete on the sheet: the note in focus goes (one undo); false when there is none.
bool roll_erase(Context& c);
// The sheet: accent, slide or tie on the note in focus, on or off.
void roll_flag(Context& c, uint8_t flag);
// INDIGO X DENIM, the selection (UiState::roll_sel), pure on the pattern and the UI state.
// Whether the note on `tick` with `key` is picked.
bool roll_picked(const UiState& ui, uint32_t tick, uint8_t key);
// The note in focus picked, or let go; false when no note is in focus or the selection is full.
bool roll_pick(const Pattern& pattern, UiState& ui);
// Every note before the pattern's end picked (the first roll_picks); how many.
unsigned roll_pick_all(const Pattern& pattern, UiState& ui);
// The picks no note answers any more let go; how many are left.
unsigned roll_picks_tidy(const Pattern& pattern, UiState& ui);
// The picked notes moved `ticks` later (earlier when negative) and `keys` up; nothing moves when one would leave the
// pattern or the keys (false). The picks go with them.
bool roll_move_picks(Pattern& pattern, UiState& ui, int ticks, int keys);
// A copy of the picked notes straight after them (their span, a whole number of steps of Q); the picks go to the
// copies. -1 when a copy would start past the end, -2 when the pattern is full.
int roll_duplicate_picks(Pattern& pattern, UiState& ui);
// The picked notes onto their nearest steps; how many moved. The picks go with them.
unsigned roll_quantize_picks(Pattern& pattern, UiState& ui);
// The picked notes deleted; how many. The selection is empty after.
unsigned roll_delete_picks(Pattern& pattern, UiState& ui);

// INDIGO X DENIM: the note in focus quantized, made legato or duplicated (the cursor goes with it), and the VIEW
// sheet's Quantize all and Legato all; each says what it did. A press on the page is one undo (PageEdit).
void roll_quantize_note(Context& c);
void roll_legato_note(Context& c);
void roll_duplicate_note(Context& c);
void roll_quantize_every(Context& c);
void roll_legato_every(Context& c);
// INDIGO X DENIM: the selection's jobs (Shift + Push picks, the SELECTION sheet does), each one undo, each says what it did.
void roll_pick_note(Context& c);
void roll_pick_every(Context& c);
void roll_picks_move(Context& c, int steps, int keys);
void roll_picks_duplicate(Context& c);
void roll_picks_quantize(Context& c);
void roll_picks_delete(Context& c);
void roll_copy(Context& c);  // LAB: the picked notes kept
void roll_paste(Context& c); // LAB: kept notes at the cursor, any track
void roll_picks_clear(Context& c);

} // namespace melodee
