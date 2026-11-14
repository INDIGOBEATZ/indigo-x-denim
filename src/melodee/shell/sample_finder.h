// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 Kerem Kilic (Ellic Studio)
// INDIGO X DENIM: a project's sample that is no longer where the project says,
// found again on the card. In turn: the path as it is; the same path on the
// card as it is mounted now (a card's folder under /media is its volume's
// serial, so another card or a fresh format moves every path); then the file by
// its name anywhere on the card, the one sharing the most folders with the old
// path winning. A name cut short by an older project's 160-character limit is
// matched by what is left of it. Service thread only (it walks the card).
#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace melodee {

class SampleFinder {
public:
    // The tops of the drives to look on (the card's first).
    void set_tops(const std::vector<std::string>& tops);
    // The file the project means: `wanted` itself when it is there, another path when found, empty when not.
    std::string find(const std::string& wanted);
    // The card's list made again at the next miss (the card changed, or Find Samples asked for a fresh look).
    void forget() { built = false; retried = false; }
    // Files in the list (tests).
    size_t indexed() const { return files; }

private:
    void build();
    std::string by_name(const std::string& wanted, bool cut) const;
    std::vector<std::string> tops;
    std::unordered_map<std::string, std::vector<std::string>> named; // a file's name, lower case: its paths
    bool built = false, retried = false; // the list made again once for a miss, until forget()
    size_t files = 0;
    int64_t built_at = 0; // seconds
};

// A path's file name in lower case (the list's key).
std::string finder_key(const std::string& path);

} // namespace melodee
