// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 Kerem Kilic (Ellic Studio)
// INDIGO X DENIM: a second palette, INDIGO (the default), beside Melodee's NIGHT.
#pragma once
#include <array>
#include <atomic>
#include <cstdint>

// Each track's colour and the two shades its other knob cards take (the pads
// use the first), and the Looper's L1-L4, for each palette (Settings > Palette).
namespace melodee::ui {

using Shades16 = std::array<std::array<uint32_t, 3>, 16>;
using Shades4 = std::array<std::array<uint32_t, 3>, 4>;

constexpr Shades16 night_track_shades {{
    {0xFF7A5C, 0xFF6F93, 0xFF9A3C}, {0xFFC145, 0xFF9F5A, 0xE2D94A}, {0x46D9B0, 0x4FC7E0, 0x8FDB6A},
    {0x9D8CFF, 0xC587F2, 0x6FA0FF}, {0x4FB8FF, 0x4FFFF4, 0x4F66FF}, {0xFF6FA8, 0xFF6FEB, 0xFF796F},
    {0xA6E04A, 0xE0D44A, 0x60E04A}, {0xD77BFF, 0x997BFF, 0xFF7BE9}, {0x2FD0D6, 0x2FD68E, 0x2F82D6},
    {0xE8D94A, 0xE88F4A, 0xADE84A}, {0xFF5C7A, 0xFF5CC6, 0xFF8A5C}, {0x5FD97A, 0x7DD95F, 0x5FD9B3},
    {0x6F8CFF, 0x6FCFFF, 0x956FFF}, {0xFF9A3C, 0xFF3F3C, 0xFFF53C}, {0xF06FD8, 0xCC6FF0, 0xF06F9C},
    {0xB8C2D6, 0xB8D0D6, 0xBCB8D6},
}};
constexpr Shades4 night_loop_shades {{
    {0x6FD3FF, 0x6FA6FF, 0x6FFFE5}, {0xFFA36F, 0xFF7A6F, 0xFFD06F}, {0xC09CFF, 0x9C9EFF, 0xE69CFF}, {0x7DEBA6, 0x7DEBD9, 0xA4EB7D},
}};
// INDIGO: magenta leads, then a neon spectrum on the deep indigo night (INDIGO'S MEDIA LAB's colours).
constexpr Shades16 indigo_track_shades {{
    {0xFF5EC4, 0xFF5EEF, 0xFF5E98}, {0xFFC266, 0xFF9865, 0xFFEB65}, {0x7EE0B0, 0x7DE095, 0x7DE0CA},
    {0x8B7BFF, 0x7B8EFF, 0xAE7BFF}, {0x5AA8FF, 0x5AD4FF, 0x5A7BFF}, {0xFF7AD9, 0xFF7AFC, 0xFF7AB5},
    {0xB8E86A, 0xDAE76A, 0x95E76A}, {0xC77DFF, 0xA37DFF, 0xEA7DFF}, {0x3FD6E0, 0x3FDFBE, 0x3FAADF},
    {0xFFE07A, 0xFFBC7A, 0xFAFF7A}, {0xFF5C7A, 0xFF5CA6, 0xFF6A5C}, {0x6FE39A, 0x6FE27A, 0x6FE2B9},
    {0x6F7DFF, 0x6FA3FF, 0x876FFF}, {0xFF9A5C, 0xFF6D5C, 0xFFC65C}, {0xE46FFF, 0xBD6FFF, 0xFF6FF3},
    {0xACB0E3, 0xACBEE3, 0xB6ACE3},
}};
constexpr Shades4 indigo_loop_shades {{
    {0x6FB4FF, 0x6FDAFF, 0x6F8DFF}, {0xFF8FD0, 0xFE8FEE, 0xFE8FB1}, {0xA99BFF, 0x9BA8FF, 0xC49BFF}, {0x7EE0B0, 0x7DE095, 0x7DE0CA},
}};

// The palette in use: 0 NIGHT, 1 INDIGO. The screen thread sets it from the settings each frame (ui/tokens.h,
// use_palette); the pads' lights read it too, so it is atomic.
constexpr unsigned palette_count = 2;
inline std::atomic<uint8_t> palette_now {1};
inline const char* palette_name(unsigned palette) { return palette % palette_count == 0 ? "NIGHT" : "INDIGO"; }

// track_shades[t][k] and loop_shades[l][k] read the palette in use.
struct TrackShades {
    const std::array<uint32_t, 3>& operator[](size_t t) const {
        return (palette_now.load(std::memory_order_relaxed) ? indigo_track_shades : night_track_shades)[t % 16];
    }
};
struct LoopShades {
    const std::array<uint32_t, 3>& operator[](size_t l) const {
        return (palette_now.load(std::memory_order_relaxed) ? indigo_loop_shades : night_loop_shades)[l % 4];
    }
};
inline constexpr TrackShades track_shades {};
inline constexpr LoopShades loop_shades {};

} // namespace melodee::ui
