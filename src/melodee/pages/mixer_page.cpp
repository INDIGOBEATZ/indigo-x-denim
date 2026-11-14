// SPDX-License-Identifier: AGPL-3.0-only
// INDIGO X DENIM: the MIXER, MUTE's page (shell/mute_mode.cpp: a tap opens it on PADS, the next on TRACKS, the next
// goes back). Eight strips at a time, each a name on top (lit for the strip in hand), its level as a tall bar in its
// colour, M and S under it, and FX where its track has effects. PADS: the kit's pads (the bank in view), TRACKS: the
// sixteen tracks; B1 or B2 again shows the other eight. A pad picks a strip (on PADS it also plays), as do the wheel
// and Minus / Plus; K1 its level, K2 its pan, K3 the pad's tune or the track's send to bus A. Push mutes the strip in
// hand, Shift + Push solos it; B3 FX opens the effects (the pad's own chain on PADS, the track's on TRACKS),
// Shift + B3 unmutes everything. Each fader is a VU meter: it moves with what the strip really plays (a track's peak
// and a pad's own voices' output, as the engine measures them), on the same dB steps as the LED meters right of the
// screen (shell/meter_lights.h), dark to light in its colour; it rises at once and falls smoothly as a meter's needle
// does, a thin line holding the last peak a moment. The cap across it is the level it is set to.
#include "melodee/pages/common.h"
#include "melodee/pages/pages.h"
#include "melodee/shell/explode.h"
#include "melodee/shell/meter_lights.h"
#include "melodee/shell/mute_mode.h"
#include "melodee/ui/widgets.h"
#include <algorithm>
#include <cmath>

namespace melodee {
namespace {
using namespace ui;
constexpr unsigned strips = 8;

bool pads_view(const Snapshot& s) { return s.ui.mixer_view == 0 && s.track().engine == EngineKind::Sampler; }

struct Strip {
    juce::String name;
    juce::Colour colour;
    float level = 0; // 0..1
    float meter = 0; // linear peak, 1 = full scale
    float shown = -120, peak = -120; // dB, as the needle stands
    bool used = false, muted = false, soloed = false, fx = false, current = false;
};

// A level in dBFS as a height up the fader, 0..1: through the side meters' LED steps, each a fifth of the way up
// (-36 lights the first: a fifth; -7 the red: the top fifth), so the fader and the LEDs read alike.
float meter_height(float db) {
    constexpr std::array<float, 7> at_db {-60, meter_mix_db[0], meter_mix_db[1], meter_mix_db[2], meter_mix_db[3], meter_mix_db[4], 0};
    constexpr std::array<float, 7> up {0, 0.2f, 0.4f, 0.6f, 0.8f, 0.9f, 1.0f};
    if (db <= at_db[0]) return 0;
    for (size_t n = 1; n < at_db.size(); ++n)
        if (db <= at_db[n]) return up[n - 1] + (up[n] - up[n - 1]) * (db - at_db[n - 1]) / (at_db[n] - at_db[n - 1]);
    return 1;
}

// The needle's ballistics, screen thread only: each strip's shown level (dB) rises at once and falls 26 dB a second;
// its peak holds 0.9 s, then falls 14 dB a second. A pad's and a track's kept apart; a new track starts its pads afresh.
struct Needle { float db = -120, peak = -120; uint32_t peak_at = 0; };
std::array<Needle, sampler_pads + track_count + bus_count + 1> needles; // LAB: the buses' and the main out's after
unsigned needles_track = ~0u;
uint32_t needles_at = 0;

Needle& needle(const Snapshot& s, unsigned key, float meter, bool live) {
    auto& n = needles[key % needles.size()];
    float db = meter > 0.00001f ? 20 * std::log10(meter) : -120.0f;
    if (!live) { n.db = db; n.peak = db; return n; } // a still picture (the previews): as measured
    float dt = std::clamp((s.now - needles_at) / 1000.0f, 0.0f, 0.1f);
    n.db = db >= n.db ? db : std::max(db, n.db - 26 * dt);
    if (db >= n.peak) { n.peak = db; n.peak_at = s.now; }
    else if (s.now - n.peak_at > 900) n.peak = std::max(n.db, n.peak - 14 * dt);
    return n;
}

bool chain_fx(const Project& project, unsigned n) {
    const auto& chain = project.chain(n);
    for (unsigned n = 0; n < chain.size; ++n) if (chain.slots[n].kind != 0) return true;
    return false;
}

// LAB: BUSES (TRACKS' fourth press): bus A-D and the main out as VU faders, as the tracks', each bus's return as
// its cap; what goes in (tracks sending or routed to it) under it.
void draw_buses(Paint& p, const Snapshot& s) {
    const unsigned count = bus_count + 1, at = s.ui.mixer_bus % count;
    auto c = colour::text;
    Header h = header_of(s);
    auto which = pill("BUSES", c);
    which.spare = true;
    h.left.push_back(which);
    h.right.push_back(tag_of(s, s.ui.track));
    SoftKeys keys {{"PADS", "TRACKS", "FX"}, 1, c};
    keys.dots[1] = 4; keys.dot_at[1] = 3;
    if (s.ui.shift) keys = SoftKeys {{"", "", "UNMUTE ALL"}, -1, c};
    frame(p, s, keys, h);
    static const juce::Colour bus_colours[] {juce::Colour(0xff4fc3f7), juce::Colour(0xffffb74d), juce::Colour(0xffba68c8), juce::Colour(0xff81c784)};
    const bool live = s.now != 0;
    if (live && s.now - needles_at > 500) for (auto& n : needles) n = Needle {};
    const float x0 = zone::pad, gap = 6, w = (zone::width - 2 * zone::pad - gap * (count - 1)) / float(count);
    const float top = zone::picture_y + 2, label_h = 15, fader_top = top + label_h + 4, fader_h = 92;
    const float info_y = fader_top + fader_h + 4;
    for (unsigned n = 0; n < count; ++n) {
        bool main = n == bus_count, current = n == at;
        juce::Colour col = main ? colour::text : bus_colours[n];
        juce::String name = main ? juce::String("MAIN") : "BUS " + juce::String::charToString(juce::juce_wchar('A' + n));
        float level = main ? 1.0f : s.project.buses[n].level / 100.0f;
        auto& nd = needle(s, sampler_pads + track_count + n, s.bus_meters[n], live);
        float x = x0 + n * (w + gap);
        if (current) p.fill(x - 1, top - 1, w + 2, info_y + 26 - top, colour::raised, 4);
        p.fill(x, top, w, label_h, current ? col : colour::surf, 3);
        p.text(name, x, top, w, label_h, p.fit(name, caps_text(current ? colour::bg : colour::text), w - 2, type::floor), juce::Justification::centred);
        float bw = w - 12, bx = x + 6;
        p.fill(bx, fader_top, bw, fader_h, colour::quiet, 3);
        for (int tick = 1; tick < 5; ++tick) p.fill(bx, fader_top + fader_h * (1 - tick / 5.0f), bw, 0.5f, colour::bg.withAlpha(0.6f));
        float up = meter_height(nd.db) * fader_h;
        if (up > 0.5f) {
            juce::ColourGradient vu(col.darker(0.8f), bx, fader_top + fader_h, col.brighter(0.5f), bx, fader_top, false);
            vu.addColour(0.55, col);
            p.g.setGradientFill(vu);
            p.g.fillRoundedRectangle(bx, fader_top + fader_h - up, bw, up, std::min(3.0f, up / 2));
        }
        float held = meter_height(nd.peak) * fader_h;
        if (held > 1.5f && held > up + 1) p.fill(bx + 1, fader_top + fader_h - held, bw - 2, 1.5f, col.brighter(0.6f).withAlpha(0.9f), 0.75f);
        if (!main) {
            float cap = fader_top + fader_h * (1 - std::clamp(level, 0.0f, 1.0f));
            p.fill(bx - 2, std::clamp(cap - 1.5f, fader_top, fader_top + fader_h - 3), bw + 4, 3, current ? colour::text : colour::sec, 1.5f);
            unsigned in = 0;
            for (const auto& t : s.project.tracks) in += t.used() && (t.mix.out == n + 1 || t.mix.sends[n] > 0);
            p.text(juce::String(s.project.buses[n].level) + " %", x, info_y, w, 12, caps_text(current ? colour::text : colour::mid), juce::Justification::centred);
            p.text(juce::String(in) + " IN", x, info_y + 13, w, 11, caps_text(in ? col : colour::dim), juce::Justification::centred);
            if (chain_fx(s.project, bus_chain(n))) p.text("FX", x, fader_top + fader_h - 12, w, 11, caps_text(colour::bg), juce::Justification::centred);
        } else {
            float db = nd.db > -60 ? nd.db : -60;
            p.text(nd.db > -60 ? juce::String(db, 1) : juce::String("-inf"), x, info_y, w, 12, caps_text(current ? colour::text : colour::mid), juce::Justification::centred);
            p.text("DB", x, info_y + 13, w, 11, caps_text(colour::dim), juce::Justification::centred);
        }
    }
    if (live) needles_at = s.now;
}

void draw(Paint& p, const Snapshot& s) {
    if (s.ui.mixer_view == 2) { draw_buses(p, s); return; } // LAB
    const auto& track = s.track();
    auto c = colour_of(s);
    bool pads = pads_view(s);
    // LAB: TRACKS shows all sixteen at once as its third press (narrow strips: numbers and coloured M / S, no words)
    const bool compact = !pads && s.ui.mixer_view == 1 && s.ui.mixer_half == 2;
    unsigned half = compact ? 0 : s.ui.mixer_half % 2;
    const unsigned count = compact ? track_count : strips;
    Header h = header_of(s);
    auto which = pill(pads ? "PADS " + juce::String(half ? "9-16" : "1-8") : compact ? juce::String("TRACKS ALL") : "TRACKS " + juce::String(half ? "9-16" : "1-8"), c);
    which.spare = true; // the soft keys' dots say it too
    h.left.push_back(which);
    { // LAB: where the strip in hand goes, when not the usual way (Shift + K1 sets it)
        static const char* const buses[] {"", "BUS A", "BUS B", "BUS C", "BUS D"};
        const auto& tr = s.track();
        unsigned out = pads ? (tr.engine == EngineKind::Sampler ? tr.pads[tr.last_pad % sampler_pads].out : 0u) : tr.mix.out;
        if (out >= 1 && out <= 4) h.left.push_back(pill(juce::String(pads ? "PAD OUT " : "OUT ") + buses[out], colour::amber));
    }
    h.right.push_back(tag_of(s, s.ui.track));
    SoftKeys keys {{"PADS", "TRACKS", "FX"}, pads ? 0 : 1, c};
    keys.dots[0] = 2; keys.dot_at[0] = uint8_t(pads ? half : 0);
    keys.dots[1] = 3; keys.dot_at[1] = uint8_t(pads ? 0 : compact ? 2 : half);
    if (s.ui.shift) keys = SoftKeys {{pads ? "FLATTEN" : "SUB LENS", pads ? "EXPLODE" : "TUNER", "UNMUTE ALL"}, -1, c}; // LAB: Flatten Pad, Explode Pads to Tracks on PADS
    frame(p, s, keys, h);

    std::array<Strip, track_count> list {};
    unsigned bank = track.bank % sampler_banks;
    for (unsigned n = 0; n < count; ++n) {
        unsigned index = half * strips + n;
        auto& st = list[n];
        if (pads) {
            unsigned kit = bank * 16 + index;
            const auto& pad = track.pads[kit];
            char label[4]; pad_label(kit, label);
            st.name = label; st.colour = c; st.used = pad.used();
            st.level = pad.level / 127.0f; st.muted = track.pad_muted(kit); st.soloed = pad.used() && pad_soloed(track, kit);
            st.current = kit == unsigned(track.last_pad % sampler_pads);
            st.fx = chain_fx(s.project, pad_chain(s.ui.track % track_count, kit));
            st.meter = s.pad_meters[kit]; // its voices' output as the engine measures it
        } else {
            const auto& t = s.project.tracks[index];
            st.name = (compact ? "" : "T") + juce::String(int(index + 1)); st.colour = track_colour(index); st.used = t.used();
            st.level = t.mix.level / 127.0f; st.muted = t.mix.mute; st.soloed = t.mix.solo; st.fx = chain_fx(s.project, index); st.meter = s.meters[index];
            st.current = index == unsigned(s.ui.track % track_count);
        }
    }
    { // the needles move on from where they stood
        const bool live = s.now != 0;
        if (needles_track != s.ui.track % track_count) { for (unsigned k = 0; k < sampler_pads; ++k) needles[k] = Needle {}; needles_track = s.ui.track % track_count; }
        if (live && s.now - needles_at > 500) for (auto& n : needles) n = Needle {}; // back on the page after a while: as it is now
        for (unsigned n = 0; n < count; ++n) {
            unsigned index = half * strips + n;
            unsigned key = pads ? bank * 16 + index : sampler_pads + index;
            auto& nd = needle(s, key, list[n].used && !list[n].muted ? list[n].meter : 0.0f, live);
            list[n].shown = nd.db; list[n].peak = nd.peak;
        }
        if (live) needles_at = s.now;
    }
    if (s.ui.mixer_view == 0 && !pads) { // PADS on a track that is not a kit
        icon(p, Icon::Pads, zone::width / 2.0f - 14, zone::picture_y + 40, 28, colour::dim, 1.4f);
        p.text("NOT A KIT", 0, zone::picture_y + 76, zone::width, 14, caps_text(colour::dim), juce::Justification::centred);
        return;
    }
    const float x0 = zone::pad, gap = compact ? 2 : 3, w = (zone::width - 2 * zone::pad - gap * (count - 1)) / float(count);
    const float top = zone::picture_y + 2, label_h = 15, fader_top = top + label_h + 4, fader_h = 92;
    const float ms_y = fader_top + fader_h + 4, ms_h = 14, fx_y = ms_y + ms_h + 2;
    for (unsigned n = 0; n < count; ++n) {
        const auto& st = list[n];
        float x = x0 + n * (w + gap);
        if (st.current) p.fill(x - 1, top - 1, w + 2, fx_y + 11 - top, colour::raised, 4); // the strip in hand
        // Its name: filled in its colour while in hand.
        auto name_style = caps_text(st.current ? colour::bg : st.used ? colour::text : colour::dim);
        p.fill(x, top, w, label_h, st.current ? st.colour : colour::surf, 3);
        p.text(st.name, x, top, w, label_h, p.fit(st.name, name_style, w - 2, type::floor), juce::Justification::centred);
        // The fader is the VU meter: a bar as tall as what the strip plays now, on the LED meters' steps (the faint
        // lines are where each LED lights), its colour from dark at the foot to light at the top as a VU meter goes
        // from green to red, and the peak held a moment as a thin light line. Muted: no bar, the cap dim.
        float bw = compact ? w - 4 : w - 8, bx = compact ? x + 2 : x + 4;
        p.fill(bx, fader_top, bw, fader_h, colour::quiet, 3);
        for (int tick = 1; tick < 5; ++tick) p.fill(bx, fader_top + fader_h * (1 - tick / 5.0f), bw, 0.5f, colour::bg.withAlpha(0.6f));
        if (st.used && !st.muted) {
            float up = meter_height(st.shown) * fader_h;
            if (up > 0.5f) {
                juce::ColourGradient vu(st.colour.darker(0.8f), bx, fader_top + fader_h, st.colour.brighter(0.5f), bx, fader_top, false);
                vu.addColour(0.55, st.colour);
                p.g.setGradientFill(vu);
                p.g.fillRoundedRectangle(bx, fader_top + fader_h - up, bw, up, std::min(3.0f, up / 2));
            }
            float held = meter_height(st.peak) * fader_h;
            if (held > 1.5f && held > up + 1) p.fill(bx + 1, fader_top + fader_h - held, bw - 2, 1.5f, st.colour.brighter(0.6f).withAlpha(0.9f), 0.75f);
        }
        if (st.used) { // the cap: the level it is set to (100, unity, as high as -12 dB up the meter)
            float cap = fader_top + fader_h * (1 - std::clamp(st.level, 0.0f, 1.0f));
            auto ink = st.muted ? colour::dim : st.current ? colour::text : colour::sec;
            p.fill(bx - 2, std::clamp(cap - 1.5f, fader_top, fader_top + fader_h - 3), bw + 4, 3, ink, 1.5f);
        }
        if (compact) { // M and S as two coloured bars, FX a dot
            float mw2 = (w - 2) / 2;
            p.fill(x, ms_y, mw2, ms_h, st.muted ? colour::rec : colour::surf, 2);
            p.fill(x + mw2 + 2, ms_y, mw2, ms_h, st.soloed ? colour::text : colour::surf, 2);
            if (st.fx) p.circle(x + w / 2, fx_y + 5, 2, st.colour);
            continue;
        }
        // M and S.
        float mw = (w - 2) / 2;
        auto box = [&](float bx2, const char* letter, bool on, juce::Colour onc) {
            p.fill(bx2, ms_y, mw, ms_h, on ? onc : colour::surf, 2);
            p.text(letter, bx2, ms_y, mw, ms_h, caps_text(on ? colour::bg : st.used ? colour::mid : colour::dim), juce::Justification::centred);
        };
        box(x, "M", st.muted, colour::rec);
        box(x + mw + 2, "S", st.soloed, colour::text);
        if (st.fx) p.text("FX", x, fx_y, w, 11, caps_text(st.colour), juce::Justification::centred);
    }
}

// The strip in hand moves by `way` (the wheel, Minus / Plus); the eight in view follow it.
void step(Context& c, int way) {
    if (c.ui.mixer_view == 2) { c.ui.mixer_bus = uint8_t(std::clamp(int(c.ui.mixer_bus) + way, 0, int(bus_count))); return; } // LAB: BUSES
    auto& t = c.track();
    if (c.ui.mixer_view == 0 && t.engine == EngineKind::Sampler) {
        unsigned bank = t.bank % sampler_banks, in_bank = t.last_pad % 16;
        int at = std::clamp(int(in_bank) + way, 0, 15);
        if (c.ui.shift && at != int(in_bank)) { // LAB: Reorder: the pad in hand moves along, its neighbour taking its place
            c.ui.shift_used = true;
            swap_pads(c, t.last_pad, bank * 16 + unsigned(at));
            c.track().last_pad = uint8_t(bank * 16 + unsigned(at));
            c.ui.mixer_half = uint8_t(at / int(strips));
            c.notice(std::string("Pad to ") + char('A' + bank) + (at < 9 ? "0" : "") + std::to_string(at + 1));
            return;
        }
        t.last_pad = uint8_t(bank * 16 + unsigned(at));
        c.ui.mixer_half = uint8_t(at / int(strips));
        return;
    }
    int at = std::clamp(int(c.ui.track % track_count) + way, 0, int(track_count) - 1);
    c.select_track(unsigned(at));
    if (c.ui.mixer_half != 2) c.ui.mixer_half = uint8_t(at / int(strips)); // LAB: all sixteen stay
}

void pad(Context& c, unsigned pad, uint8_t velocity, bool down) {
    auto& t = c.track();
    if (c.ui.mixer_view == 2) { if (down && pad % 16 <= bus_count) c.ui.mixer_bus = uint8_t(pad % 16); return; } // LAB: BUSES: pads 1-5 pick
    if (c.ui.mixer_view == 0 && t.engine == EngineKind::Sampler) { // PADS: it plays, and is the strip in hand
        c.play_pad(pad, velocity, down);
        if (down) { t.last_pad = uint8_t(t.bank % sampler_banks * 16 + pad % 16); c.ui.mixer_half = uint8_t(pad % 16 / strips); }
        return;
    }
    if (!down) return; // TRACKS: a pad picks its track
    c.select_track(pad % track_count);
    if (c.ui.mixer_half != 2) c.ui.mixer_half = uint8_t(pad % 16 / strips);
}

void push(Context& c) { // the strip in hand muted, or not
    if (c.ui.mixer_view == 2) return; // LAB: BUSES
    auto& t = c.track();
    if (c.ui.mixer_view == 0 && t.engine == EngineKind::Sampler) {
        unsigned kit = t.last_pad % sampler_pads;
        if (!t.pads[kit].used()) { c.notice("Empty pad"); return; }
        set_pad_mute(c, c.ui.track, kit, !t.pad_muted(kit));
        return;
    }
    c.set_mute(c.ui.track, !t.mix.mute);
}

void soft(Context& c, int key) {
    auto& ui = c.ui;
    if (ui.shift) { // UNMUTE ALL; LAB: PADS FLATTEN, EXPLODE; TRACKS SUB LENS, TUNER
        ui.shift_used = true;
        if (key == 2) unmute_all(c);
        else if (ui.mixer_view == 2) return;
        else if (ui.mixer_view == 0) { if (key == 0) c.flatten_start(); else explode_pads(c); }
        else c.open(key == 0 ? PageId::SubLens : PageId::Tuner);
        return;
    }
    if (key == 0) { // PADS, again the other eight
        if (ui.mixer_view == 0) ui.mixer_half = uint8_t((ui.mixer_half + 1) % 2);
        else { ui.mixer_view = 0; ui.mixer_half = uint8_t(c.track().last_pad % 16 / strips); }
        if (c.track().engine != EngineKind::Sampler) c.notice("Not a kit");
        return;
    }
    if (key == 1) { // TRACKS, again the other eight
        if (ui.mixer_view == 1 && ui.mixer_half < 2) ui.mixer_half = uint8_t(ui.mixer_half + 1); // LAB: 1-8, 9-16, all sixteen,
        else if (ui.mixer_view == 1) ui.mixer_view = 2;                                          // then the buses
        else { ui.mixer_view = 1; ui.mixer_half = uint8_t(ui.track % track_count / strips); }
        return;
    }
    if (ui.mixer_view == 2) { // LAB: the bus's chain, or the master's
        if (ui.mixer_bus < bus_count) { ui.sends_bus = ui.mixer_bus; c.open(PageId::Bus); } else c.open(PageId::Master);
        return;
    }
    // FX: the pad's own chain on PADS (the pad in hand), the track's on TRACKS
    c.open(ui.mixer_view == 0 && c.track().engine == EngineKind::Sampler ? PageId::PadFx : PageId::Rack);
}
} // namespace

// Shift + Push on the mixer: the strip in hand soloed, or not (shell/shell.cpp).
void mixer_solo(Context& c) {
    auto& t = c.track();
    if (c.ui.mixer_view == 0 && t.engine == EngineKind::Sampler) {
        unsigned kit = t.last_pad % sampler_pads;
        if (!t.pads[kit].used()) { c.notice("Empty pad"); return; }
        bool alone = pad_soloed(t, kit);
        for (unsigned n = 0; n < sampler_pads; ++n)
            if (t.pads[n].used()) set_pad_mute(c, c.ui.track, n, alone ? false : n != kit);
        return;
    }
    c.set_solo(c.ui.track, !t.mix.solo);
}

const PageSpec mixer_page {draw, pad, step, push, soft, step};

} // namespace melodee
