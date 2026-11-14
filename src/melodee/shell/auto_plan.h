// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 Kerem Kilic (Ellic Studio)
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace melodee {

// INDIGO X DENIM LAB: the autosampler's plan: which notes were played into one take, when (ms from its start) and
// for how long, so the service thread can cut it into a file a note and an SFZ instrument over them.
struct AutoPlan {
    std::string name;
    std::vector<uint8_t> notes;
    uint32_t first_ms = 500, slot_ms = 3000, hold_ms = 2000;
    uint8_t velocity = 100;
};

} // namespace melodee
