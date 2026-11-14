// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 Kerem Kilic (Ellic Studio)
// INDIGO X DENIM LAB: a signal's spectrum for the screen (the EQ's picture), from engine/listen.h's ScopeRing: the last
// 2048 frames, Hann-windowed, a radix-2 FFT, folded into `bands` log-spaced bands from 20 Hz to 20 kHz, in dB
// (-90 .. 0), each band falling 30 dB a second once it is past its peak. Screen thread only; no allocation after the
// first call.
#pragma once

#include "melodee/engine/listen.h"
#include <array>
#include <cmath>
#include <complex>

namespace melodee::ui {

template <unsigned bands>
class Spectrum {
public:
    static constexpr unsigned size = 2048;
    // `dt` the seconds since the last update (for the fall), `rate` the device's sample rate.
    const std::array<float, bands>& update(const ScopeRing& ring, float rate, float dt) {
        ring.last(buffer.data(), size);
        for (unsigned i = 0; i < size; ++i) {
            float w = 0.5f - 0.5f * std::cos(2 * float(M_PI) * i / (size - 1));
            data[i] = std::complex<float>(buffer[i] * w, 0);
        }
        fft(data);
        const float lo = std::log(20.0f), hi = std::log(std::min(20000.0f, rate / 2));
        for (unsigned b = 0; b < bands; ++b) {
            float f0 = std::exp(lo + (hi - lo) * b / bands), f1 = std::exp(lo + (hi - lo) * (b + 1) / bands);
            unsigned k0 = std::max(1u, unsigned(f0 * size / rate)), k1 = std::max(k0 + 1, unsigned(f1 * size / rate));
            float power = 0;
            for (unsigned k = k0; k < k1 && k < size / 2; ++k) power = std::max(power, std::norm(data[k]));
            // Hann's coherent gain (0.5) and the FFT's size: a full-scale sine reads about 0 dB.
            float db = power > 0 ? 10 * std::log10(power) - 20 * std::log10(size * 0.25f) : -120;
            db = std::clamp(db, -90.0f, 0.0f);
            shown[b] = db >= shown[b] ? db : std::max(db, shown[b] - 30 * dt);
        }
        return shown;
    }

private:
    static void fft(std::array<std::complex<float>, size>& a) {
        for (unsigned i = 1, j = 0; i < size; ++i) {
            unsigned bit = size >> 1;
            for (; j & bit; bit >>= 1) j ^= bit;
            j ^= bit;
            if (i < j) std::swap(a[i], a[j]);
        }
        for (unsigned len = 2; len <= size; len <<= 1) {
            float ang = -2 * float(M_PI) / len;
            std::complex<float> wl(std::cos(ang), std::sin(ang));
            for (unsigned i = 0; i < size; i += len) {
                std::complex<float> w(1, 0);
                for (unsigned j = 0; j < len / 2; ++j) {
                    auto u = a[i + j], v = a[i + j + len / 2] * w;
                    a[i + j] = u + v; a[i + j + len / 2] = u - v;
                    w *= wl;
                }
            }
        }
    }
    std::array<float, size> buffer {};
    std::array<std::complex<float>, size> data {};
    std::array<float, bands> shown = [] { std::array<float, bands> s {}; s.fill(-90); return s; }();
};

} // namespace melodee::ui
