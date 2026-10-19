// Panaudium FX group "Space": Delay, ProVerb, Innerspace, Shimmer. AGPL-3.0 (see LICENSE).
#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <thread>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>
#include "FxUnit.h"
#include "../Dsp.h"

namespace op {

// =============================================================================
// Internal DSP Helpers for Space FX
// =============================================================================

// Simple Schroeder allpass filter for diffusers and reverb building blocks.
struct SpaceAllpass {
    DelayLine dl;
    float g = 0.5f;
    void init(int maxLen) { dl.init(maxLen); }
    void reset() { dl.clear(); }
    inline float tick(float x, float delaySamples) {
        const float d = dl.read(delaySamples);
        const float v = x + g * d;
        dl.push(v);
        return d - g * v;
    }
};

// 1-pole shelving tilt filter (crossover ~1 kHz).
struct SpaceTiltFilter {
    float lp = 0.f;
    void reset() { lp = 0.f; }
    inline float tick(float x, float toneDb, float sr) {
        const float alpha = 1.0f - std::exp(-kTwoPi * 1000.f / sr);
        lp += alpha * (x - lp);
        const float hp = x - lp;
        const float lowGain = dbToGain(toneDb * 0.5f);
        const float highGain = dbToGain(-toneDb * 0.5f);
        return lp * lowGain + hp * highGain;
    }
};

// FIFO queue for block-based FFT convolution interfacing with arbitrary host block sizes.
struct SpaceFifo {
    std::vector<float> buf;
    int head = 0;
    int tail = 0;
    int count = 0;
    int cap = 0;

    void init(int capacity) {
        cap = capacity;
        buf.assign(size_t(cap), 0.f);
        head = tail = count = 0;
    }
    void reset() {
        std::fill(buf.begin(), buf.end(), 0.f);
        head = tail = count = 0;
    }
    inline void write(float x) {
        if (count < cap) {
            buf[size_t(head)] = x;
            head = (head + 1);
            if (head >= cap) head = 0;
            ++count;
        }
    }
    inline float read() {
        if (count > 0) {
            const float x = buf[size_t(tail)];
            tail = (tail + 1);
            if (tail >= cap) tail = 0;
            --count;
            return x;
        }
        return 0.f;
    }
    inline int size() const { return count; }
};

// Iterative radix-2 complex FFT of length N (used by partitioned convolution).
template<int N>
struct Radix2Fft {
    std::array<int, N> bitRev {};
    std::array<float, N / 2> twiddleRe {};
    std::array<float, N / 2> twiddleIm {};
    bool initialized = false;

    void init() {
        if (initialized) return;
        int bits = 0;
        while ((1 << bits) < N) ++bits;
        for (int i = 0; i < N; ++i) {
            int rev = 0;
            for (int b = 0; b < bits; ++b) {
                if (i & (1 << b)) rev |= (1 << (bits - 1 - b));
            }
            bitRev[size_t(i)] = rev;
        }
        for (int k = 0; k < N / 2; ++k) {
            const float angle = -kTwoPi * float(k) / float(N);
            twiddleRe[size_t(k)] = std::cos(angle);
            twiddleIm[size_t(k)] = std::sin(angle);
        }
        initialized = true;
    }

    void transform(float* re, float* im, bool inverse) const {
        for (int i = 0; i < N; ++i) {
            const int j = bitRev[size_t(i)];
            if (i < j) {
                std::swap(re[i], re[j]);
                std::swap(im[i], im[j]);
            }
        }
        for (int len = 2; len <= N; len <<= 1) {
            const int half = len >> 1;
            const int step = N / len;
            for (int i = 0; i < N; i += len) {
                for (int j = 0; j < half; ++j) {
                    const int twIdx = j * step;
                    const float wr = twiddleRe[size_t(twIdx)];
                    const float wi = inverse ? -twiddleIm[size_t(twIdx)] : twiddleIm[size_t(twIdx)];
                    const int idx = i + j + half;
                    const float tr = wr * re[idx] - wi * im[idx];
                    const float ti = wr * im[idx] + wi * re[idx];
                    re[idx] = re[i + j] - tr;
                    im[idx] = im[i + j] - ti;
                    re[i + j] += tr;
                    im[i + j] += ti;
                }
            }
        }
        if (inverse) {
            const float invN = 1.0f / float(N);
            for (int i = 0; i < N; ++i) {
                re[i] *= invN;
                im[i] *= invN;
            }
        }
    }
};

// Dual-tap granular delay-line pitch shifter.
struct GranularPitchShifter {
    DelayLine dl;
    float ph = 0.f;
    float winSamples = 2400.f; // ~50-80 ms

    void init(int maxLen) {
        dl.init(maxLen);
        ph = 0.f;
    }
    void reset() {
        dl.clear();
        ph = 0.f;
    }
    void setWindow(float sec, float sr) {
        winSamples = clampf(sec * sr, 100.f, float(dl.mask - 2));
    }
    inline float tick(float x, float ratio) {
        dl.push(x);
        // Phase increment to achieve transposition ratio
        const float dph = (1.0f - ratio) / winSamples;
        ph += dph;
        if (ph >= 1.0f) ph -= 1.0f;
        else if (ph < 0.0f) ph += 1.0f;

        const float ph2 = (ph >= 0.5f) ? (ph - 0.5f) : (ph + 0.5f);
        const float d1 = 1.0f + ph * winSamples;
        const float d2 = 1.0f + ph2 * winSamples;

        // Raised-cosine window: w1 + w2 == 1 identically (using fsin lookup table)
        const float cosVal = fsin(ph + 0.25f);
        const float w1 = 0.5f * (1.0f - cosVal);
        const float w2 = 1.0f - w1;

        const float s1 = dl.read(d1);
        const float s2 = dl.read(d2);
        return s1 * w1 + s2 * w2;
    }
};

// Fast dual-tap crossfader for digital delay time changes (no pitch warping, no zippering).
struct DigitalCrossfader {
    float curD = 0.f;
    float oldD = 0.f;
    float newD = 0.f;
    int fadeCount = 0;
    int fadeLen = 512;

    void reset(float d, float sr) {
        curD = oldD = newD = d;
        fadeLen = std::max(64, int(sr * 0.015f)); // ~15 ms crossfade
        fadeCount = fadeLen;
    }

    inline float read(const DelayLine& dl, float targetD) {
        if (fadeCount >= fadeLen) {
            if (std::abs(targetD - curD) > 1.5f) {
                oldD = curD;
                newD = targetD;
                fadeCount = 0;
            }
        }
        if (fadeCount < fadeLen) {
            const float t = float(fadeCount) / float(fadeLen);
            fadeCount++;
            const float sOld = dl.read(oldD);
            const float sNew = dl.read(newD);
            if (fadeCount >= fadeLen) curD = newD;
            return lerpf(sOld, sNew, t);
        }
        return dl.read(curD);
    }
};

// =============================================================================
// 1. FX_Delay: Delay
// =============================================================================
// Models:
// 0: Digital   - clean stereo delay with crossfaded read heads preventing zippering
// 1: Tape Echo - tape saturation in loop, wow & flutter modulation, hiss, pitch glide
// 2: Ping-Pong - mono-summed input into L, cross-feedback L <-> R, spread width
// 3: Dual Tap  - two taps at T and 0.75*T panned by spread with shared feedback
// 4: Diffuse   - echoes smeared through 4 allpasses in feedback loop turning into wash
class DelayFx : public FxUnit {
public:
    void prepare(float sampleRate, int maxBlock) override {
        (void)maxBlock;
        sr = sampleRate;
        // Max delay 2.0 s + margins for spread/modulation up to 192 kHz
        const int maxSamples = int(sr * 2.7f) + 1024;
        dlL.init(maxSamples);
        dlR.init(maxSamples);

        for (int i = 0; i < 4; ++i) {
            diffL[size_t(i)].init(int(sr * 0.04f) + 128);
            diffR[size_t(i)].init(int(sr * 0.04f) + 128);
        }

        dampL.setCutoff(5000.f, sr);
        dampR.setCutoff(5000.f, sr);
        reset();
    }

    void reset() override {
        dlL.clear();
        dlR.clear();
        for (auto& a : diffL) a.reset();
        for (auto& a : diffR) a.reset();
        smoothedTimeL = smoothedTimeR = 0.375f * sr;
        digL.reset(0.375f * sr, sr);
        digR.reset(0.375f * sr, sr);
        wowPh = 0.f;
        flutterPh = 0.f;
        duckEnv = 0.f;
        rng = Rng(0x50aceu);
    }

    void process(float* L, float* R, int n, const float* k, int model) override {
        // Knobs: Time(s), Feedback(%), Tone(Hz), Wow(%), Spread(%), Ducking(%)
        const float timeSec = fxKnob(FX_Delay, 0).value(k[0]);
        const float feedback = clampf(fxKnob(FX_Delay, 1).value(k[1]), 0.f, 0.999f);
        const float toneHz = fxKnob(FX_Delay, 2).value(k[2]);
        const float wow = clampf(fxKnob(FX_Delay, 3).value(k[3]), 0.f, 1.f);
        const float spread = clampf(fxKnob(FX_Delay, 4).value(k[4]), 0.f, 1.f);
        const float ducking = clampf(fxKnob(FX_Delay, 5).value(k[5]), 0.f, 1.f);

        dampL.setCutoff(toneHz, sr);
        dampR.setCutoff(toneHz, sr);

        // Ducking envelope follower coefficients (~10 ms attack, ~250 ms release)
        const float duckAtt = 1.0f - std::exp(-1.0f / (0.010f * sr));
        const float duckRel = 1.0f - std::exp(-1.0f / (0.250f * sr));

        // Diffuser lengths (primes in ms)
        static constexpr float diffMs[4] = { 3.1f, 5.3f, 8.7f, 11.9f };

        // Target delay times in samples
        float targetTimeL = timeSec * sr;
        float targetTimeR = timeSec * sr;
        if (model == 0) { // Digital: spread offsets R by up to +25%
            targetTimeR *= (1.0f + 0.25f * spread);
        } else if (model == 1) { // Tape: slight stereo offset
            targetTimeR *= (1.0f + 0.05f * spread);
        }

        for (int i = 0; i < n; ++i) {
            const float inL = L[i];
            const float inR = R[i];

            // Ducking envelope follower on dry input level
            const float dryPeak = std::max(std::abs(inL), std::abs(inR));
            if (dryPeak > duckEnv) {
                duckEnv += duckAtt * (dryPeak - duckEnv);
            } else {
                duckEnv += duckRel * (dryPeak - duckEnv);
            }
            // Ducking reduces wet level while input is loud
            const float duckGain = 1.0f - ducking * clampf(duckEnv * 2.5f, 0.f, 0.92f);

            // Tape pitch glide vs digital smooth crossfade
            float dL = targetTimeL;
            float dR = targetTimeR;

            if (model == 1) { // Tape Echo
                // Pitch glide (tau ~ 45 ms)
                smoothedTimeL += (targetTimeL - smoothedTimeL) * (1.0f / (0.045f * sr));
                smoothedTimeR += (targetTimeR - smoothedTimeR) * (1.0f / (0.045f * sr));

                // Wow (slow ~0.5 Hz) and flutter (~6 Hz + drift)
                wowPh += 0.5f / sr; if (wowPh >= 1.f) wowPh -= 1.f;
                flutterPh += 6.3f / sr; if (flutterPh >= 1.f) flutterPh -= 1.f;
                const float flutterMod = fsin(flutterPh) + 0.3f * rng.bipolar();
                const float totalWow = (fsin(wowPh) * 0.0018f + flutterMod * 0.0006f) * wow * sr;

                dL = smoothedTimeL + totalWow;
                dR = smoothedTimeR + totalWow * 0.9f;
            } else {
                // Smooth parameter changes without zippering
                smoothedTimeL += (targetTimeL - smoothedTimeL) * 0.005f;
                smoothedTimeR += (targetTimeR - smoothedTimeR) * 0.005f;
                dL = smoothedTimeL;
                dR = smoothedTimeR;
            }

            float outL = 0.f, outR = 0.f;

            if (model == 0) { // Digital: clean stereo with crossfaded read heads
                const float dl_outL = digL.read(dlL, targetTimeL);
                const float dl_outR = digR.read(dlR, targetTimeR);
                outL = dl_outL;
                outR = dl_outR;

                // Feedback path with tone damping and fastTanh limiting
                const float fbL = fastTanh(dampL.tick(dl_outL) * feedback);
                const float fbR = fastTanh(dampR.tick(dl_outR) * feedback);
                dlL.push(inL + fbL);
                dlR.push(inR + fbR);

            } else if (model == 1) { // Tape Echo
                const float dl_outL = dlL.read(dL);
                const float dl_outR = dlR.read(dR);
                outL = dl_outL;
                outR = dl_outR;

                // Tape hiss + saturation inside feedback loop
                const float hiss = rng.bipolar() * 0.0002f * (1.0f + wow);
                const float fbL = fastTanh((dampL.tick(dl_outL) + hiss) * (feedback * 1.35f)) * 0.85f;
                const float fbR = fastTanh((dampR.tick(dl_outR) + hiss) * (feedback * 1.35f)) * 0.85f;
                dlL.push(inL + fbL);
                dlR.push(inR + fbR);

            } else if (model == 2) { // Ping-Pong
                const float monoIn = 0.5f * (inL + inR);
                const float dl_outL = dlL.read(dL);
                const float dl_outR = dlR.read(dR);

                // Alternating cross-feedback: L feeds R, R feeds L
                const float fbL = fastTanh(dampL.tick(dl_outR) * feedback);
                const float fbR = fastTanh(dampR.tick(dl_outL) * feedback);
                dlL.push(monoIn + fbL);
                dlR.push(fbR);

                // Spread controls stereo separation width
                outL = dl_outL * (0.5f + 0.5f * spread) + dl_outR * (0.5f - 0.5f * spread);
                outR = dl_outR * (0.5f + 0.5f * spread) + dl_outL * (0.5f - 0.5f * spread);

            } else if (model == 3) { // Dual Tap
                // Tap 1 at Time, Tap 2 at Time * 0.75 (dotted feel)
                const float tap1L = dlL.read(dL);
                const float tap2L = dlL.read(dL * 0.75f);
                const float tap1R = dlR.read(dR);
                const float tap2R = dlR.read(dR * 0.75f);

                // Panned by spread
                outL = tap1L * (0.5f + 0.5f * spread) + tap2R * (0.5f - 0.5f * spread);
                outR = tap2L * (0.5f - 0.5f * spread) + tap1R * (0.5f + 0.5f * spread);

                const float sharedFb = 0.5f * (tap1L + tap2R);
                const float fbL = fastTanh(dampL.tick(sharedFb) * feedback);
                const float fbR = fastTanh(dampR.tick(sharedFb) * feedback);
                dlL.push(inL + fbL);
                dlR.push(inR + fbR);

            } else { // Diffuse: allpass diffuser chain inside loop
                float dl_outL = dlL.read(dL);
                float dl_outR = dlR.read(dR);
                outL = dl_outL;
                outR = dl_outR;

                // Diffuse echoes through 4 allpasses in feedback loop
                for (int a = 0; a < 4; ++a) {
                    const float dSamples = diffMs[a] * 0.001f * sr;
                    dl_outL = diffL[size_t(a)].tick(dl_outL, dSamples);
                    dl_outR = diffR[size_t(a)].tick(dl_outR, dSamples * 1.15f);
                }

                const float fbL = fastTanh(dampL.tick(dl_outL) * feedback);
                const float fbR = fastTanh(dampR.tick(dl_outR) * feedback);
                dlL.push(inL + fbL);
                dlR.push(inR + fbR);
            }

            // Fully wet output scaled by ducking gain
            L[i] = clampf(outL * duckGain, -2.5f, 2.5f);
            R[i] = clampf(outR * duckGain, -2.5f, 2.5f);
        }
    }

private:
    float sr = 48000.f;
    DelayLine dlL, dlR;
    std::array<SpaceAllpass, 4> diffL, diffR;
    OnePole dampL, dampR;
    float smoothedTimeL = 0.f, smoothedTimeR = 0.f;
    DigitalCrossfader digL, digR;
    float wowPh = 0.f, flutterPh = 0.f;
    float duckEnv = 0.f;
    Rng rng;
};

// =============================================================================
// 2. FX_ProVerb: Pro-Verb
// =============================================================================
// Models:
// 0: Room       - short lines, strong sparse early reflections, little modulation
// 1: Hall       - medium/long lines, moderate modulation, smooth balanced decay
// 2: Plate      - dense input diffusion, bright, high-frequency sustain
// 3: Cathedral  - longest lines, slow build-up, warm low-mids, decay x1.5
// 4: Dark Space - deep slow chorused modulation, heavy high-frequency damping
// 5: Bloom      - ambient swelling envelope feeding late reverb
class ProVerbFx : public FxUnit {
public:
    static constexpr int N = 8;

    void prepare(float sampleRate, int maxBlock) override {
        (void)maxBlock;
        sr = sampleRate;
        // Lines sized for longest Cathedral lines at 192 kHz
        const int maxLineSamples = int(sr * 0.35f) + 2048;
        for (auto& d : lines) d.init(maxLineSamples);
        for (auto& p : preDiff) p.init(int(sr * 0.05f) + 128);
        predelayL.init(int(sr * 0.30f) + 128);
        predelayR.init(int(sr * 0.30f) + 128);
        earlyRefl.init(int(sr * 0.10f) + 128);
        reset();
    }

    void reset() override {
        for (auto& d : lines) d.clear();
        for (auto& p : preDiff) p.clear();
        predelayL.clear();
        predelayR.clear();
        earlyRefl.clear();
        for (int k = 0; k < N; ++k) {
            lpDamp[k] = 0.f;
            lowShelfLp[k] = 0.f;
        }
        modPh = 0.f;
        bloomEnv = 0.f;
        bloomIn = 0.f;
    }

    void process(float* L, float* R, int n, const float* k, int model) override {
        // Knobs: Size(%), Decay(s), Predelay(ms), Diffusion(%), Lows(dB), Highs(dB)
        const float sizeNorm = clampf(fxKnob(FX_ProVerb, 0).value(k[0]), 0.05f, 1.f);
        float decaySec = fxKnob(FX_ProVerb, 1).value(k[1]);
        const float predelayMs = fxKnob(FX_ProVerb, 2).value(k[2]);
        const float diffusion = clampf(fxKnob(FX_ProVerb, 3).value(k[3]), 0.f, 1.f);
        const float lowsDb = fxKnob(FX_ProVerb, 4).value(k[4]);
        const float highsDb = fxKnob(FX_ProVerb, 5).value(k[5]);

        if (model == 3) decaySec *= 1.5f; // Cathedral: decay scaled up x1.5

        // Base line lengths (ms) per model
        static constexpr float baseLengths[6][N] = {
            { 15.3f, 19.1f, 21.5f, 24.3f, 28.7f, 32.1f, 35.7f, 39.3f }, // Room
            { 29.7f, 37.1f, 41.1f, 43.7f, 53.3f, 59.3f, 67.1f, 73.9f }, // Hall
            { 22.3f, 27.1f, 31.7f, 37.3f, 41.9f, 47.1f, 53.3f, 59.7f }, // Plate
            { 41.3f, 52.1f, 57.7f, 63.1f, 74.3f, 83.7f, 94.1f, 103.9f }, // Cathedral
            { 33.1f, 39.7f, 45.3f, 51.1f, 61.3f, 69.7f, 79.1f, 87.3f }, // Dark Space
            { 31.1f, 38.3f, 43.1f, 49.7f, 57.3f, 65.1f, 73.7f, 81.9f }  // Bloom
        };

        const float sizeScale = 0.5f + 1.2f * sizeNorm;
        float lineSamples[N];
        float decayGain[N];

        for (int i = 0; i < N; ++i) {
            lineSamples[i] = baseLengths[model][i] * 0.001f * sizeScale * sr;
            // RT60 per-line decay gain: g = 10^(-3 * len / (RT60 * sr))
            decayGain[i] = std::pow(10.f, -3.f * lineSamples[i] / (decaySec * sr));
        }

        // Loop shelving filters: High-shelf (~4 kHz) and Low-shelf (~250 Hz)
        float lpAlpha = clampf(0.35f * std::pow(0.5f, highsDb / 6.0f), 0.02f, 0.85f);
        if (model == 2) lpAlpha *= 0.6f; // Plate: brighter
        if (model == 4) lpAlpha = std::max(lpAlpha, 0.5f); // Dark Space: heavy high damping

        const float lowShelfAlpha = 1.0f - std::exp(-kTwoPi * 250.f / sr);
        const float lowGain = clampf(dbToGain(lowsDb * 0.5f), 0.35f, 1.4f);

        // Input diffusion allpasses
        static constexpr float apMs[4] = { 4.77f, 3.59f, 12.73f, 9.31f };
        float diffG = clampf(0.25f + 0.6f * diffusion, 0.15f, 0.85f);
        if (model == 2) diffG = clampf(0.65f + 0.3f * diffusion, 0.5f, 0.92f); // Plate: very dense

        const float predelaySamples = clampf(predelayMs * 0.001f * sr, 1.f, float(predelayL.mask - 2));

        // Early reflections tap delays (ms) for Room
        static constexpr float erMs[6] = { 8.5f, 14.3f, 21.7f, 29.1f, 38.5f, 47.9f };
        static constexpr float erGainL[6] = { 0.42f, -0.35f, 0.28f, -0.22f, 0.16f, -0.10f };
        static constexpr float erGainR[6] = { -0.32f, 0.40f, -0.25f, 0.30f, -0.14f, 0.18f };

        // Modulation depth and rate
        float modDepth = 3.5f;
        float modRate = 0.45f;
        if (model == 0) { modDepth = 1.0f; modRate = 0.25f; } // Room: light
        if (model == 4) { modDepth = 9.5f; modRate = 0.20f; } // Dark Space: deep chorused tail

        for (int i = 0; i < n; ++i) {
            // Predelay
            predelayL.push(L[i]);
            predelayR.push(R[i]);
            const float preL = predelayL.read(predelaySamples);
            const float preR = predelayR.read(predelaySamples);

            // Room early reflections
            float erOutL = 0.f, erOutR = 0.f;
            if (model == 0) {
                earlyRefl.push(0.5f * (preL + preR));
                for (int t = 0; t < 6; ++t) {
                    const float s = earlyRefl.read(erMs[t] * 0.001f * sr);
                    erOutL += s * erGainL[t];
                    erOutR += s * erGainR[t];
                }
            }

            // Input diffusion: 4 Schroeder allpasses
            float inSig = 0.5f * (preL + preR);

            // Bloom swell envelope: tracks presence of dry input, rises from 0 to 1
            // over ~0.65 s of sustained input, falling slowly when input stops.
            if (model == 5) {
                const float dryPeak = std::max(std::abs(preL), std::abs(preR));
                if (dryPeak > bloomIn) {
                    bloomIn += 0.02f * (dryPeak - bloomIn);
                } else {
                    bloomIn += (1.0f / (0.15f * sr)) * (dryPeak - bloomIn); // ~150 ms hold
                }
                const bool hasInput = (bloomIn > 0.005f);
                if (hasInput) {
                    bloomEnv += (1.0f - bloomEnv) * (1.0f / (0.65f * sr)); // rises over ~0.65 s
                } else {
                    bloomEnv += (0.0f - bloomEnv) * (1.0f / (2.0f * sr));  // falls slowly
                }
            }

            for (int a = 0; a < 4; ++a) {
                const float d = preDiff[a].read(apMs[a] * 0.001f * sr);
                const float v = inSig + diffG * d;
                preDiff[a].push(v);
                inSig = d - diffG * v;
            }

            // Modulation LFO
            modPh += modRate / sr;
            if (modPh >= 1.f) modPh -= 1.f;

            float y[N];
            for (int k = 0; k < N; ++k) {
                const float mod = modDepth * fsin(modPh + float(k) * 0.125f);
                float val = lines[k].read(lineSamples[k] + mod);

                // High shelf damping
                lpDamp[k] += lpAlpha * (val - lpDamp[k]);
                val = lpDamp[k];

                // Low shelf damping
                lowShelfLp[k] += lowShelfAlpha * (val - lowShelfLp[k]);
                val += (lowGain - 1.0f) * lowShelfLp[k];

                // RT60 loop gain + anti-denormal flush + soft-clip
                val = fastTanh(val * decayGain[k]);
                if (std::abs(val) < 1e-15f) val = 0.f;
                y[k] = val;
            }

            // Fast Walsh-Hadamard Transform 8x8 (normalized by 1/sqrt(8) ≈ 0.35355339f)
            for (int h = 1; h < N; h <<= 1) {
                for (int a = 0; a < N; a += h * 2) {
                    for (int b = a; b < a + h; ++b) {
                        const float x = y[b];
                        const float z = y[b + h];
                        y[b] = x + z;
                        y[b + h] = x - z;
                    }
                }
            }

            // Feedback injection & stereo tap decorrelation
            float oL = 0.f, oR = 0.f;
            constexpr float fdnNorm = 0.35355339f;

            for (int k = 0; k < N; ++k) {
                const float v = y[k] * fdnNorm;
                // Alternating feed to lines
                const float inj = inSig * 0.5f;
                lines[k].push(v + inj);

                // Decorrelated stereo taps
                if (k & 1) oR += v; else oL += v;
            }

            // Bloom swell on late reverb output (normalised so steady-state level matches Hall)
            if (model == 5) {
                oL *= bloomEnv;
                oR *= bloomEnv;
            }

            // Add early reflections and scale fully wet output
            // Cathedral trimmed from 1.55 to ~1.0 (0.70 * 1.0 / 1.55 ≈ 0.45)
            const float outScale = (model == 3) ? 0.45f : 0.7f;
            L[i] = clampf((oL * outScale + erOutL * 0.35f), -2.5f, 2.5f);
            R[i] = clampf((oR * outScale + erOutR * 0.35f), -2.5f, 2.5f);
        }
    }

private:
    float sr = 48000.f;
    std::array<DelayLine, N> lines;
    std::array<DelayLine, 4> preDiff;
    DelayLine predelayL, predelayR;
    DelayLine earlyRefl;
    float lpDamp[N] {};
    float lowShelfLp[N] {};
    float modPh = 0.f;
    float bloomEnv = 0.f;
    float bloomIn = 0.f;
};

// =============================================================================
// 3. FX_Innerspace: Innerspace
// =============================================================================
// Uniformly partitioned FFT convolution (block size 256, overlap-save) with
// procedurally synthesised impulse responses.
// Models:
// 0: Coin Dropper - accelerating metallic clicks, geometric bouncing intervals
// 1: Spring Tank  - dispersive downward chirps, repeating darker echoes
// 2: Metal Box    - dense modal ring, ~40 inharmonic 3D box modes
// 3: Glass Jar    - high-Q long resonances + short bright initial noise burst
// 4: Piano Lid    - sympathetic string resonances, harmonic series with beating
// 5: Cave Drip    - long dark diffuse noise tail with random pitched drips
// 6: Paper Tube   - hollow tube resonance around 150 Hz with fast HF decay
// 7: Gong Wash    - inharmonic shimmer swelling over ~0.35 s then decaying
class InnerspaceFx : public FxUnit {
public:
    static constexpr int B = 1024;      // Partition block size
    static constexpr int N = 2048;      // FFT size = 2 * B
    static constexpr int numBins = N / 2 + 1; // 1025 non-redundant frequency bins (0..N/2)

    ~InnerspaceFx() override { stopWorker(); }

    void prepare(float sampleRate, int maxBlock) override {
        stopWorker();   // never reallocate while the IR builder thread is running
        sr = sampleRate;
        fft.init();

        // Max IR length 4 seconds at up to 192 kHz
        maxPartitions = int(sr * 4.0f / float(B)) + 4;
        const int totalSpectraFloats = maxPartitions * numBins;

        for (int s = 0; s < 2; ++s) {
            partL_re[s].assign(size_t(totalSpectraFloats), 0.f);
            partL_im[s].assign(size_t(totalSpectraFloats), 0.f);
            partR_re[s].assign(size_t(totalSpectraFloats), 0.f);
            partR_im[s].assign(size_t(totalSpectraFloats), 0.f);
            activePartitions[s] = 2;
        }

        inputHistL_re.assign(size_t(totalSpectraFloats), 0.f);
        inputHistL_im.assign(size_t(totalSpectraFloats), 0.f);
        inputHistR_re.assign(size_t(totalSpectraFloats), 0.f);
        inputHistR_im.assign(size_t(totalSpectraFloats), 0.f);

        irScratchL.assign(size_t(maxPartitions * B), 0.f);
        irScratchR.assign(size_t(maxPartitions * B), 0.f);

        const int fifoCap = std::max(maxBlock * 8, 16384);
        inFifoL.init(fifoCap);
        inFifoR.init(fifoCap);
        outFifoL.init(fifoCap);
        outFifoR.init(fifoCap);

        predelayL.init(int(sr * 0.25f) + 128);
        predelayR.init(int(sr * 0.25f) + 128);

        reset();
        // Initial IR built synchronously here (prepare is never called on the audio thread).
        activeSet.store(1);
        buildIr(0, 1.0f, 0.6f);
        activeSet.store(0);
        isCrossfading = false;
        state.store(kIdle);
        quit.store(false);
        worker = std::thread([this] { workerLoop(); });
    }

    // Clears audio history only; the IR spectra are kept (they are data, not state, and rebuilding them is
    // far too expensive for the audio thread, which calls reset() when a slot switches unit type).
    void reset() override {
        std::fill(inputHistL_re.begin(), inputHistL_re.end(), 0.f);
        std::fill(inputHistL_im.begin(), inputHistL_im.end(), 0.f);
        std::fill(inputHistR_re.begin(), inputHistR_re.end(), 0.f);
        std::fill(inputHistR_im.begin(), inputHistR_im.end(), 0.f);

        inFifoL.reset();
        inFifoR.reset();
        outFifoL.reset();
        outFifoR.reset();

        // Prime output FIFO with B zeros for fixed B-sample overlap-save latency
        for (int i = 0; i < B; ++i) {
            outFifoL.write(0.f);
            outFifoR.write(0.f);
        }

        prevInL.fill(0.f);
        prevInR.fill(0.f);
        predelayL.clear();
        predelayR.clear();
        tiltL.reset();
        tiltR.reset();

        histW = 0;
        isCrossfading = false;
        if (state.load(std::memory_order_acquire) == kSwapping) state.store(kIdle, std::memory_order_release);
    }

    // Off the audio thread (preset load / prepareToPlay / offline render): build the IR for these settings now and
    // block until it is ready, so the first audio block already uses it and renders are deterministic.
    void prime(const float* k, int model) override {
        reqModel.store(model, std::memory_order_relaxed);
        reqSize.store(fxKnob(FX_Innerspace, 0).value(k[0]), std::memory_order_relaxed);
        reqDecay.store(clampf(fxKnob(FX_Innerspace, 1).value(k[1]), 0.05f, 1.f), std::memory_order_relaxed);
        const uint32_t mine = reqSerial.fetch_add(1, std::memory_order_acq_rel) + 1;
        for (int waited = 0; waited < 3000 && builtSerial.load(std::memory_order_acquire) < mine; ++waited)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    void process(float* L, float* R, int n, const float* k, int model) override {
        // Knobs: Size(x), Decay(%), Tone(dB), Predelay(ms), Width(%)
        const float sizeNorm = fxKnob(FX_Innerspace, 0).value(k[0]);
        const float decay = clampf(fxKnob(FX_Innerspace, 1).value(k[1]), 0.05f, 1.f);
        const float toneDb = fxKnob(FX_Innerspace, 2).value(k[2]);
        const float predelayMs = fxKnob(FX_Innerspace, 3).value(k[3]);
        const float width = clampf(fxKnob(FX_Innerspace, 4).value(k[4]), 0.f, 1.f);

        // Ask the builder thread for a new IR when model/size/decay move; it coalesces bursts of requests.
        if (model != lastModel || std::abs(sizeNorm - lastSize) > 0.02f || std::abs(decay - lastDecay) > 0.02f) {
            lastModel = model; lastSize = sizeNorm; lastDecay = decay;
            reqModel.store(model, std::memory_order_relaxed);
            reqSize.store(sizeNorm, std::memory_order_relaxed);
            reqDecay.store(decay, std::memory_order_relaxed);
            reqSerial.fetch_add(1, std::memory_order_release);
        }

        const float predelaySamples = clampf(predelayMs * 0.001f * sr, 1.f, float(predelayL.mask - 2));

        for (int i = 0; i < n; ++i) {
            inFifoL.write(L[i]);
            inFifoR.write(R[i]);

            // When a full partition of B samples has arrived, process block convolution
            while (inFifoL.size() >= B) {
                processBlock();
            }

            // Read linear convolution output from FIFO
            float wetL = outFifoL.read();
            float wetR = outFifoR.read();

            // Predelay
            predelayL.push(wetL);
            predelayR.push(wetR);
            wetL = predelayL.read(predelaySamples);
            wetR = predelayR.read(predelaySamples);

            // Tone shelving tilt filter
            wetL = tiltL.tick(wetL, toneDb, sr);
            wetR = tiltR.tick(wetR, toneDb, sr);

            // Stereo Width control via mid/side scaling
            const float mid = 0.5f * (wetL + wetR);
            const float side = 0.5f * (wetL - wetR);
            wetL = mid + side * (width * 1.6f);
            wetR = mid - side * (width * 1.6f);


            L[i] = clampf(wetL, -2.5f, 2.5f);
            R[i] = clampf(wetR, -2.5f, 2.5f);
        }
    }

private:
    void processBlock() {
        // A freshly built IR is waiting in the inactive set: switch to it and crossfade over this block.
        int expected = kReady;
        if (state.compare_exchange_strong(expected, kSwapping, std::memory_order_acq_rel)) {
            activeSet.store(1 - activeSet.load(std::memory_order_relaxed), std::memory_order_release);
            isCrossfading = true;
        }
        // Overlap-save input: [prev B samples, current B samples]
        for (int i = 0; i < B; ++i) {
            timeL_re[size_t(i)] = prevInL[size_t(i)];
            timeL_re[size_t(i + B)] = inFifoL.read();
            prevInL[size_t(i)] = timeL_re[size_t(i + B)];
            timeL_im[size_t(i)] = 0.f;
            timeL_im[size_t(i + B)] = 0.f;

            timeR_re[size_t(i)] = prevInR[size_t(i)];
            timeR_re[size_t(i + B)] = inFifoR.read();
            prevInR[size_t(i)] = timeR_re[size_t(i + B)];
            timeR_im[size_t(i)] = 0.f;
            timeR_im[size_t(i + B)] = 0.f;
        }

        // Forward FFT of input block
        fft.transform(timeL_re.data(), timeL_im.data(), false);
        fft.transform(timeR_re.data(), timeR_im.data(), false);

        // Store only non-redundant bins 0..N/2 in circular input spectra history
        const int wOffset = histW * numBins;
        std::memcpy(&inputHistL_re[size_t(wOffset)], timeL_re.data(), sizeof(float) * numBins);
        std::memcpy(&inputHistL_im[size_t(wOffset)], timeL_im.data(), sizeof(float) * numBins);
        std::memcpy(&inputHistR_re[size_t(wOffset)], timeR_re.data(), sizeof(float) * numBins);
        std::memcpy(&inputHistR_im[size_t(wOffset)], timeR_im.data(), sizeof(float) * numBins);

        histW = (histW + 1);
        if (histW >= maxPartitions) histW = 0;

        // Perform frequency-domain multiplication and accumulation for bins 0..N/2
        std::fill(accL_re.begin(), accL_re.end(), 0.f);
        std::fill(accL_im.begin(), accL_im.end(), 0.f);
        std::fill(accR_re.begin(), accR_re.end(), 0.f);
        std::fill(accR_im.begin(), accR_im.end(), 0.f);

        accumulatePartitions(activeSet.load(std::memory_order_relaxed), accL_re.data(), accL_im.data(), accR_re.data(), accR_im.data());

        // Reconstruct conjugate symmetry for bins N/2+1 .. N-1 before inverse FFT
        for (int k = 1; k < N / 2; ++k) {
            accL_re[size_t(N - k)] = accL_re[size_t(k)];
            accL_im[size_t(N - k)] = -accL_im[size_t(k)];
            accR_re[size_t(N - k)] = accR_re[size_t(k)];
            accR_im[size_t(N - k)] = -accR_im[size_t(k)];
        }
        accL_im[0] = 0.f;
        accL_im[size_t(N / 2)] = 0.f;
        accR_im[0] = 0.f;
        accR_im[size_t(N / 2)] = 0.f;

        fft.transform(accL_re.data(), accL_im.data(), true);
        fft.transform(accR_re.data(), accR_im.data(), true);

        if (isCrossfading) {
            std::fill(oldL_re.begin(), oldL_re.end(), 0.f);
            std::fill(oldL_im.begin(), oldL_im.end(), 0.f);
            std::fill(oldR_re.begin(), oldR_re.end(), 0.f);
            std::fill(oldR_im.begin(), oldR_im.end(), 0.f);

            const int otherSet = 1 - activeSet.load(std::memory_order_relaxed);
            accumulatePartitions(otherSet, oldL_re.data(), oldL_im.data(), oldR_re.data(), oldR_im.data());

            for (int k = 1; k < N / 2; ++k) {
                oldL_re[size_t(N - k)] = oldL_re[size_t(k)];
                oldL_im[size_t(N - k)] = -oldL_im[size_t(k)];
                oldR_re[size_t(N - k)] = oldR_re[size_t(k)];
                oldR_im[size_t(N - k)] = -oldR_im[size_t(k)];
            }
            oldL_im[0] = 0.f;
            oldL_im[size_t(N / 2)] = 0.f;
            oldR_im[0] = 0.f;
            oldR_im[size_t(N / 2)] = 0.f;

            fft.transform(oldL_re.data(), oldL_im.data(), true);
            fft.transform(oldR_re.data(), oldR_im.data(), true);

            for (int i = 0; i < B; ++i) {
                const float t = float(i) / float(B);
                const float yL = lerpf(oldL_re[size_t(i + B)], accL_re[size_t(i + B)], t);
                const float yR = lerpf(oldR_re[size_t(i + B)], accR_re[size_t(i + B)], t);
                outFifoL.write(yL);
                outFifoR.write(yR);
            }
            isCrossfading = false;
            state.store(kIdle, std::memory_order_release);   // old set is free for the builder again
        } else {
            // Overlap-save: second half [B..2B-1] is the exact linear convolution output
            for (int i = 0; i < B; ++i) {
                outFifoL.write(accL_re[size_t(i + B)]);
                outFifoR.write(accR_re[size_t(i + B)]);
            }
        }
    }

    void accumulatePartitions(int set, float* accL_re, float* accL_im, float* accR_re, float* accR_im) const {
        const int numPart = activePartitions[set];
        for (int p = 0; p < numPart; ++p) {
            int histIdx = histW - 1 - p;
            while (histIdx < 0) histIdx += maxPartitions;
            if (histIdx >= maxPartitions) histIdx %= maxPartitions;

            const float* xrL = &inputHistL_re[size_t(histIdx * numBins)];
            const float* xiL = &inputHistL_im[size_t(histIdx * numBins)];
            const float* hrL = &partL_re[set][size_t(p * numBins)];
            const float* hiL = &partL_im[set][size_t(p * numBins)];

            const float* xrR = &inputHistR_re[size_t(histIdx * numBins)];
            const float* xiR = &inputHistR_im[size_t(histIdx * numBins)];
            const float* hrR = &partR_re[set][size_t(p * numBins)];
            const float* hiR = &partR_im[set][size_t(p * numBins)];

            for (int k = 0; k < numBins; ++k) {
                accL_re[k] += xrL[k] * hrL[k] - xiL[k] * hiL[k];
                accL_im[k] += xrL[k] * hiL[k] + xiL[k] * hrL[k];
                accR_re[k] += xrR[k] * hrR[k] - xiR[k] * hiR[k];
                accR_im[k] += xrR[k] * hiR[k] + xiR[k] * hrR[k];
            }
        }
    }

    // Builder thread: rebuilds the IR whenever a newer request exists and the audio thread isn't mid-swap.
    void workerLoop() {
        uint32_t built = reqSerial.load(std::memory_order_acquire);
        while (!quit.load(std::memory_order_acquire)) {
            const uint32_t want = reqSerial.load(std::memory_order_acquire);
            // Build when idle, or overwrite a finished-but-not-yet-swapped IR (the inactive set isn't being read).
            int s = kIdle;
            const bool claim = want != built && (state.compare_exchange_strong(s, kBuilding, std::memory_order_acq_rel)
                                                 || (s = kReady, state.compare_exchange_strong(s, kBuilding, std::memory_order_acq_rel)));
            if (claim) {
                buildIr(reqModel.load(std::memory_order_relaxed), reqSize.load(std::memory_order_relaxed), reqDecay.load(std::memory_order_relaxed));
                built = want;
                builtSerial.store(want, std::memory_order_release);
                state.store(kReady, std::memory_order_release);
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        }
    }
    void stopWorker() {
        quit.store(true, std::memory_order_release);
        if (worker.joinable()) worker.join();
    }

    // Synthesises an IR into the INACTIVE spectrum set (never the one the audio thread is reading).
    void buildIr(int model, float sizeNorm, float decay) {
        const int targetSet = 1 - activeSet.load(std::memory_order_acquire);

        // Base durations (s) per model
        static constexpr float baseDurations[8] = {
            1.6f, 1.4f, 2.2f, 1.8f, 2.8f, 2.5f, 0.45f, 2.6f
        };

        const float durationSec = clampf(baseDurations[model] * sizeNorm * (0.4f + 0.9f * decay), 0.15f, 3.8f);
        int numPart = int(durationSec * sr / float(B)) + 1;
        if (numPart < 2) numPart = 2;
        if (numPart > maxPartitions) numPart = maxPartitions;

        const int totalSamples = numPart * B;
        std::fill(irScratchL.begin(), irScratchL.begin() + totalSamples, 0.f);
        std::fill(irScratchR.begin(), irScratchR.begin() + totalSamples, 0.f);

        Rng synthRng(0x19940422u + uint32_t(model * 37));

        if (model == 0) { // Coin Dropper: accelerating metallic clicks
            float dt = 0.16f * sizeNorm;
            float t = 0.005f;
            int bounce = 0;
            static constexpr float partL[4] = { 2850.f, 4320.f, 6180.f, 8420.f };
            static constexpr float partR[4] = { 2920.f, 4410.f, 6050.f, 8580.f };

            while (t < durationSec && dt > 0.002f) {
                const int s0 = int(t * sr);
                const float amp = std::pow(0.90f, float(bounce));
                const float tau = 0.022f * (0.5f + 0.7f * decay);
                const int clickLen = std::min(int(tau * 6.f * sr), totalSamples - s0);

                for (int m = 0; m < clickLen && (s0 + m) < totalSamples; ++m) {
                    const float u = float(m) / sr;
                    const float env = amp * std::exp(-u / tau);
                    float sL = 0.f, sR = 0.f;
                    for (int p = 0; p < 4; ++p) {
                        sL += fsin(partL[p] * u);
                        sR += fsin(partR[p] * u);
                    }
                    irScratchL[size_t(s0 + m)] += env * sL * 0.25f;
                    irScratchR[size_t(s0 + m)] += env * sR * 0.25f;
                }
                t += dt;
                dt *= 0.88f;
                ++bounce;
            }

        } else if (model == 1) { // Spring Tank: dispersive downward chirps
            const float echoInterval = 0.035f * sizeNorm;
            int echo = 0;
            for (float t = 0.002f; t < durationSec; t += echoInterval, ++echo) {
                const int s0L = int(t * sr);
                const int s0R = int((t + 0.003f) * sr);
                const float amp = std::exp(-float(echo) * 0.40f / (0.3f + 0.7f * decay));
                const float chirpLenSec = 0.026f;
                const int chirpSamples = int(chirpLenSec * sr);
                const float fScale = std::pow(0.85f, float(echo));

                for (int m = 0; m < chirpSamples; ++m) {
                    const float u = float(m) / sr;
                    const float env = amp * (1.0f - u / chirpLenSec);
                    // Dispersive chirp: downward frequency sweep
                    const float fL = (4400.f * std::exp(-u / 0.008f) + 380.f) * fScale;
                    const float fR = (4100.f * std::exp(-u / 0.009f) + 360.f) * fScale;

                    if (s0L + m < totalSamples) irScratchL[size_t(s0L + m)] += env * fsin(fL * u);
                    if (s0R + m < totalSamples) irScratchR[size_t(s0R + m)] += env * fsin(fR * u);
                }
            }

        } else if (model == 2) { // Metal Box: dense modal ring (~40 inharmonic modes)
            for (int m = 0; m < 40; ++m) {
                const float frac = float(m + 1) / 40.f;
                const float fm = 200.f + 5600.f * std::pow(frac, 1.35f) + 35.f * std::sin(float(m) * 2.3f);
                const float tau = (0.25f + 2.4f * decay) * (1.0f - 0.65f * frac) * sizeNorm;
                const float phL = float(m) * 1.618f;
                const float phR = phL + 0.35f + float(m) * 0.2f;

                for (int i = 0; i < totalSamples; ++i) {
                    const float u = float(i) / sr;
                    const float env = std::exp(-u / tau);
                    if (env < 0.001f && i > 1000) break;
                    irScratchL[size_t(i)] += env * fsin(fm * u + phL) * 0.12f;
                    irScratchR[size_t(i)] += env * fsin(fm * u + phR) * 0.12f;
                }
            }

        } else if (model == 3) { // Glass Jar: high-Q resonances + short bright noise burst
            static constexpr float glassFreqs[5] = { 640.f, 1180.f, 1790.f, 2430.f, 3120.f };
            const float tau = (0.8f + 2.8f * decay) * sizeNorm;

            // Short bright noise burst (~12 ms)
            const int burstLen = std::min(int(0.015f * sr), totalSamples);
            for (int i = 0; i < burstLen; ++i) {
                const float env = std::exp(-float(i) / (0.003f * sr));
                irScratchL[size_t(i)] += synthRng.bipolar() * env * 0.6f;
                irScratchR[size_t(i)] += synthRng.bipolar() * env * 0.6f;
            }

            for (int p = 0; p < 5; ++p) {
                const float fL = glassFreqs[p];
                const float fR = glassFreqs[p] * 1.005f; // slightly detuned
                for (int i = 0; i < totalSamples; ++i) {
                    const float u = float(i) / sr;
                    const float env = std::exp(-u / tau);
                    if (env < 0.0005f && i > 1000) break;
                    irScratchL[size_t(i)] += env * fsin(fL * u) * 0.35f;
                    irScratchR[size_t(i)] += env * fsin(fR * u + 0.25f) * 0.35f;
                }
            }

        } else if (model == 4) { // Piano Lid: sympathetic strings with slow beating
            static constexpr float chordNotes[10] = {
                65.4f, 98.0f, 130.8f, 164.8f, 196.0f, 261.6f, 329.6f, 392.0f, 523.3f, 659.3f
            };
            const float tau = (1.2f + 3.2f * decay) * sizeNorm;

            for (int c = 0; c < 10; ++c) {
                const float f0 = chordNotes[c];
                for (int h = 1; h <= 2; ++h) {
                    const float fh = f0 * float(h);
                    if (fh > 0.45f * sr) continue;
                    for (int i = 0; i < totalSamples; ++i) {
                        const float u = float(i) / sr;
                        // Soft attack ~35 ms
                        const float att = 1.0f - std::exp(-u / 0.035f);
                        const float env = att * std::exp(-u / tau);
                        if (env < 0.0005f && i > 2000) break;
                        // Slow beating between coupled string modes (~0.4 Hz)
                        const float beat = fsin(0.4f * u);
                        irScratchL[size_t(i)] += env * (fsin(fh * u) + beat * 0.3f) * 0.15f;
                        irScratchR[size_t(i)] += env * (fsin(fh * 1.002f * u + 0.3f) - beat * 0.3f) * 0.15f;
                    }
                }
            }

        } else if (model == 5) { // Cave Drip: long dark diffuse tail with random drips
            const float tau = (1.0f + 2.5f * decay) * sizeNorm;
            // Dark diffuse noise tail
            float noiseL = 0.f, noiseR = 0.f;
            for (int i = 0; i < totalSamples; ++i) {
                const float u = float(i) / sr;
                const float env = std::exp(-u / tau);
                noiseL += 0.04f * (synthRng.bipolar() - noiseL); // dark lowpass noise
                noiseR += 0.04f * (synthRng.bipolar() - noiseR);
                irScratchL[size_t(i)] = noiseL * env * 0.6f;
                irScratchR[size_t(i)] = noiseR * env * 0.6f;
            }

            // Occasional pitched drips
            static constexpr float dripTimes[5] = { 0.18f, 0.45f, 0.85f, 1.35f, 1.95f };
            for (int d = 0; d < 5; ++d) {
                const float t = dripTimes[d] * sizeNorm;
                if (t >= durationSec) break;
                const int s0 = int(t * sr);
                const float dripFreq = 1850.f + float(d) * 190.f;
                const int dripLen = std::min(int(0.04f * sr), totalSamples - s0);

                for (int m = 0; m < dripLen; ++m) {
                    const float u = float(m) / sr;
                    const float env = std::exp(-u / 0.012f);
                    const float s = fsin((dripFreq + 300.f * u) * u) * env * 0.4f;
                    if (d & 1) {
                        irScratchR[size_t(s0 + m)] += s;
                    } else {
                        irScratchL[size_t(s0 + m)] += s;
                    }
                }
            }

        } else if (model == 6) { // Paper Tube: hollow tube resonance (~150 Hz)
            const float tau = (0.12f + 0.40f * decay) * sizeNorm;
            for (int h = 0; h < 9; ++h) {
                // Odd harmonics of closed-open tube (150, 450, 750, 1050 Hz...)
                const float fh = 150.f * float(2 * h + 1);
                if (fh > 0.45f * sr) continue;
                const float hTau = tau * std::exp(-float(h) * 0.28f);

                for (int i = 0; i < totalSamples; ++i) {
                    const float u = float(i) / sr;
                    const float env = std::exp(-u / hTau);
                    if (env < 0.001f && i > 500) break;
                    irScratchL[size_t(i)] += env * fsin(fh * u) * 0.25f;
                    irScratchR[size_t(i)] += env * fsin(fh * u + 0.15f) * 0.25f;
                }
            }

        } else { // Gong Wash: inharmonic shimmer swelling over ~0.35 s
            const float tau = (1.2f + 2.8f * decay) * sizeNorm;
            for (int m = 0; m < 32; ++m) {
                const float fm = 160.f * (1.0f + 1.25f * float(m) + 0.06f * std::pow(float(m), 1.4f));
                if (fm > 0.45f * sr) continue;

                for (int i = 0; i < totalSamples; ++i) {
                    const float u = float(i) / sr;
                    // Swell energy over ~0.35 s then exponential decay
                    float env = 0.f;
                    if (u < 0.35f) {
                        env = std::sin(kPi * 0.5f * u / 0.35f);
                    } else {
                        env = std::exp(-(u - 0.35f) / tau);
                    }
                    if (env < 0.0005f && i > 3000) break;
                    irScratchL[size_t(i)] += env * fsin(fm * u + float(m)) * 0.10f;
                    irScratchR[size_t(i)] += env * fsin(fm * u + float(m) + 0.3f) * 0.10f;
                }
            }
        }

        // Energy normalisation: unit total energy per channel
        // scale so sum of squares of that channel's IR == 1.0
        double sumSqL = 0.0, sumSqR = 0.0;
        for (int i = 0; i < totalSamples; ++i) {
            sumSqL += double(irScratchL[size_t(i)]) * double(irScratchL[size_t(i)]);
            sumSqR += double(irScratchR[size_t(i)]) * double(irScratchR[size_t(i)]);
        }
        const float normScaleL = (sumSqL > 1e-12) ? float(1.0 / std::sqrt(sumSqL)) : 0.f;
        const float normScaleR = (sumSqR > 1e-12) ? float(1.0 / std::sqrt(sumSqR)) : 0.f;

        // Global wet trim and per-model trim to ensure resonant models stay within 2x on chords
        static constexpr float modelTrim[8] = {
            // Calibrated from the FX torture test so each model's default wet RMS is ~1x the input.
            2.00f, // Coin Dropper
            0.30f, // Spring Tank
            1.12f, // Metal Box (modal ring)
            1.74f, // Glass Jar (high-Q resonances)
            0.11f, // Piano Lid (sympathetic strings ring hard on matching chords)
            0.30f, // Cave Drip
            1.60f, // Paper Tube
            2.20f  // Gong Wash
        };
        const float globalTrim = 0.95f;
        const float scaleL = normScaleL * globalTrim * modelTrim[model];
        const float scaleR = normScaleR * globalTrim * modelTrim[model];

        for (int i = 0; i < totalSamples; ++i) {
            irScratchL[size_t(i)] *= scaleL;
            irScratchR[size_t(i)] *= scaleR;
        }

        // Partition the IR and compute FFT for each partition (store only bins 0..N/2)
        for (int p = 0; p < numPart; ++p) {
            for (int i = 0; i < B; ++i) {
                timeRe[size_t(i)] = irScratchL[size_t(p * B + i)];
                timeRe[size_t(i + B)] = 0.f; // Zero-padded second half
                timeIm[size_t(i)] = 0.f;
                timeIm[size_t(i + B)] = 0.f;
            }
            fft.transform(timeRe.data(), timeIm.data(), false);
            std::memcpy(&partL_re[targetSet][size_t(p * numBins)], timeRe.data(), sizeof(float) * numBins);
            std::memcpy(&partL_im[targetSet][size_t(p * numBins)], timeIm.data(), sizeof(float) * numBins);

            for (int i = 0; i < B; ++i) {
                timeRe[size_t(i)] = irScratchR[size_t(p * B + i)];
                timeRe[size_t(i + B)] = 0.f;
                timeIm[size_t(i)] = 0.f;
                timeIm[size_t(i + B)] = 0.f;
            }
            fft.transform(timeRe.data(), timeIm.data(), false);
            std::memcpy(&partR_re[targetSet][size_t(p * numBins)], timeRe.data(), sizeof(float) * numBins);
            std::memcpy(&partR_im[targetSet][size_t(p * numBins)], timeIm.data(), sizeof(float) * numBins);
        }

        activePartitions[targetSet] = numPart;
    }

    float sr = 48000.f;
    int maxPartitions = 750;
    Radix2Fft<N> fft;

    std::vector<float> partL_re[2], partL_im[2];
    std::vector<float> partR_re[2], partR_im[2];
    int activePartitions[2] = { 2, 2 };
    std::atomic<int> activeSet { 0 };
    bool isCrossfading = false;
    // Builder hand-off: Idle -> Building (worker writes inactive set) -> Ready -> Swapping (audio crossfades) -> Idle.
    enum { kIdle, kBuilding, kReady, kSwapping };
    std::atomic<int> state { kIdle };
    std::atomic<bool> quit { false };
    std::atomic<uint32_t> reqSerial { 0 }, builtSerial { 0 };
    std::atomic<int> reqModel { 0 };
    std::atomic<float> reqSize { 1.f }, reqDecay { 0.6f };
    std::thread worker;

    std::vector<float> inputHistL_re, inputHistL_im;
    std::vector<float> inputHistR_re, inputHistR_im;
    int histW = 0;

    std::array<float, B> prevInL {}, prevInR {};
    SpaceFifo inFifoL, inFifoR;
    SpaceFifo outFifoL, outFifoR;

    std::vector<float> irScratchL, irScratchR;
    DelayLine predelayL, predelayR;
    SpaceTiltFilter tiltL, tiltR;

    int lastModel = -1;
    float lastSize = -1.f;
    float lastDecay = -1.f;

    // Preallocated scratch arrays for processBlock and buildIr
    std::array<float, N> timeL_re {}, timeL_im {};
    std::array<float, N> timeR_re {}, timeR_im {};
    std::array<float, N> accL_re {}, accL_im {};
    std::array<float, N> accR_re {}, accR_im {};
    std::array<float, N> oldL_re {}, oldL_im {};
    std::array<float, N> oldR_re {}, oldR_im {};
    std::array<float, N> timeRe {}, timeIm {};
};

// =============================================================================
// 4. FX_Shimmer: Shimmer
// =============================================================================
// Reverb with pitch shifter in feedback path: reverb output -> pitch shift ->
// tone filter -> feedback scale -> fastTanh soft limiting -> reverb input.
// Models:
// 0: Octave Up       - +12 semitones
// 1: Fifth Up        - +7 semitones
// 2: Octave Down     - -12 semitones
// 3: Detune Doubler  - ±Detune only (no transposition; lush chorused tail)
// 4: Crystal         - +12 and +19 semitones in parallel
class ShimmerFx : public FxUnit {
public:
    static constexpr int N = 8;

    void prepare(float sampleRate, int maxBlock) override {
        (void)maxBlock;
        sr = sampleRate;
        const int maxLineSamples = int(sr * 0.25f) + 1024;
        for (auto& d : lines) d.init(maxLineSamples);
        for (auto& p : preDiff) p.init(int(sr * 0.04f) + 128);

        const int pitchBufSamples = int(sr * 0.15f) + 1024;
        shifterLA.init(pitchBufSamples);
        shifterLB.init(pitchBufSamples);
        shifterRA.init(pitchBufSamples);
        shifterRB.init(pitchBufSamples);

        shifterLA.setWindow(0.065f, sr);
        shifterLB.setWindow(0.065f, sr);
        shifterRA.setWindow(0.065f, sr);
        shifterRB.setWindow(0.065f, sr);

        dampL.setCutoff(8000.f, sr);
        dampR.setCutoff(8000.f, sr);
        reset();
    }

    void reset() override {
        for (auto& d : lines) d.clear();
        for (auto& p : preDiff) p.clear();
        shifterLA.reset();
        shifterLB.reset();
        shifterRA.reset();
        shifterRB.reset();
        for (int k = 0; k < N; ++k) lpDamp[k] = 0.f;
        fbStateL = fbStateR = 0.f;
        modPh = 0.f;
    }

    void process(float* L, float* R, int n, const float* k, int model) override {
        // Knobs: Feedback(%), Shift Mix(%), Size(%), Decay(s), Tone(Hz), Detune(ct)
        const float feedback = clampf(fxKnob(FX_Shimmer, 0).value(k[0]), 0.f, 0.98f);
        const float shiftMix = clampf(fxKnob(FX_Shimmer, 1).value(k[1]), 0.f, 1.f);
        const float sizeNorm = clampf(fxKnob(FX_Shimmer, 2).value(k[2]), 0.1f, 1.f);
        const float decaySec = fxKnob(FX_Shimmer, 3).value(k[3]);
        const float toneHz = fxKnob(FX_Shimmer, 4).value(k[4]);
        const float detuneCt = fxKnob(FX_Shimmer, 5).value(k[5]);

        dampL.setCutoff(toneHz, sr);
        dampR.setCutoff(toneHz, sr);

        // Semitone transposition per model
        float baseStA = 12.f;
        float baseStB = 12.f;
        bool dualShifter = false;

        switch (model) {
        case 0: baseStA = 12.f; break;                           // Octave Up (+12)
        case 1: baseStA = 7.f;  break;                           // Fifth Up (+7)
        case 2: baseStA = -12.f; break;                          // Octave Down (-12)
        case 3: baseStA = 0.f;   break;                          // Detune Doubler (0)
        case 4: baseStA = 12.f; baseStB = 19.f; dualShifter = true; break; // Crystal (+12 & +19)
        }

        // Opposite detune for L and R
        const float detuneSt = detuneCt / 100.f;
        const float ratioLA = semitoneRatio(baseStA + detuneSt);
        const float ratioRA = semitoneRatio(baseStA - detuneSt);
        const float ratioLB = semitoneRatio(baseStB + detuneSt);
        const float ratioRB = semitoneRatio(baseStB - detuneSt);

        // FDN base lengths (ms)
        static constexpr float baseLengths[N] = {
            23.3f, 29.1f, 35.7f, 41.3f, 47.9f, 53.7f, 61.1f, 67.3f
        };
        const float sizeScale = 0.5f + 1.2f * sizeNorm;
        float lineSamples[N];
        float decayGain[N];

        for (int i = 0; i < N; ++i) {
            lineSamples[i] = baseLengths[i] * 0.001f * sizeScale * sr;
            decayGain[i] = std::pow(10.f, -3.f * lineSamples[i] / (decaySec * sr));
        }

        static constexpr float apMs[4] = { 3.7f, 5.9f, 9.1f, 13.7f };
        static constexpr float injSign[8] = { 1.f, -1.f, 1.f, -1.f, -1.f, 1.f, -1.f, 1.f };

        for (int i = 0; i < n; ++i) {
            const float inL = L[i];
            const float inR = R[i];

            // Reverb input: dry signal + pitch-shifted feedback
            const float dryIn = 0.5f * (inL + inR);
            const float fbIn = 0.5f * (fbStateL + fbStateR);
            // Reduced input gain into reverb FDN to prevent runaway accumulation
            float revIn = dryIn * 0.28f + fbIn * 0.40f;

            // Input diffusion allpasses
            for (int a = 0; a < 4; ++a) {
                const float d = preDiff[a].read(apMs[a] * 0.001f * sr);
                const float v = revIn + 0.6f * d;
                preDiff[a].push(v);
                revIn = d - 0.6f * v;
            }

            modPh += 0.35f / sr;
            if (modPh >= 1.f) modPh -= 1.f;

            float y[N];
            for (int k = 0; k < N; ++k) {
                const float mod = 3.0f * fsin(modPh + float(k) * 0.125f);
                float val = lines[k].read(lineSamples[k] + mod);
                lpDamp[k] += 0.25f * (val - lpDamp[k]);
                val = lpDamp[k] * decayGain[k];
                if (std::abs(val) < 1e-15f) val = 0.f;
                y[k] = val;
            }

            // Fast Walsh-Hadamard 8x8
            for (int h = 1; h < N; h <<= 1) {
                for (int a = 0; a < N; a += h * 2) {
                    for (int b = a; b < a + h; ++b) {
                        const float x = y[b];
                        const float z = y[b + h];
                        y[b] = x + z;
                        y[b + h] = x - z;
                    }
                }
            }

            float oL = 0.f, oR = 0.f;
            constexpr float fdnNorm = 0.35355339f;

            for (int k = 0; k < N; ++k) {
                const float v = y[k] * fdnNorm;
                lines[k].push(v + revIn * (0.35f * injSign[k]));
                if (k & 1) oR += v; else oL += v;
            }

            // Pitch shift the reverb output for feedback
            float shiftL = shifterLA.tick(oL, ratioLA);
            float shiftR = shifterRA.tick(oR, ratioRA);
            if (dualShifter) {
                // Crystal: combine +12 and +19 semitones
                shiftL = 0.45f * shiftL + 0.55f * shifterLB.tick(oL, ratioLB);
                shiftR = 0.45f * shiftR + 0.55f * shifterRB.tick(oR, ratioRB);
            }

            // Shift Mix: blend between unshifted and pitch-shifted signal
            const float loopL = lerpf(oL, shiftL, shiftMix);
            const float loopR = lerpf(oR, shiftR, shiftMix);

            // Tone lowpass filter + feedback gain + fastTanh soft limiting in loop
            // Scaled so maxed feedback stays strictly stable and below 2x without clipping
            fbStateL = fastTanh(dampL.tick(loopL) * feedback * 0.75f);
            fbStateR = fastTanh(dampR.tick(loopR) * feedback * 0.75f);

            // Wet output: balanced mix so shifted component is clearly audible
            // at default Shift Mix (0.6) and Feedback (0.5), with default rms_ratio ~1.0
            // and maxed feedback staying below ~2x without clipping
            const float wetL = oL * (0.32f * (1.0f - shiftMix * 0.5f)) + shiftL * (0.42f * shiftMix) + fbStateL * 0.22f;
            const float wetR = oR * (0.32f * (1.0f - shiftMix * 0.5f)) + shiftR * (0.42f * shiftMix) + fbStateR * 0.22f;

            // x6 makeup measured by the FX torture test (default wet RMS ~1x input, maxed ~1.6x).
            L[i] = clampf(wetL * 6.f, -2.5f, 2.5f);
            R[i] = clampf(wetR * 6.f, -2.5f, 2.5f);
        }
    }

private:
    float sr = 48000.f;
    std::array<DelayLine, N> lines;
    std::array<DelayLine, 4> preDiff;
    GranularPitchShifter shifterLA, shifterLB;
    GranularPitchShifter shifterRA, shifterRB;
    OnePole dampL, dampR;
    float lpDamp[N] {};
    float fbStateL = 0.f, fbStateR = 0.f;
    float modPh = 0.f;
};

// =============================================================================
// Factory function for Space FX group
// =============================================================================
inline std::unique_ptr<FxUnit> makeSpaceFx(int type) {
    switch (type) {
    case FX_Delay:      return std::make_unique<DelayFx>();
    case FX_ProVerb:    return std::make_unique<ProVerbFx>();
    case FX_Innerspace: return std::make_unique<InnerspaceFx>();
    case FX_Shimmer:    return std::make_unique<ShimmerFx>();
    default:            return nullptr;
    }
}

} // namespace op
