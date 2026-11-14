// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 Kerem Kilic (Ellic Studio)
// INDIGO X DENIM LAB: Reference A/B: a finished song heard in place of the mix, to compare by ear. The song is read
// on the service thread (SampleLibrary::reference) and handed to the engine host as an offer to reference_slot.
#include "melodee/shell/context.h"
#include "melodee/engine/engine_host.h"
#include "melodee/engine/dsp.h"
#include <algorithm>
#include "melodee/shell/sample_library.h"
#include "melodee/engine/engine_info.h"
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

namespace melodee {

// INDIGO X DENIM LAB: a voice memo: up to three minutes of the input, in stereo. The buffer is made here (the
// MIDI thread), filled by the audio thread, and handed to the service thread to write a moment after it stopped, so
// the audio thread is surely done with it.
void Context::voice_memo() {
    if (memo_stopped_at) return; // the last one is still being handed over
    if (ui.memo_on) { memo_end(now); ui.memo_on = false; return; }
    if (ui.auto_running) { notice("Autosampler running"); return; }
    if (!library) { notice("Memo: no card"); return; }
    unsigned rate = unsigned(dsp::sample_rate());
    if (!memo_begin(size_t(rate) * 180)) return; // three minutes (stereo floats: about 70 MB at 48 kHz)
    ui.memo_on = true; ui.memo_at = now;
    notice("Memo: recording");
}

// The take's buffer made and handed to the audio thread (false: one is running or being handed over).
bool Context::memo_begin(size_t frames) {
    if (memo_stopped_at || host.memo_on.load(std::memory_order_relaxed)) return false;
    memo_store = std::make_unique<std::vector<float>>(frames * 2);
    host.memo_frames.store(0, std::memory_order_relaxed);
    host.memo_capacity.store(uint32_t(frames), std::memory_order_relaxed);
    host.memo_buffer.store(memo_store->data(), std::memory_order_release);
    host.memo_on.store(true, std::memory_order_release);
    return true;
}

void Context::memo_end(uint32_t t) {
    host.memo_on.store(false, std::memory_order_release);
    host.memo_buffer.store(nullptr, std::memory_order_release);
    memo_stopped_at = std::max<uint32_t>(t, 1);
}

void Context::memo_tick(uint32_t t) {
    if (ui.memo_on && host.memo_frames.load(std::memory_order_relaxed) >= host.memo_capacity.load(std::memory_order_relaxed)) voice_memo(); // full: stop
    autosample_tick(t);
    if (!memo_stopped_at || t - memo_stopped_at < 150 || !memo_store) return;
    memo_stopped_at = 0;
    uint32_t frames = host.memo_frames.load(std::memory_order_acquire);
    memo_store->resize(std::min<size_t>(size_t(frames) * 2, memo_store->size()));
    if (library && frames) {
        if (memo_for_auto) library->write_autosample(std::move(*memo_store), unsigned(dsp::sample_rate()), auto_plan);
        else library->write_memo(std::move(*memo_store), unsigned(dsp::sample_rate()));
    }
    memo_for_auto = false;
    memo_store.reset();
}

} // namespace melodee

namespace melodee {

// INDIGO X DENIM LAB: the autosampler. The selected track (an instrument, or a MIDI track driving a synth whose
// sound comes back through the input) plays each note of the plan; the input runs into one take the whole time.
// Notes are played from the screen thread's tick (a few ms of jitter): the service thread finds each note's start in
// its slot when it cuts the take.
void Context::autosample_start() {
    if (ui.auto_running) return;
    if (ui.memo_on || memo_stopped_at) { notice("Memo still recording"); return; }
    const auto& t = track();
    if (t.engine == EngineKind::Sampler || !t.used()) { notice("Pick a keys or MIDI track"); return; }
    if (!library) { notice("Autosampler: no card"); return; }
    AutoPlan plan;
    plan.name = "Auto " + std::string(engine_info(t.engine).name);
    unsigned low = std::min(ui.auto_low, ui.auto_high), high = std::max(ui.auto_low, ui.auto_high), step = std::max<unsigned>(ui.auto_step, 1);
    for (unsigned n = low; n <= high && plan.notes.size() < 128; n += step) plan.notes.push_back(uint8_t(n));
    plan.hold_ms = uint32_t(ui.auto_hold) * 100; plan.slot_ms = plan.hold_ms + uint32_t(ui.auto_tail) * 100 + 200;
    plan.velocity = ui.auto_velocity;
    uint32_t total_ms = plan.first_ms + uint32_t(plan.notes.size()) * plan.slot_ms + 500;
    if (total_ms > 240000) { notice("Over 4 min: fewer notes"); return; }
    if (!memo_begin(size_t(uint64_t(total_ms) * unsigned(dsp::sample_rate()) / 1000))) return;
    auto_plan = std::move(plan);
    memo_for_auto = true;
    auto_t0 = now; auto_step_at = 0; auto_track = ui.track;
    ui.auto_running = true; ui.auto_at = 0; ui.auto_count = uint8_t(std::min<size_t>(auto_plan.notes.size(), 255));
    notice("Autosampling " + std::to_string(auto_plan.notes.size()) + " notes");
}

void Context::autosample_stop() {
    if (!ui.auto_running) return;
    if (auto_step_at % 2 == 1 && auto_step_at / 2 < auto_plan.notes.size()) note(auto_track, auto_plan.notes[auto_step_at / 2], 0); // the note held ends
    ui.auto_running = false;
    memo_end(now);
    memo_for_auto = auto_step_at / 2 > 0; // what was taken so far is still cut
    auto_plan.notes.resize(std::min<size_t>(auto_plan.notes.size(), auto_step_at / 2 + (auto_step_at % 2)));
    if (!memo_for_auto) notice("Autosampler stopped");
}

void Context::autosample_tick(uint32_t t) {
    if (!ui.auto_running) return;
    const size_t count = auto_plan.notes.size();
    while (auto_step_at < 2 * count) {
        unsigned k = auto_step_at / 2;
        bool off = auto_step_at % 2;
        uint32_t due = auto_plan.first_ms + k * auto_plan.slot_ms + (off ? auto_plan.hold_ms : 0);
        if (t - auto_t0 < due) break;
        note(auto_track, auto_plan.notes[k], off ? 0 : auto_plan.velocity);
        ++auto_step_at;
        ui.auto_at = uint8_t(std::min<unsigned>(k + 1, 255));
    }
    uint32_t end = auto_plan.first_ms + uint32_t(count) * auto_plan.slot_ms + 300;
    if (auto_step_at >= 2 * count && t - auto_t0 >= end) {
        ui.auto_running = false;
        memo_end(t);
        notice("Autosampler: cutting");
    }
}

} // namespace melodee
