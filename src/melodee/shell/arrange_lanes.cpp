// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 Kerem Kilic (Ellic Studio)
// INDIGO X DENIM LAB: ARRANGE's lanes for the screen: each track's pattern's notes in the view, as marks.
#include "melodee/shell/arrange_lanes.h"
#include "melodee/shell/bindings.h"
#include <algorithm>

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
        int lo = 127, hi = 0;
        for (const auto& e : p.events) if (e.tick < length) { lo = std::min<int>(lo, e.key); hi = std::max<int>(hi, e.key); }
        if (lo > hi) continue;
        for (uint32_t base = from / length * length; base < to && lane.count < ArrangeLane::most; base += length)
            for (const auto& e : p.events) {
                if (e.tick >= length || lane.count >= ArrangeLane::most) break;
                uint32_t a = base + e.tick, b = a + std::max<uint32_t>(e.length, 1);
                if (b <= from || a >= to) continue;
                auto& m = lane.marks[lane.count++];
                m.x0 = uint16_t((std::max(a, from) - from) * px); m.x1 = uint16_t((std::min(b, to) - from) * px);
                m.y = uint8_t(hi == lo ? 128 : (e.key - lo) * 255 / (hi - lo));
                m.velocity = e.velocity; m.ghost = base > 0;
            }
    }
}

} // namespace melodee
