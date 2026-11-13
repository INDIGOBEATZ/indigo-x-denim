// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 Kerem Kilic (Ellic Studio)
// INDIGO X DENIM: the palettes (ui/palettes.h): Melodee's NIGHT and INDIGO'S MEDIA LAB's themes.
#pragma once
#include "melodee/ui/palettes.h"
#include <array>
#include <atomic>
#include <cstdint>

// Each track's colour and the two shades its other knob cards take (the pads
// use the first), and the Looper's L1-L4, in the palette in use (Settings > Palette).
namespace melodee::ui {

constexpr unsigned palette_count = unsigned(palettes.size());
// The palette in use. The screen thread sets it from the settings each frame (ui/tokens.h, use_palette); the pads'
// lights read it too, so it is atomic.
inline std::atomic<uint8_t> palette_now {uint8_t(default_palette)};
inline const char* palette_name(unsigned palette) { return palettes[palette % palette_count].name; }

// track_shades[t][k] and loop_shades[l][k] read the palette in use.
struct TrackShades {
    const std::array<uint32_t, 3>& operator[](size_t t) const { return palettes[palette_now.load(std::memory_order_relaxed) % palette_count].tracks[t % 16]; }
};
struct LoopShades {
    const std::array<uint32_t, 3>& operator[](size_t l) const { return palettes[palette_now.load(std::memory_order_relaxed) % palette_count].loops[l % 4]; }
};
inline constexpr TrackShades track_shades {};
inline constexpr LoopShades loop_shades {};

} // namespace melodee::ui
