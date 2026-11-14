// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 Kerem Kilic (Ellic Studio)
#include "melodee/shell/shortcuts.h"
#include "melodee/shell/context.h"
#include "melodee/shell/project_store.h"

namespace melodee {
namespace {
const char* const names[shortcut_count] {
    "As printed", "Mixer", "Master", "Tuner", "Sub Lens", "Arrange", "Reference A/B", "Load reference",
    "Quick save", "Find samples", "Looper", "Samples", "Tempo", "Scenes", "Motion", "Tap tempo",
};
} // namespace

const char* shortcut_name(unsigned id) { return names[id % shortcut_count]; }

bool run_shortcut(Context& c, unsigned pad) {
    unsigned id = c.ui.settings.shortcuts[pad % 16] % shortcut_count;
    switch (id) {
    case 0: return false;
    case 1: c.open(PageId::Mixer); break;
    case 2: c.open(PageId::Master); break;
    case 3: c.open(PageId::Tuner); break;
    case 4: c.open(PageId::SubLens); break;
    case 5: c.open(PageId::Song); c.ui.song_all = true; break;
    case 6: c.reference_toggle(); break;
    case 7: c.reference_pick(); break;
    case 8:
        if (!c.store) { c.notice("No card"); break; }
        c.save_project_to(c.project_path.empty() ? c.store->free_path_in(c.save_folder(), "") : c.project_path);
        c.notice("Saved");
        break;
    case 9: c.find_samples(); break;
    case 10: c.open(PageId::Looper); break;
    case 11: c.open(PageId::Samples); break;
    case 12: c.open(PageId::Tempo); break;
    case 13: c.open(PageId::Scenes); break;
    case 14: c.open(PageId::Motion); break;
    case 15: c.tap_tempo(); break;
    default: return false;
    }
    return true;
}
} // namespace melodee
