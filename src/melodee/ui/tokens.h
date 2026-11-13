// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 Kerem Kilic (Ellic Studio)
#pragma once
#include <juce_graphics/juce_graphics.h>
#include "melodee/ui/shades.h"
#include <array>
#include <cstdint>

// Melodee's design tokens at device size (the boards are drawn at 2x: halve
// them). NIGHT palette, one colour per track, and the four fixed zones of
// every screen.
namespace melodee::ui {

inline juce::Colour hex(uint32_t rgb, float alpha = 1) { return juce::Colour(0xff000000u | rgb).withAlpha(alpha); }

namespace colour {
// INDIGO X DENIM: the palette's colours, set by use_palette() on the screen thread before each frame.
inline juce::Colour bg = hex(0x060419), panel = hex(0x0A0822), surf = hex(0x0D0B28), raised = hex(0x141230),
    quiet = hex(0x08061D), line = hex(0x262449), text = hex(0xFFFFFF), sec = hex(0xC4C2D6), mid = hex(0x9C9AB8),
    dim = hex(0x4E4E6C), rec = hex(0xE5173A), amber = hex(0xFFC266),
    play = hex(0x7EE0B0); // the transport playing (the place bar), apart from any track's colour
// The brand's two ends (INDIGO: indigo to magenta): the page's own soft key and the boot screen run between them.
inline juce::Colour brand_a = hex(0x2F39BA), brand_b = hex(0xFF5EC4);
inline bool brand_gradient = true; // NIGHT: off, the track's colour alone
// INDIGO X DENIM: the place bar's colour while playing, and one colour for every waveform (wave_fixed: else the track's).
inline juce::Colour place = hex(0xB46BFF), wave = hex(0x5CC8FF);
inline bool wave_fixed = true;
}

// Sets the palette in use (ui/palettes.h): the colours above, the track and loop shades, the pads' lights.
inline void use_palette(unsigned palette) {
    palette %= palette_count;
    static unsigned applied = palette_count; // none yet
    if (applied == palette) return;          // each frame asks: only a change does anything
    applied = palette;
    palette_now.store(uint8_t(palette), std::memory_order_relaxed);
    const auto& p = palettes[palette];
    using namespace colour;
    bg = hex(p.bg); panel = hex(p.panel); surf = hex(p.surf); raised = hex(p.raised); quiet = hex(p.quiet); line = hex(p.line);
    text = hex(p.text); sec = hex(p.sec); mid = hex(p.mid); dim = hex(p.dim); rec = hex(p.rec); amber = hex(p.amber); play = hex(p.play);
    brand_a = hex(p.brand_a); brand_b = hex(p.brand_b); brand_gradient = p.gradient;
    place = hex(p.place); wave_fixed = p.wave != 0; wave = wave_fixed ? hex(p.wave) : text;
}

// Track colours 1-16 with their knob shades, and the Looper's L1-L4: ui/shades.h (the pads use them too).
inline juce::Colour track_colour(unsigned track) { return hex(track_shades[track % 16][0]); }
inline juce::Colour knob_shade(unsigned track, unsigned knob) { return hex(track_shades[track % 16][knob % 3]); }
inline juce::Colour loop_colour(unsigned loop) { return hex(loop_shades[loop % 4][0]); }
inline juce::Colour loop_shade(unsigned loop, unsigned knob) { return hex(loop_shades[loop % 4][knob % 3]); }

// Zones, top to bottom, at device size.
namespace zone {
constexpr int width = 320, height = 240, pad = 6;
constexpr int soft_y = 0, soft_h = 22;
constexpr int header_y = 22, header_h = 22;
constexpr int picture_y = 44, picture_h = 144;
constexpr int knobs_y = 188, knobs_h = 52;
}

// Type sizes in device pixels. Readable at arm's length on the 320 x 240
// panel: nothing is drawn under `floor`, and Paint::fit never shrinks below
// it. Where a page runs out of room it shows fewer things at a time instead.
namespace type {
constexpr float floor = 11;
constexpr float caps = 11, soft = 11, small = 11, body = 13, value = 22, title = 20, list = 15, bpm = 15;
constexpr float caps_tracking = 0.06f; // letter spacing of capitals, in em
}

// How strongly something sits behind an overlay: CSS opacity .35 over BG.
constexpr float behind_overlay = 0.65f;

} // namespace melodee::ui
