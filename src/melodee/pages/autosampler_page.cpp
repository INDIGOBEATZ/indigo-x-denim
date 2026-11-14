// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 Kerem Kilic (Ellic Studio)
// INDIGO X DENIM LAB: the autosampler (after SXL's): the selected track (a synth, or a MIDI track playing an outside
// synth that comes back through the input) played note by note, each kept as a file, with an SFZ instrument over
// them in Samples/Autosampled. The wheel picks a row, Minus / Plus or Push change it; B3 starts and stops.
#include "melodee/engine/engine_info.h"
#include "melodee/model/scales.h"
#include "melodee/pages/common.h"
#include "melodee/pages/pages.h"
#include "melodee/shell/context.h"
#include "melodee/ui/widgets.h"
#include <algorithm>

namespace melodee {
namespace {
using namespace ui;

constexpr uint8_t steps[] {1, 2, 3, 4, 6, 12};
constexpr unsigned rows = 6;

juce::String row_name(unsigned r) {
    static const char* n[] {"Lowest note", "Highest note", "Every", "Hold", "Tail", "Velocity"};
    return n[r % rows];
}
juce::String row_value(const UiState& ui, unsigned r) {
    switch (r) {
    case 0: return juce::String(note_name(ui.auto_low));
    case 1: return juce::String(note_name(ui.auto_high));
    case 2: return ui.auto_step == 12 ? juce::String("Octave") : juce::String(int(ui.auto_step)) + (ui.auto_step == 1 ? " semitone" : " semitones");
    case 3: return juce::String(ui.auto_hold / 10.0, 1) + " s";
    case 4: return juce::String(ui.auto_tail / 10.0, 1) + " s";
    default: return juce::String(int(ui.auto_velocity));
    }
}
unsigned note_count(const UiState& ui) {
    unsigned low = std::min(ui.auto_low, ui.auto_high), high = std::max(ui.auto_low, ui.auto_high);
    return (high - low) / std::max<unsigned>(ui.auto_step, 1) + 1;
}

void draw(Paint& p, const Snapshot& s) {
    const auto& ui = s.ui;
    auto c = colour_of(s);
    Header h = header_of(s);
    h.left.push_back(pill(Icon::Mic, "AUTOSAMPLER"));
    h.right.push_back(tag_of(s, ui.track));
    SoftKeys keys {{"", "", ui.auto_running ? "STOP" : "START"}, -1, c, {KeyIcon::None, KeyIcon::None, ui.auto_running ? KeyIcon::None : KeyIcon::Check}};
    frame(p, s, keys, h);
    float x = zone::pad, w = zone::width - 2 * zone::pad;
    if (ui.auto_running) {
        float top = zone::picture_y + 18;
        juce::String at = juce::String(int(ui.auto_at)) + " / " + juce::String(int(ui.auto_count));
        p.text("SAMPLING", 0, top, zone::width, 16, caps_text(colour::rec), juce::Justification::centred);
        p.text(at, 0, top + 20, zone::width, 30, plain(colour::text, type::value, Weight::SemiBold), juce::Justification::centred);
        float frac = ui.auto_count ? float(ui.auto_at) / ui.auto_count : 0;
        p.fill(x + 20, top + 62, w - 40, 6, colour::line, 3);
        p.fill(x + 20, top + 62, (w - 40) * frac, 6, colour::rec, 3);
        p.text("Keep the source quiet between notes", 0, top + 80, zone::width, 14, plain(colour::mid, type::small), juce::Justification::centred);
        return;
    }
    for (unsigned r = 0; r < rows; ++r) {
        float y = zone::picture_y + 1 + r * (list_row_h + list_row_gap);
        bool here = r == ui.auto_row % rows;
        if (here) { p.fill(x, y, w, list_row_h, colour::raised, 3); p.fill(x, y, 2.5f, list_row_h, c, 1); }
        p.text(row_name(r), x + 10, y, w / 2, list_row_h, plain(here ? colour::text : colour::sec, type::list, here ? Weight::Medium : Weight::Regular));
        juce::String value = row_value(ui, r);
        auto style = plain(here ? colour::text : colour::mid, type::body, Weight::Medium);
        float vw = p.width(value, style);
        p.text(value, x + w - 9 - vw, y, vw + 1, list_row_h, style);
    }
    unsigned n = note_count(ui);
    unsigned seconds = (500 + n * (ui.auto_hold * 100 + ui.auto_tail * 100 + 200) + 999) / 1000;
    bool kit = s.track().engine == EngineKind::Sampler;
    juce::String info = kit ? juce::String("Pick a keys or MIDI track")
                            : juce::String(n) + (n == 1 ? " NOTE" : " NOTES") + juce::String::fromUTF8(" \xc2\xb7 ") + juce::String(seconds / 60) + ":" + juce::String(seconds % 60).paddedLeft('0', 2);
    p.text(info, 0, zone::picture_y + rows * (list_row_h + list_row_gap) + 2, zone::width, 14, caps_text(kit ? colour::amber : colour::mid), juce::Justification::centred);
}

void change(Context& c, int way) {
    auto& ui = c.ui;
    if (ui.auto_running) return;
    switch (ui.auto_row % rows) {
    case 0: ui.auto_low = uint8_t(std::clamp(int(ui.auto_low) + way, 0, 127)); break;
    case 1: ui.auto_high = uint8_t(std::clamp(int(ui.auto_high) + way, 0, 127)); break;
    case 2: {
        unsigned i = 0;
        while (i < 5 && steps[i] != ui.auto_step) ++i;
        ui.auto_step = steps[std::clamp(int(i) + way, 0, 5)];
        break;
    }
    case 3: ui.auto_hold = uint8_t(std::clamp(int(ui.auto_hold) + way * 5, 5, 80)); break;
    case 4: ui.auto_tail = uint8_t(std::clamp(int(ui.auto_tail) + way * 5, 0, 40)); break;
    default: ui.auto_velocity = uint8_t(std::clamp(int(ui.auto_velocity) + way * 5, 1, 127)); break;
    }
}
void wheel(Context& c, int delta) {
    if (c.ui.auto_running) return;
    c.ui.auto_row = uint8_t(std::clamp(int(c.ui.auto_row % rows) + delta, 0, int(rows) - 1));
}
void push(Context& c) { change(c, 1); }
void soft(Context& c, int key) {
    if (key != 2) return;
    if (c.ui.auto_running) c.autosample_stop(); else c.autosample_start();
}
} // namespace

const PageSpec autosampler_page {draw, play_pad, wheel, push, soft, change};

} // namespace melodee
