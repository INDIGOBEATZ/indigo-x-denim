// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 Kerem Kilic (Ellic Studio)
// INDIGO X DENIM LAB: a sample's musical key (as Jsen Surreal's SXL detects it on the Tune page): the energy of each
// semitone from C2 to B6 over its first 20 seconds (Goertzel filters on a mono copy brought down to about 11 kHz),
// folded into twelve pitch classes and matched against the Krumhansl-Kessler major and minor profiles. Service
// thread (a few tens of milliseconds for a long sample).
#pragma once

#include <cstddef>
#include <string>

namespace melodee {

// 0..11 C..B major, 12..23 C..B minor, -1 when it cannot tell (too short, too quiet, or no tone to speak of).
int detect_key(const float* left, const float* right, size_t frames, float rate);
// "C", "F#m", "" for -1.
std::string key_name(int key);

} // namespace melodee
