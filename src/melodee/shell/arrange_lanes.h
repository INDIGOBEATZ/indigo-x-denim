// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 Kerem Kilic (Ellic Studio)
#pragma once
#include "melodee/shell/snapshot.h"
#include <functional>

namespace melodee {
struct PatternBank;
// INDIGO X DENIM LAB: ARRANGE's lanes (Snapshot::arrange) from the tracks' playing patterns, the view in out.ui;
// position_of(t): where track t is in its pattern (ticks, -1 none).
void fill_arrange_lanes(Snapshot& out, const PatternBank& patterns, const std::function<int(unsigned)>& position_of);
} // namespace melodee
