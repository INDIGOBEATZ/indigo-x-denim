// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 Kerem Kilic (Ellic Studio)
// INDIGO X DENIM LAB: what the screen listens to, written on the audio thread without locks or allocation.
//  - ScopeRing: the last 4096 frames of a signal (mono), for the EQ's spectrum (ui/spectrum.h reads it).
//  - Loudness: EBU R128 / ITU-R BS.1770 loudness of the master: K-weighting, momentary (400 ms), short-term (3 s)
//    and integrated (gated at -70 LUFS and 10 LU under the ungated level), each 100 ms.
#pragma once

#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>

namespace melodee {

struct ScopeRing {
    static constexpr unsigned size = 4096;
    std::array<float, size> data {};
    std::atomic<uint32_t> written {0};
    // Audio thread: a block, left and right summed (null: silence).
    void push(const float* l, const float* r, unsigned frames) {
        uint32_t w = written.load(std::memory_order_relaxed);
        for (unsigned f = 0; f < frames; ++f) data[(w + f) % size] = l && r ? (l[f] + r[f]) * 0.5f : 0.0f;
        written.store(w + frames, std::memory_order_release);
    }
    // Any thread: the newest `n` frames, oldest first (a frame being written may tear: a picture, not audio).
    void last(float* out, unsigned n) const {
        uint32_t w = written.load(std::memory_order_acquire);
        for (unsigned i = 0; i < n; ++i) out[i] = data[(w + size - n + i) % size];
    }
};

class Loudness {
public:
    static constexpr int lowest = -70; // LUFS: the absolute gate, and the histogram's floor
    static constexpr unsigned bins = 700; // 0.1 LU each, -70 .. 0 LUFS

    // Audio thread (or before it runs): the filters for this rate, everything measured forgotten.
    void prepare(float rate) {
        // ITU-R BS.1770-4: the head's shelf and the RLB high-pass, as biquads for this rate.
        double fs = rate;
        {
            double f0 = 1681.974450955533, g = 3.999843853973347, q = 0.7071752369554196;
            double k = std::tan(M_PI * f0 / fs), vh = std::pow(10.0, g / 20), vb = std::pow(vh, 0.4996667741545416);
            double a0 = 1 + k / q + k * k;
            shelf = {(vh + vb * k / q + k * k) / a0, 2 * (k * k - vh) / a0, (vh - vb * k / q + k * k) / a0, 2 * (k * k - 1) / a0, (1 - k / q + k * k) / a0};
        }
        {
            double f0 = 38.13547087602444, q = 0.5003270373238773, k = std::tan(M_PI * f0 / fs);
            double a0 = 1 + k / q + k * k;
            high = {1, -2, 1, 2 * (k * k - 1) / a0, (1 - k / q + k * k) / a0};
        }
        per_block = unsigned(std::lround(rate * 0.1));
        reset();
    }
    void reset() {
        for (auto& s : state) s = {};
        sum = 0; in_block = 0; blocks = 0; ring = {};
        for (auto& b : histogram) b.store(0, std::memory_order_relaxed);
        m.store(-INFINITY, std::memory_order_relaxed); st.store(-INFINITY, std::memory_order_relaxed);
    }
    // Audio thread: the integrated reading starts over (a new take: Play from stopped).
    void restart_integrated() { for (auto& b : histogram) b.store(0, std::memory_order_relaxed); }

    void process(const float* l, const float* r, unsigned frames) {
        if (!per_block) return;
        for (unsigned f = 0; f < frames; ++f) {
            double a = weigh(state[0], state[1], l[f]), b = weigh(state[2], state[3], r[f]);
            sum += a * a + b * b;
            if (++in_block >= per_block) close_block();
        }
    }

    // Any thread: the readings in LUFS (-inf: silence or not yet).
    float momentary() const { return m.load(std::memory_order_relaxed); }
    float short_term() const { return st.load(std::memory_order_relaxed); }
    float integrated() const {
        double energy = 0, count = 0;
        for (unsigned i = 0; i < bins; ++i) { double c = histogram[i].load(std::memory_order_relaxed); energy += c * from_lufs(bin_lufs(i)); count += c; }
        if (count <= 0) return -INFINITY;
        double gate = to_lufs(energy / count) - 10;
        energy = count = 0;
        for (unsigned i = 0; i < bins; ++i) {
            if (bin_lufs(i) < gate) continue;
            double c = histogram[i].load(std::memory_order_relaxed); energy += c * from_lufs(bin_lufs(i)); count += c;
        }
        return count > 0 ? float(to_lufs(energy / count)) : -INFINITY;
    }

private:
    struct Coefficients { double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0; };
    struct State { double x1 = 0, x2 = 0, y1 = 0, y2 = 0; };
    static double run(const Coefficients& c, State& s, double x) {
        double y = c.b0 * x + c.b1 * s.x1 + c.b2 * s.x2 - c.a1 * s.y1 - c.a2 * s.y2;
        s.x2 = s.x1; s.x1 = x; s.y2 = s.y1; s.y1 = y;
        return y;
    }
    double weigh(State& a, State& b, float x) { return run(high, b, run(shelf, a, x)); }
    static double to_lufs(double energy) { return energy > 0 ? -0.691 + 10 * std::log10(energy) : -INFINITY; }
    static double from_lufs(double lufs) { return std::pow(10.0, (lufs + 0.691) / 10); }
    static double bin_lufs(unsigned i) { return lowest + (i + 0.5) * 0.1; }

    void close_block() {
        ring[blocks % ring.size()] = sum / per_block; // this 100 ms's mean square (both channels summed)
        ++blocks; sum = 0; in_block = 0;
        auto mean = [&](unsigned n) {
            unsigned have = blocks < n ? blocks : n;
            double e = 0;
            for (unsigned k = 0; k < have; ++k) e += ring[(blocks - 1 - k) % ring.size()];
            return have ? e / have : 0.0;
        };
        double momentary_energy = mean(4);
        m.store(float(to_lufs(momentary_energy)), std::memory_order_relaxed);
        st.store(float(to_lufs(mean(30))), std::memory_order_relaxed);
        if (blocks >= 4) { // each 400 ms gating block, overlapping by 75 %
            double lufs = to_lufs(momentary_energy);
            if (lufs >= lowest) {
                unsigned bin = unsigned(std::min<double>(bins - 1, (lufs - lowest) / 0.1));
                histogram[bin].fetch_add(1, std::memory_order_relaxed);
            }
        }
    }

    Coefficients shelf, high;
    std::array<State, 4> state {};
    double sum = 0;
    unsigned in_block = 0, per_block = 0;
    uint32_t blocks = 0;
    std::array<double, 30> ring {};
    std::array<std::atomic<uint32_t>, bins> histogram {};
    std::atomic<float> m {-INFINITY}, st {-INFINITY};
};

} // namespace melodee
