// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 Kerem Kilic (Ellic Studio)
// INDIGO X DENIM LAB: Explode Pads to Tracks (as Jsen Surreal's SXL has it): each used pad of the kit in hand onto an
// empty track of its own, moved to its pad 1 (compact), with its effects, its notes from every pattern and the
// track's mix strip; the kit keeps what found no empty track, and goes once nothing is left on it. One undo per
// track it touches. MIDI thread.
#include "melodee/shell/explode.h"
#include "melodee/shell/context.h"
#include "melodee/shell/track_copy.h"
#include <algorithm>
#include <cstdio>
#include <vector>

namespace melodee {

namespace {
// Only `key`'s notes and lanes, moved to `to`; everything else of the track's patterns (length, Q, feel, motion,
// punches, follow) as it was.
Pattern only_key(const Pattern& from, uint8_t key, uint8_t to) {
    Pattern p = from;
    p.events.clear();
    for (auto e : from.events) if (e.key == key) { e.key = to; p.events.push_back(e); }
    p.lanes.clear();
    for (auto l : from.lanes) if (l.key == key) { l.key = to; p.lanes.push_back(l); }
    p.euclids.clear();
    for (auto l : from.euclids) if (l.key == key) { l.key = to; p.euclids.push_back(l); }
    return p;
}

void drop_key(Pattern& p, uint8_t key) {
    p.events.erase(std::remove_if(p.events.begin(), p.events.end(), [&](const Event& e) { return e.key == key; }), p.events.end());
    p.lanes.erase(std::remove_if(p.lanes.begin(), p.lanes.end(), [&](const LaneLength& l) { return l.key == key; }), p.lanes.end());
    p.euclids.erase(std::remove_if(p.euclids.begin(), p.euclids.end(), [&](const EuclidLane& l) { return l.key == key; }), p.euclids.end());
}
} // namespace

// INDIGO X DENIM LAB: Reorder (as SXL's): pad `a` and pad `b` of the selected kit trade places, with their effects,
// mutes and every note and lane that names them, so nothing plays differently. One undo.
void swap_pads(Context& c, unsigned a, unsigned b) {
    const unsigned t = c.ui.track % track_count;
    a %= sampler_pads; b %= sampler_pads;
    if (a == b || c.project.tracks[t].engine != EngineKind::Sampler) return;
    if (c.ui.recording) { c.notice("Reorder: not in a take"); return; }
    const TrackState before = track_state(c, t);
    auto next = std::make_unique<TrackState>(before);
    auto& tr = next->track;
    std::swap(tr.pads[a], tr.pads[b]);
    std::swap(tr.pad_fx.at[a], tr.pad_fx.at[b]);
    bool ma = tr.pad_muted(a), mb = tr.pad_muted(b);
    tr.set_pad_muted(a, mb); tr.set_pad_muted(b, ma);
    const uint8_t ka = uint8_t(sampler_first_note + a), kb = uint8_t(sampler_first_note + b);
    auto swap_key = [&](uint8_t& key) { if (key == ka) key = kb; else if (key == kb) key = ka; };
    for (auto& p : next->patterns) {
        for (auto& e : p.events) swap_key(e.key);
        std::stable_sort(p.events.begin(), p.events.end(), [](const Event& x, const Event& y) { return x.tick != y.tick ? x.tick < y.tick : x.key < y.key; });
        for (auto& l : p.lanes) swap_key(l.key);
        std::sort(p.lanes.begin(), p.lanes.end(), [](const LaneLength& x, const LaneLength& y) { return x.key < y.key; });
        for (auto& l : p.euclids) swap_key(l.key);
        std::sort(p.euclids.begin(), p.euclids.end(), [](const EuclidLane& x, const EuclidLane& y) { return x.key < y.key; });
    }
    if (tr.last_pad == a) tr.last_pad = uint8_t(b); else if (tr.last_pad == b) tr.last_pad = uint8_t(a);
    Edit edit = c.begin_edit("Reorder pads", t, -1, false);
    edit.track_before = std::make_unique<TrackState>(before);
    restore_track(c, t, *next);
    edit.track_after = std::move(next);
    c.edits.push(std::move(edit));
}

void explode_pads(Context& c) {
    const unsigned src = c.ui.track % track_count;
    const auto& kit = c.project.tracks[src];
    if (kit.engine != EngineKind::Sampler) { c.notice("Explode: a kit track"); return; }
    if (c.ui.recording) { c.notice("Explode: not in a take"); return; }
    std::vector<unsigned> used, free;
    for (unsigned k = 0; k < sampler_pads; ++k) if (kit.pads[k].used()) used.push_back(k);
    for (unsigned t = 0; t < track_count; ++t) if (t != src && !track_clearable(c.project.tracks[t], c.patterns.at[t])) free.push_back(t);
    if (used.size() < 2) { c.notice("Explode: two pads or more"); return; }
    if (free.empty()) { c.notice("Explode: no empty track"); return; }
    const TrackState before = track_state(c, src);
    const unsigned n = unsigned(std::min(used.size(), free.size()));
    for (unsigned i = 0; i < n; ++i) {
        const unsigned k = used[i], to = free[i];
        auto next = std::make_unique<TrackState>(before);
        auto& t = next->track;
        t.pads = {};
        t.pads[0] = before.track.pads[k];
        t.pad_fx = PadChains {};
        t.pad_fx.at[0] = before.track.pad_fx.at[k];
        t.pad_mutes = {};
        t.zone_trims = {};
        t.bank = 0; t.last_pad = 0; t.levels = false;
        const auto& ref = c.project.samples[t.pads[0].sample % sample_slots];
        std::snprintf(t.name, sizeof(t.name), "%s", ref.name[0] ? ref.name : before.track.name);
        for (unsigned p = 0; p < pattern_count; ++p)
            next->patterns[p] = only_key(before.patterns[p], uint8_t(sampler_first_note + k), uint8_t(sampler_first_note));
        std::string what = "Explode pad to track " + std::to_string(to + 1);
        Edit edit = c.begin_edit(what.c_str(), to, -1, false);
        edit.track_before = std::make_unique<TrackState>(track_state(c, to));
        restore_track(c, to, *next);
        edit.track_after = std::move(next);
        c.edits.push(std::move(edit));
    }
    // The kit without what went.
    auto rest = std::make_unique<TrackState>(before);
    for (unsigned i = 0; i < n; ++i) {
        const unsigned k = used[i];
        rest->track.pads[k] = PadSound {};
        rest->track.pad_fx.at[k] = Chain {};
        rest->track.pad_fx.at[k].size = pad_slots;
        rest->track.set_pad_muted(k, false);
        for (auto& p : rest->patterns) drop_key(p, uint8_t(sampler_first_note + k));
    }
    Edit edit = c.begin_edit("Explode pads", src, -1, false);
    edit.track_before = std::make_unique<TrackState>(before);
    restore_track(c, src, *rest);
    edit.track_after = std::move(rest);
    c.edits.push(std::move(edit));
    if (n == used.size()) clear_track(c, src); // nothing left on it
    c.select_track(free[0]);
    char text[48];
    if (n == used.size()) std::snprintf(text, sizeof(text), "%u pads to %u tracks", n, n);
    else std::snprintf(text, sizeof(text), "%u to tracks, %u kept: no room", n, unsigned(used.size() - n));
    c.notice(text);
}

} // namespace melodee
