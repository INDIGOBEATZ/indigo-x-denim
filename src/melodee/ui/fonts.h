// SPDX-License-Identifier: AGPL-3.0-only
// INDIGO X DENIM: the typefaces the screen can use (Settings > Font), each a variable font under the SIL Open Font
// License, embedded in the binary (ui/fonts_data.cpp, fetched by src/melodee/dependencies.py). Rubik is Melodee's.
#pragma once
#include <array>
#include <atomic>
#include <cstdint>

extern "C" const unsigned char melodee_rubik_start[], melodee_rubik_end[];
extern "C" const unsigned char indigo_font_inter_start[], indigo_font_inter_end[];
extern "C" const unsigned char indigo_font_spacegrotesk_start[], indigo_font_spacegrotesk_end[];
extern "C" const unsigned char indigo_font_outfit_start[], indigo_font_outfit_end[];
extern "C" const unsigned char indigo_font_sora_start[], indigo_font_sora_end[];
extern "C" const unsigned char indigo_font_manrope_start[], indigo_font_manrope_end[];
extern "C" const unsigned char indigo_font_lexend_start[], indigo_font_lexend_end[];
extern "C" const unsigned char indigo_font_jetbrainsmono_start[], indigo_font_jetbrainsmono_end[];

namespace melodee::ui {

struct Typeface { const char* name; const unsigned char* start; const unsigned char* end; float height_em; };
// height_em: ascent + descent in em, which turns type::caps_tracking (em) into JUCE's kerning factor.
inline const std::array<Typeface, 8>& typefaces() {
    static const std::array<Typeface, 8> list {{
        {"Rubik", melodee_rubik_start, melodee_rubik_end, 1.185f},
        {"Inter", indigo_font_inter_start, indigo_font_inter_end, 1.21f},
        {"Space Grotesk", indigo_font_spacegrotesk_start, indigo_font_spacegrotesk_end, 1.276f},
        {"Outfit", indigo_font_outfit_start, indigo_font_outfit_end, 1.26f},
        {"Sora", indigo_font_sora_start, indigo_font_sora_end, 1.26f},
        {"Manrope", indigo_font_manrope_start, indigo_font_manrope_end, 1.366f},
        {"Lexend", indigo_font_lexend_start, indigo_font_lexend_end, 1.25f},
        {"JetBrains Mono", indigo_font_jetbrainsmono_start, indigo_font_jetbrainsmono_end, 1.32f},
    }};
    return list;
}
constexpr unsigned font_count = 8;
inline const char* font_name(unsigned font) { return typefaces()[font % font_count].name; }
// The typeface in use: set from the settings each frame on the screen thread (ui/paint.h, use_font).
inline std::atomic<uint8_t> font_now {0};

} // namespace melodee::ui
