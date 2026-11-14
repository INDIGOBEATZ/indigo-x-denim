// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 Kerem Kilic (Ellic Studio)
#include "melodee/shell/mute_mode.h"
#include "melodee/model/pad_params.h"
#include "melodee/shell/bindings.h"
#include "melodee/shell/looper_jobs.h"
#include "melodee/shell/context.h"

namespace melodee {

// INDIGO X DENIM: MUTE is the mixer's button, never mute mode (no pad mutes by accident). A tap opens the mixer on
// PADS (TRACKS on a track that is not a kit), the next shows TRACKS, the next goes back to the page it came from.
// Shift + MUTE: ALL on a kit (the pad pages' knobs set every used pad), else everything unmuted.
void mute_button(Context& c, bool pressed) {
    auto& ui = c.ui;
    ui.mute = false; ui.mute_latch = false;
    if (!pressed) return;
    if (ui.shift) {
        ui.shift_used = true;
        if (c.track().engine == EngineKind::Sampler) {
            ui.all_pads = !ui.all_pads;
            c.notice(ui.all_pads ? "ALL pads: on" : "ALL pads: off");
        } else unmute_all(c);
        return;
    }
    bool kit = c.track().engine == EngineKind::Sampler;
    if (ui.page != PageId::Mixer) {
        ui.mute_from = ui.page; ui.mute_opened = true;
        ui.mixer_view = kit ? 0 : 1;
        ui.mixer_half = uint8_t(kit ? c.track().last_pad % 16 / 8 : ui.track % track_count / 8);
        c.open(PageId::Mixer);
    } else if (ui.mixer_view == 0) {
        ui.mixer_view = 1; ui.mixer_half = uint8_t(ui.track % track_count / 8);
    } else {
        ui.mute_opened = false;
        c.open(ui.mute_from == PageId::Mixer ? PageId::Stage : ui.mute_from);
    }
}

void set_pad_mute(Context& c, unsigned t, unsigned kit, bool on) {
    t %= track_count; kit %= sampler_pads;
    auto& track = c.project.tracks[t];
    if (track.pad_muted(kit) == on) return;
    track.set_pad_muted(kit, on);
    c.send({Command::Type::PadMute, uint8_t(t), 0, uint16_t(kit), int16_t(on)});
    // INDIGO X DENIM LAB: in a take, the mute is recorded too (a point on the pad's Mute lane), and plays back so
    if (t == c.ui.track % track_count) c.record_motion(motion_lane(motion_pad_param(PadParam::Mute), kit), on ? 1 : 0);
}

void mute_pad(Context& c, unsigned pad, bool kit_pads) {
    if (c.ui.page == PageId::Looper) { loop_mute_pad(c, pad); return; } // the loop tracks, a column each
    auto& ui = c.ui;
    ui.mute_used = true;
    pad %= 16;
    if (!mute_on_kit(ui, kit_pads)) { // the tracks
        const auto& mix = c.project.tracks[pad].mix;
        if (ui.shift) { ui.shift_used = true; c.set_solo(pad, !mix.solo); }
        else c.set_mute(pad, !mix.mute);
        return;
    }
    auto& t = c.track();
    unsigned kit = t.bank % sampler_banks * 16 + pad;
    if (!t.pads[kit].used()) { c.notice(pad_text("Empty", kit)); return; }
    if (!ui.shift) { set_pad_mute(c, ui.track, kit, !t.pad_muted(kit)); return; }
    // Shift: solo the pad (every other used pad muted); on the soloed pad, the kit back.
    ui.shift_used = true;
    bool alone = pad_soloed(t, kit);
    for (unsigned n = 0; n < sampler_pads; ++n)
        if (t.pads[n].used()) set_pad_mute(c, ui.track, n, alone ? false : n != kit);
    c.notice(alone ? std::string("Solo off") : pad_text("Solo", kit));
}

void unmute_all(Context& c) {
    for (unsigned t = 0; t < track_count; ++t) {
        auto& track = c.project.tracks[t];
        if (track.mix.mute) c.set_mute(t, false);
        if (track.mix.solo) c.set_solo(t, false);
        if (track.any_pad_muted()) {
            track.pad_mutes = {};
            c.send({Command::Type::PadMute, uint8_t(t), 0, 0xffff, 0});
        }
    }
    c.notice("Unmute all");
}

} // namespace melodee
