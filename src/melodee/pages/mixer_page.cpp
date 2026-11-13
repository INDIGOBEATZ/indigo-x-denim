// SPDX-License-Identifier: AGPL-3.0-only
// INDIGO X DENIM: the MIXER, MUTE's page (shell/mute_mode.cpp: a tap opens it on PADS, the next on TRACKS, the next
// goes back). Eight strips at a time, each a name on top (lit for the strip in hand), its level as a tall bar in its
// colour, M and S under it, and FX where its track has effects. PADS: the kit's pads (the bank in view), TRACKS: the
// sixteen tracks; B1 or B2 again shows the other eight. A pad picks a strip (on PADS it also plays), as do the wheel
// and Minus / Plus; K1 its level, K2 its pan, K3 the pad's tune or the track's send to bus A. Push mutes the strip in
// hand, Shift + Push solos it; B3 FX opens the track's effects, Shift + B3 unmutes everything.
#include "melodee/pages/common.h"
#include "melodee/pages/pages.h"
#include "melodee/shell/mute_mode.h"
#include "melodee/ui/widgets.h"
#include <algorithm>

namespace melodee {
namespace {
using namespace ui;
constexpr unsigned strips = 8;

bool pads_view(const Snapshot& s) { return s.ui.mixer_view == 0 && s.track().engine == EngineKind::Sampler; }

struct Strip {
    juce::String name;
    juce::Colour colour;
    float level = 0; // 0..1
    bool used = false, muted = false, soloed = false, fx = false, current = false;
};

bool track_fx(const Project& project, unsigned t) {
    const auto& chain = project.chain(t);
    for (unsigned n = 0; n < chain.size; ++n) if (chain.slots[n].kind != 0) return true;
    return false;
}

void draw(Paint& p, const Snapshot& s) {
    const auto& track = s.track();
    auto c = colour_of(s);
    bool pads = pads_view(s);
    unsigned half = s.ui.mixer_half % 2;
    Header h = header_of(s);
    h.left.push_back(pill(pads ? "PADS " + juce::String(half ? "9-16" : "1-8") : "TRACKS " + juce::String(half ? "9-16" : "1-8"), c));
    h.right.push_back(tag_of(s, s.ui.track));
    SoftKeys keys {{"PADS", "TRACKS", "FX"}, pads ? 0 : 1, c};
    keys.dots[0] = 2; keys.dot_at[0] = uint8_t(pads ? half : 0);
    keys.dots[1] = 2; keys.dot_at[1] = uint8_t(pads ? 0 : half);
    if (s.ui.shift) keys = SoftKeys {{"", "", "UNMUTE ALL"}, -1, c};
    frame(p, s, keys, h);

    std::array<Strip, strips> list {};
    unsigned bank = track.bank % sampler_banks;
    for (unsigned n = 0; n < strips; ++n) {
        unsigned index = half * strips + n;
        auto& st = list[n];
        if (pads) {
            unsigned kit = bank * 16 + index;
            const auto& pad = track.pads[kit];
            char label[4]; pad_label(kit, label);
            st.name = label; st.colour = c; st.used = pad.used();
            st.level = pad.level / 127.0f; st.muted = track.pad_muted(kit); st.soloed = pad.used() && pad_soloed(track, kit);
            st.current = kit == unsigned(track.last_pad % sampler_pads);
        } else {
            const auto& t = s.project.tracks[index];
            st.name = "T" + juce::String(int(index + 1)); st.colour = track_colour(index); st.used = t.used();
            st.level = t.mix.level / 127.0f; st.muted = t.mix.mute; st.soloed = t.mix.solo; st.fx = track_fx(s.project, index);
            st.current = index == unsigned(s.ui.track % track_count);
        }
    }
    if (s.ui.mixer_view == 0 && !pads) { // PADS on a track that is not a kit
        icon(p, Icon::Pads, zone::width / 2.0f - 14, zone::picture_y + 40, 28, colour::dim, 1.4f);
        p.text("NOT A KIT", 0, zone::picture_y + 76, zone::width, 14, caps_text(colour::dim), juce::Justification::centred);
        return;
    }
    constexpr float x0 = zone::pad, gap = 3, w = (zone::width - 2 * zone::pad - gap * (strips - 1)) / float(strips);
    const float top = zone::picture_y + 2, label_h = 15, fader_top = top + label_h + 4, fader_h = 92;
    const float ms_y = fader_top + fader_h + 4, ms_h = 14, fx_y = ms_y + ms_h + 2;
    for (unsigned n = 0; n < strips; ++n) {
        const auto& st = list[n];
        float x = x0 + n * (w + gap);
        if (st.current) p.fill(x - 1, top - 1, w + 2, fx_y + 11 - top, colour::raised, 4); // the strip in hand
        // Its name: filled in its colour while in hand.
        auto name_style = caps_text(st.current ? colour::bg : st.used ? colour::text : colour::dim);
        p.fill(x, top, w, label_h, st.current ? st.colour : colour::surf, 3);
        p.text(st.name, x, top, w, label_h, p.fit(st.name, name_style, w - 2, type::floor), juce::Justification::centred);
        // The level: a tall bar from the bottom, dim while muted.
        float bw = w - 10, bx = x + (w - bw) / 2;
        p.fill(bx, fader_top, bw, fader_h, colour::quiet, 3);
        if (st.used) {
            float fill = std::clamp(st.level, 0.0f, 1.0f) * fader_h;
            auto ink = st.muted ? st.colour.withAlpha(0.3f) : st.colour;
            if (fill >= 1) p.fill(bx, fader_top + fader_h - fill, bw, fill, ink, 3);
            p.fill(bx - 2, fader_top + fader_h * (1 - 100.0f / 127.0f), bw + 4, 1, colour::mid.withAlpha(0.6f)); // unity (100)
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
    auto& t = c.track();
    if (c.ui.mixer_view == 0 && t.engine == EngineKind::Sampler) {
        unsigned bank = t.bank % sampler_banks, in_bank = t.last_pad % 16;
        int at = std::clamp(int(in_bank) + way, 0, 15);
        t.last_pad = uint8_t(bank * 16 + unsigned(at));
        c.ui.mixer_half = uint8_t(at / int(strips));
        return;
    }
    int at = std::clamp(int(c.ui.track % track_count) + way, 0, int(track_count) - 1);
    c.select_track(unsigned(at));
    c.ui.mixer_half = uint8_t(at / int(strips));
}

void pad(Context& c, unsigned pad, uint8_t velocity, bool down) {
    auto& t = c.track();
    if (c.ui.mixer_view == 0 && t.engine == EngineKind::Sampler) { // PADS: it plays, and is the strip in hand
        c.play_pad(pad, velocity, down);
        if (down) { t.last_pad = uint8_t(t.bank % sampler_banks * 16 + pad % 16); c.ui.mixer_half = uint8_t(pad % 16 / strips); }
        return;
    }
    if (!down) return; // TRACKS: a pad picks its track
    c.select_track(pad % track_count);
    c.ui.mixer_half = uint8_t(pad % 16 / strips);
}

void push(Context& c) { // the strip in hand muted, or not
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
    if (ui.shift) { ui.shift_used = true; if (key == 2) unmute_all(c); return; }
    if (key == 0) { // PADS, again the other eight
        if (ui.mixer_view == 0) ui.mixer_half = uint8_t((ui.mixer_half + 1) % 2);
        else { ui.mixer_view = 0; ui.mixer_half = uint8_t(c.track().last_pad % 16 / strips); }
        if (c.track().engine != EngineKind::Sampler) c.notice("Not a kit");
        return;
    }
    if (key == 1) { // TRACKS, again the other eight
        if (ui.mixer_view == 1) ui.mixer_half = uint8_t((ui.mixer_half + 1) % 2);
        else { ui.mixer_view = 1; ui.mixer_half = uint8_t(ui.track % track_count / strips); }
        return;
    }
    c.open(PageId::Rack); // FX: the track's effects
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
