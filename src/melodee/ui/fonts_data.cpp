// SPDX-License-Identifier: AGPL-3.0-only
// INDIGO X DENIM: the typefaces beside Rubik (ui/fonts.h), each under the SIL Open Font License 1.1 (its OFL.txt
// beside it in build/deps/fonts, THIRD_PARTY.md), embedded so the binary carries them to the MPC and to previews.
// src/melodee/dependencies.py fetches the pinned files; the assembler reads them from the build folder.
#include "melodee/ui/fonts.h"

#define INDIGO_FONT(name, file)            \
    "    .balign 16\n"                     \
    "    .global indigo_font_" name "_start\n" \
    "indigo_font_" name "_start:\n"        \
    "    .incbin \"build/deps/fonts/" file "\"\n" \
    "    .global indigo_font_" name "_end\n"   \
    "indigo_font_" name "_end:\n"

asm(".section .rodata\n"
    INDIGO_FONT("inter", "Inter.ttf")
    INDIGO_FONT("spacegrotesk", "SpaceGrotesk.ttf")
    INDIGO_FONT("outfit", "Outfit.ttf")
    INDIGO_FONT("sora", "Sora.ttf")
    INDIGO_FONT("manrope", "Manrope.ttf")
    INDIGO_FONT("lexend", "Lexend.ttf")
    INDIGO_FONT("jetbrainsmono", "JetBrainsMono.ttf")
    "    .previous\n");
