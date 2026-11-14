// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 Kerem Kilic (Ellic Studio)
// INDIGO X DENIM LAB: Reference A/B: a finished song heard in place of the mix, to compare by ear. The song is read
// on the service thread (SampleLibrary::reference) and handed to the engine host as an offer to reference_slot.
#include "melodee/shell/context.h"
#include "melodee/engine/engine_host.h"
#include "melodee/shell/sample_library.h"
#include <cmath>

namespace melodee {

void Context::reference_pick() {
    if (!library) { notice("No card"); return; }
    open(PageId::Samples);
    ui.ref_pick = true;
    notice("Pick a reference song");
}

void Context::reference_load(const std::string& path) {
    ui.ref_pick = false;
    if (!library) return;
    library->reference(path);
    notice("Reference: loading");
    back();
}

void Context::reference_toggle() {
    if (!host.ref_loaded.load(std::memory_order_relaxed)) { reference_pick(); return; }
    bool on = !host.ref_on.load(std::memory_order_relaxed);
    host.ref_on.store(on, std::memory_order_relaxed);
    notice(on ? "B: reference" : "A: your mix");
}

// The reference's level put where the mix's integrated loudness is (as last measured while the mix played).
void Context::reference_match() {
    if (!library || !host.ref_loaded.load(std::memory_order_relaxed)) { notice("No reference"); return; }
    float mix = host.loudness().integrated(), ref = library->reference_lufs();
    if (!std::isfinite(mix) || host.ref_on.load(std::memory_order_relaxed)) { notice("Play your mix first (A)"); return; }
    if (!std::isfinite(ref)) { notice("Reference is silent"); return; }
    float db = std::clamp(mix - ref, -30.0f, 12.0f);
    host.ref_gain.store(std::pow(10.0f, db / 20), std::memory_order_relaxed);
    char text[32];
    std::snprintf(text, sizeof text, "Matched %+.1f dB", double(db));
    notice(text);
}

} // namespace melodee
