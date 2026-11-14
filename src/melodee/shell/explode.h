// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 Kerem Kilic (Ellic Studio)
// INDIGO X DENIM LAB: Explode Pads to Tracks (shell/explode.cpp).
#pragma once

namespace melodee {

class Context;

// Each used pad of the selected kit onto an empty track of its own (its pad 1), notes, effects and all.
void explode_pads(Context& c);
// INDIGO X DENIM LAB: two pads of the selected kit trade places, notes, effects and mutes with them.
void swap_pads(Context& c, unsigned a, unsigned b);

} // namespace melodee
