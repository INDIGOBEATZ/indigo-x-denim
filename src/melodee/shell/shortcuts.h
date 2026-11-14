// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 Kerem Kilic (Ellic Studio)
#pragma once
// INDIGO X DENIM LAB: custom Shift shortcuts (Settings > SHORTCUTS): each Shift + pad may do one of these instead of
// its printed job. 0 is "as printed".
#include <cstdint>

namespace melodee {
class Context;

constexpr unsigned shortcut_count = 19;
const char* shortcut_name(unsigned id);           // "Tuner", "Quick save"; 0 "As printed"
// Shift + pad's custom job, when one is set; false: none, the printed job runs.
bool run_shortcut(Context& c, unsigned pad);
} // namespace melodee
