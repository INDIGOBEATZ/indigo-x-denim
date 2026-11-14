// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 Kerem Kilic (Ellic Studio)
// SAMPLES (Sample Select on a sampler track): the MPC's sample browser.
// MPC drum programs (KIT) first, then instruments (XPM, XTY, SFZ), then folders, as categories, then the WAV files. The wheel and Minus /
// Plus move, Push opens a folder or loads the file onto the last pad hit
// (keeping the pad's settings; the whole file, Trim or Chop shape it), or a
// bank file (BANK, Samples/Banks) into the kit's bank in view, or an MPC drum
// program (KIT) as the track's kit, as Load Kit does, or installs an MPC
// expansion (.xpn, INSTALL) onto the card and opens its folder; Shift +
// Sample Select here saves that bank as a bank file (Save Bank). B1
// goes back out, B2 picks the drive (the card, or the internal one: the
// factory categories and the stock app's samples, read only), B3 hears each
// file as the cursor lands on it. Shift + pad loads the file onto the
// kit's next empty pad (here instead of the printed Shift + pad jobs). K2
// jumps. The pads play the kit.
// INDIGO X DENIM: Shift + B3 STAR stars the file (LAB: or the folder) under the cursor, or takes
// its star off; starred rows carry a star. B2 goes on from the internal
// drive to FAVOURITES: every starred file, from either drive, by name, which
// Push loads as anywhere else (a star taken off there leaves the list).
// Shift + B1 SET START makes the folder in view the one Sample Select opens
// in (Settings > SCREEN > Sample folder, where −/+ clears it).
// A folder of folders where any has a cover (MPC expansions: the art their
// Expansion.xml names) shows as a grid of covers, three across, its name
// under each; the wheel moves through them as through the list. Shift + B2
// GRID / LIST switches the folder in view between the two, kept per folder.
#include "melodee/shell/smf_jobs.h"
#include "melodee/pages/common.h"
#include "melodee/pages/pages.h"
#include "melodee/engine/dsp.h"
#include "melodee/shell/sample_library.h"
#include "melodee/ui/widgets.h"
#include <cstdio>
#include <algorithm>
#include <filesystem>
#include <map>
#include <vector>

namespace melodee {
namespace {
using namespace ui;
constexpr unsigned visible = list_rows;

juce::String folder_title(const Snapshot& s) {
    if (s.ui.files_drive == unsigned(Drive::Favourites)) return "STARRED";
    if (s.ui.files_drive == unsigned(Drive::Internal) && s.files_top) return "CATEGORIES";
    if (s.files_folder.empty()) return "NO CARD";
    return juce::String(std::filesystem::path(s.files_folder).filename().string());
}

// INDIGO X DENIM: a cover's pixels as an image, made once (screen thread).
const juce::Image& cover_image(const std::shared_ptr<const std::vector<uint32_t>>& cover) {
    static std::map<const void*, std::pair<std::shared_ptr<const std::vector<uint32_t>>, juce::Image>> made;
    auto found = made.find(cover.get());
    if (found != made.end()) return found->second.second;
    if (made.size() > 128) made.clear();
    constexpr int n = int(SampleFile::cover_size);
    juce::Image image(juce::Image::ARGB, n, n, false);
    for (int y = 0; y < n; ++y)
        for (int x = 0; x < n; ++x) image.setPixelAt(x, y, juce::Colour((*cover)[size_t(y * n + x)]));
    return made.emplace(cover.get(), std::make_pair(cover, image)).first->second.second;
}

// INDIGO X DENIM: the grid where the folder asks for it (Shift + B2), or as it comes: a folder of folders with covers.
bool grid_of(const std::vector<SampleFile>& files, uint8_t layout) {
    if (layout == 1) return false;
    if (layout == 2) return !files.empty();
    return !files.empty() && std::all_of(files.begin(), files.end(), [](const SampleFile& f) { return f.folder; })
           && std::any_of(files.begin(), files.end(), [](const SampleFile& f) { return bool(f.cover); });
}
bool grid_view(const Snapshot& s) { return grid_of(s.files, s.files_layout); }

// The covers' grid: three across, two rows in view, the row with the cursor kept in it.
void draw_covers(Paint& p, const Snapshot& s, juce::Colour c) {
    constexpr unsigned across = 3, rows = 2;
    unsigned count = unsigned(s.files.size()), cursor = std::min<unsigned>(s.ui.files_cursor, count - 1);
    unsigned row = cursor / across, total_rows = (count + across - 1) / across;
    unsigned first_row = row < 1 ? 0 : std::min(row - 1, total_rows > rows ? total_rows - rows : 0);
    constexpr float gap = 4, x0 = zone::pad, w = (zone::width - 2 * zone::pad - gap * (across - 1)) / float(across);
    constexpr float h = (zone::picture_h - 6 - gap) / float(rows), art = h - 19;
    for (unsigned r = 0; r < rows; ++r)
        for (unsigned k = 0; k < across; ++k) {
            unsigned n = (first_row + r) * across + k;
            if (n >= count) continue;
            const auto& file = s.files[n];
            float x = x0 + k * (w + gap), y = zone::picture_y + 3 + r * (h + gap);
            bool current = n == cursor;
            p.fill(x, y, w, h, current ? colour::raised : colour::panel, 4);
            float ax = x + (w - art) / 2, ay = y + 3;
            if (file.cover) p.g.drawImage(cover_image(file.cover), int(ax), int(ay), int(art), int(art), 0, 0, int(SampleFile::cover_size), int(SampleFile::cover_size));
            else {
                p.fill(ax, ay, art, art, colour::surf, 3);
                Icon glyph = file.folder ? Icon::Folder : file.kit ? Icon::Pads : file.instrument ? Icon::Keyboard : file.bank ? Icon::Layers
                           : file.midi ? Icon::Note : Icon::Wav;
                icon(p, glyph, ax + art / 2 - 11, ay + art / 2 - 11, 22, file.starred ? c : colour::dim, 1.4f);
            }
            if (current) p.ring(x, y, w, h, c, 4, 1.5f);
            auto name = name_case(juce::String(file.name));
            auto style = plain(current ? colour::text : colour::sec, type::small, current ? Weight::SemiBold : Weight::Regular);
            p.text(name, x + 3, y + h - 15, w - 6, 13, p.fit(name, style, w - 6, type::floor), juce::Justification::centred);
        }
}

void draw(Paint& p, const Snapshot& s) {
    const auto& track = s.track();
    auto c = colour_of(s);
    Header h = header_of(s);
    Drive drive = Drive(s.ui.files_drive);
    auto drive_chip = drive == Drive::Favourites ? chip(Icon::Star, "FAVS", c) : chip(drive_name(drive), c);
    drive_chip.spare = true; // a long folder name keeps its room: B2 names the other drive
    h.left.push_back(drive_chip);
    if (s.ui.ref_pick) h.left.push_back(pill("PICK REFERENCE", colour::amber)); // LAB
    h.left.push_back(pill(folder_title(s) + juce::String::fromUTF8(" \xc2\xb7 ") + juce::String(s.files.size()), c));
    h.right.push_back(tag_of(s, s.ui.track));
    juce::String other = drive_name(Drive((unsigned(drive) + 1) % unsigned(Drive::Count)));
    // The file under the cursor as it previews (mock 7): its row fills as it plays; B3 lit while there is sound.
    PreviewPlace heard = s.ui.preview ? preview_place(s.heads, int(sampler_pads), uint16_t(preview_slot), dsp::model_rate) : PreviewPlace {};
    SoftKeys keys {{s.files_top ? "" : "BACK", other, "PREVIEW"}, -1, c, {s.files_top ? KeyIcon::None : KeyIcon::Back, KeyIcon::None, KeyIcon::None}};
    keys.toggles = 4; // PREVIEW names a thing that is on or off: its LED, never ON / OFF (the row fills while it sounds)
    keys.lit = s.ui.preview ? 4 : 0;
    if (s.ui.shift) { // Shift + B3: the file under the cursor starred, or its star off
        bool on_file = !s.files.empty(); // LAB: a folder too
        bool starred = on_file && s.files[std::min<size_t>(s.ui.files_cursor, s.files.size() - 1)].starred;
        bool folder_here = drive != Drive::Favourites && !s.files_folder.empty();
        keys = SoftKeys {{folder_here ? "SET START" : "", grid_view(s) ? "LIST" : "GRID", on_file ? (starred ? "UNSTAR" : "STAR") : ""}, -1, c};
        keys.glyphs = {folder_here ? Icon::Folder : Icon::None, Icon::None, on_file ? Icon::Star : Icon::None};
    }
    frame(p, s, keys, h);
    if (s.files.empty()) { // a glyph and two words: no card, or nothing here
        bool no_card = drive == Drive::Card && s.sample_root.empty(), favs = drive == Drive::Favourites;
        icon(p, favs ? Icon::Star : no_card ? Icon::Card : Icon::Folder, zone::width / 2.0f - 14, zone::picture_y + 40, 28, colour::dim, 1.4f);
        p.text(favs ? "NONE YET" : no_card ? "NO CARD" : "EMPTY", 0, zone::picture_y + 76, zone::width, 14, caps_text(colour::dim), juce::Justification::centred);
        return;
    }
    if (grid_view(s)) { draw_covers(p, s, c); return; } // INDIGO X DENIM: a folder of expansions as their covers
    unsigned count = unsigned(s.files.size()), cursor = std::min<unsigned>(s.ui.files_cursor, count - 1);
    unsigned first = cursor < 2 ? 0 : std::min(cursor - 2, count > visible ? count - visible : 0);
    for (unsigned row = 0; row < visible && first + row < count; ++row) {
        const auto& file = s.files[first + row];
        ListRow r;
        r.name = name_case(juce::String(file.name)); r.accent = c; r.current = first + row == cursor;
        r.starred = file.starred;
        const juce::String path(file.path);
        juce::String format = path.endsWithIgnoreCase(".xpm") ? "XPM" : path.endsWithIgnoreCase(".xty") ? "XTY" : path.endsWithIgnoreCase(".exs") ? "EXS"
                            : path.endsWithIgnoreCase(".xpn") ? "XPN" : path.endsWithIgnoreCase(".sf2") || path.contains(".sf2#") || path.contains(".SF2#") ? "SF2"
                            : path.endsWithIgnoreCase(".dspreset") ? "DS" : "SFZ";
        if (file.folder) { r.icon = Icon::Folder; r.chevron = true; }
        else if (file.bank) r.icon = Icon::Layers;
        else if (file.kit) { r.icon = Icon::Pads; r.fact = format; }
        else if (file.expansion) { r.icon = Icon::ArrowDown; r.fact = format; } // Push installs it
        else if (file.instrument) { r.icon = Icon::Keyboard; r.fact = format; }
        else if (file.midi) { r.icon = Icon::Note; r.fact = "MIDI"; } // roadmap P4: Push lays it on the pattern
        else {
            r.icon = Icon::Wav;
            for (unsigned n = 0; n < sample_slots && r.pad.isEmpty(); ++n) // which pad of this kit plays it
                if (file.path == s.project.samples[n].path)
                    for (unsigned k = 0; k < sampler_pads; ++k)
                        if (track.pads[k].plays(uint16_t(n))) { char label[4]; pad_label(k, label); r.pad = juce::String(label); break; }
        }
        if (r.current && heard.shown && !file.folder && !file.bank) { // the time heard of its length
            char now[16], of[24];
            std::snprintf(now, sizeof(now), "%.1f", heard.heard);
            std::snprintf(of, sizeof(of), " / %.1f s", heard.length);
            r.tag_lead = now; r.tag = of; r.fact = {};
            r.played = heard.through; r.played_wake = heard.wake; r.played_head = heard.strength;
        }
        p.g.saveState();
        list_row(p, zone::pad, zone::picture_y + 0.5f + row * (list_row_h + list_row_gap), zone::width - 2 * zone::pad, r);
        p.g.restoreState();
    }
}

void wheel(Context& c, int delta) {
    int was = c.ui.files_cursor;
    c.ui.files_cursor = uint16_t(std::clamp<int>(c.ui.files_cursor + delta, 0, std::max(int(c.ui.files_count) - 1, 0)));
    if (c.ui.files_cursor != was) c.preview_cursor();
}

void push(Context& c) {
    if (!c.library) return;
    auto files = c.library->files();
    if (c.ui.files_cursor >= files.size()) return;
    const auto& file = files[c.ui.files_cursor];
    if (file.folder && Drive(c.ui.files_drive) == Drive::Favourites) { // LAB: a starred folder: into it, on its own drive
        auto top = c.library->card();
        Drive d = !top.empty() && file.path.rfind(top, 0) == 0 ? Drive::Card : Drive::Internal;
        c.ui.files_drive = uint8_t(d);
        c.library->browse(d, file.path);
    } else if (file.folder) c.library->browse(Drive(c.ui.files_drive), file.path);
    else if (c.ui.ref_pick) { if (!file.bank && !file.kit && !file.expansion && !file.midi) c.reference_load(file.path); } // LAB: Reference A/B's song
    else if (file.bank) c.load_bank_file(file.path, file.name);
    else if (file.kit) c.load_kit_file(file.path, file.name);
    else if (file.expansion) c.library->install(file.path);
    else if (file.midi) import_midi(c, file.path); // roadmap P4: its notes onto the track's pattern
    else c.load_file(file.path, file.name, false);
}

void soft(Context& c, int key) {
    if (!c.library) return;
    auto& ui = c.ui;
    if (ui.shift) { // Shift + B3 STAR, Shift + B1 SET START, Shift + B2 GRID / LIST
        ui.shift_used = true;
        if (key == 1) { // INDIGO X DENIM: the folder in view as a grid or a list, kept for it
            bool grid = grid_of(c.library->files(), c.library->layout());
            c.library->set_layout(c.library->folder(), grid ? 1 : 2);
            c.notice(grid ? "List" : "Grid");
            return;
        }
        if (key == 0) { // INDIGO X DENIM: this folder where Sample Select opens
            auto folder = c.library->folder();
            if (folder.empty() || Drive(ui.files_drive) == Drive::Favourites) { c.notice("A folder first"); return; }
            auto top = c.library->card();
            bool card = !top.empty() && folder.rfind(top, 0) == 0;
            std::string stored = card ? "card:" + folder.substr(std::min(folder.size(), top.size() + 1)) : folder;
            if (stored.size() >= ui.settings.start_folder.size()) { c.notice("Folder name too long"); return; }
            ui.settings.set_start_folder(stored);
            c.change_setting(Settings::Rows, 0); // saves the settings (no row steps)
            c.notice("Start folder set");
            return;
        }
        if (key != 2) return;
        auto files = c.library->files();
        if (ui.files_cursor >= files.size()) return; // LAB: a folder may be starred too, for FAVS
        const auto& file = files[ui.files_cursor];
        c.library->star(file.path, file.name);
        c.notice(file.starred ? "Star off" : "Starred");
        return;
    }
    if (key == 0) c.library->browse_up();
    if (key == 1) { ui.files_drive = uint8_t((ui.files_drive + 1) % unsigned(Drive::Count)); c.library->browse(Drive(ui.files_drive), ""); }
    if (key == 2) { ui.preview = !ui.preview; c.preview_cursor(); } // its LED says which
}
} // namespace

// Shift + pad on the browser: the file under the cursor onto the next empty pad.
bool samples_shift_pad(Context& c) {
    if (c.ui.page != PageId::Samples || !c.library || c.track().engine != EngineKind::Sampler) return false;
    auto files = c.library->files();
    if (c.ui.files_cursor >= files.size() || files[c.ui.files_cursor].folder) { c.notice("Shift + pad: a file first"); return true; }
    const auto& file = files[c.ui.files_cursor];
    if (file.bank) { c.notice("Push loads a bank"); return true; }
    if (file.kit) { c.notice("Push loads a kit"); return true; }
    if (file.expansion) { c.notice("Push installs it"); return true; }
    if (file.midi) { c.notice("Push lays it on the pattern"); return true; }
    c.load_file(file.path, file.name, true);
    return true;
}

const PageSpec samples_page {draw, play_pad, wheel, push, soft, wheel}; // Minus / Plus move the list

} // namespace melodee
