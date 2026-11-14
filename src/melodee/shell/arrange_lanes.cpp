// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 Kerem Kilic (Ellic Studio)
// INDIGO X DENIM LAB: ARRANGE's lanes for the screen: each track's pattern's notes in the view, as marks.
#include "melodee/shell/arrange_lanes.h"
#include "melodee/shell/bindings.h"
#include <algorithm>
#include <array>

namespace melodee {

void fill_arrange_lanes(Snapshot& out, const PatternBank& patterns, const std::function<int(unsigned)>& position_of) {
    const auto& ui = out.ui;
    const uint32_t span = arrange_zooms[ui.arr_zoom % (sizeof(arrange_zooms) / sizeof(arrange_zooms[0]))] * out.project.bar_ticks();
    const uint32_t from = ui.arr_view, to = from + span;
    const float px = arrange_lane_w / float(std::max<uint32_t>(span, 1));
    for (unsigned t = 0; t < track_count; ++t) {
        auto& lane = out.arrange[t];
        lane = ArrangeLane {};
        const auto& tr = out.project.tracks[t];
        lane.used = tr.used();
        if (!lane.used) continue;
        const auto& p = patterns.at[t][tr.pattern % pattern_count];
        const uint32_t length = std::max<uint32_t>(p.length, 1);
        lane.length = length;
        if (length >= from && length <= to) lane.end_x = int16_t((length - from) * px);
        int pos = position_of(t);
        if (out.playing && pos >= 0) { // its playhead, in the pass of its loop the view shows
            uint32_t at = uint32_t(pos) % length;
            for (uint32_t base = from / length * length; base < to; base += length)
                if (base + at >= from && base + at < to) { lane.play_x = int16_t((base + at - from) * px); break; }
        }
        // One mark a pixel column and pitch band: zoomed far out, a pattern's repeats stay drawn all the way.
        std::array<uint8_t, 280> taken {};
        int lo = 127, hi = 0;
        for (const auto& e : p.events) if (e.tick < length) { lo = std::min<int>(lo, e.key); hi = std::max<int>(hi, e.key); }
        if (lo > hi) continue;
        uint32_t longest = 1; // the longest note: one starting that much before the view still reaches into it
        for (const auto& e : p.events) if (e.tick < length) longest = std::max<uint32_t>(longest, e.length);
        for (uint32_t base = from / length * length; base < to && lane.count < ArrangeLane::most; base += length) {
            // Only the notes this pass of the loop can show (the events are in order): zoomed in on a long pattern,
            // a few instead of all of them, every frame.
            uint32_t seek = from > base ? from - base : 0;
            seek = seek > longest ? seek - longest : 0;
            auto first = std::lower_bound(p.events.begin(), p.events.end(), seek, [](const Event& e, uint32_t t) { return e.tick < t; });
            for (auto it = first; it != p.events.end(); ++it) {
                const auto& e = *it;
                if (e.tick >= length || base + e.tick >= to || lane.count >= ArrangeLane::most) break;
                uint32_t a = base + e.tick, b = a + std::max<uint32_t>(e.length, 1);
                if (b <= from || a >= to) continue;
                uint16_t x0 = uint16_t((std::max(a, from) - from) * px), x1 = uint16_t((std::min(b, to) - from) * px);
                uint8_t y = uint8_t(hi == lo ? 128 : (e.key - lo) * 255 / (hi - lo));
                uint8_t band = uint8_t(1u << (y >> 5));
                if (x1 <= x0 + 1 && x0 < taken.size()) { if (taken[x0] & band) continue; taken[x0] |= band; }
                auto& m = lane.marks[lane.count++];
                m.x0 = x0; m.x1 = x1; m.y = y;
                m.velocity = e.velocity; m.ghost = base > 0;
            }
        }
    }
}

} // namespace melodee
