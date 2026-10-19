// Panaudium effects rack: chorus, ping-pong delay, 8x8 feedback-delay-network reverb. AGPL-3.0 (see LICENSE).
#pragma once
#include "Dsp.h"

namespace op {

struct Chorus {
    DelayLine l, r; float ph = 0, sr = 48000;
    void prepare(float rate) { sr = rate; l.init(int(sr * 0.05f)); r.init(int(sr * 0.05f)); }
    void process(float* L, float* R, int n, float mix, float rateHz) {
        if (mix <= 0.f) { ph += rateHz * float(n) / sr; ph -= std::floor(ph); return; }
        for (int i = 0; i < n; ++i) {
            ph += rateHz / sr; if (ph >= 1.f) ph -= 1.f;
            const float mL = 0.012f + 0.004f * fsin(ph), mR = 0.012f + 0.004f * fsin(ph + 0.25f);
            l.push(L[i]); r.push(R[i]);
            const float wl = l.read(mL * sr), wr = r.read(mR * sr);
            L[i] = L[i] * (1.f - 0.4f * mix) + wr * mix * 0.8f;   // cross-fed for width
            R[i] = R[i] * (1.f - 0.4f * mix) + wl * mix * 0.8f;
        }
    }
};

struct PingPongDelay {
    DelayLine l, r; OnePole dampL, dampR; float sr = 48000;
    void prepare(float rate) { sr = rate; l.init(int(sr * 1.6f)); r.init(int(sr * 1.6f)); dampL.setCutoff(5000, sr); dampR.setCutoff(5000, sr); }
    void process(float* L, float* R, int n, float time, float fb, float mix) {
        const float d = time * sr;
        for (int i = 0; i < n; ++i) {
            const float dl = l.read(d), dr = r.read(d);
            l.push(0.5f * (L[i] + R[i]) + fb * dampR.tick(dr));
            r.push(fb * dampL.tick(dl));
            L[i] += mix * dl; R[i] += mix * dr;
        }
    }
};

// 8-line FDN with a Hadamard feedback matrix (cf. the "Matrix Reverb 8x8" class in Omnisphere's FX rack).
struct FdnReverb {
    static constexpr int N = 8;
    DelayLine line[N]; DelayLine pre[4]; float len[N] {}, g[N] {}, lpS[N] {}, sr = 48000, size = -1, damp = -1;
    float lpCoef = 0.3f; float modPh = 0;
    void prepare(float rate) {
        sr = rate;
        for (auto& d : line) d.init(int(sr * 0.25f));
        for (auto& p : pre) p.init(int(sr * 0.03f));
        size = -1;
    }
    void update(float sz, float dp) {
        if (sz == size && dp == damp) return;
        size = sz; damp = dp;
        static const float base[N] = { 29.7f, 37.1f, 41.1f, 43.7f, 53.3f, 59.3f, 67.1f, 73.9f };
        const float scale = 0.6f + 1.6f * sz;
        const float rt60 = 0.4f + sz * sz * 14.f;
        for (int k = 0; k < N; ++k) {
            len[k] = base[k] * 0.001f * scale * sr;
            g[k] = std::pow(10.f, -3.f * len[k] / (rt60 * sr));
        }
        lpCoef = 0.05f + 0.8f * dp;
    }
    void process(float* L, float* R, int n, float sz, float dp, float mix) {
        update(sz, dp);
        if (mix <= 0.f) return;
        static const float apLen[4] = { 4.77f, 3.59f, 12.73f, 9.31f };
        for (int i = 0; i < n; ++i) {
            modPh += 0.37f / sr; if (modPh >= 1.f) modPh -= 1.f;
            float in = 0.5f * (L[i] + R[i]);
            // input diffusion: 4 Schroeder allpasses
            for (int a = 0; a < 4; ++a) {
                const float d = pre[a].read(apLen[a] * 0.001f * sr);
                const float v = in + 0.6f * d;
                pre[a].push(v);
                in = d - 0.6f * v;
            }
            float y[N];
            for (int k = 0; k < N; ++k) {
                const float mod = (k < 2) ? 6.f * fsin(modPh + 0.5f * float(k)) : 0.f;  // light modulation vs metallic ringing
                y[k] = line[k].read(len[k] + mod);
                lpS[k] += (1.f - lpCoef) * (y[k] - lpS[k]);
                y[k] = lpS[k] * g[k];
            }
            // fast Walsh-Hadamard transform (orthogonal, normalised by 1/sqrt(8))
            for (int h = 1; h < N; h <<= 1)
                for (int a = 0; a < N; a += h * 2)
                    for (int b = a; b < a + h; ++b) { const float x = y[b], z = y[b + h]; y[b] = x + z; y[b + h] = x - z; }
            float oL = 0, oR = 0;
            for (int k = 0; k < N; ++k) {
                const float v = y[k] * 0.35355339f;
                line[k].push(v + ((k & 1) ? 0.f : in) * 0.5f + ((k & 1) ? in : 0.f) * 0.5f);
                if (k & 1) oR += v; else oL += v;
            }
            L[i] = L[i] * (1.f - 0.5f * mix) + oL * mix * 0.6f;
            R[i] = R[i] * (1.f - 0.5f * mix) + oR * mix * 0.6f;
        }
    }
};

} // namespace op
