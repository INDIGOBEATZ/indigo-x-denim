// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 Kerem Kilic (Ellic Studio)
#pragma once
#include <algorithm>
#include <array>
#include <bitset>
#include "melodee/model/input_config.h"
#include "melodee/model/midi_config.h"
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <string>
#include "melodee/ui/shades.h"
#include "melodee/ui/fonts.h"

// Melodee's own settings, not the project's: how the pads respond and
// light, whether their pressure is used, how the knobs, Knob FX and the fader take over a value, the screen and where its
// place bar sits, the audio
// device's rate and buffer, and the MPC's recording toggles
// (Shift + Pad 4 Count-In, Shift + Pad 10 Rec Quantize), what Double Seq puts in the new half and Input Config (Shift + Sample). Kept as key=value lines in the
// `settings` file next to the projects (with the project to open at start).
namespace melodee {

// How a knob or the fader takes over a value it does not sit at (the MPC's
// MIDI Config): Pickup waits until it reaches the value, Scaled moves the
// value the way the control goes, scaled so the two meet at the end of its
// travel, Instant jumps to the control (parity phase 8; shell/takeover.h).
enum class Takeover : uint8_t { Pickup, Scaled, Instant, Count };
inline const char* takeover_name(Takeover t) {
    static const char* const names[] {"Pickup", "Scaled", "Instant"};
    return names[unsigned(t) % unsigned(Takeover::Count)];
}

struct Settings {
    enum Row : uint8_t { Palette, PadResponse, PadPressure, PadLights, KnobTakeover, KnobFxTakeover, FaderTakeover, DoubleSeq, CaptureBars, CaptureQuantize,
                         Screen, PlaceBarRow, SampleRate,
                         AudioBuffer, RecallGrabRow, RecallGap, RecallTrimEnd, TakeAutoEnd, AutoEndAfter, SilenceBelow, Font, LevelLeds, StartFolder, ProjectFolder, PreviewLevel, SystemBar, FlattenTail, ShiftMapDelay, ChopNotes, PreviewTempo, SeqFirst, Rows };
    uint8_t pad_response = 1; // 0 soft, 1 medium, 2 hard, 3 full level (127, Shift + Pad 1)
    uint8_t response_before = 1; // what Shift + Pad 1 goes back to
    bool count_in = false;    // a bar before Seq Record records (phase 2)
    bool rec_quantize = true; // recorded notes onto the grid (off: phase 2)
    uint8_t rec_grid = 0;     // and which grid: 0 the pattern's Q, else 1 + a Division, 1/4 .. 1/64 (grid_division(), model/pattern.h)
    bool zero_snap = true;    // Chop's cuts onto zero crossings (Chop's B2; shell/chop.h)
    bool double_copies = true; // Double Seq (Shift + Pad 3): the new half plays the notes again (the MPC's way), or starts empty
    // Capture (Shift + Seq Record; round 2, capture C): how many of the last bars played it keeps (roadmap P5: 0 Auto,
    // the phrase as long as it is), and how it puts them on the grid: 0 as played, 1 onto sixteenths, 2 onto eighths.
    static constexpr std::array<uint8_t, 7> capture_lengths {{0, 1, 2, 4, 8, 16, 32}};
    uint8_t capture_bars = 0, capture_quantise = 1;
    uint8_t click = 0;        // the metronome: 0 off, 1 on, 2 only while Seq Record (Shift + Tap Tempo)
    uint8_t click_level = 40;
    // INDIGO X DENIM, Roll's VIEW sheet: the view turns its pages with the playhead while playing (the cursor stays
    // where it was), and the rows of keys outside the project's scale sink (keys only, not a kit's lanes).
    bool roll_follow = true, roll_scale = false; // follow on as it comes: the playhead stays in view
    // INDIGO X DENIM: the screen's palette (ui/palettes.h: 0 NIGHT, 1 NEON LAB as it comes, then the Media Lab's
    // themes) and its typeface (ui/fonts.h: 0 Rubik as it comes), chosen apart.
    uint8_t palette = uint8_t(ui::default_palette), font = 0;
    // INDIGO X DENIM: the level LEDs beside the screen (main out, or the input while sampling): 0 on, 1 off; and the
    // folder Sample Select opens in ("card:" and a path under the card's top, or a path on the internal drive;
    // empty: the browser as Melodee has it). Set on Samples with Shift + B1, cleared here.
    uint8_t level_leds = 0;
    // INDIGO X DENIM LAB: the browser's preview as loud as this (0..120 %, 100 the pad's unity), and the thin CPU / RAM
    // bar along the screen's foot (pages/overlays.cpp).
    uint8_t preview_level = 80;
    bool system_bar = false;
    uint8_t flatten_tail = 10;    // INDIGO X DENIM LAB: Flatten Pad's tail after the sound, tenths of a second (0..100)
    bool preview_tempo = false;   // INDIGO X DENIM LAB: the browser's preview at the project's tempo, its own read from its name ("92 BPM")
    // INDIGO X DENIM LAB: Settings > SHORTCUTS: what each Shift + pad does instead of the printed job (0: as printed;
    // shell/shortcuts.h names the rest).
    std::array<uint8_t, 16> shortcuts {};
    uint8_t seq_first = 0;        // INDIGO X DENIM LAB: the editor SEQ opens: 0 the one used last (kit: STEPS, keys: ROLL), 1 STEPS, 2 ROLL
    bool chop_notes = false;      // INDIGO X DENIM LAB: Chop's TO PADS also writes the slices as notes (off: the pattern stays empty; the file key is chop_events)
    uint8_t shift_map_delay = 2;  // INDIGO X DENIM LAB: the Shift pads' map after Shift is held: 0 off, 1 at once, 2 0.65 s, 3 1 s, 4 2 s, 5 3 s
    uint32_t shift_map_ms() const { static const uint32_t ms[] {0, 0, 650, 1000, 2000, 3000}; return ms[shift_map_delay % 6]; }
    std::array<char, 160> start_folder {};
    // INDIGO X DENIM: where a new project is saved, a folder under Melodee/Projects ("" its top).
    std::array<char, 160> project_folder {};
    std::string project_folder_text() const { return std::string(project_folder.data(), strnlen(project_folder.data(), project_folder.size())); }
    void set_project_folder(const std::string& path) {
        project_folder.fill(0);
        std::copy_n(path.begin(), std::min(path.size(), project_folder.size() - 1), project_folder.begin());
    }
    std::string start_folder_text() const { return std::string(start_folder.data(), strnlen(start_folder.data(), start_folder.size())); }
    void set_start_folder(const std::string& path) {
        start_folder.fill(0);
        std::copy_n(path.begin(), std::min(path.size(), start_folder.size() - 1), start_folder.begin());
    }
    bool full_level() const { return pad_response == 3; }
    // Shift + Pad 1: full level on, or back to the curve it came from.
    void toggle_full_level() {
        if (full_level()) pad_response = response_before == 3 ? 1 : response_before;
        else { response_before = pad_response; pad_response = 3; }
    }
    bool pressure = true;     // off: the pads send no pressure (synths, Pad Punch depth)
    uint8_t pad_lights = 0;   // 0 track colours, 1 white, 2 off
    // The MPC's defaults: Scaled for the knobs and Knob FX, Pickup for the fader. Before parity
    // phase 8 one row, "Knob pickup" (key pickup): off reads as Instant for all three.
    Takeover knob_takeover = Takeover::Scaled, knob_fx_takeover = Takeover::Scaled, fader_takeover = Takeover::Pickup;
    uint8_t screen = 100;     // brightness, 20..100 %
    bool place_bottom = false; // the place bar (shell/place_bar.h): under the knob cards, else between the soft keys and the header
    // The audio device (App::audio_format()): its rate, and the 128-frame periods queued ahead of the jacks. More
    // periods ride out a slow block (no crackle) and are heard later; a higher rate costs more CPU a second.
    static constexpr std::array<unsigned, 3> rates {{44100, 48000, 96000}};
    static constexpr std::array<uint8_t, 5> buffers {{2, 3, 4, 6, 8}}; // periods: 256, 384, 512, 768, 1024 frames
    uint32_t audio_rate = 44100;
    uint8_t audio_periods = 3;
    InputConfig input;        // Input Config: what Sample Record and Audio Recall hear (parity phase 6)
    SamplingPrefs sampling;   // how Audio Recall cuts the ring, and when a Free take ends on its own
    MidiConfig midi;          // MIDI Config (Shift + Pad 8, parity phase 9)
    // Starred factory sounds, by engine (EngineKind's order), for the Sounds page's FAVOURITES.
    static constexpr unsigned engines = 9, sounds = 256; // a row per EngineKind (MIDI the eighth, PLUGIN the ninth)
    std::array<std::bitset<sounds>, engines> favourites {};
    bool favourite(unsigned engine, unsigned sound) const { return engine < engines && sound < sounds && favourites[engine][sound]; }
    void toggle_favourite(unsigned engine, unsigned sound) { if (engine < engines && sound < sounds) favourites[engine].flip(sound); }

    // MIDI Config's Reset Factory Settings: every setting back to how Melodee comes, the starred sounds kept.
    void reset_factory() {
        auto kept = favourites;
        *this = Settings {};
        favourites = kept;
    }

    static const char* row_name(unsigned row) {
        static const char* names[] {"Palette", "Pad response", "Pad pressure", "Pad lights", "Knob takeover", "Knob FX takeover",
                                    "Fader takeover", "Double Seq", "Capture bars", "Capture quantize", "Screen", "Place bar", "Sample rate", "Audio buffer", "Recall grab", "Recall gap",
                                    "Recall trim end", "Take auto end", "Auto end after", "Silence below", "Font", "Level LEDs", "Sample folder", "Project folder", "Preview volume", "CPU / RAM bar", "Flatten tail", "Shift pad map", "Chop to pads", "Preview tempo", "SEQ opens"};
        return names[row % Rows];
    }
    std::string value_text(unsigned row) const {
        static const char* responses[] {"Soft", "Medium", "Hard", "Full level"};
        static const char* lights[] {"Track colours", "White", "Off"};
        switch (row) {
        case Palette: return ui::palette_name(palette);
        case Font: return ui::font_name(font);
        case LevelLeds: return level_leds ? "Off" : "On";
        case PreviewLevel: return std::to_string(preview_level) + " %";
        case SystemBar: return system_bar ? "On" : "Off";
        case FlattenTail: { char t[16]; std::snprintf(t, sizeof(t), "%.1f s", flatten_tail / 10.0); return t; }
        case PreviewTempo: return preview_tempo ? "Project" : "Own";
        case ChopNotes: return chop_notes ? "Pads + notes" : "Pads only";
        case SeqFirst: { static const char* d[] {"Last used", "STEPS", "ROLL"}; return d[seq_first % 3]; } // LAB
        case ShiftMapDelay: { static const char* d[] {"Off", "At once", "0.65 s", "1 s", "2 s", "3 s"}; return d[shift_map_delay % 6]; }
        case StartFolder: {
            auto path = start_folder_text();
            if (path.empty()) return "Samples";
            auto cut = path.find_last_of('/');
            auto name = cut == std::string::npos ? path.substr(path.find(':') + 1) : path.substr(cut + 1);
            return name.empty() ? std::string("Card") : name;
        }
        case ProjectFolder: {
            auto path = project_folder_text();
            if (path.empty()) return "Projects";
            auto cut = path.find_last_of('/');
            return cut == std::string::npos ? path : path.substr(cut + 1);
        }
        case PadResponse: return responses[pad_response % 4];
        case PadPressure: return pressure ? "On" : "Off";
        case PadLights: return lights[pad_lights % 3];
        case KnobTakeover: return takeover_name(knob_takeover);
        case KnobFxTakeover: return takeover_name(knob_fx_takeover);
        case FaderTakeover: return takeover_name(fader_takeover);
        case DoubleSeq: return double_copies ? "Copy notes" : "Empty";
        case CaptureBars: return !capture_bars ? std::string("Auto") : std::to_string(capture_bars) + (capture_bars == 1 ? " bar" : " bars");
        case CaptureQuantize: return capture_quantise == 0 ? "As played" : capture_quantise == 1 ? "1/16" : "1/8";
        case PlaceBarRow: return place_bottom ? "Bottom" : "Top";
        case SampleRate: return audio_rate == 44100 ? "44.1 kHz" : std::to_string(audio_rate / 1000) + " kHz";
        case AudioBuffer: { // the frames queued, and how late that is heard at the rate chosen
            unsigned frames = 128u * audio_periods;
            char text[32];
            std::snprintf(text, sizeof(text), "%u  %.1f ms", frames, 1000.0 * frames / audio_rate);
            return text;
        }
        case RecallGrabRow: return recall_grab_name(sampling.grab);
        case RecallGap: return seconds_text(SamplingPrefs::gap_seconds[sampling.gap % SamplingPrefs::gap_count]);
        case RecallTrimEnd: return sampling.trim_end ? "On" : "Off";
        case TakeAutoEnd: return auto_end_name(sampling.auto_end);
        case AutoEndAfter: return seconds_text(SamplingPrefs::end_seconds[sampling.end_after % SamplingPrefs::end_count]);
        case SilenceBelow: return std::to_string(sampling.silence_db()) + " dB";
        default: return std::to_string(screen) + " %";
        }
    }
    // One step along a row.
    void step(unsigned row, int way) {
        switch (row) {
        case LevelLeds: level_leds = level_leds ? 0 : 1; break;
        case PreviewLevel: preview_level = uint8_t(std::clamp(int(preview_level) + way * 10, 0, 120)); break;
        case SystemBar: system_bar = !system_bar; break;
        case FlattenTail: flatten_tail = uint8_t(std::clamp(int(flatten_tail) + way * 5, 0, 100)); break;
        case ChopNotes: chop_notes = !chop_notes; break;
        case SeqFirst: seq_first = uint8_t(std::clamp(int(seq_first) + way, 0, 2)); break; // LAB
        case PreviewTempo: preview_tempo = !preview_tempo; break;
        case ShiftMapDelay: shift_map_delay = uint8_t(std::clamp(int(shift_map_delay) + way, 0, 5)); break;
        case StartFolder: start_folder.fill(0); break; // back to the browser's own start
        case ProjectFolder: project_folder.fill(0); break; // back to Melodee/Projects itself
        case Font: font = uint8_t((font + ui::font_count + (way > 0 ? 1 : ui::font_count - 1)) % ui::font_count); break;
        case Palette: palette = uint8_t((palette + ui::palette_count + (way > 0 ? 1 : ui::palette_count - 1)) % ui::palette_count); break;
        case PadResponse: pad_response = uint8_t((pad_response + 4 + way) % 4); break;
        case PadPressure: pressure = !pressure; break;
        case PadLights: pad_lights = uint8_t((pad_lights + 3 + way) % 3); break;
        case KnobTakeover: knob_takeover = step_takeover(knob_takeover, way); break;
        case KnobFxTakeover: knob_fx_takeover = step_takeover(knob_fx_takeover, way); break;
        case FaderTakeover: fader_takeover = step_takeover(fader_takeover, way); break;
        case DoubleSeq: double_copies = !double_copies; break;
        case CaptureBars: capture_bars = capture_lengths[step_in(capture_index(), unsigned(capture_lengths.size()), way)]; break;
        case CaptureQuantize: capture_quantise = uint8_t(step_in(capture_quantise, 3, way)); break;
        case PlaceBarRow: place_bottom = !place_bottom; break;
        case Screen: screen = uint8_t(std::clamp(int(screen) + way * 10, 20, 100)); break;
        case SampleRate: audio_rate = rates[size_t(std::clamp(rate_index() + (way < 0 ? -1 : 1), 0, int(rates.size()) - 1))]; break;
        case AudioBuffer: audio_periods = buffers[size_t(std::clamp(buffer_index() + (way < 0 ? -1 : 1), 0, int(buffers.size()) - 1))]; break;
        case RecallGrabRow: sampling.grab = RecallGrab(step_in(unsigned(sampling.grab), unsigned(RecallGrab::Count), way)); break;
        case RecallGap: sampling.gap = uint8_t(step_in(sampling.gap, SamplingPrefs::gap_count, way)); break;
        case RecallTrimEnd: sampling.trim_end = !sampling.trim_end; break;
        case TakeAutoEnd: sampling.auto_end = AutoEnd(step_in(unsigned(sampling.auto_end), unsigned(AutoEnd::Count), way)); break;
        case AutoEndAfter: sampling.end_after = uint8_t(step_in(sampling.end_after, SamplingPrefs::end_count, way)); break;
        case SilenceBelow: sampling.silence = uint8_t(step_in(sampling.silence, SamplingPrefs::silence_count, way)); break;
        default: break;
        }
    }
    unsigned capture_index() const {
        for (unsigned n = 0; n < capture_lengths.size(); ++n) if (capture_lengths[n] == capture_bars) return n;
        return 0;
    }
    // A list's next or previous entry, stopping at its ends.
    static unsigned step_in(unsigned at, unsigned count, int way) { return unsigned(std::clamp(int(at) + (way < 0 ? -1 : 1), 0, int(count) - 1)); }
    static std::string seconds_text(float s) {
        char text[16];
        std::snprintf(text, sizeof(text), s == std::floor(s) ? "%.0f s" : "%.1f s", double(s));
        return text;
    }
    int rate_index() const { return int(std::find(rates.begin(), rates.end(), audio_rate) - rates.begin()) % int(rates.size()); }
    int buffer_index() const {
        auto at = std::lower_bound(buffers.begin(), buffers.end(), audio_periods);
        return at == buffers.end() ? int(buffers.size()) - 1 : int(at - buffers.begin());
    }
    static Takeover step_takeover(Takeover t, int way) {
        int count = int(Takeover::Count);
        return Takeover((int(t) + count + (way < 0 ? -1 : 1)) % count);
    }
    // A pad's velocity through the response curve.
    uint8_t velocity(uint8_t v) const {
        if (pad_response == 3) return 127; // Full Level, as on the MPC
        if (pad_response == 1 || v == 0) return v;
        float x = v / 127.0f, curved = std::pow(x, pad_response == 0 ? 0.6f : 1.7f);
        return uint8_t(std::clamp(int(curved * 127 + 0.5f), 1, 127));
    }

    std::string text() const {
        std::ostringstream out;
        // pickup= stays for the builds before parity phase 8, which know only that.
        out << "pad_response=" << int(pad_response) << "\npad_lights=" << int(pad_lights)
            << "\npickup=" << int(knob_takeover != Takeover::Instant) << "\nknob_takeover=" << int(knob_takeover)
            << "\nknob_fx_takeover=" << int(knob_fx_takeover) << "\nfader_takeover=" << int(fader_takeover)
            << "\nscreen=" << int(screen) << "\nplace_bar=" << int(place_bottom) << "\ncount_in=" << int(count_in) << "\nrec_quantize=" << int(rec_quantize) << "\nrec_grid=" << int(rec_grid)
            << "\nzero_snap=" << int(zero_snap) << "\ndouble_copies=" << int(double_copies) << "\nclick=" << int(click) << "\nclick_level=" << int(click_level) << "\npalette=" << int(palette) << "\nfont=" << int(font) << "\nlevel_leds=" << int(level_leds) << "\npreview_level=" << int(preview_level) << "\nsystem_bar=" << int(system_bar) << "\nflatten_tail=" << int(flatten_tail) << "\nshift_map_delay=" << int(shift_map_delay) << "\nchop_events=" << int(chop_notes) << "\nseq_first=" << int(seq_first) << "\nshortcuts=" << shortcuts_text() << "\npreview_tempo=" << int(preview_tempo) << "\nstart_folder=" << start_folder_text() << "\nproject_folder=" << project_folder_text() << "\nroll_follow=" << int(roll_follow) << "\nroll_scale=" << int(roll_scale)
            << "\ncapture_bars=" << int(capture_bars) << "\ncapture_quantize=" << int(capture_quantise)
            << "\nresponse_before=" << int(response_before) << "\npressure=" << int(pressure)
            << "\ninput_source=" << int(input.source) << "\nmonitor=" << int(input.monitor) << "\nthreshold=" << int(input.threshold)
            << "\nrec_length=" << int(input.length) << "\nrec_fx=" << int(input.fx) << "\nrec_gain=" << int(input.gain)
            << "\ninput_track=" << int(input.track) << "\ninput_tap=" << int(input.post)
            << "\nrecall_grab=" << int(sampling.grab) << "\nrecall_gap=" << int(sampling.gap) << "\nrecall_trim=" << int(sampling.trim_end)
            << "\nauto_end=" << int(sampling.auto_end) << "\nauto_end_after=" << int(sampling.end_after)
            << "\nsilence_db=" << sampling.silence_db()
            << "\naudio_rate=" << audio_rate << "\naudio_periods=" << int(audio_periods) << '\n';
        write_midi_config(out, midi);
        for (unsigned e = 0; e < engines; ++e) {
            if (favourites[e].none()) continue;
            out << "favourites" << e << '=';
            bool first = true;
            for (unsigned s = 0; s < sounds; ++s)
                if (favourites[e][s]) { out << (first ? "" : ",") << s; first = false; }
            out << '\n';
        }
        return out.str();
    }
    std::string shortcuts_text() const {
        std::string out;
        for (unsigned n = 0; n < shortcuts.size(); ++n) out += (n ? "," : "") + std::to_string(int(shortcuts[n]));
        return out;
    }
    void read_shortcuts(const std::string& text) {
        std::istringstream in(text);
        std::string part;
        for (unsigned n = 0; n < shortcuts.size() && std::getline(in, part, ','); ++n) shortcuts[n] = uint8_t(std::clamp(std::atoi(part.c_str()), 0, 63));
    }
    void read(const std::string& text) {
        std::istringstream in(text);
        std::string line;
        int pickup = -1;          // the old row, read only when the file has none of the new ones
        bool takeovers = false;
        auto takeover = [&](int value) { takeovers = true; return Takeover(std::clamp(value, 0, int(Takeover::Count) - 1)); };
        while (std::getline(in, line)) {
            auto eq = line.find('=');
            if (eq == std::string::npos) continue;
            std::string key = line.substr(0, eq);
            int value = std::atoi(line.c_str() + eq + 1);
            if (key == "pad_response") pad_response = uint8_t(std::clamp(value, 0, 3));
            else if (key == "pad_lights") pad_lights = uint8_t(std::clamp(value, 0, 2));
            else if (key == "pickup") pickup = value != 0;
            else if (key == "knob_takeover") knob_takeover = takeover(value);
            else if (key == "knob_fx_takeover") knob_fx_takeover = takeover(value);
            else if (key == "fader_takeover") fader_takeover = takeover(value);
            else if (key == "pressure") pressure = value != 0;
            else if (key == "screen") screen = uint8_t(std::clamp(value, 20, 100));
            else if (key == "place_bar") place_bottom = value != 0;
            else if (key == "count_in") count_in = value != 0;
            else if (key == "rec_quantize") rec_quantize = value != 0;
            else if (key == "rec_grid") rec_grid = uint8_t(std::clamp(value, 0, 9)); // 0 Q, 1..9 a Division
            else if (key == "zero_snap") zero_snap = value != 0;
            else if (key == "double_copies") double_copies = value != 0;
            else if (key == "capture_bars") { capture_bars = 0; for (auto n : capture_lengths) if (n && int(n) <= value) capture_bars = n; } // the nearest below; 0 Auto
            else if (key == "capture_quantize") capture_quantise = uint8_t(std::clamp(value, 0, 2));
            else if (key == "click") click = uint8_t(std::clamp(value, 0, 2));
            else if (key == "click_level") click_level = uint8_t(std::clamp(value, 0, 100));
            else if (key == "preview_level") preview_level = uint8_t(std::clamp(value, 0, 120));
            else if (key == "system_bar") system_bar = value != 0;
            else if (key == "flatten_tail") flatten_tail = uint8_t(std::clamp(value, 0, 100));
            else if (key == "preview_tempo") preview_tempo = value != 0;
            else if (key == "chop_events") chop_notes = value != 0; // LAB: the old chop_notes (on) is left behind: off by default now
            else if (key == "seq_first") seq_first = uint8_t(value % 3);
            else if (key == "shortcuts") read_shortcuts(line.substr(eq + 1));
            else if (key == "shift_map_delay") shift_map_delay = uint8_t(std::clamp(value, 0, 5));
            else if (key == "level_leds") level_leds = value == 1 ? 1 : 0; // 2 and 3 (the bank lights, taken out): on
            else if (key == "start_folder") set_start_folder(line.substr(eq + 1));
            else if (key == "project_folder") set_project_folder(line.substr(eq + 1));
            else if (key == "font") font = uint8_t(std::clamp(value, 0, int(ui::font_count) - 1));
            else if (key == "palette") palette = uint8_t(std::clamp(value, 0, int(ui::palette_count) - 1));
            else if (key == "roll_follow") roll_follow = value != 0;
            else if (key == "roll_scale") roll_scale = value != 0;
            else if (key == "response_before") response_before = uint8_t(std::clamp(value, 0, 2));
            else if (key == "input_source") input.source = InputSource(std::clamp(value, 0, int(InputSource::Count) - 1));
            else if (key == "monitor") input.monitor = Monitor(std::clamp(value, 0, int(Monitor::Count) - 1));
            else if (key == "threshold") input.threshold = int8_t(std::clamp(value, int(InputConfig::threshold_min), 0));
            else if (key == "rec_length") input.length = uint8_t(std::clamp(value, 0, int(rec_lengths) - 1));
            else if (key == "rec_fx") input.fx = value != 0;
            else if (key == "rec_gain") input.gain = int8_t(std::clamp(value, -12, 24));
            else if (key == "input_track") input.track = uint8_t(std::clamp(value, 0, 15));
            else if (key == "input_tap") input.post = value != 0;
            else if (key == "recall_grab") sampling.grab = RecallGrab(std::clamp(value, 0, int(RecallGrab::Count) - 1));
            else if (key == "recall_gap") sampling.gap = uint8_t(std::clamp(value, 0, int(SamplingPrefs::gap_count) - 1));
            else if (key == "recall_trim") sampling.trim_end = value != 0;
            else if (key == "auto_end") sampling.auto_end = AutoEnd(std::clamp(value, 0, int(AutoEnd::Count) - 1));
            else if (key == "auto_end_after") sampling.end_after = uint8_t(std::clamp(value, 0, int(SamplingPrefs::end_count) - 1));
            else if (key == "silence_db") sampling.silence = SamplingPrefs::silence_index(value);
            else if (key == "audio_rate") { if (std::find(rates.begin(), rates.end(), unsigned(value)) != rates.end()) audio_rate = uint32_t(value); }
            else if (key == "audio_periods") { audio_periods = uint8_t(std::clamp(value, 2, 8)); audio_periods = buffers[size_t(buffer_index())]; }
            else if (read_midi_config(midi, key, value)) {}
            else if (key.rfind("favourites", 0) == 0) { // favourites3=0,4,12
                unsigned e = unsigned(std::atoi(key.c_str() + 10));
                if (e >= engines) continue;
                favourites[e].reset();
                std::istringstream list(line.substr(eq + 1));
                std::string item;
                while (std::getline(list, item, ','))
                    if (!item.empty()) { int s = std::atoi(item.c_str()); if (s >= 0 && unsigned(s) < sounds) favourites[e].set(unsigned(s)); }
            }
        }
        if (!takeovers && pickup == 0) knob_takeover = knob_fx_takeover = fader_takeover = Takeover::Instant; // "Knob pickup Off"
    }
};

} // namespace melodee
