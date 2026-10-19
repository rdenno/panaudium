// Panaudium FX group "Modulation": Stompbox, Phaser, Chorus, Flanger, Tremolo, Rotary. AGPL-3.0 (see LICENSE).
#pragma once
#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>
#include "../Dsp.h"
#include "FxUnit.h"

namespace op {

// ============================================================================
// Helper: 1st order TPT allpass filter H(s) = (s - w0)/(s + w0)
// ============================================================================
struct Allpass1 {
    float s = 0.f;
    inline float tick(float x, float g) {
        // g = tan(pi * fc / sr)
        // In bilinear TPT: lp = v + s, v = (x - s) * g / (1 + g)
        // Allpass y = 2 * lp - x
        const float v = (x - s) * g / (1.f + g);
        const float lp = v + s;
        s = lp + v;
        return 2.f * lp - x;
    }
    void reset() { s = 0.f; }
};

// ============================================================================
// FX_Stompbox: 6 classic drive/distortion/fuzz pedals with 2x oversampling
// ============================================================================
class StompboxFx : public FxUnit {
public:
    void prepare(float sampleRate, int maxBlock) override {
        sr = sampleRate;
        maxB = maxBlock;
        const float sr2 = 2.f * sr;
        for (int ch = 0; ch < 2; ++ch) {
            svfDown[ch].set(0.44f * sr, 0.707f, sr2);
        }
        reset();
    }

    void reset() override {
        for (int ch = 0; ch < 2; ++ch) {
            prevIn[ch] = 0.f;
            svfDown[ch].reset();
            tsHp[ch].y = 0.f;
            tsTone[ch].y = 0.f;
            ffTone[ch].y = 0.f;
            ratBw[ch].y = 0.f;
            ratTone[ch].y = 0.f;
            muffLp[ch].y = 0.f;
            muffHp[ch].y = 0.f;
            bdShelf[ch].y = 0.f;
            bdEnv[ch] = 0.f;
            octDc[ch].x1 = 0.f; octDc[ch].y1 = 0.f;
            octTone[ch].y = 0.f;
            dcBlock[ch].x1 = 0.f; dcBlock[ch].y1 = 0.f;
        }
    }

    void process(float* L, float* R, int n, const float* k, int model) override {
        const float drive = fxKnob(FX_Stompbox, 0).value(k[0]);
        const float tone  = fxKnob(FX_Stompbox, 1).value(k[1]);
        const float lvlDb = fxKnob(FX_Stompbox, 2).value(k[2]);
        const float bias  = fxKnob(FX_Stompbox, 3).value(k[3]);
        const float lvlGain = dbToGain(lvlDb);
        const float sr2 = 2.f * sr;

        model = clampf(float(model), 0.f, 5.f);

        for (int i = 0; i < n; ++i) {
            float in[2] = { L[i], R[i] };
            for (int ch = 0; ch < 2; ++ch) {
                const float x = in[ch];
                const float xPrev = prevIn[ch];
                prevIn[ch] = x;

                // 2x linear upsampling
                const float s0 = 0.5f * (x + xPrev);
                const float s1 = x;

                // Sub-sample 0 through model nonlinearity
                const float y0 = processDrive(s0, ch, model, drive, tone, bias, sr2);
                float dummyL, dummyB, dummyH;
                svfDown[ch].tick(y0, dummyL, dummyB, dummyH);

                // Sub-sample 1 through model nonlinearity
                const float y1 = processDrive(s1, ch, model, drive, tone, bias, sr2);
                float downSampled;
                svfDown[ch].tick(y1, downSampled, dummyB, dummyH);

                // Output post-processing: DC blocker + level + safe limit
                float out = dcBlock[ch].tick(downSampled) * lvlGain;
                out = fastTanh(out);
                if (ch == 0) L[i] = out; else R[i] = out;
            }
        }
    }

private:
    float sr = 48000.f;
    int maxB = 512;
    float prevIn[2] = {0.f, 0.f};
    Svf svfDown[2];
    OnePole tsHp[2], tsTone[2];
    OnePole ffTone[2];
    OnePole ratBw[2], ratTone[2];
    OnePole muffLp[2], muffHp[2];
    OnePole bdShelf[2];
    float bdEnv[2] = {0.f, 0.f};
    DcBlock octDc[2];
    OnePole octTone[2];
    DcBlock dcBlock[2];

    inline float processDrive(float x, int ch, int model, float drive, float tone, float bias, float sr2) {
        switch (model) {
        case 0: {
            // Tube Screamer: 720 Hz high-pass feeding symmetric soft clipping (tanh),
            // blended back with clean low end (mid hump), gain up to +40 dB, post LP 700 Hz..6 kHz.
            tsHp[ch].setCutoff(720.f, sr2);
            const float lowClean = tsHp[ch].tick(x);
            const float highPass = x - lowClean;
            const float gain = dbToGain(6.f + drive * 34.f);
            const float clipped = fastTanh(highPass * gain);
            const float blended = clipped * 0.75f + lowClean * 0.25f;
            const float toneFc = 700.f * std::pow(6000.f / 700.f, tone);
            tsTone[ch].setCutoff(toneFc, sr2);
            const float out = tsTone[ch].tick(blended);
            const float comp = 1.6f / (1.f + 0.22f * std::sqrt(gain));
            return out * comp;
        }
        case 1: {
            // Fuzz Face: Germanium-style asymmetric hard-ish clipping, gain up to +50 dB.
            // Bias shifts operating point (gating/sputtering at high bias), gentle post low-pass.
            const float gain = dbToGain(10.f + drive * 40.f);
            const float dcBias = (bias - 0.15f) * 0.45f;
            const float v = (x + dcBias) * gain;
            float clipped = (v >= 0.f) ? (fastTanh(v * 1.3f) * 0.7f) : (fastTanh(v * 0.85f) * 0.85f);
            if (bias > 0.45f) {
                const float gate = (bias - 0.45f) * 0.35f;
                if (std::fabs(clipped) < gate) {
                    const float r = std::fabs(clipped) / gate;
                    clipped *= (r * r);
                }
            }
            const float toneFc = 1200.f * std::pow(8000.f / 1200.f, tone);
            ffTone[ch].setCutoff(toneFc, sr2);
            const float out = ffTone[ch].tick(clipped);
            const float comp = 1.5f / (1.f + 0.18f * std::sqrt(gain));
            return out * comp;
        }
        case 2: {
            // Rat: Op-amp gain up to +60 dB with bandwidth falling as drive rises;
            // hard clipping (diodes to ground), filter tone low-pass 600 Hz..15 kHz.
            const float gain = dbToGain(12.f + drive * 48.f);
            const float bwFc = 15000.f * std::pow(1000.f / 15000.f, drive);
            ratBw[ch].setCutoff(bwFc, sr2);
            const float pre = ratBw[ch].tick(x * gain);
            float clipped = clampf(pre, -0.68f, 0.68f);
            clipped = fastTanh(clipped * 1.5f) * 0.65f;
            const float toneFc = 600.f * std::pow(15000.f / 600.f, tone);
            ratTone[ch].setCutoff(toneFc, sr2);
            const float out = ratTone[ch].tick(clipped);
            const float comp = 1.8f / (1.f + 0.15f * std::sqrt(gain));
            return out * comp;
        }
        case 3: {
            // Big Muff: Two cascaded soft clipping stages, then tone stack:
            // crossfade between low-pass (~1 kHz) and high-pass (~1 kHz) giving mid scoop at noon.
            const float gain1 = dbToGain(6.f + drive * 24.f);
            const float gain2 = dbToGain(6.f + drive * 24.f);
            const float s1 = fastTanh(x * gain1);
            const float s2 = fastTanh(s1 * gain2);
            muffLp[ch].setCutoff(1000.f, sr2);
            muffHp[ch].setCutoff(1000.f, sr2);
            const float lpB = muffLp[ch].tick(s2);
            const float hpB = s2 - muffHp[ch].tick(s2);
            const float scooped = (1.f - tone) * lpB * 1.45f + tone * hpB * 1.45f;
            const float comp = 1.25f / (1.f + 0.12f * std::sqrt(gain1 * gain2));
            return scooped * comp;
        }
        case 4: {
            // Blues Driver: Mild asymmetric clipping with light dynamic compression;
            // tone = treble shelf above 2 kHz.
            const float gain = dbToGain(6.f + drive * 30.f);
            float sig = x * gain;
            bdEnv[ch] += 0.003f * (std::fabs(sig) - bdEnv[ch]);
            const float compEnv = 1.f / (1.f + 0.55f * bdEnv[ch]);
            sig *= compEnv;
            const float clipped = fastTanh(sig + 0.12f * sig * sig) - 0.02f;
            bdShelf[ch].setCutoff(2200.f, sr2);
            const float highs = clipped - bdShelf[ch].tick(clipped);
            const float trebleBoost = (tone - 0.5f) * 2.2f;
            const float out = clipped + highs * trebleBoost;
            const float comp = 1.2f / (1.f + 0.15f * std::sqrt(gain));
            return out * comp;
        }
        case 5: default: {
            // Octave Fuzz: Full-wave rectification (|x|) blended with fuzz;
            // Bias controls rectifier mix, tone = low-pass 800 Hz..10 kHz.
            const float gain = dbToGain(10.f + drive * 38.f);
            const float fuzz = fastTanh(x * gain);
            float rect = fastTanh(std::fabs(x) * gain * 1.35f);
            rect = octDc[ch].tick(rect); // remove DC from rectification
            const float wet = lerpf(fuzz, rect, bias);
            const float toneFc = 800.f * std::pow(10000.f / 800.f, tone);
            octTone[ch].setCutoff(toneFc, sr2);
            const float out = octTone[ch].tick(wet);
            const float comp = 1.4f / (1.f + 0.18f * std::sqrt(gain));
            return out * comp;
        }
        }
    }
};

// ============================================================================
// FX_Phaser: Cascaded 1st-order allpass phaser with log LFO sweeps & feedback
// ============================================================================
class PhaserFx : public FxUnit {
public:
    void prepare(float sampleRate, int maxBlock) override {
        sr = sampleRate;
        maxB = maxBlock;
        reset();
    }

    void reset() override {
        phase = 0.f;
        for (int ch = 0; ch < 2; ++ch) {
            fbState[ch] = 0.f;
            for (int s = 0; s < 12; ++s) ap[ch][s].reset();
            for (int c = 0; c < 3; ++c) bpFb[ch][c] = 0.f;
        }
    }

    void process(float* L, float* R, int n, const float* k, int model) override {
        const float rate   = fxKnob(FX_Phaser, 0).value(k[0]);
        const float color  = fxKnob(FX_Phaser, 1).value(k[1]);
        const float depth  = fxKnob(FX_Phaser, 2).value(k[2]);
        const float center = fxKnob(FX_Phaser, 3).value(k[3]);
        const float stereo = fxKnob(FX_Phaser, 4).value(k[4]);

        model = clampf(float(model), 0.f, 4.f);

        float rateMul = 1.0f;
        if (model == 1) {
            rateMul = 1.35f; // Small Stone: slightly faster sweep
        } else if (model == 3) {
            rateMul = 0.65f; // Twelve Stage: slower, smoother sweep
        }
        const float effRate = rate * rateMul;

        for (int i = 0; i < n; ++i) {
            phase += effRate / sr;
            if (phase >= 1.f) phase -= 1.f;

            float in[2] = { L[i], R[i] };
            for (int ch = 0; ch < 2; ++ch) {
                float stMul = 0.5f;
                if (model == 0) stMul = 0.35f;
                else if (model == 3) stMul = 0.85f;
                const float phOffset = (ch == 0) ? 0.f : (stereo * stMul);
                float ph = phase + phOffset;
                ph -= std::floor(ph);

                float wet = 0.f;
                switch (model) {
                case 0: {
                    // Phase 90: 4 stages, gentle negative-ish feedback, lower centre, narrower sweep
                    const float lfo = fsin(ph);
                    const float effCenter = center * 0.52f;
                    const float fc = clampf(effCenter * std::exp2(lfo * depth * 1.25f), 30.f, 0.45f * sr);
                    const float g = std::tan(kPi * fc / sr);
                    const float fb = -color * 0.38f;
                    float v = in[ch] + fastTanh(fbState[ch] * fb);
                    for (int s = 0; s < 4; ++s) v = ap[ch][s].tick(v, g);
                    fbState[ch] = v;
                    wet = 0.5f * (in[ch] + v);
                    break;
                }
                case 1: {
                    // Small Stone: 4 stages, strong positive resonant feedback, brighter centre, faster sweep
                    const float lfo = fsin(ph);
                    const float effCenter = center * 1.85f;
                    const float fc = clampf(effCenter * std::exp2(lfo * depth * 2.2f), 40.f, 0.45f * sr);
                    const float g = std::tan(kPi * fc / sr);
                    const float fb = clampf(color * 1.75f + 0.12f, 0.f, 0.93f);
                    float v = in[ch] + fastTanh(fbState[ch] * fb);
                    for (int s = 0; s < 4; ++s) v = ap[ch][s].tick(v, g);
                    fbState[ch] = v;
                    wet = 0.5f * (in[ch] + v);
                    break;
                }
                case 2: {
                    // Uni-Vibe: 4 stages with staggered break frequencies, skewed lamp LFO, mixed with dry
                    float lamp;
                    if (ph < 0.28f) lamp = std::sin(kPi * (ph / 0.28f) * 0.5f);
                    else lamp = std::cos(kPi * ((ph - 0.28f) / 0.72f) * 0.5f);
                    const float lfo = lamp * 2.f - 1.f;

                    static const float stagg[4] = { 0.4f, 1.8f, 0.7f, 2.5f };
                    const float fb = color * 0.35f;
                    float v = in[ch] + fastTanh(fbState[ch] * fb);
                    for (int s = 0; s < 4; ++s) {
                        const float fc_s = clampf(center * std::exp2(lfo * depth * 1.8f) * stagg[s], 40.f, 0.45f * sr);
                        const float g_s = std::tan(kPi * fc_s / sr);
                        v = ap[ch][s].tick(v, g_s);
                    }
                    fbState[ch] = v;
                    wet = 0.5f * in[ch] + 0.5f * v;
                    break;
                }
                case 3: {
                    // Twelve Stage: 12 stages (6 deep notches), wider sweep range, slower and smoother
                    const float lfo = fsin(ph);
                    const float effCenter = center * 0.95f;
                    const float fc = clampf(effCenter * std::exp2(lfo * depth * 3.4f), 25.f, 0.45f * sr);
                    const float g = std::tan(kPi * fc / sr);
                    const float fb = clampf(color * 0.85f, 0.f, 0.90f);
                    const float chainIn = in[ch] + fastTanh(fbState[ch] * fb);
                    float v = chainIn;
                    for (int s = 0; s < 12; ++s) v = ap[ch][s].tick(v, g);
                    fbState[ch] = v;
                    wet = 0.5f * (chainIn + v);
                    break;
                }
                case 4: default: {
                    // Barber Pole: Endless rising sweep via 3 phaser chains with sawtooth LFOs
                    // offset in phase (120 deg) and crossfaded with raised-cosine (Hann) windows
                    float sumOut = 0.f;
                    for (int c = 0; c < 3; ++c) {
                        float ph_c = ph + float(c) * (1.f / 3.f);
                        ph_c -= std::floor(ph_c);
                        const float fc_c = clampf(center * std::pow(6.f, ph_c * 2.f - 1.f), 40.f, 0.45f * sr);
                        const float g_c = std::tan(kPi * fc_c / sr);
                        const float w_c = 0.5f * (1.f - std::cos(kTwoPi * ph_c));
                        float v = in[ch] + fastTanh(bpFb[ch][c] * color * 0.7f);
                        for (int s = 0; s < 4; ++s) v = ap[ch][c * 4 + s].tick(v, g_c);
                        bpFb[ch][c] = v;
                        sumOut += w_c * 0.5f * (in[ch] + v);
                    }
                    wet = sumOut / 1.5f;
                    break;
                }
                }
                if (ch == 0) L[i] = wet; else R[i] = wet;
            }
        }
    }

private:
    float sr = 48000.f;
    int maxB = 512;
    float phase = 0.f;
    Allpass1 ap[2][12];
    float fbState[2] = {0.f, 0.f};
    float bpFb[2][3] = {{0.f, 0.f, 0.f}, {0.f, 0.f, 0.f}};
};

// ============================================================================
// FX_Chorus: Modulated fractional delay line choruses & string-machine ensembles
// ============================================================================
class ChorusFx : public FxUnit {
public:
    void prepare(float sampleRate, int maxBlock) override {
        sr = sampleRate;
        maxB = maxBlock;
        // Allocate worst-case delay buffer for up to 192 kHz (0.08 s -> ~15k samples -> 16384/32768)
        const int minBuf = int(sr * 0.08f);
        for (int ch = 0; ch < 2; ++ch) {
            for (int v = 0; v < 3; ++v) lines[ch][v].init(minBuf);
        }
        reset();
    }

    void reset() override {
        phase = 0.f;
        phFast = 0.f;
        for (int i = 0; i < 3; ++i) phTri[i] = 0.f;
        for (int ch = 0; ch < 2; ++ch) {
            for (int v = 0; v < 3; ++v) lines[ch][v].clear();
            toneFilter[ch].y = 0.f;
        }
    }

    void process(float* L, float* R, int n, const float* k, int model) override {
        const float rate    = fxKnob(FX_Chorus, 0).value(k[0]);
        const float depth   = fxKnob(FX_Chorus, 1).value(k[1]);
        const float width   = fxKnob(FX_Chorus, 2).value(k[2]);
        const float delayMs = fxKnob(FX_Chorus, 3).value(k[3]);
        const float tone    = fxKnob(FX_Chorus, 4).value(k[4]);

        model = clampf(float(model), 0.f, 4.f);

        // Tone filter cutoff
        const float toneFc = 800.f * std::pow(18000.f / 800.f, tone);
        toneFilter[0].setCutoff(toneFc, sr);
        toneFilter[1].setCutoff(toneFc, sr);

        const float baseSamples = delayMs * 0.001f * sr;

        for (int i = 0; i < n; ++i) {
            phase += rate / sr;
            if (phase >= 1.f) phase -= 1.f;

            phFast += (rate * 10.f) / sr;
            if (phFast >= 1.f) phFast -= 1.f;

            for (int t = 0; t < 3; ++t) {
                static const float rMul[3] = { 0.92f, 1.0f, 1.16f };
                phTri[t] += (rate * rMul[t]) / sr;
                if (phTri[t] >= 1.f) phTri[t] -= 1.f;
            }

            const float inL = L[i], inR = R[i];
            float wetL = 0.f, wetR = 0.f;

            switch (model) {
            case 0: {
                // CE-2: Single voice, triangle LFO, warm tone low-pass, width spreads LFO phase
                const float modL = tri(phase);
                const float modR = tri(phase + width * 0.5f);
                const float dDepth = depth * 0.003f * sr;
                lines[0][0].push(inL);
                lines[1][0].push(inR);
                wetL = lines[0][0].read(baseSamples + modL * dDepth);
                wetR = lines[1][0].read(baseSamples + modR * dDepth);
                break;
            }
            case 1: {
                // Dimension: Two voices with LFOs in antiphase (180 deg), very subtle depth,
                // wide stereo spatial crossfeed, virtually zero pitch wobble
                const float dDepth = depth * 0.00065f * sr;
                const float mod1 = fsin(phase);
                const float mod2 = fsin(phase + 0.5f);
                const float monoIn = 0.5f * (inL + inR);
                lines[0][0].push(monoIn);
                lines[1][0].push(monoIn);
                const float v1 = lines[0][0].read(baseSamples + mod1 * dDepth);
                const float v2 = lines[1][0].read(baseSamples + mod2 * dDepth);
                wetL = v1 - 0.38f * width * v2;
                wetR = v2 - 0.38f * width * v1;
                break;
            }
            case 2: {
                // Solina Ensemble: Three delay lines modulated by three LFOs 120 deg apart,
                // each LFO = slow (~0.6 Hz) + fast (~6 Hz) components scaled by Rate
                const float monoIn = 0.5f * (inL + inR);
                lines[0][0].push(monoIn);
                const float dDepth = depth * 0.0022f * sr;

                const float v0 = lines[0][0].read(baseSamples + (0.8f * fsin(phase) + 0.2f * fsin(phFast)) * dDepth);
                const float v1 = lines[0][0].read(baseSamples + (0.8f * fsin(phase + 0.3333f) + 0.2f * fsin(phFast + 0.3333f)) * dDepth);
                const float v2 = lines[0][0].read(baseSamples + (0.8f * fsin(phase + 0.6667f) + 0.2f * fsin(phFast + 0.6667f)) * dDepth);

                wetL = v0 * (0.5f + 0.5f * width) + v1 * 0.5f + v2 * (0.5f - 0.5f * width);
                wetR = v0 * (0.5f - 0.5f * width) + v1 * 0.5f + v2 * (0.5f + 0.5f * width);
                break;
            }
            case 3: {
                // Juno: Two voices, triangle LFOs in quadrature (90 deg), faster rate ("mode II"),
                // darker BBD character with slight analog dither
                const float dDepth = depth * 0.0026f * sr;
                const float mod1 = tri(phase * 1.5f);
                const float mod2 = tri(phase * 1.5f + 0.25f);
                lines[0][0].push(inL);
                lines[1][0].push(inR);
                const float v1 = lines[0][0].read(baseSamples + mod1 * dDepth);
                const float v2 = lines[1][0].read(baseSamples + mod2 * dDepth);
                const float noise = rng.bipolar() * 0.0003f;
                const float rawL = v1 + noise;
                const float rawR = v2 + noise;
                wetL = lerpf(0.5f * (rawL + rawR), rawL, width);
                wetR = lerpf(0.5f * (rawL + rawR), rawR, width);
                break;
            }
            case 4: default: {
                // Tri-Chorus: Three voices panned left/centre/right with independent smoothed multi-rate modulation
                const float monoIn = 0.5f * (inL + inR);
                lines[0][0].push(monoIn);
                const float dDepth = depth * 0.003f * sr;
                const float vL = lines[0][0].read(baseSamples + fsin(phTri[0]) * dDepth);
                const float vC = lines[0][0].read(baseSamples + fsin(phTri[1]) * dDepth);
                const float vR = lines[0][0].read(baseSamples + fsin(phTri[2]) * dDepth);
                wetL = vL * (0.6f + 0.4f * width) + vC * 0.5f + vR * (0.4f - 0.4f * width);
                wetR = vL * (0.4f - 0.4f * width) + vC * 0.5f + vR * (0.6f + 0.4f * width);
                break;
            }
            }

            L[i] = toneFilter[0].tick(wetL);
            R[i] = toneFilter[1].tick(wetR);
        }
    }

private:
    float sr = 48000.f;
    int maxB = 512;
    float phase = 0.f;
    float phFast = 0.f;
    float phTri[3] = {0.f, 0.f, 0.f};
    DelayLine lines[2][3];
    OnePole toneFilter[2];
    Rng rng;

    static inline float tri(float p) {
        p -= std::floor(p);
        return (p < 0.5f) ? (4.f * p - 1.f) : (3.f - 4.f * p);
    }
};

// ============================================================================
// FX_Flanger: Short delay around Manual swept by LFO with feedback & comb sum
// ============================================================================
class FlangerFx : public FxUnit {
public:
    void prepare(float sampleRate, int maxBlock) override {
        sr = sampleRate;
        maxB = maxBlock;
        // Manual max 12 ms + depth -> ~24 ms -> 0.05s buffer at 192 kHz
        const int minBuf = int(sr * 0.05f);
        for (int ch = 0; ch < 2; ++ch) {
            for (int l = 0; l < 3; ++l) lines[ch][l].init(minBuf);
        }
        reset();
    }

    void reset() override {
        phase = 0.f;
        for (int ch = 0; ch < 2; ++ch) {
            delState[ch] = 0.f;
            for (int l = 0; l < 3; ++l) {
                lines[ch][l].clear();
                bpDelState[ch][l] = 0.f;
            }
        }
    }

    void process(float* L, float* R, int n, const float* k, int model) override {
        const float rate     = fxKnob(FX_Flanger, 0).value(k[0]);
        const float depth    = fxKnob(FX_Flanger, 1).value(k[1]);
        const float fb       = fxKnob(FX_Flanger, 2).value(k[2]);
        const float manualMs = fxKnob(FX_Flanger, 3).value(k[3]);
        const float stereo   = fxKnob(FX_Flanger, 4).value(k[4]);

        model = clampf(float(model), 0.f, 3.f);

        float rateMul = 1.0f;
        if (model == 2) {
            rateMul = 0.85f; // Barber Pole: continuous hypnotic rise
        }
        const float effRate = rate * rateMul;

        for (int i = 0; i < n; ++i) {
            phase += effRate / sr;
            if (phase >= 1.f) phase -= 1.f;

            float in[2] = { L[i], R[i] };
            for (int ch = 0; ch < 2; ++ch) {
                float stMul = 0.5f;
                if (model == 1) stMul = 0.35f;
                else if (model == 2) stMul = 0.7f;
                const float phOffset = (ch == 0) ? 0.f : (stereo * stMul);
                float ph = phase + phOffset;
                ph -= std::floor(ph);

                float wet = 0.f;
                switch (model) {
                case 0: {
                    // Jet: Classic positive feedback, triangle LFO sweeping 0.3..12 ms
                    const float mod = 0.5f * (tri(ph) + 1.f); // 0..1
                    const float dSamples = (manualMs + depth * 10.f * mod) * 0.001f * sr;
                    const float del = lines[ch][0].read(dSamples);
                    lines[ch][0].push(in[ch] + fastTanh(del * fb * 0.92f));
                    wet = 0.5f * (in[ch] + del);
                    break;
                }
                case 1: {
                    // Through-Zero: Deep tape-style cancellation. Dry delayed by Manual in line 0,
                    // wet swept through it in line 1 so the comb collapses to zero delay each cycle.
                    const float dRef = manualMs * 0.001f * sr;
                    lines[ch][0].push(in[ch]);
                    const float dryDelayed = lines[ch][0].read(dRef);

                    // Swept line crosses dRef twice per LFO cycle (at tri(ph) == 0)
                    const float maxSweep = dRef * 0.92f * depth;
                    const float dMod = dRef + tri(ph) * maxSweep;
                    const float delMod = lines[ch][1].read(dMod);

                    // Comb difference collapses to zero when dMod == dRef
                    const float comb = dryDelayed - delMod;
                    lines[ch][1].push(in[ch] + fastTanh(comb * fb * 0.65f));

                    wet = clampf(comb * 0.85f, -2.f, 2.f);
                    break;
                }
                case 2: {
                    // Barber Pole: Continuously rising, highly resonant comb via 3 crossfaded delay lines
                    const float effFb = (fb >= 0.f)
                        ? clampf(0.35f + fb * 1.14f, 0.f, 0.92f)
                        : clampf(fb * 1.14f - 0.35f, -0.92f, 0.f);
                    float sumDel = 0.f;
                    for (int c = 0; c < 3; ++c) {
                        float ph_c = ph + float(c) * (1.f / 3.f);
                        ph_c -= std::floor(ph_c);
                        // Delay decreases as phase advances -> comb frequencies rise continuously
                        const float sawRise = 1.f - ph_c;
                        const float d_c = (manualMs + depth * 9.f * sawRise) * 0.001f * sr;
                        const float w_c = 0.5f * (1.f - std::cos(kTwoPi * ph_c));
                        const float del_c = lines[ch][c].read(d_c);
                        lines[ch][c].push(in[ch] + fastTanh(del_c * effFb));
                        sumDel += w_c * del_c;
                    }
                    wet = 0.5f * (in[ch] + (sumDel / 1.5f));
                    break;
                }
                case 3: default: {
                    // Metallic: Very short delays (0.3..2 ms), boosted feedback for robotic resonance
                    const float mod = 0.5f * (tri(ph) + 1.f);
                    const float dMs = 0.3f + (manualMs / 12.f) * 1.5f + depth * 0.6f * mod;
                    const float dSamples = dMs * 0.001f * sr;
                    const float del = lines[ch][0].read(dSamples);
                    const float effFb = clampf(fb * 1.05f, -0.96f, 0.96f);
                    lines[ch][0].push(in[ch] + fastTanh(del * effFb));
                    wet = 0.5f * (in[ch] + del);
                    break;
                }
                }

                if (ch == 0) L[i] = wet; else R[i] = wet;
            }
        }
    }

private:
    float sr = 48000.f;
    int maxB = 512;
    float phase = 0.f;
    DelayLine lines[2][3];
    float delState[2] = {0.f, 0.f};
    float bpDelState[2][3] = {{0.f, 0.f, 0.f}, {0.f, 0.f, 0.f}};

    static inline float tri(float p) {
        p -= std::floor(p);
        return (p < 0.5f) ? (4.f * p - 1.f) : (3.f - 4.f * p);
    }
};

// ============================================================================
// FX_Tremolo: Amplitude modulation, harmonic vibrato, auto-pan, stutter gate
// ============================================================================
class TremoloFx : public FxUnit {
public:
    void prepare(float sampleRate, int maxBlock) override {
        sr = sampleRate;
        maxB = maxBlock;
        for (int ch = 0; ch < 2; ++ch) {
            svfHarm[ch].set(800.f, 0.707f, sr);
        }
        reset();
    }

    void reset() override {
        phase = 0.f;
        for (int ch = 0; ch < 2; ++ch) {
            sqSmooth[ch] = 0.f;
            gateSmooth[ch] = 1.f;
            svfHarm[ch].reset();
        }
    }

    void process(float* L, float* R, int n, const float* k, int model) override {
        const float rate      = fxKnob(FX_Tremolo, 0).value(k[0]);
        const float depth     = fxKnob(FX_Tremolo, 1).value(k[1]);
        const float shape     = fxKnob(FX_Tremolo, 2).value(k[2]);
        const float stereoDeg = fxKnob(FX_Tremolo, 3).value(k[3]);

        model = clampf(float(model), 0.f, 4.f);

        const float phOffset = stereoDeg / 360.f;

        // Slew time constants for click-free gates
        const float sqSlew = 0.002f + shape * 0.02f;
        const float sqAlpha = 1.f - std::exp(-1.f / (sqSlew * sr));
        const float gateAlpha = 1.f - std::exp(-1.f / (0.0025f * sr));

        // 16-step rhythmic pattern definitions for Stutter Gate
        static const uint8_t kPatterns[4][16] = {
            { 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0 }, // straight 8ths
            { 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 1, 0 }, // dotted 8th / syncopated
            { 1, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 0, 0 }, // trance gate
            { 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0 }  // staccato 16ths
        };
        const int patIdx = clampf(int(shape * 4.f), 0, 3);

        for (int i = 0; i < n; ++i) {
            phase += rate / sr;
            if (phase >= 1.f) phase -= 1.f;

            float phL = phase;
            float phR = phase + phOffset;
            phR -= std::floor(phR);

            switch (model) {
            case 0: {
                // Sine Trem: Amplitude LFO, Shape morphs sine -> triangle
                const float sL = 0.5f * (1.f + fsin(phL));
                const float sR = 0.5f * (1.f + fsin(phR));
                const float tL = (phL < 0.5f) ? (2.f * phL) : (2.f * (1.f - phL));
                const float tR = (phR < 0.5f) ? (2.f * phR) : (2.f * (1.f - phR));
                const float lfoL = lerpf(sL, tL, shape);
                const float lfoR = lerpf(sR, tR, shape);
                L[i] *= (1.f - depth * lfoL);
                R[i] *= (1.f - depth * lfoR);
                break;
            }
            case 1: {
                // Square Trem: Square LFO with Shape controlling edge smoothing (click-free)
                const float rawL = (phL < 0.5f) ? 1.f : 0.f;
                const float rawR = (phR < 0.5f) ? 1.f : 0.f;
                sqSmooth[0] += sqAlpha * (rawL - sqSmooth[0]);
                sqSmooth[1] += sqAlpha * (rawR - sqSmooth[1]);
                L[i] *= (1.f - depth * sqSmooth[0]);
                R[i] *= (1.f - depth * sqSmooth[1]);
                break;
            }
            case 2: {
                // Harmonic Trem: Crossover split at ~800 Hz; modulate lows and highs in antiphase
                float lowL, bpL, highL, lowR, bpR, highR;
                svfHarm[0].tick(L[i], lowL, bpL, highL);
                svfHarm[1].tick(R[i], lowR, bpR, highR);
                const float gLowL  = 1.f - depth * 0.5f * (1.f + fsin(phL));
                const float gHighL = 1.f - depth * 0.5f * (1.f - fsin(phL));
                const float gLowR  = 1.f - depth * 0.5f * (1.f + fsin(phR));
                const float gHighR = 1.f - depth * 0.5f * (1.f - fsin(phR));
                L[i] = lowL * gLowL + highL * gHighL;
                R[i] = lowR * gLowR + highR * gHighR;
                break;
            }
            case 3: {
                // Auto-Pan: Equal-power pan between channels driven by LFO
                const float p = fsin(phL); // -1..+1
                const float pan = 0.5f + 0.5f * depth * p; // 0..1
                const float gL = std::cos(kPi * 0.5f * pan);
                const float gR = std::sin(kPi * 0.5f * pan);
                const float mid = 0.5f * (L[i] + R[i]);
                const float side = 0.5f * (L[i] - R[i]);
                L[i] = mid * gL * 1.414f + side * (1.f - depth);
                R[i] = mid * gR * 1.414f - side * (1.f - depth);
                break;
            }
            case 4: default: {
                // Stutter Gate: 16-step rhythmic pattern with short attack/release ramps
                const int step = int(phase * 16.f) & 15;
                const float tgt = kPatterns[patIdx][step] ? 1.f : (1.f - depth);
                gateSmooth[0] += gateAlpha * (tgt - gateSmooth[0]);
                gateSmooth[1] += gateAlpha * (tgt - gateSmooth[1]);
                L[i] *= gateSmooth[0];
                R[i] *= gateSmooth[1];
                break;
            }
            }
        }
    }

private:
    float sr = 48000.f;
    int maxB = 512;
    float phase = 0.f;
    float sqSmooth[2] = {0.f, 0.f};
    float gateSmooth[2] = {1.f, 1.f};
    Svf svfHarm[2];
};

// ============================================================================
// FX_Rotary: Rotating speaker cabinet (Leslie 122 / 147 / Fender Vibratone)
// ============================================================================
class RotaryFx : public FxUnit {
public:
    void prepare(float sampleRate, int maxBlock) override {
        sr = sampleRate;
        maxB = maxBlock;
        // Doppler buffer ~ 5 ms at 192 kHz -> 4096 is ample
        const int minBuf = int(sr * 0.02f);
        hornDelay.init(minBuf);
        drumDelay.init(minBuf);
        vibDelay.init(minBuf);
        reset();
    }

    void reset() override {
        hornPhase = 0.f;
        drumPhase = 0.f;
        vibPhase = 0.f;
        curHornHz = 0.8f;
        curDrumHz = 0.7f;
        curVibHz = 1.0f;
        hornDelay.clear();
        drumDelay.clear();
        vibDelay.clear();
        for (int ch = 0; ch < 2; ++ch) {
            svfCross[ch].reset();
            vibLp[ch].y = 0.f;
            vibHp[ch].y = 0.f;
            hornTone[ch].y = 0.f;
        }
    }

    void process(float* L, float* R, int n, const float* k, int model) override {
        const float speed    = fxKnob(FX_Rotary, 0).value(k[0]);
        const float accel    = fxKnob(FX_Rotary, 1).value(k[1]);
        const float drive    = fxKnob(FX_Rotary, 2).value(k[2]);
        const float hornDrum = fxKnob(FX_Rotary, 3).value(k[3]);
        const float micDist  = fxKnob(FX_Rotary, 4).value(k[4]);

        model = clampf(float(model), 0.f, 2.f);

        // Target speeds
        float hornTgt = 0.8f, drumTgt = 0.7f, vibTgt = 1.0f;
        if (model == 0) {
            // Leslie 122: classic darker/warmer cabinet, slow chorale ~0.75 Hz horn / ~0.65 Hz drum
            hornTgt = lerpf(0.75f, 6.6f, speed);
            drumTgt = lerpf(0.65f, 5.6f, speed);
        } else if (model == 1) {
            // Leslie 147: slightly faster slow-speed ~1.15 Hz horn / ~0.95 Hz drum
            hornTgt = lerpf(1.15f, 7.2f, speed);
            drumTgt = lerpf(0.95f, 6.0f, speed);
        } else {
            // Vibratone: single rotor 1.0 / 6.5 Hz
            vibTgt = lerpf(1.0f, 6.5f, speed);
        }

        // Inertia: Horn speeds up faster than heavy drum rotor
        const float hAlpha = 1.f - std::exp(-float(n) / (std::max(0.05f, accel) * sr));
        const float dAlpha = 1.f - std::exp(-float(n) / (std::max(0.05f, accel * 2.5f) * sr));
        curHornHz += hAlpha * (hornTgt - curHornHz);
        curDrumHz += dAlpha * (drumTgt - curDrumHz);
        curVibHz  += hAlpha * (vibTgt  - curVibHz);

        if (model == 2) {
            // Vibratone: Guitar-cab bandpass (~250 Hz - 4 kHz), single rotor
            const float driveGain = 1.f + drive * 5.5f;
            for (int ch = 0; ch < 2; ++ch) {
                vibHp[ch].setCutoff(250.f, sr);
                vibLp[ch].setCutoff(4000.f, sr);
            }
            for (int i = 0; i < n; ++i) {
                vibPhase += curVibHz / sr;
                if (vibPhase >= 1.f) vibPhase -= 1.f;

                const float inL = fastTanh(L[i] * driveGain);
                const float inR = fastTanh(R[i] * driveGain);

                const float mid = 0.5f * (inL + inR);
                const float cabIn = vibLp[0].tick(mid - vibHp[0].tick(mid));

                vibDelay.push(cabIn);

                // Doppler modulation
                const float modD = (1.8f + 0.65f * (1.f - micDist * 0.45f) * fsin(vibPhase)) * 0.001f * sr;
                const float del = vibDelay.read(modD);

                // Amplitude modulation with virtual mics at +/-90 deg
                const float amL = 1.f - (0.4f * (1.f - micDist * 0.45f)) * 0.5f * (1.f - fsin(vibPhase + 0.25f));
                const float amR = 1.f - (0.4f * (1.f - micDist * 0.45f)) * 0.5f * (1.f - fsin(vibPhase - 0.25f));

                const float outL = del * amL * 1.3f;
                const float outR = del * amR * 1.3f;

                // Room diffusion from Mic Dist
                const float bleed = micDist * 0.25f;
                L[i] = outL * (1.f - bleed) + 0.5f * (outL + outR) * bleed;
                R[i] = outR * (1.f - bleed) + 0.5f * (outL + outR) * bleed;
            }
        } else {
            // Leslie 122 vs Leslie 147:
            // 122: Crossover split at 800 Hz, darker and warmer, slightly more drum, smooth drive
            // 147: Crossover split at 1050 Hz (~1 kHz), brighter & louder horn (+4 dB), more drive/grit
            const float crossFc = (model == 1) ? 1050.f : 800.f;
            svfCross[0].set(crossFc, 0.707f, sr);
            svfCross[1].set(crossFc, 0.707f, sr);

            const float hornGain = 2.f * hornDrum * (model == 1 ? 1.585f : 0.90f);
            const float drumGain = 2.f * (1.f - hornDrum) * (model == 1 ? 0.85f : 1.30f);

            const float driveGain = (model == 1) ? (1.f + drive * 7.2f) : (1.f + drive * 3.8f);

            // Horn tone filter setup
            if (model == 1) {
                hornTone[0].setCutoff(2500.f, sr);
                hornTone[1].setCutoff(2500.f, sr);
            } else {
                hornTone[0].setCutoff(6000.f, sr);
                hornTone[1].setCutoff(6000.f, sr);
            }

            const float hornAmBase = (model == 1) ? 0.62f : 0.45f;
            const float hornAmDepth = hornAmBase * (1.f - micDist * 0.45f);
            const float drumAmDepth = 0.35f * (1.f - micDist * 0.45f);

            for (int i = 0; i < n; ++i) {
                hornPhase += curHornHz / sr;
                if (hornPhase >= 1.f) hornPhase -= 1.f;

                drumPhase += curDrumHz / sr;
                if (drumPhase >= 1.f) drumPhase -= 1.f;

                float inL, inR;
                if (model == 1) {
                    // 147: gritty asymmetric tube saturation
                    const float pL = L[i] * driveGain;
                    const float pR = R[i] * driveGain;
                    inL = fastTanh(pL + 0.16f * pL * pL);
                    inR = fastTanh(pR + 0.16f * pR * pR);
                } else {
                    // 122: smooth symmetric tube saturation
                    inL = fastTanh(L[i] * driveGain);
                    inR = fastTanh(R[i] * driveGain);
                }

                float lowL, bpL, highL, lowR, bpR, highR;
                svfCross[0].tick(inL, lowL, bpL, highL);
                svfCross[1].tick(inR, lowR, bpR, highR);

                hornDelay.push(0.5f * (highL + highR));
                drumDelay.push(0.5f * (lowL + lowR));

                // Horn rotor: sharper Doppler and dispersion (stronger AM for 147)
                const float hornD_L = (1.8f + 0.85f * (1.f - micDist * 0.45f) * fsin(hornPhase + 0.25f)) * 0.001f * sr;
                const float hornD_R = (1.8f + 0.85f * (1.f - micDist * 0.45f) * fsin(hornPhase - 0.25f)) * 0.001f * sr;
                const float hornAm_L = 1.f - hornAmDepth * 0.5f * (1.f - fsin(hornPhase + 0.25f));
                const float hornAm_R = 1.f - hornAmDepth * 0.5f * (1.f - fsin(hornPhase - 0.25f));
                const float hornRawL = hornDelay.read(hornD_L) * hornAm_L;
                const float hornRawR = hornDelay.read(hornD_R) * hornAm_R;

                float hornOutL, hornOutR;
                if (model == 1) {
                    // 147: bright high-shelf presence boost
                    hornOutL = hornRawL + 0.35f * (hornRawL - hornTone[0].tick(hornRawL));
                    hornOutR = hornRawR + 0.35f * (hornRawR - hornTone[1].tick(hornRawR));
                } else {
                    // 122: warm vintage low-pass softening
                    hornOutL = hornTone[0].tick(hornRawL);
                    hornOutR = hornTone[1].tick(hornRawR);
                }

                // Drum rotor: milder Doppler, broader dispersion
                const float drumD_L = (2.2f + 0.35f * (1.f - micDist * 0.45f) * fsin(drumPhase + 0.25f)) * 0.001f * sr;
                const float drumD_R = (2.2f + 0.35f * (1.f - micDist * 0.45f) * fsin(drumPhase - 0.25f)) * 0.001f * sr;
                const float drumAm_L = 1.f - drumAmDepth * 0.5f * (1.f - fsin(drumPhase + 0.25f));
                const float drumAm_R = 1.f - drumAmDepth * 0.5f * (1.f - fsin(drumPhase - 0.25f));
                const float drumOutL = drumDelay.read(drumD_L) * drumAm_L;
                const float drumOutR = drumDelay.read(drumD_R) * drumAm_R;

                const float outL = hornOutL * hornGain + drumOutL * drumGain;
                const float outR = hornOutR * hornGain + drumOutR * drumGain;

                // Room diffusion from Mic Dist
                const float bleed = micDist * 0.25f;
                L[i] = outL * (1.f - bleed) + 0.5f * (outL + outR) * bleed;
                R[i] = outR * (1.f - bleed) + 0.5f * (outL + outR) * bleed;
            }
        }
    }

private:
    float sr = 48000.f;
    int maxB = 512;
    float hornPhase = 0.f;
    float drumPhase = 0.f;
    float vibPhase = 0.f;
    float curHornHz = 0.8f;
    float curDrumHz = 0.7f;
    float curVibHz = 1.0f;
    DelayLine hornDelay;
    DelayLine drumDelay;
    DelayLine vibDelay;
    Svf svfCross[2];
    OnePole vibLp[2], vibHp[2];
    OnePole hornTone[2];
};

// ============================================================================
// Factory function for Modulation group
// ============================================================================
inline std::unique_ptr<FxUnit> makeModulationFx(int type) {
    switch (type) {
    case FX_Stompbox: return std::make_unique<StompboxFx>();
    case FX_Phaser:   return std::make_unique<PhaserFx>();
    case FX_Chorus:   return std::make_unique<ChorusFx>();
    case FX_Flanger:  return std::make_unique<FlangerFx>();
    case FX_Tremolo:  return std::make_unique<TremoloFx>();
    case FX_Rotary:   return std::make_unique<RotaryFx>();
    default:          return nullptr;
    }
}

} // namespace op