// SPDX-License-Identifier: AGPL-3.0-only
// INDIGO X DENIM: the typefaces the screen can use (Settings > Font), each a variable font under the SIL Open Font
// License, embedded in the binary (ui/fonts_data.cpp, fetched by src/melodee/dependencies.py). Rubik is Melodee's.
#pragma once
#include <array>
#include <atomic>
#include <cstdint>

namespace melodee::ui {

// The names, in Settings' order; their files and metrics are the screen's alone (ui/paint.cpp), so the model and
// the tests need no font data.
constexpr std::array<const char*, 8> font_names {{"Rubik", "Inter", "Space Grotesk", "Outfit", "Sora", "Manrope", "Lexend", "JetBrains Mono"}};
constexpr unsigned font_count = 8;
inline const char* font_name(unsigned font) { return font_names[font % font_count]; }
// The typeface in use: set from the settings each frame on the screen thread (ui/paint.h, use_font).
inline std::atomic<uint8_t> font_now {0};

} // namespace melodee::ui
