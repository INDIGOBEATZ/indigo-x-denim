// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 Kerem Kilic (Ellic Studio)
// INDIGO X DENIM LAB: a sample's key (shell/key_detect.h).
#include "melodee/shell/key_detect.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace melodee {

int detect_key(const float* left, const float* right, size_t frames, float rate) {
    if (!left || frames < size_t(rate * 2) || rate < 8000) return -1; // a hit or a short one shot has no key to speak of
    // A mono copy at about 11 kHz, at most 20 s of it.
    const unsigned step = std::max(1u, unsigned(rate / 11025.0f));
    const float low_rate = rate / step;
    const size_t taken = std::min(frames, size_t(rate * 20));
    std::vector<float> mono;
    mono.reserve(taken / step + 1);
    for (size_t i = 0; i + step <= taken; i += step) {
        float sum = 0;
        for (unsigned k = 0; k < step; ++k) sum += left[i + k] + (right ? right[i + k] : left[i + k]);
        mono.push_back(sum / (2.0f * step));
    }
    if (mono.size() < 4096) return -1;
    std::array<double, 12> chroma {};
    const size_t window = 4096;
    double loudest = 0;
    std::vector<double> hann(window);
    for (size_t i = 0; i < window; ++i) hann[i] = 0.5 - 0.5 * std::cos(2 * M_PI * double(i) / (window - 1));
    for (size_t at = 0; at + window <= mono.size(); at += window) {
        double block_energy = 0;
        for (size_t i = 0; i < window; ++i) block_energy += double(mono[at + i]) * mono[at + i];
        if (block_energy < 1e-6 * window) continue; // silence says nothing
        for (int note = 36; note < 96; ++note) { // C2 .. B6
            double f = 440.0 * std::pow(2.0, (note - 69) / 12.0);
            if (f >= low_rate / 2) break;
            double coeff = 2 * std::cos(2 * M_PI * f / low_rate), s1 = 0, s2 = 0;
            for (size_t i = 0; i < window; ++i) {
                double s0 = mono[at + i] * hann[i] + coeff * s1 - s2;
                s2 = s1; s1 = s0;
            }
            double power = s1 * s1 + s2 * s2 - coeff * s1 * s2;
            chroma[size_t(note % 12)] += std::sqrt(std::max(0.0, power));
            loudest = std::max(loudest, power);
        }
    }
    double total = 0;
    for (double c : chroma) total += c;
    if (total <= 0 || loudest < 1e-3) return -1;
    static const double major[12] {6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88};
    static const double minor[12] {6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17};
    auto correlate = [&](const double* profile, int root) {
        double mx = 0, my = 0;
        for (int i = 0; i < 12; ++i) { mx += chroma[size_t(i)]; my += profile[i]; }
        mx /= 12; my /= 12;
        double num = 0, dx = 0, dy = 0;
        for (int i = 0; i < 12; ++i) {
            double x = chroma[size_t((i + root) % 12)] - mx, y = profile[i] - my;
            num += x * y; dx += x * x; dy += y * y;
        }
        return dx > 0 && dy > 0 ? num / std::sqrt(dx * dy) : -1.0;
    };
    int best = -1;
    double best_r = 0.35; // under this the tone is too vague to name
    for (int root = 0; root < 12; ++root) {
        double r = correlate(major, root);
        if (r > best_r) { best_r = r; best = root; }
        r = correlate(minor, root);
        if (r > best_r) { best_r = r; best = 12 + root; }
    }
    return best;
}

std::string key_name(int key) {
    static const char* names[12] {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    if (key < 0 || key >= 24) return {};
    return std::string(names[key % 12]) + (key >= 12 ? "m" : "");
}

} // namespace melodee
