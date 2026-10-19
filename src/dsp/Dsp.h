// Panaudium DSP building blocks. AGPL-3.0 (see LICENSE).
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace op {

constexpr float kPi = 3.14159265358979f;
constexpr float kTwoPi = 6.28318530717959f;

inline float clampf(float x, float a, float b) { return x < a ? a : (x > b ? b : x); }
inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
inline float semitoneRatio(float st) { return std::exp2(st * (1.f / 12.f)); }
inline float midiToHz(float n) { return 440.f * std::exp2((n - 69.f) * (1.f / 12.f)); }
inline float dbToGain(float db) { return std::pow(10.f, db * 0.05f); }
// Soft saturator (rational tanh approximation, |x| clamped).
inline float fastTanh(float x) {
    x = clampf(x, -3.f, 3.f);
    const float x2 = x * x;
    return x * (27.f + x2) / (27.f + 9.f * x2);
}

// Linear-congruential noise — same constants Omnisphere's VowelFilter uses for dither (0x19660d / 0x3c6ef35f).
struct Rng {
    uint32_t s = 0x12345678u;
    explicit Rng(uint32_t seed = 0x12345678u) : s(seed ? seed : 1u) {}
    uint32_t next() { s = s * 1664525u + 1013904223u; return s; }
    float bipolar() { return float(int32_t(next())) * (1.f / 2147483648.f); }
    float unipolar() { return float(next() >> 8) * (1.f / 16777216.f); }
};

// Shared sine table with linear interpolation; phase in cycles [0,1).
struct SineTable {
    static constexpr int N = 4096;
    float t[N + 1];
    SineTable() { for (int i = 0; i <= N; ++i) t[i] = std::sin(kTwoPi * float(i) / float(N)); }
    static const SineTable& get() { static SineTable s; return s; }
    inline float operator()(float phase) const {
        phase -= std::floor(phase);
        const float f = phase * N;
        const int i = int(f);
        const float fr = f - float(i);
        return t[i] + (t[i + 1] - t[i]) * fr;
    }
};
inline float fsin(float cycles) { return SineTable::get()(cycles); }

inline float polyBlep(float t, float dt) {
    if (t < dt) { t /= dt; return t + t - t * t - 1.f; }
    if (t > 1.f - dt) { t = (t - 1.f) / dt; return t * t + t + t + 1.f; }
    return 0.f;
}

// ADSR: linear attack, exponential decay/release (reaches ~-40 dB in the given time).
struct Adsr {
    enum Stage { Idle, Attack, Decay, Sustain, Release } stage = Idle;
    float level = 0, a = 0.01f, d = 0.3f, s = 0.7f, r = 0.3f, sr = 48000;
    void set(float A, float D, float S, float R, float rate) { a = A; d = D; s = S; r = R; sr = rate; }
    void gate(bool on) {
        if (on) stage = Attack;
        else if (stage != Idle) stage = Release;
    }
    // Advance n samples at once (control rate) and return the level.
    float advance(int n) {
        switch (stage) {
        case Idle: level = 0; break;
        case Attack:
            level += float(n) / std::max(1.f, a * sr);
            if (level >= 1.f) { level = 1.f; stage = Decay; }
            break;
        case Decay: {
            const float c = std::exp(-4.6f * float(n) / std::max(1.f, d * sr));
            level = s + (level - s) * c;
            if (std::fabs(level - s) < 1e-4f) { level = s; stage = Sustain; }
            break;
        }
        case Sustain: level = s; break;
        case Release: {
            const float c = std::exp(-4.6f * float(n) / std::max(1.f, r * sr));
            level *= c;
            if (level < 1e-4f) { level = 0; stage = Idle; }
            break;
        }
        }
        return level;
    }
    bool active() const { return stage != Idle; }
};

// Zavalishin TPT state-variable filter.
struct Svf {
    float g = 0, k = 1, a1 = 0, a2 = 0, a3 = 0, ic1 = 0, ic2 = 0;
    void set(float fc, float q, float sr) {
        fc = clampf(fc, 10.f, 0.49f * sr);
        g = std::tan(kPi * fc / sr);
        k = 1.f / std::max(0.05f, q);
        a1 = 1.f / (1.f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
    void reset() { ic1 = ic2 = 0; }
    // Returns lp, bp, hp through references.
    inline void tick(float x, float& lp, float& bp, float& hp) {
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.f * v1 - ic1;
        ic2 = 2.f * v2 - ic2;
        lp = v2; bp = v1; hp = x - k * v1 - v2;
    }
    inline float bp(float x) { float l, b, h; tick(x, l, b, h); return b; }
    inline float lp(float x) { float l, b, h; tick(x, l, b, h); return l; }
};

struct OnePole {  // y = (1-p) x + p y
    float p = 0, y = 0;
    void setCutoff(float fc, float sr) { p = std::exp(-kTwoPi * clampf(fc, 1.f, 0.49f * sr) / sr); }
    inline float tick(float x) { y = (1.f - p) * x + p * y; return y; }
};

struct DcBlock {
    float x1 = 0, y1 = 0, R = 0.995f;
    inline float tick(float x) { const float y = x - x1 + R * y1; x1 = x; y1 = y; return y; }
};

// Ring-buffer delay with linear-interpolated fractional read.
struct DelayLine {
    std::vector<float> buf;
    int w = 0, mask = 0;
    void init(int minSize) {
        int n = 1;
        while (n < minSize) n <<= 1;
        buf.assign(size_t(n), 0.f);
        mask = n - 1; w = 0;
    }
    void clear() { std::fill(buf.begin(), buf.end(), 0.f); }
    inline void push(float x) { buf[size_t(w)] = x; w = (w + 1) & mask; }
    // delay in samples (>= 1): value written `d` samples ago.
    inline float read(float d) const {
        d = clampf(d, 1.f, float(mask - 1));
        const int di = int(d);
        const float fr = d - float(di);
        const float a = buf[size_t((w - di) & mask)], b = buf[size_t((w - di - 1) & mask)];
        return a + (b - a) * fr;
    }
};

// Smooth random walk in [-1,1] (used for analog-style drift).
struct Drift {
    float v = 0, target = 0, coef = 0.999f; int count = 0;
    inline float advance(int n, Rng& rng, float sr) {
        count -= n;
        if (count <= 0) { target = rng.bipolar(); count = int(sr * (0.3f + 0.7f * rng.unipolar())); }
        const float c = std::exp(-float(n) / (0.4f * sr));
        v = target + (v - target) * c;
        return v;
    }
};

} // namespace op
