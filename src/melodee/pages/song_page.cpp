// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 Kerem Kilic (Ellic Studio)
// SONG (Shift + Pad 12): an arrange view of our own. B1 Export (parity
// phase 6): B2 an audio mixdown, B3 the song as a new pattern. One lane per track,
// time left to right; a clip is a pattern placed on a lane, in the track's
// colour with its notes in small, notches at each repeat. Markers name the
// parts, the brace is the loop, the strip at the bottom is the whole song.
// The wheel moves the cursor by the bar, pads pick the lane. Every clip job
// is on a button of its own, no menu: B3 places the pattern on K2 for the
// length on K3 (on a clip, duplicates it after itself), B2 mutes the clip,
// Shift + B2 its motion, Shift + B3 gives it its own pattern, Push opens its
// notes (on empty time, places as B3 does), Chop splits it at the cursor,
// Erase deletes it, Note Repeat repeats it to the next marker, Loop loops
// its bars. K1 zoom. Minus / Plus jump between markers (or clip edges while
// there are none). Shift + Push adds a marker at the cursor, or names it.
// Play here plays the song from the cursor. Shift + K1 sets the tempo lane at
// the cursor's bar (roadmap P5; under 30 BPM, none there), Shift + K2 its
// shape, a step on its bar or a ramp from the point before; with points the
// lane is a strip over the tracks, its tempo at the cursor beside it.
#include "melodee/pages/common.h"
#include "melodee/pages/pages.h"
#include "melodee/shell/bindings.h"
#include "melodee/ui/widgets.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace melodee {
namespace {
using namespace ui;
// Six lanes at a time, tall enough for the clips' names; the window follows
// the selected track.
constexpr unsigned lanes = 6;
constexpr float x0 = 34, x1 = zone::width - zone::pad, lane_top = zone::picture_y + 29, lane_h = 15, lane_gap = 1;
constexpr float strip_h = 14; // roadmap P5: the tempo lane, over the tracks (one track lane fewer while it has points)

float lh_now = lane_h; // INDIGO X DENIM LAB: the lanes' height now (all sixteen lanes: thinner); screen thread
unsigned zoom_of(const UiState& ui) { return song_zooms[ui.song_zoom % (sizeof(song_zooms) / sizeof(song_zooms[0]))]; }
// The first bar in view: the remembered one, moved just enough to show the cursor.
unsigned view_of(const UiState& ui) {
    unsigned zoom = zoom_of(ui), view = ui.song_view;
    if (ui.song_cursor < view) view = ui.song_cursor;
    if (ui.song_cursor >= view + zoom) view = ui.song_cursor - zoom + 1;
    return view;
}
juce::String clock_of(const Song& song, unsigned bar, unsigned tempo_tenths, unsigned bar_ticks) { // through the tempo lane (roadmap P5)
    unsigned seconds = unsigned(song.tempo.seconds(bar, double(bar_ticks) / ticks_per_quarter, std::max(tempo_tenths, 300u)) + 0.5);
    char text[16];
    std::snprintf(text, sizeof(text), "%u:%02u", seconds / 60, seconds % 60);
    return text;
}
juce::String dash() { return juce::String::fromUTF8("\xe2\x80\x93"); }

void draw_clip(Paint& p, const Snapshot& s, const Clip& clip, float y, unsigned view, float bw, bool selected) {
    float cx = x0 + (float(clip.bar) - float(view)) * bw, cw = clip.bars * bw;
    float left = std::max(cx, x0), right = std::min(cx + cw, x1);
    if (right <= left + 1) return;
    auto c = track_colour(clip.track);
    p.g.saveState();
    p.g.reduceClipRegion(juce::Rectangle<int>(int(left), int(y), int(right - left + 1), int(lh_now)));
    p.fill(cx + 0.5f, y, cw - 1, lh_now, selected ? c : c.withAlpha(clip.muted ? 0.12f : 0.38f), 2.5f);
    const auto& sketch = s.sketches[clip.track][clip.pattern];
    // Its notes in small: the pattern repeated across the clip.
    // In sixteenths: the sketch keeps the first note of each.
    unsigned bar = s.project.bar_ticks(), length = std::max<unsigned>((sketch.length + sixteenth_ticks - 1) / sixteenth_ticks, 1);
    float step_w = bw * sixteenth_ticks / bar;
    int lo = 127, hi = 0;
    for (unsigned k = 0; k < std::min<unsigned>(length, 64); ++k)
        if (sketch.keys[k]) { lo = std::min<int>(lo, sketch.keys[k]); hi = std::max<int>(hi, sketch.keys[k]); }
    unsigned steps = unsigned(clip.bars * bar / sixteenth_ticks);
    auto note = selected ? colour::bg : c;
    if (hi >= lo && step_w >= 0.6f && !clip.muted)
        for (unsigned k = 0; k < steps; ++k) {
            unsigned at = k % length;
            if (at >= 64 || !sketch.keys[at]) continue;
            float nx = cx + k * step_w, ny = y + lh_now - 3 - (hi > lo ? float(sketch.keys[at] - lo) / (hi - lo) : 0.5f) * (lh_now - 7);
            if (nx > right) break;
            p.fill(nx, ny, std::max(1.0f, step_w * 0.8f), 1, note);
        }
    // A notch at each repeat of the pattern.
    float repeat = length * step_w;
    if (repeat >= 4)
        for (float rx = cx + repeat; rx < cx + cw - 1; rx += repeat) p.fill(rx, y, 0.5f, lh_now, colour::bg);
    if (lh_now >= 12) p.text("P" + juce::String(clip.pattern + 1), cx + 3, y, 24, lh_now, plain(selected ? colour::bg : c, type::small, Weight::Bold));
    if (selected) p.ring(cx + 0.5f, y, cw - 1, lh_now, colour::text, 2.5f);
    p.g.restoreState();
}

// B1-B3 follow what is under the cursor; with Shift, the clip's second jobs.
SoftKeys keys_of(const Snapshot& s, const Clip* clip, juce::Colour c) {
    if (s.ui.song_marq) { // LAB: the marquee's jobs (Shift: its mode, paste at the cursor, let it go)
        static const char* modes[] {"", "MODE MRQ", "MODE TRK", "MODE ARR"};
        if (s.ui.shift) return {{modes[s.ui.song_marq % 4], "PASTE", "DESELECT"}, -1, c};
        return {{"COPY", "DELETE", "LOOP"}, -1, c};
    }
    if (s.ui.shift) return {{s.ui.song_all ? "6 TRACKS" : "ALL TRACKS", clip ? "MOTION" : "", clip ? "OWN PATTERN" : ""}, -1, c};
    if (clip) return {{"EXPORT", clip->muted ? "UNMUTE" : "MUTE", "DUPLICATE"}, -1, c};
    return {{"EXPORT", "", s.project.tracks[s.ui.track].used() ? "PLACE" : ""}, -1, c};
}

// INDIGO X DENIM LAB: the ERASE wizard's bars, lanes and words.
struct Wipe { unsigned from, to, t0, t1; };
Wipe wipe_of(const UiState& ui, const Song& song) {
    Wipe w {0, song_bars, ui.track, ui.track};
    switch (ui.wipe_range % 4) {
    case 0: if (ui.song_marq) { w.from = ui.marq_from; w.to = ui.marq_to; } break;
    case 1: if (song.looping()) { w.from = song.loop_start; w.to = song.loop_end; } break;
    case 2: w.from = ui.song_cursor; break;
    default: break;
    }
    if (ui.wipe_lanes % 3 == 2) { w.t0 = 0; w.t1 = track_count - 1; }
    else if (ui.wipe_lanes % 3 == 1 && ui.song_marq) {
        w.t0 = ui.song_marq == 3 ? 0 : ui.song_marq == 2 ? ui.track : std::min(ui.marq_t0, ui.marq_t1);
        w.t1 = ui.song_marq == 3 ? track_count - 1 : ui.song_marq == 2 ? ui.track : std::max(ui.marq_t0, ui.marq_t1);
    }
    return w;
}
const char* const wipe_ranges[] {"Marquee", "Loop", "Cursor to end", "Whole song"};
const char* const wipe_lanes_words[] {"This track", "Marquee lanes", "All tracks"};
const char* const wipe_whats[] {"Clips", "Muted clips", "Markers", "Everything"};

void draw_wipe(Paint& p, const Snapshot& s, const Header& h) {
    const auto& song = s.project.song;
    const auto& ui = s.ui;
    SoftKeys keys {{"CANCEL", "", "ERASE"}, -1, colour::text, {KeyIcon::Back, KeyIcon::None, KeyIcon::None}};
    frame(p, s, keys, h);
    Wipe w = wipe_of(ui, song);
    unsigned clips = 0, markers = 0;
    unsigned what = ui.wipe_what % 4;
    for (unsigned n = 0; n < song.count; ++n) {
        const auto& cl = song.clips[n];
        if (cl.track < w.t0 || cl.track > w.t1 || cl.end() <= w.from || cl.bar >= w.to) continue;
        if (what == 0 || what == 3 || (what == 1 && cl.muted)) ++clips;
    }
    if (what >= 2) for (unsigned m = 0; m < song.marker_count; ++m) markers += song.markers[m].bar >= w.from && song.markers[m].bar < w.to;
    MenuSheet m;
    m.title = "ERASE";
    m.detail = "BARS " + juce::String(w.from + 1) + dash() + juce::String(std::min<unsigned>(w.to, std::max(song.length(), w.from + 1)));
    m.accent = colour::rec;
    bool marq = ui.song_marq != 0;
    m.rows[0] = {"Bars", wipe_ranges[ui.wipe_range % 4], false, false, !marq && ui.wipe_range % 4 == 0};
    m.rows[1] = {"Lanes", w.t0 == w.t1 ? juce::String("Track ") + juce::String(w.t0 + 1) : juce::String(w.t0 + 1) + dash() + juce::String(w.t1 + 1), false, false, false};
    m.rows[2] = {"What", wipe_whats[what], false, false, false};
    juce::String count = juce::String(clips) + (clips == 1 ? " clip" : " clips");
    if (what >= 2) count = (what == 3 ? count + ", " : juce::String()) + juce::String(markers) + (markers == 1 ? " marker" : " markers");
    m.rows[3] = {"Erase", count, false, true, clips + markers == 0};
    m.count = 4; m.current = ui.wipe_row % 4;
    menu_sheet(p, m);
}

void wipe_wheel(Context& c, int delta) { c.ui.wipe_row = uint8_t(std::clamp<int>(c.ui.wipe_row + delta, 0, 3)); }

void wipe_now(Context& c) {
    auto& song = c.project.song;
    Wipe w = wipe_of(c.ui, song);
    unsigned what = c.ui.wipe_what % 4, gone = 0;
    Edit edit = c.begin_edit("Erase song", c.ui.track, -1, true); // one undo
    if (what == 0 || what == 3) {
        unsigned before = song.count;
        for (unsigned t = w.t0; t <= w.t1; ++t) song.cut(t, w.from, w.to);
        gone += before > song.count ? before - song.count : 0;
    } else if (what == 1) {
        for (unsigned n = 0; n < song.count;) {
            const auto& cl = song.clips[n];
            if (cl.muted && cl.track >= w.t0 && cl.track <= w.t1 && cl.end() > w.from && cl.bar < w.to) { song.erase(n); ++gone; }
            else ++n;
        }
    }
    if (what >= 2) {
        unsigned kept = 0;
        for (unsigned m = 0; m < song.marker_count; ++m)
            if (song.markers[m].bar >= w.from && song.markers[m].bar < w.to) ++gone;
            else song.markers[kept++] = song.markers[m];
        song.marker_count = uint8_t(kept);
    }
    c.sync_song();
    c.commit_edit(std::move(edit));
    c.ui.song_wipe = false;
    c.notice("Erased bars " + std::to_string(w.from + 1) + "-" + std::to_string(std::min<unsigned>(w.to, std::max(song.length(), w.from + 1))));
}

void wipe_push(Context& c) {
    auto& ui = c.ui;
    switch (ui.wipe_row % 4) {
    case 0: ui.wipe_range = uint8_t((ui.wipe_range + 1) % 4); if (ui.wipe_range == 0 && !ui.song_marq) ui.wipe_range = 1; break;
    case 1: ui.wipe_lanes = uint8_t((ui.wipe_lanes + 1) % 3); if (ui.wipe_lanes == 1 && !ui.song_marq) ui.wipe_lanes = 2; break;
    case 2: ui.wipe_what = uint8_t((ui.wipe_what + 1) % 4); break;
    default: wipe_now(c); break;
    }
}

void draw(Paint& p, const Snapshot& s) {
    const auto& song = s.project.song;
    auto c = colour_of(s);
    unsigned zoom = zoom_of(s.ui), view = view_of(s.ui), cursor = s.ui.song_cursor;
    float bw = (x1 - x0) / zoom;
    int selected_clip = song.clip_at(s.ui.track, cursor);
    Header h = header_of(s);
    h.left.push_back(pill("BAR " + juce::String(cursor + 1) + juce::String::fromUTF8(" \xc2\xb7 ") + clock_of(song, cursor, s.project.tempo, s.project.bar_ticks())));
    if (zoom == song_zooms[0]) h.left.push_back(pill("SNAP 1 BAR"));
    // Following MIDI clock or MTC in, its chip takes the loop's room (the ruler's brace still shows the loop).
    else if (song.looping() && !s.following && !s.ui.song_marq) h.left.push_back(pill("LOOP " + juce::String(song.loop_start + 1) + dash() + juce::String(song.loop_end), colour::text));
    h.right.push_back(tag_of(s, s.ui.track));
    if (s.ui.song_marq) { // LAB
        static const char* modes[] {"", "MRQ", "TRK", "ARR"};
        h.left.push_back(pill(juce::String(modes[s.ui.song_marq % 4]) + " " + juce::String(s.ui.marq_from + 1) + dash() + juce::String(s.ui.marq_to), colour::text));
    }

    if (s.ui.song_wipe) { draw_wipe(p, s, h); return; } // LAB
    if (s.ui.song_export) {
        // Export (B1), as on the MPC: B2 an audio mixdown, B3 a new pattern; a tile under each says what it makes.
        // Roadmap P4: the wheel picks what B2 makes: the mixdown as float, 24 or 16 bits, the stems, or a MIDI file.
        static const char* const b2[] {"MIXDOWN", "MIXDOWN", "MIXDOWN", "STEMS", "MIDI FILE"};
        unsigned kind = s.ui.export_kind % 5;
        SoftKeys keys {{"CANCEL", b2[kind], "PATTERN"}, -1, colour::text, {KeyIcon::Back, KeyIcon::None, KeyIcon::None}};
        keys.glyphs = {Icon::None, kind == 4 ? Icon::Note : kind == 3 ? Icon::Layers : Icon::Wav, Icon::Pattern};
        frame(p, s, keys, h);
        p.dim_zone(zone::picture_y, zone::picture_h, 0.86f);
        p.dim_zone(zone::knobs_y, zone::knobs_h);
        unsigned bars = song.length(), seconds = unsigned(song.tempo.seconds(bars, double(s.project.bar_ticks()) / ticks_per_quarter, std::max(300u, unsigned(s.project.tempo))) + 0.5);
        if (!bars && kind != 4) { // nothing to export yet (a MIDI file takes the patterns playing)
            icon(p, Icon::Pattern, zone::width / 2.0f - 14, zone::picture_y + 44, 28, colour::amber, 1.4f);
            p.text("EMPTY SONG", 0, zone::picture_y + 80, zone::width, 14, caps_text(colour::amber), juce::Justification::centred);
            return;
        }
        unsigned free = pattern_count; // the first pattern number no track uses
        for (unsigned n = 0; n < pattern_count && free == pattern_count; ++n) {
            bool used = false;
            for (unsigned t = 0; t < track_count; ++t) used = used || s.sketches[t][n].notes;
            if (!used) free = n;
        }
        juce::String length = juce::String(seconds / 60) + ":" + juce::String(seconds % 60).paddedLeft('0', 2);
        unsigned stems = 0;
        for (const auto& tr : s.project.tracks) stems += tr.used();
        static const char* const formats[] {"32-BIT FLOAT", "24-BIT DITHER", "16-BIT DITHER", "", "SMF 96 PPQ"};
        juce::String sub = kind == 3 ? juce::String(stems) + " STEMS" : juce::String(formats[kind]);
        key_tiles(p, {KeyTile {}, value_tile(kind == 4 ? Icon::Note : kind == 3 ? Icon::Layers : Icon::Wav, bars ? length : juce::String("P"), sub, true), // the wheel picks
                      value_tile(Icon::Pattern, free < pattern_count ? "P" + juce::String(free + 1) : juce::String("FULL"),
                                 juce::String(bars) + (bars == 1 ? " BAR" : " BARS"))});
        return;
    }
    frame(p, s, keys_of(s, selected_clip >= 0 ? &song.clips[size_t(selected_clip)] : nullptr, c), h);

    // Markers, the ruler and the loop brace.
    float label_every = zoom <= 8 ? 1 : zoom / 4.0f;
    for (unsigned m = 0; m < song.marker_count; ++m) {
        const auto& marker = song.markers[m];
        if (marker.bar < view || marker.bar >= view + zoom) continue;
        float mx = x0 + (marker.bar - view) * bw;
        // LAB: zoomed far out markers sit close: a name gets only the room up to the next one (none under 18 px)
        float room = 80;
        for (unsigned o = 0; o < song.marker_count; ++o)
            if (song.markers[o].bar > marker.bar) room = std::min(room, (song.markers[o].bar - marker.bar) * bw - 4);
        juce::String name(marker.name);
        auto style = caps_text(colour::mid);
        while (name.length() > 1 && p.width(name, style) > room) name = name.dropLastCharacters(1);
        if (room >= 18) p.text(name, mx + 2, zone::picture_y, room, 13, style);
        p.fill(mx, zone::picture_y, 0.5f, lane_top - zone::picture_y + lanes * (lane_h + lane_gap), colour::line);
    }
    for (unsigned b = view; b < view + zoom; ++b)
        if (unsigned(b - view) % unsigned(label_every) == 0)
            p.text(juce::String(b + 1), x0 + (b - view) * bw + 2, zone::picture_y + 13, 30, 13, plain(colour::mid, type::small, Weight::SemiBold));
    if (song.looping()) {
        float lx = x0 + (float(song.loop_start) - float(view)) * bw, rx = x0 + (float(song.loop_end) - float(view)) * bw;
        lx = std::max(lx, x0); rx = std::min(rx, x1);
        if (rx > lx) {
            p.fill(lx, lane_top - 3, rx - lx, 1, colour::sec);
            p.fill(lx, lane_top - 4, 1, 3, colour::sec);
            p.fill(rx - 1, lane_top - 4, 1, 3, colour::sec);
        }
    }
    // Roadmap P5: the tempo lane, a strip over the tracks: its line across the view (a step straight up or down on
    // its bar, a ramp sloping from the point before), a dot a point (the cursor's ringed), the tempo at the cursor.
    bool tempo_lane = !song.tempo.empty();
    float tracks_top = lane_top + (tempo_lane ? strip_h + 2 : 0);
    unsigned shown = tempo_lane ? lanes - 1 : lanes;
    if (tempo_lane) {
        const auto& lane = song.tempo;
        double low = lane.at(view, s.project.tempo), high = low; // the tempos in view, at least 10 BPM tall
        auto take = [&](double t) { low = std::min(low, t); high = std::max(high, t); };
        take(lane.at(view + zoom, s.project.tempo));
        for (unsigned n = 0; n < lane.count; ++n)
            if (lane.points[n].bar > view && lane.points[n].bar < view + zoom) { take(lane.points[n].tempo); take(lane.at(lane.points[n].bar - 0.001, s.project.tempo)); }
        if (high - low < 100) { double mid = (high + low) / 2; low = mid - 50; high = mid + 50; }
        float sy = lane_top, sh = strip_h;
        auto y_of = [&](double tempo) { return sy + sh - 2 - float((tempo - low) / (high - low)) * (sh - 4); };
        auto x_of = [&](double bar) { return x0 + float(bar - view) * bw; };
        p.fill(x0, sy, x1 - x0, sh, colour::quiet, 2);
        juce::Path line;
        double now = lane.at(view, s.project.tempo);
        line.startNewSubPath(x0, y_of(now));
        for (unsigned n = 0; n < lane.count; ++n) {
            const auto& pt = lane.points[n];
            if (pt.bar <= view) continue;
            if (pt.bar >= view + zoom) { // past the view: where its ramp has got to at the view's end
                if (pt.ramp) line.lineTo(x1, y_of(lane.at(view + zoom, s.project.tempo)));
                else line.lineTo(x1, y_of(now));
                now = -1;
                break;
            }
            float px = x_of(pt.bar);
            if (pt.ramp) line.lineTo(px, y_of(pt.tempo));
            else { line.lineTo(px, y_of(now)); line.lineTo(px, y_of(pt.tempo)); }
            now = pt.tempo;
        }
        if (now >= 0) line.lineTo(x1, y_of(now));
        p.g.setColour(colour::amber);
        p.g.strokePath(line, juce::PathStrokeType(1.5f));
        for (unsigned n = 0; n < lane.count; ++n) {
            const auto& pt = lane.points[n];
            if (pt.bar < view || pt.bar >= view + zoom) continue;
            float px = x_of(pt.bar), py = y_of(pt.tempo);
            if (pt.bar == cursor) p.ring(px - 3.5f, py - 3.5f, 7, 7, colour::text, 3.5f, 1.2f);
            p.fill(px - 2, py - 2, 4, 4, colour::amber, 2);
        }
        double here = lane.at(cursor, s.project.tempo);
        p.text(juce::String(int(std::lround(here / 10))), zone::pad, sy, 26, sh, plain(colour::amber, type::small, Weight::SemiBold),
               juce::Justification::centred);
    }
    // Lanes: six tracks in view (five under the tempo lane), the selected one among them; LAB: or all sixteen, thin.
    float lane_room = lane_h;
    if (s.ui.song_all) { shown = track_count; lane_room = std::max(4.0f, std::floor((zone::knobs_y - 26 - tracks_top) / track_count) - lane_gap); }
    lh_now = lane_room;
    unsigned first = s.ui.song_all ? 0 : std::min<unsigned>(s.ui.track >= 2 ? s.ui.track - 2 : 0, track_count - shown);
    for (unsigned row = 0; row < shown; ++row) {
        unsigned t = first + row;
        float y = tracks_top + row * (lane_room + lane_gap);
        auto tc = track_colour(t);
        bool selected = t == s.ui.track;
        if (s.ui.song_all) { // a colour tab for each lane, the selected one wide and bright
            p.fill(zone::pad + (selected ? 0 : 8), y, selected ? 22 : 14, lane_room, s.project.tracks[t].used() ? tc.withAlpha(selected ? 1.0f : 0.55f) : colour::quiet, 1.5f);
        } else {
            if (selected) p.fill(zone::pad, y, 22, lane_room, tc, 3);
            p.text(juce::String(t + 1), zone::pad, y, 22, lane_room, plain(selected ? colour::bg : s.project.tracks[t].used() ? tc : colour::dim, type::small, Weight::Bold),
                   juce::Justification::centred);
        }
        for (unsigned n = 0; n < song.count; ++n)
            if (song.clips[n].track == t) draw_clip(p, s, song.clips[n], y, view, bw, int(n) == selected_clip);
    }
    float bottom = tracks_top + shown * (lane_room + lane_gap);
    if (s.ui.song_marq) { // LAB: the marquee over its bars and lanes
        unsigned t0 = s.ui.song_marq == 3 ? 0 : s.ui.song_marq == 2 ? s.ui.track : std::min(s.ui.marq_t0, s.ui.marq_t1);
        unsigned t1 = s.ui.song_marq == 3 ? track_count - 1 : s.ui.song_marq == 2 ? s.ui.track : std::max(s.ui.marq_t0, s.ui.marq_t1);
        int r0 = int(t0) - int(first), r1 = int(t1) - int(first);
        r0 = std::max(r0, 0); r1 = std::min(r1, int(shown) - 1);
        float mx0 = x0 + (float(s.ui.marq_from) - float(view)) * bw, mx1 = x0 + (float(s.ui.marq_to) - float(view)) * bw;
        mx0 = std::max(mx0, x0); mx1 = std::min(mx1, x1);
        if (r1 >= r0 && mx1 > mx0) {
            float my0 = tracks_top + r0 * (lane_room + lane_gap) - 1, my1 = tracks_top + (r1 + 1) * (lane_room + lane_gap);
            p.fill(mx0, my0, mx1 - mx0, my1 - my0, colour::text.withAlpha(0.13f), 2);
            p.ring(mx0, my0, mx1 - mx0, my1 - my0, colour::text, 2, 1);
        }
    }
    p.fill(x0 - 1, lane_top - 2, 0.5f, bottom - lane_top + 2, colour::line);
    // The cursor, and the playhead when the song plays.
    float cx = x0 + (float(cursor) - float(view)) * bw;
    p.fill(cx, zone::picture_y + 14, 1, bottom - zone::picture_y - 14, c);
    p.fill(cx - 2, zone::picture_y + 13, 5, 5, c, 1);
    if (s.song_playing) {
        unsigned bar = s.project.bar_ticks();
        float at = float(s.song_bar) + float(s.ticks % bar) / bar;
        if (at >= view && at < view + zoom) {
            float px = x0 + (at - view) * bw;
            p.fill(px, zone::picture_y + 17, 1, bottom - zone::picture_y - 17, colour::text);
            juce::Path tri;
            tri.addTriangle(px - 3, zone::picture_y + 13, px + 4, zone::picture_y + 13, px + 0.5f, zone::picture_y + 18);
            p.g.setColour(colour::text); p.g.fillPath(tri);
        }
    }
    // The punch lane: punches written into the song, in their target's colour.
    if (song.punch_count) {
        p.text("P", zone::pad, bottom - 3, 22, 11, caps_text(colour::dim), juce::Justification::centred);
        for (unsigned n = 0; n < song.punch_count; ++n) {
            const auto& e = song.punches[n];
            float from = float(e.bar) + e.step / 16.0f, to = from + e.length / 16.0f;
            if (to <= view || from >= view + zoom) continue;
            float px = x0 + (std::max(from, float(view)) - view) * bw, pw = std::max(1.5f, (std::min(to, float(view + zoom)) - std::max(from, float(view))) * bw);
            p.fill(px, bottom, pw, 2.5f, e.target == 255 ? colour::text : track_colour(e.target), 1);
        }
    }
    // The whole song, with the window on it, after its length in bars.
    unsigned total = std::max({song.length(), view + zoom, 16u});
    juce::String length = juce::String(song.length()) + (song.length() == 1 ? " BAR" : " BARS");
    auto length_style = caps_text(colour::dim);
    float oy = bottom + (song.punch_count ? 13 : 6), lw = p.width(length, length_style) + 1, sx = zone::pad + lw + 6, ow = x1 - sx;
    p.text(length, zone::pad, oy - 5.5f, lw, 11, length_style);
    p.fill(sx, oy, ow, 2, colour::line, 1);
    p.fill(sx + ow * view / total, oy - 1, std::max(4.0f, ow * zoom / total), 4, colour::mid, 2);
}

int clip_index(const Context& c) { return c.project.song.clip_at(c.ui.track, c.ui.song_cursor); }

void place(Context& c) {
    if (!c.track().used()) return;
    Clip clip;
    clip.track = c.ui.track; clip.pattern = c.ui.song_pattern; clip.bar = c.ui.song_cursor; clip.bars = c.ui.song_length;
    if (c.project.song.place(clip)) c.sync_song();
}

void duplicate(Context& c, int index) {
    Clip copy = c.project.song.clips[size_t(index)];
    copy.bar = copy.end();
    if (!c.project.song.place(copy)) return;
    c.ui.song_cursor = copy.bar;
    c.ui.song_view = uint16_t(view_of(c.ui));
    c.sync_song();
}

// A copy of its pattern in the first empty slot, so editing it changes this clip alone.
void own_pattern(Context& c, int index) {
    auto& song = c.project.song;
    Clip clip = song.clips[size_t(index)];
    const auto& patterns = c.patterns.at[clip.track];
    for (unsigned n = 0; n < pattern_count; ++n) {
        bool placed = false;
        for (unsigned k = 0; k < song.count; ++k) placed |= song.clips[k].track == clip.track && song.clips[k].pattern == n;
        if (placed || !patterns[n].empty() || n == clip.pattern) continue;
        c.copy_pattern(clip.track, clip.pattern, n);
        song.clips[size_t(index)].pattern = uint8_t(n);
        c.sync_song();
        return;
    }
    c.notice("No free pattern");
}

// Into the clip's pattern: its notes, or with `motion` its motion lanes.
void open_clip(Context& c, int index, bool motion) {
    Clip clip = c.project.song.clips[size_t(index)];
    c.launch(clip.track, clip.pattern);
    c.sync_song();
    auto engine = c.track().engine;
    c.open(motion ? PageId::Motion : engine == EngineKind::Sampler ? PageId::StepEdit : PageId::Roll);
}

void pad(Context& c, unsigned pad, uint8_t, bool down) {
    if (!down) return;
    if (c.ui.song_marq == 1) c.ui.marq_t1 = uint8_t(pad % track_count); // LAB: the marquee's lanes reach to the pad's
    c.select_track(pad); // pads pick the lane
}

void wheel(Context& c, int delta) {
    if (c.ui.song_wipe) { wipe_wheel(c, delta); return; } // LAB
    if (c.ui.song_export) { c.ui.export_kind = uint8_t(std::clamp(int(c.ui.export_kind) + delta, 0, 4)); return; } // what B2 makes (roadmap P4)
    auto& ui = c.ui;
    if (ui.shift) { // LAB: Shift + wheel draws the marquee from where the cursor was (as MPC 3.10's Shift + drag)
        ui.shift_used = true;
        if (!ui.song_marq) { ui.song_marq = 1; ui.marq_anchor = ui.song_cursor; ui.marq_t0 = ui.marq_t1 = uint8_t(ui.track); }
    }
    ui.song_cursor = uint16_t(std::clamp<int>(ui.song_cursor + delta, 0, int(song_bars) - 1));
    ui.song_view = uint16_t(view_of(ui));
    if (ui.shift && ui.song_marq) {
        ui.marq_from = std::min(ui.marq_anchor, ui.song_cursor);
        ui.marq_to = uint16_t(std::max(ui.marq_anchor, ui.song_cursor) + 1);
    }
}

// LAB: the marquee's lanes, bars and jobs.
void marquee_lanes(const UiState& ui, unsigned& t0, unsigned& t1) {
    t0 = ui.song_marq == 3 ? 0 : ui.song_marq == 2 ? ui.track : std::min(ui.marq_t0, ui.marq_t1);
    t1 = ui.song_marq == 3 ? track_count - 1 : ui.song_marq == 2 ? ui.track : std::max(ui.marq_t0, ui.marq_t1);
}

void marquee_copy(Context& c) {
    const auto& song = c.project.song;
    unsigned t0, t1;
    marquee_lanes(c.ui, t0, t1);
    c.song_board.clear();
    c.song_board_bars = uint16_t(c.ui.marq_to - c.ui.marq_from);
    for (unsigned n = 0; n < song.count; ++n) {
        Clip clip = song.clips[n];
        if (clip.track < t0 || clip.track > t1 || clip.end() <= c.ui.marq_from || clip.bar >= c.ui.marq_to) continue;
        uint16_t from = std::max(clip.bar, c.ui.marq_from), to = std::min(clip.end(), c.ui.marq_to);
        clip.bar = uint16_t(from - c.ui.marq_from); clip.bars = uint16_t(to - from);
        clip.track = uint8_t(clip.track - t0);
        c.song_board.push_back(clip);
    }
    c.notice(std::to_string(c.song_board.size()) + (c.song_board.size() == 1 ? " clip copied" : " clips copied"));
}

void marquee_delete(Context& c) {
    unsigned t0, t1;
    marquee_lanes(c.ui, t0, t1);
    Edit edit = c.begin_edit("Marquee delete", c.ui.track, -1, true); // one undo
    for (unsigned t = t0; t <= t1; ++t) c.project.song.cut(t, c.ui.marq_from, c.ui.marq_to);
    c.sync_song();
    c.commit_edit(std::move(edit));
    c.notice("Deleted bars " + std::to_string(c.ui.marq_from + 1) + "-" + std::to_string(c.ui.marq_to));
}

void marquee_paste(Context& c) {
    if (c.song_board.empty()) { c.notice("Nothing copied"); return; }
    unsigned at = c.ui.song_cursor, lane = c.ui.track;
    unsigned placed = 0;
    Edit edit = c.begin_edit("Marquee paste", c.ui.track, -1, true);
    for (auto clip : c.song_board) {
        unsigned t = lane + clip.track;
        if (t >= track_count || at + clip.bar + clip.bars > song_bars) continue;
        clip.track = uint8_t(t); clip.bar = uint16_t(at + clip.bar);
        if (c.project.song.place(clip)) ++placed;
    }
    c.sync_song();
    c.commit_edit(std::move(edit));
    // The pasted bars, selected and in view (as MPC 3.10 scrolls to what it pasted).
    c.ui.song_marq = c.ui.song_marq ? c.ui.song_marq : 1;
    c.ui.marq_from = uint16_t(at); c.ui.marq_to = uint16_t(std::min<unsigned>(at + c.song_board_bars, song_bars));
    c.ui.song_cursor = uint16_t(std::max<int>(int(c.ui.marq_to) - 1, int(at)));
    c.ui.song_view = uint16_t(view_of(c.ui));
    c.notice(std::to_string(placed) + (placed == 1 ? " clip pasted" : " clips pasted"));
}

void marquee_loop(Context& c) { // whole bars, as MPC 3.10's Loop to Marquee Selection
    auto& song = c.project.song;
    Edit edit = c.begin_edit("Loop to marquee", c.ui.track, -1, true);
    song.loop_start = c.ui.marq_from; song.loop_end = c.ui.marq_to;
    c.sync_song();
    c.commit_edit(std::move(edit));
    c.notice("Loop bars " + std::to_string(c.ui.marq_from + 1) + "-" + std::to_string(c.ui.marq_to));
}

void push(Context& c) {
    if (c.ui.song_wipe) { wipe_push(c); return; } // LAB
    if (c.ui.song_export) return;
    int index = clip_index(c);
    if (index >= 0) open_clip(c, index, false);
    else place(c);
}

void soft(Context& c, int key) {
    if (c.ui.song_wipe) { if (key == 0) c.ui.song_wipe = false; if (key == 2) wipe_now(c); return; } // LAB: CANCEL, ERASE
    if (c.ui.song_export) { c.song_export_soft(key); return; } // B1 Cancel, B2 Audio mixdown, B3 New pattern
    if (c.ui.song_marq) { // LAB: COPY, DELETE, LOOP; Shift: MODE, PASTE, DESELECT
        if (c.ui.shift) {
            c.ui.shift_used = true;
            if (key == 0) c.ui.song_marq = uint8_t(c.ui.song_marq % 3 + 1);
            if (key == 1) marquee_paste(c);
            if (key == 2) c.ui.song_marq = 0;
            return;
        }
        if (key == 0) marquee_copy(c);
        if (key == 1) marquee_delete(c);
        if (key == 2) marquee_loop(c);
        return;
    }
    int index = clip_index(c);
    if (c.ui.shift) {
        if (key == 0) { c.ui.shift_used = true; c.ui.song_all = !c.ui.song_all; return; } // LAB: all sixteen lanes
        if (index < 0) return;
        if (key == 1) open_clip(c, index, true);
        if (key == 2) own_pattern(c, index);
        return;
    }
    if (key == 0) c.ui.song_export = true; // Export, as on the MPC (Patterns is on Seq)
    if (key == 1 && index >= 0) {
        auto& clip = c.project.song.clips[size_t(index)];
        clip.muted = !clip.muted;
        c.sync_song();
    }
    if (key == 2) { if (index >= 0) duplicate(c, index); else place(c); }
}

// Minus / Plus: the previous or next marker, or clip edge while there are no markers.
void nudge(Context& c, int way) {
    const auto& song = c.project.song;
    int cursor = c.ui.song_cursor, best = -1;
    auto consider = [&](int bar) {
        if (way > 0 ? bar > cursor && (best < 0 || bar < best) : bar < cursor && (best < 0 || bar > best)) best = bar;
    };
    for (unsigned m = 0; m < song.marker_count; ++m) consider(song.markers[m].bar);
    if (!song.marker_count)
        for (unsigned n = 0; n < song.count; ++n) { consider(song.clips[n].bar); consider(song.clips[n].end()); }
    if (best < 0) best = way > 0 ? cursor : 0;
    c.ui.song_cursor = uint16_t(best);
    c.ui.song_view = uint16_t(view_of(c.ui));
}
} // namespace

// Shift + Push: a marker at the cursor with the next ready name, or on a
// marker the Name sheet to type its name (K1 there still steps the ready ones).
void song_marker(Context& c) {
    if (c.project.song.marker_at(c.ui.song_cursor) >= 0) c.name_marker(c.ui.song_cursor);
    else c.project.song.step_marker(c.ui.song_cursor);
}

// Chop: the clip in two at the cursor.
void song_chop(Context& c) {
    auto& song = c.project.song;
    int index = clip_index(c);
    if (index < 0 || c.ui.song_cursor <= song.clips[size_t(index)].bar) { c.notice("Chop: cursor inside a clip"); return; }
    if (song.count >= song.clips.size()) { c.notice("Chop: the song is full"); return; } // the tail would be lost
    Clip clip = song.clips[size_t(index)];
    unsigned cursor = c.ui.song_cursor;
    song.clips[size_t(index)].bars = uint16_t(cursor - clip.bar);
    Clip tail = clip;
    tail.bar = uint16_t(cursor); tail.bars = uint16_t(clip.end() - cursor);
    song.place(tail);
    c.sync_song();
}

void song_erase(Context& c) {
    int index = clip_index(c);
    // LAB: on empty time, with the marquee up, or open already (then it closes): the ERASE wizard.
    if (c.ui.song_wipe) { c.ui.song_wipe = false; return; }
    if (index < 0 || c.ui.song_marq) {
        c.ui.song_wipe = true; c.ui.wipe_row = 0;
        c.ui.wipe_range = c.ui.song_marq ? 0 : c.project.song.looping() ? 1 : 3;
        c.ui.wipe_lanes = c.ui.song_marq ? 1 : 0;
        return;
    }
    c.project.song.erase(unsigned(index));
    c.sync_song();
}

// Note Repeat: the clip again and again up to the next marker.
void song_repeat(Context& c) {
    auto& song = c.project.song;
    int index = clip_index(c);
    if (index < 0) return;
    Clip clip = song.clips[size_t(index)];
    unsigned until = song.next_marker(clip.bar);
    if (until <= clip.end()) { c.notice("Repeat: needs a marker ahead"); return; }
    for (unsigned at = clip.end(); at < until;) {
        Clip copy = clip;
        copy.bar = uint16_t(at); copy.bars = uint16_t(std::min<unsigned>(clip.bars, until - at));
        if (!song.place(copy)) break;
        at += copy.bars;
    }
    c.sync_song();
}

// Loop: the brace on the clip's bars; again, or on empty time, off.
bool song_loop(Context& c) {
    auto& song = c.project.song;
    int index = clip_index(c);
    if (index < 0 && !song.looping()) return false;
    bool same = index >= 0 && song.loop_start == song.clips[size_t(index)].bar && song.loop_end == song.clips[size_t(index)].end();
    if (index < 0 || same) song.loop_start = song.loop_end = 0;
    else { song.loop_start = song.clips[size_t(index)].bar; song.loop_end = song.clips[size_t(index)].end(); }
    c.sync_song();
    return true;
}

const PageSpec song_page {draw, pad, wheel, push, soft, nudge};

} // namespace melodee
