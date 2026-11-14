// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 Kerem Kilic (Ellic Studio)
// INDIGO X DENIM: missing samples found again (shell/sample_finder.h).
#include "melodee/shell/sample_finder.h"
#include <algorithm>
#include <chrono>
#include <filesystem>

namespace fs = std::filesystem;

namespace melodee {
namespace {
constexpr size_t most_files = 400000; // a card's list stops here
constexpr int deepest = 14;
constexpr size_t old_limit = 159;     // the characters an older project kept of a path

int64_t seconds() {
    return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    return s;
}

bool listed(const std::string& extension) {
    static const char* kinds[] = {".wav", ".wave", ".aif", ".aiff", ".flac", ".mp3", ".ogg", ".sfz", ".xpm", ".xty", ".xpj",
                                  ".exs", ".sf2", ".dspreset"};
    for (const char* k : kinds) if (extension == k) return true;
    return false;
}

bool there(const std::string& path) {
    std::error_code error;
    return !path.empty() && fs::is_regular_file(path, error);
}

// The folders two paths share from their ends, the file's own name not counted.
int shared_tail(const std::string& a, const std::string& b) {
    fs::path pa = fs::path(lower(a)).parent_path(), pb = fs::path(lower(b)).parent_path();
    int n = 0;
    while (!pa.empty() && !pb.empty() && pa.filename() == pb.filename() && pa.filename() != "/") {
        ++n;
        pa = pa.parent_path();
        pb = pb.parent_path();
    }
    return n;
}
} // namespace

std::string finder_key(const std::string& path) { return lower(fs::path(path).filename().string()); }

void SampleFinder::set_tops(const std::vector<std::string>& t) {
    std::vector<std::string> kept;
    for (const auto& top : t) if (!top.empty()) kept.push_back(top);
    if (kept != tops) { tops = kept; built = false; }
}

void SampleFinder::build() {
    named.clear();
    files = 0;
    built = true;
    built_at = seconds();
    for (const auto& top : tops) {
        std::error_code error;
        fs::recursive_directory_iterator it(top, fs::directory_options::skip_permission_denied, error), end;
        while (!error && it != end && files < most_files) {
            const auto& entry = *it;
            auto name = entry.path().filename().string();
            std::error_code e;
            bool folder = entry.is_directory(e);
            if (folder) {
                // Hidden folders, the card's boot folder and the system's own are no sample's.
                if (name.empty() || name[0] == '.' || name == "autorun" || name == "lost+found" || it.depth() >= deepest)
                    it.disable_recursion_pending();
            } else if (!name.empty() && name[0] != '.' && listed(lower(entry.path().extension().string()))) {
                named[lower(name)].push_back(entry.path().string());
                ++files;
            }
            it.increment(error);
        }
    }
}

std::string SampleFinder::by_name(const std::string& wanted, bool cut) const {
    auto key = finder_key(wanted);
    if (key.empty()) return {};
    const std::vector<std::string>* paths = nullptr;
    std::vector<std::string> gathered;
    if (auto at = named.find(key); at != named.end()) paths = &at->second;
    else if (cut && key.size() >= 3) { // the name was cut short: every file whose name starts so
        for (const auto& [name, list] : named)
            if (name.compare(0, key.size(), key) == 0) gathered.insert(gathered.end(), list.begin(), list.end());
        paths = &gathered;
    }
    if (!paths || paths->empty()) return {};
    const std::string* best = nullptr;
    int best_tail = -1;
    for (const auto& p : *paths) {
        int tail = shared_tail(p, wanted);
        if (tail > best_tail && there(p)) { best_tail = tail; best = &p; }
    }
    return best ? *best : std::string();
}

std::string SampleFinder::find(const std::string& wanted) {
    if (wanted.empty() || wanted[0] != '/') return {};
    if (there(wanted)) return wanted;
    // The same place on a drive mounted elsewhere: the path's first one to three folders taken for each top.
    size_t cut_at = 0;
    for (int strip = 0; strip < 3; ++strip) {
        cut_at = wanted.find('/', cut_at + 1);
        if (cut_at == std::string::npos) break;
        auto rest = wanted.substr(cut_at);
        for (const auto& top : tops) {
            auto moved = top + rest;
            if (moved != wanted && there(moved)) return moved;
        }
    }
    bool cut = wanted.size() >= old_limit;
    if (!built) build();
    auto found = by_name(wanted, cut);
    if (found.empty() && !retried && seconds() - built_at > 20) { retried = true; build(); found = by_name(wanted, cut); } // files copied on since
    return found;
}

} // namespace melodee
