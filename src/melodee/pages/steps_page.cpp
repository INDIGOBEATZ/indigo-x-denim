// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 Kerem Kilic (Ellic Studio)
// STEPS (B1 on a sequence page, or Seq held + B1): the pads are the sixteen steps of the bar in view, pad 1
// bottom left. Tap a pad: the step on (with the last note played) or off.
// The step in hand is on the cards (length, velocity, chance; on a kit the
// last pad's note in it). Hold a pad: the cards lock the Motion lanes on that
// step (roadmap P1) and the wheel changes its pitch. B1 again: the
// TRIG row, K1 the ratchet's hits, K2 their velocity ramp, Shift + K2 a kit
// pad's pitch ramp. Minus / Plus change bar; Erase + pad clears.
// The picture is the bar as a drum machine draws it: sixteen columns in
// four beats, each stacking its notes by pitch, taller for harder hits, a dot
// over it for an accent, an arc under it for a tie, a ratchet as its hits
// side by side as tall as they play; the numbers under them,
// the step in hand lit; the pattern's bars as blocks at the foot.
// INDIGO X DENIM: Shift + K3 ZOOM zooms out to 2, 4 or 8 bars in view (the
// helicopter view): every note as a mark by its time and pitch, the bar in
// hand lit (the pads still play its sixteen steps), the playhead over it.
#include "melodee/model/scales.h"
#include "melodee/pages/common.h"
#include "melodee/pages/pages.h"
#include "melodee/model/generators.h"
#include "melodee/shell/step_lane.h"
#include "melodee/ui/widgets.h"
#include <algorithm>

namespace melodee {
namespace {
using namespace ui;
constexpr uint32_t tap_ms = 300;

unsigned bars_of(const Pattern& p) { return std::max(1u, (p.steps() + bar_steps - 1) / bar_steps); } // pages of sixteen steps

// INDIGO X DENIM: the helicopter view, `span` bars from the group the bar in hand is in.
void draw_overview(Paint& p, const Snapshot& s, juce::Colour c, unsigned span) {
    const auto& pattern = s.pattern;
    unsigned bars = bars_of(pattern), bar = std::min<unsigned>(s.ui.steps_bar, bars - 1);
    span = std::min(span, bars);
    unsigned first_bar = std::min(bar / span * span, bars - span);
    unsigned q = std::max(1u, pattern.q()), from_step = first_bar * bar_steps, view = span * bar_steps;
    uint32_t from = from_step * q, to = (from_step + view) * q;
    constexpr float x0 = zone::pad, top = zone::picture_y + 4, height = 104;
    const float w = zone::width - 2 * zone::pad, step_w = w / float(view);
    int low = 128, high = -1; // the pitches in view, top to bottom
    for (const auto& e : pattern.events) {
        if (e.tick >= std::min<uint32_t>(to, pattern.length)) break;
        if (e.tick < from) continue;
        low = std::min(low, int(e.key)); high = std::max(high, int(e.key));
    }
    for (unsigned b = 0; b < span; ++b) { // each bar: lit if in hand, its number under it
        float bx = x0 + b * bar_steps * step_w, bw = bar_steps * step_w;
        bool here = first_bar + b == bar;
        p.fill(bx + 0.5f, top, bw - 1, height, here ? colour::raised : colour::panel, 3);
        for (unsigned beat = 1; beat < 4; ++beat) p.fill(bx + beat * 4 * step_w, top + 2, 0.5f, height - 4, colour::line);
        auto number = juce::String(int(first_bar + b + 1));
        p.text(number, bx, top + height + 3, bw, 14, plain(here ? c : colour::mid, type::small, here ? Weight::Bold : Weight::SemiBold), juce::Justification::centred);
    }
    if (high >= 0) {
        float rows = float(std::max(high - low, 11)), mid_pad = (rows - float(high - low)) / 2;
        for (const auto& e : pattern.events) {
            if (e.tick >= std::min<uint32_t>(to, pattern.length)) break;
            if (e.tick < from) continue;
            float x = x0 + float(e.tick - from) / float(q) * step_w;
            float len = std::max(float(e.length) / float(q) * step_w - 1, std::max(step_w * 0.6f, 1.5f));
            float y = top + 6 + (1 - (float(e.key - low) + mid_pad) / rows) * (height - 16);
            p.fill(x + 0.5f, y, std::min(len, x0 + w - x - 1), 4, c.withAlpha(0.45f + e.velocity / 127.0f * 0.55f), 1.5f);
        }
    } else {
        p.text("NO NOTES", x0, top + height / 2 - 7, w, 14, caps_text(colour::dim), juce::Justification::centred);
    }
    int cursor = s.ui.step_cursor;
    if (cursor >= int(from_step) && cursor < int(from_step + view)) // the step in hand
        p.fill(x0 + float(unsigned(cursor) - from_step) * step_w, top + height - 3, std::max(step_w, 2.0f), 3, c, 1);
    if (s.playing && pattern.length) { // the playhead
        uint32_t at = uint32_t(s.position % pattern.length);
        if (at >= from && at < to) p.fill(x0 + float(at - from) / float(q) * step_w, top, 1.5f, height, colour::text);
    }
}

void draw(Paint& p, const Snapshot& s) {
    const auto& pattern = s.pattern;
    auto c = colour_of(s);
    unsigned bars = bars_of(pattern), bar = std::min<unsigned>(s.ui.steps_bar, bars - 1), first = bar * bar_steps;
    int cursor = s.ui.step_cursor;
    bool held = s.ui.held_step >= 0;
    int highlight = held ? s.ui.held_step : cursor;
    const auto& cur = pattern.step(unsigned(std::max(highlight, 0)) % pattern.steps());
    Header h = header_of(s);
    juce::String step = "STEP " + juce::String(highlight + 1);
    if (const Note* n = cur.first()) step += juce::String::fromUTF8(" \xc2\xb7 ") + juce::String(note_name(n->key));
    int in_hand = lane_event(pattern, unsigned(std::max(highlight, 0)) % pattern.steps(), steps_lane(s.track()));
    if (in_hand >= 0 && pattern.events[size_t(in_hand)].hits() > 1) // a ratchet: its hits
        step += juce::String::fromUTF8(" \xc2\xb7 \xc3\x97") + juce::String(pattern.events[size_t(in_hand)].hits());
    if (in_hand >= 0 && pattern.events[size_t(in_hand)].condition != trig_always) // its trig condition
        step += juce::String::fromUTF8(" \xc2\xb7 ") + trig_name(pattern.events[size_t(in_hand)].condition);
    if (held) step += juce::String::fromUTF8(" \xc2\xb7 LOCK"); // held: the pots lock it (roadmap P1)
    h.left.push_back(pill(step, c));
    if (s.ui.euclid_cards && steps_lane(s.track()) >= 0) // the EUCLID row: the lane's rhythm, E(hits, steps)
        if (const EuclidLane* e = euclid_of(pattern, unsigned(steps_lane(s.track()))))
            h.left.push_back(pill("E(" + juce::String(e->hits) + "," + juce::String(e->steps) + ")"));
    if (steps_lane(s.track()) >= 0 && pattern.own_lane(unsigned(steps_lane(s.track())))) // its lane loops on its own
        h.left.push_back(pill("LANE " + juce::String((pattern.lane_length(unsigned(steps_lane(s.track()))) + pattern.q() - 1) / pattern.q())));
    h.right.push_back(tag_of(s, s.ui.track));
    SoftKeys keys {{"STEPS", "ROLL", "MOTION"}, 0, c};
    bool kit_lane = steps_lane(s.track()) >= 0;
    keys.dots[0] = kit_lane ? 3 : 2; // STEPS again: the TRIG row of cards; on a kit again the EUCLID row (roadmap P1)
    keys.dot_at[0] = s.ui.euclid_cards && kit_lane ? 2 : s.ui.trig_cards ? 1 : 0;
    if (s.ui.sheet_open) keys = SoftKeys {{"BACK", "", "DO IT"}, 2, c, {KeyIcon::Back, KeyIcon::None, KeyIcon::Check}};
    frame(p, s, keys, h);
    if (s.ui.steps_zoom & 3) { draw_overview(p, s, c, 1u << (s.ui.steps_zoom & 3)); return; } // INDIGO X DENIM: zoomed out

    // Pitch as rows of the project's scale, six of them centred on the bar's notes.
    std::array<uint8_t, 12> degrees {};
    unsigned count = scale_degrees(s.project.scale, degrees);
    auto row_key = [&](int index) { // scale note `index` above the root of octave 0
        int octave = index >= 0 ? index / int(count) : -((-index + int(count) - 1) / int(count));
        return 12 * octave + s.project.root + degrees[size_t(index - octave * int(count))];
    };
    auto index_of = [&](int key) {
        int best = 0;
        for (int i = 0; i < 80; ++i) if (row_key(i) <= key) best = i;
        return best;
    };
    int lo = 1000, hi = -1;
    for (unsigned st = first; st < first + bar_steps && st < pattern.steps(); ++st)
        for (auto& n : pattern.step(st).notes)
            if (n.velocity) { lo = std::min(lo, index_of(n.key)); hi = std::max(hi, index_of(n.key)); }
    if (hi < 0) lo = hi = index_of(s.track().step_note());
    constexpr int rows = 6;
    int base = (lo + hi) / 2 - rows / 2 + 1;
    constexpr float top = zone::picture_y + 12, ch = 96, colw = 16.5f, stepw = 19.1f, beat_gap = 2.6f;
    auto x_of = [&](unsigned k) { return zone::pad + k * stepw + float(k / 4) * beat_gap; };
    // A kit lane of its own length (roadmap P1): its steps end sooner and its playhead goes round on its own.
    int lane = steps_lane(s.track());
    uint32_t loop = lane >= 0 ? pattern.lane_length(unsigned(lane)) : pattern.length;
    unsigned lane_steps = std::max(1u, (loop + pattern.q() - 1) / pattern.q());
    uint64_t elapsed = uint64_t(s.pass) * std::max<uint32_t>(pattern.length, 1) + s.position % std::max<uint32_t>(pattern.length, 1);
    int playing_step = s.playing ? int((elapsed % std::max<uint32_t>(loop, 1)) / pattern.q()) : -1;
    for (unsigned k = 0; k < bar_steps; ++k) {
        unsigned st = first + k;
        float x = x_of(k);
        bool inside = st < pattern.steps(), here = int(st) == highlight, past = st >= lane_steps; // past: the lane's end
        p.fill(x, top, colw, ch, here ? c.withAlpha(0.2f) : inside && !past ? colour::quiet : colour::bg, 2);
        if (int(st) == playing_step) p.fill(x, top - 3, colw, 2, colour::text, 1); // where it plays now
        auto number = plain(here ? colour::text : k % 4 == 0 ? colour::sec : colour::dim, type::small, here || k % 4 == 0 ? Weight::Bold : Weight::Medium);
        p.text(juce::String(k + 1), x - 2, top + ch + 2, colw + 4, 14, number, juce::Justification::centred);
        if (!inside) continue;
        const auto& step = pattern.step(st);
        auto in = step_events(pattern, st);
        for (unsigned k = 0; k < in.count && k < notes_per_step; ++k) { // each note at its pitch's row, as tall as it is hard
            const auto& e = pattern.events[in.first + k];
            int row = std::clamp(index_of(e.key) - base, 0, rows - 1);
            float cy = top + ch - (row + 0.5f) * (ch / rows);
            unsigned hits = e.hits();
            if (hits == 1) {
                float bh = 4 + 7 * e.velocity / 127.0f;
                p.fill(x + 1.5f, cy - bh / 2, colw - 3, bh, here ? colour::text : past ? colour::line : c, 2);
                continue;
            }
            // A ratchet: its hits side by side, each as tall as it plays (the ramp).
            float gap = 0.75f, hw = (colw - 3 - gap * (hits - 1)) / hits;
            for (unsigned n = 0; n < hits; ++n) {
                float bh = 4 + 7 * e.hit_velocity(e.velocity, n) / 127.0f;
                p.fill(x + 1.5f + n * (hw + gap), cy - bh / 2, hw, bh, here ? colour::text : past ? colour::line : c, 1);
            }
        }
        if (step.empty()) continue;
        int lane_note = lane_event(pattern, st, steps_lane(s.track()));
        int kit_pad = s.track().engine == EngineKind::Sampler && steps_lane(s.track()) >= 0 ? steps_lane(s.track()) - int(sampler_first_note) : -1;
        if (lock_in(pattern, st * pattern.q(), (st + 1) * pattern.q(), kit_pad)) // a Motion lock: a square at its top
            p.fill(x + 2, top + 2, 4, 4, here ? colour::text : colour::sec, 1);
        if (lane_note >= 0 && pattern.events[size_t(lane_note)].condition != trig_always) // a trig condition: a ring at its foot
            p.ring(x + colw / 2 - 3, top + ch - 9, 6, 6, here ? colour::text : colour::sec, 3, 1.2f);
        if (step.flags & step_accent) p.circle(x + colw / 2, top - 6.5f, 2, c);      // an accent: a dot over it
        if (step.flags & step_tie && k + 1 < bar_steps) {                            // a tie: an arc under it to the next
            juce::Path arc;
            float ax = x + colw / 2, bx = x_of(k + 1) + colw / 2, ay = top + ch - 2;
            arc.startNewSubPath(ax, ay); arc.cubicTo(ax + 4, ay + 6, bx - 4, ay + 6, bx, ay);
            p.g.setColour(colour::sec); p.g.strokePath(arc, juce::PathStrokeType(1.3f));
        }
        if (step.flags & step_slide) icon(p, Icon::Slide, x + colw - 9, top + 2, 8, colour::sec, 1.6f); // a slide: its sign
    }
    if (s.ui.sheet_open) { // Push: VARY, a variation of the pattern into the next empty one (roadmap P1)
        p.dim_zone(zone::picture_y, zone::picture_h, 0.7f);
        MenuSheet m;
        m.title = "INTO THE NEXT EMPTY PATTERN"; m.accent = c;
        bool kit = s.track().engine == EngineKind::Sampler || drum_pads(s.track());
        for (unsigned v = 0; v < unsigned(Variation::Count); ++v) {
            bool here = variation_on_kit(Variation(v)) || !kit;
            m.rows[v] = {variation_name(Variation(v)), v < 3 ? juce::String(s.ui.vary_amount) + " %" : juce::String(), false, false, !here};
        }
        m.count = unsigned(Variation::Count); m.current = s.ui.sheet_row % m.count;
        m.detail = {};
        wide_sheet(p, m, "VARY", c); // two columns: the cards (K1 the amount) stay in view
        return;
    }
    // The pattern's bars as blocks at the foot, the one in view lit.
    unsigned shown = std::min(bars, 8u), window = bar / shown * shown;
    float bw = (zone::width - 2 * zone::pad - (shown - 1) * 3.0f) / shown, by = zone::picture_y + zone::picture_h - 8;
    for (unsigned b = 0; b < shown; ++b)
        p.fill(zone::pad + b * (bw + 3), by, bw, 5, window + b == bar ? c : colour::line, 2);
}

// The step's notes out; on a kit only the last pad's lane (shell/step_lane.h).
void clear_step(Context& c, unsigned st, int lane) {
    if (lane < 0) { c.set_step(c.ui.track, st, Step {}); return; }
    if (clear_lane(c.pattern(), st, lane)) c.sync_pattern(c.ui.track, c.track().pattern);
}

void pad(Context& c, unsigned pad, uint8_t velocity, bool down) {
    if (down) c.ui.sheet_open = false; // a pad leaves the VARY sheet and does its job
    auto& pattern = c.pattern();
    unsigned st = unsigned(c.ui.steps_bar) * bar_steps + pad % 16;
    if (st >= pattern.steps()) return;
    int lane = steps_lane(c.track()); // a kit: the last pad's lane, the other pads' notes left alone
    if (down) {
        if (c.ui.erase) { c.edit_pattern(c.ui.track, c.track().pattern, "Erase step", [&] { clear_step(c, st, lane); }); return; }
        c.ui.step_cursor = int16_t(st);
        c.ui.held_step = int16_t(st);
        c.ui.held_since = c.now;
        c.ui.held_edited = false;
        c.ui.held_created = !step_velocity(pattern, st, lane);
        if (c.ui.held_created) {
            uint8_t v = uint8_t(std::max<int>(velocity, 64));
            if (lane >= 0) {
                if (add_lane_note(pattern, st, uint8_t(lane), v)) c.sync_pattern(c.ui.track, c.track().pattern);
                else c.notice("The pattern is full");
            } else {
                Step step;
                step.notes[0] = {c.track().step_note(), v};
                c.set_step(c.ui.track, st, step);
            }
        }
        return;
    }
    if (c.ui.held_step != int(st)) return;
    bool tap = c.now - c.ui.held_since < tap_ms;
    if (!c.ui.held_created && !c.ui.held_edited && tap) clear_step(c, st, lane);
    c.ui.held_step = -1;
}

void wheel(Context& c, int delta) {
    if (c.ui.sheet_open) { c.ui.sheet_row = uint8_t(std::clamp<int>(c.ui.sheet_row + delta, 0, int(Variation::Count) - 1)); return; }
    auto& pattern = c.pattern();
    if (c.ui.held_step >= 0 && steps_lane(c.track()) >= 0) {
        // A kit (or Drums pads): the lane's note moves to the next pad (not onto one already there), and the lane with it.
        auto& t = c.track();
        bool kit = t.engine == EngineKind::Sampler;
        int lane = steps_lane(t), to = std::clamp(lane + delta, int(kit ? sampler_first_note : drum_first_key), int(kit ? sampler_last_key : drum_last_key));
        unsigned st = unsigned(c.ui.held_step);
        if (to != lane && !step_velocity(pattern, st, to) && move_lane(pattern, st, lane, uint8_t(to))) {
            t.last_note = uint8_t(to);
            if (kit) t.last_pad = uint8_t(to - int(sampler_first_note));
            c.sync_pattern(c.ui.track, t.pattern);
        }
        c.ui.held_edited = true;
        return;
    }
    if (c.ui.held_step >= 0) {
        // The held step's notes move together, one semitone a detent.
        Step step = pattern.step(unsigned(c.ui.held_step));
        for (auto& n : step.notes) if (n.velocity) n.key = uint8_t(std::clamp(n.key + delta, 0, 127));
        if (const Note* n = step.first()) {
            auto& t = c.track();
            t.last_note = n->key;
            if (t.engine == EngineKind::Sampler && n->key >= sampler_first_note && n->key < sampler_first_note + sampler_pads)
                t.last_pad = uint8_t(n->key - sampler_first_note); // on a kit the note is a pad
        }
        c.set_step(c.ui.track, unsigned(c.ui.held_step), step);
        c.ui.held_edited = true;
        return;
    }
    c.ui.step_cursor = int16_t(std::clamp<int>(c.ui.step_cursor + delta, 0, int(pattern.steps()) - 1));
    c.ui.steps_bar = uint8_t(c.ui.step_cursor / bar_steps);
}

void nudge(Context& c, int way) {
    unsigned bars = bars_of(c.pattern());
    c.ui.steps_bar = uint8_t(std::clamp<int>(c.ui.steps_bar + way, 0, int(bars) - 1));
    c.ui.step_cursor = int16_t(c.ui.steps_bar * bar_steps + c.ui.step_cursor % bar_steps);
}

void push(Context& c) { // VARY: Push opens it on the row last used; there Push makes it
    if (c.ui.sheet_open) { c.ui.sheet_open = false; c.vary(Variation(c.ui.sheet_row % unsigned(Variation::Count))); return; }
    c.ui.sheet_open = true;
}

void soft(Context& c, int key) {
    if (c.ui.sheet_open) { // BACK, -, DO IT
        if (key == 0) c.ui.sheet_open = false;
        if (key == 2) push(c);
        return;
    }
    if (key == 0) { // STEPS again: the step's TRIG cards, on a kit then the lane's EUCLID ones, and back
        bool kit = steps_lane(c.track()) >= 0;
        if (!c.ui.trig_cards && !c.ui.euclid_cards) c.ui.trig_cards = true;
        else if (c.ui.trig_cards && kit) { c.ui.trig_cards = false; c.ui.euclid_cards = true; }
        else { c.ui.trig_cards = false; c.ui.euclid_cards = false; }
        return;
    }
    static const PageId pages[] {PageId::Steps, PageId::Roll, PageId::Motion};
    c.open(pages[key % 3]);
}
} // namespace

const PageSpec steps_page {draw, pad, wheel, push, soft, nudge};

} // namespace melodee
