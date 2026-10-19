// Panaudium FX group "Tone": EQ, Compressor, LoFi, RingShift, Wah, Tape, Amp. AGPL-3.0 (see LICENSE).
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include "FxUnit.h"
#include "../Dsp.h"

namespace op {

// =================================================================================================
// Biquad Filter (RBJ Audio EQ Cookbook, Direct Form II Transposed)
// =================================================================================================
struct Biquad {
    float b0 = 1.f, b1 = 0.f, b2 = 0.f, a1 = 0.f, a2 = 0.f;
    float s1 = 0.f, s2 = 0.f;

    void reset() { s1 = s2 = 0.f; }

    inline float tick(float x) {
        float y = b0 * x + s1;
        s1 = b1 * x - a1 * y + s2;
        s2 = b2 * x - a2 * y;
        if (std::fabs(s1) < 1e-15f) s1 = 0.f;
        if (std::fabs(s2) < 1e-15f) s2 = 0.f;
        return y;
    }

    void copyCoeffsFrom(const Biquad& o) {
        b0 = o.b0; b1 = o.b1; b2 = o.b2; a1 = o.a1; a2 = o.a2;
    }

    void setLowShelf(float fc, float gainDb, float q, float sr) {
        fc = clampf(fc, 10.f, 0.49f * sr);
        q = std::max(0.05f, q);
        const float A = std::pow(10.f, gainDb * 0.025f);
        const float w0 = kTwoPi * fc / sr;
        const float cosw0 = std::cos(w0);
        const float sinw0 = std::sin(w0);
        const float alpha = sinw0 / (2.f * q);
        const float sqrtA2 = 2.f * std::sqrt(std::max(1e-4f, A)) * alpha;
        const float ap1 = A + 1.f, am1 = A - 1.f;

        const float a0 = ap1 + am1 * cosw0 + sqrtA2;
        if (std::fabs(a0) < 1e-9f) return;
        const float inv = 1.f / a0;
        b0 = (A * (ap1 - am1 * cosw0 + sqrtA2)) * inv;
        b1 = (2.f * A * (am1 - ap1 * cosw0)) * inv;
        b2 = (A * (ap1 - am1 * cosw0 - sqrtA2)) * inv;
        a1 = (-2.f * (am1 + ap1 * cosw0)) * inv;
        a2 = (ap1 + am1 * cosw0 - sqrtA2) * inv;
    }

    void setHighShelf(float fc, float gainDb, float q, float sr) {
        fc = clampf(fc, 10.f, 0.49f * sr);
        q = std::max(0.05f, q);
        const float A = std::pow(10.f, gainDb * 0.025f);
        const float w0 = kTwoPi * fc / sr;
        const float cosw0 = std::cos(w0);
        const float sinw0 = std::sin(w0);
        const float alpha = sinw0 / (2.f * q);
        const float sqrtA2 = 2.f * std::sqrt(std::max(1e-4f, A)) * alpha;
        const float ap1 = A + 1.f, am1 = A - 1.f;

        const float a0 = ap1 - am1 * cosw0 + sqrtA2;
        if (std::fabs(a0) < 1e-9f) return;
        const float inv = 1.f / a0;
        b0 = (A * (ap1 + am1 * cosw0 + sqrtA2)) * inv;
        b1 = (-2.f * A * (am1 + ap1 * cosw0)) * inv;
        b2 = (A * (ap1 + am1 * cosw0 - sqrtA2)) * inv;
        a1 = (2.f * (am1 - ap1 * cosw0)) * inv;
        a2 = (ap1 - am1 * cosw0 - sqrtA2) * inv;
    }

    void setPeak(float fc, float gainDb, float q, float sr) {
        fc = clampf(fc, 10.f, 0.49f * sr);
        q = std::max(0.05f, q);
        const float A = std::pow(10.f, gainDb * 0.025f);
        const float w0 = kTwoPi * fc / sr;
        const float cosw0 = std::cos(w0);
        const float sinw0 = std::sin(w0);
        const float alpha = sinw0 / (2.f * q);

        const float a0 = 1.f + alpha / A;
        if (std::fabs(a0) < 1e-9f) return;
        const float inv = 1.f / a0;
        b0 = (1.f + alpha * A) * inv;
        b1 = (-2.f * cosw0) * inv;
        b2 = (1.f - alpha * A) * inv;
        a1 = (-2.f * cosw0) * inv;
        a2 = (1.f - alpha / A) * inv;
    }

    void setLowPass(float fc, float q, float sr) {
        fc = clampf(fc, 10.f, 0.49f * sr);
        q = std::max(0.05f, q);
        const float w0 = kTwoPi * fc / sr;
        const float cosw0 = std::cos(w0);
        const float sinw0 = std::sin(w0);
        const float alpha = sinw0 / (2.f * q);

        const float a0 = 1.f + alpha;
        if (std::fabs(a0) < 1e-9f) return;
        const float inv = 1.f / a0;
        b0 = (0.5f * (1.f - cosw0)) * inv;
        b1 = (1.f - cosw0) * inv;
        b2 = b0;
        a1 = (-2.f * cosw0) * inv;
        a2 = (1.f - alpha) * inv;
    }

    void setHighPass(float fc, float q, float sr) {
        fc = clampf(fc, 10.f, 0.49f * sr);
        q = std::max(0.05f, q);
        const float w0 = kTwoPi * fc / sr;
        const float cosw0 = std::cos(w0);
        const float sinw0 = std::sin(w0);
        const float alpha = sinw0 / (2.f * q);

        const float a0 = 1.f + alpha;
        if (std::fabs(a0) < 1e-9f) return;
        const float inv = 1.f / a0;
        b0 = (0.5f * (1.f + cosw0)) * inv;
        b1 = (-(1.f + cosw0)) * inv;
        b2 = b0;
        a1 = (-2.f * cosw0) * inv;
        a2 = (1.f - alpha) * inv;
    }

    void setBandPass(float fc, float q, float sr) {
        fc = clampf(fc, 10.f, 0.49f * sr);
        q = std::max(0.05f, q);
        const float w0 = kTwoPi * fc / sr;
        const float cosw0 = std::cos(w0);
        const float sinw0 = std::sin(w0);
        const float alpha = sinw0 / (2.f * q);

        const float a0 = 1.f + alpha;
        if (std::fabs(a0) < 1e-9f) return;
        const float inv = 1.f / a0;
        b0 = alpha * inv;
        b1 = 0.f;
        b2 = -alpha * inv;
        a1 = (-2.f * cosw0) * inv;
        a2 = (1.f - alpha) * inv;
    }

    void setNotch(float fc, float q, float sr) {
        fc = clampf(fc, 10.f, 0.49f * sr);
        q = std::max(0.05f, q);
        const float w0 = kTwoPi * fc / sr;
        const float cosw0 = std::cos(w0);
        const float sinw0 = std::sin(w0);
        const float alpha = sinw0 / (2.f * q);

        const float a0 = 1.f + alpha;
        if (std::fabs(a0) < 1e-9f) return;
        const float inv = 1.f / a0;
        b0 = inv;
        b1 = (-2.f * cosw0) * inv;
        b2 = inv;
        a1 = (-2.f * cosw0) * inv;
        a2 = (1.f - alpha) * inv;
    }
};

// =================================================================================================
// 1. FX_EQ — Studio EQ
// Models: Clean, Tube, Pultec Trick
// =================================================================================================
class EqUnit final : public FxUnit {
public:
    void prepare(float sampleRate, int maxBlock) override {
        sr = sampleRate; (void)maxBlock;
        reset();
    }

    void reset() override {
        for (auto& b : bqL) b.reset();
        for (auto& b : bqR) b.reset();
        for (int i = 0; i < 6; ++i) prevK[size_t(i)] = -999.f;
        prevModel = -1;
    }

    void process(float* L, float* R, int n, const float* k, int model) override {
        bool update = (model != prevModel);
        for (int i = 0; i < 6; ++i) {
            if (std::fabs(k[i] - prevK[size_t(i)]) > 1e-5f) update = true;
        }

        const float lowDb    = fxKnob(FX_EQ, 0).value(k[0]);
        const float lowFreq  = fxKnob(FX_EQ, 1).value(k[1]);
        const float midDb    = fxKnob(FX_EQ, 2).value(k[2]);
        const float midFreq  = fxKnob(FX_EQ, 3).value(k[3]);
        const float highDb   = fxKnob(FX_EQ, 4).value(k[4]);
        const float highFreq = fxKnob(FX_EQ, 5).value(k[5]);

        if (update) {
            for (int i = 0; i < 6; ++i) prevK[size_t(i)] = k[i];
            prevModel = model;

            if (model == 2) {
                // Pultec Trick: low boost + simultaneous attenuation slightly higher up;
                // high broad boost + 10 kHz shelf cut.
                if (lowDb >= 0.f) {
                    bqL[0].setLowShelf(lowFreq, lowDb * 1.15f, 0.85f, sr);
                    bqL[1].setPeak(lowFreq * 2.0f, -lowDb * 0.85f, 1.4f, sr);
                } else {
                    bqL[0].setLowShelf(lowFreq, lowDb, 0.8f, sr);
                    bqL[1].setPeak(lowFreq, 0.f, 1.f, sr);
                }
                bqL[2].setPeak(midFreq, midDb, 0.9f, sr);
                if (highDb >= 0.f) {
                    bqL[3].setPeak(highFreq, highDb, 0.6f, sr);
                    bqL[4].setHighShelf(10000.f, -highDb * 0.4f, 0.707f, sr);
                } else {
                    bqL[3].setHighShelf(highFreq, highDb, 0.707f, sr);
                    bqL[4].setHighShelf(10000.f, 0.f, 0.707f, sr);
                }
            } else {
                // Clean or Tube: low shelf, mid peak (Q ~0.9), high shelf.
                bqL[0].setLowShelf(lowFreq, lowDb, 0.707f, sr);
                bqL[1].setPeak(midFreq, midDb, 0.9f, sr);
                bqL[2].setHighShelf(highFreq, highDb, 0.707f, sr);
                if (model == 1) {
                    // Tube: subtle low-mid warmth and band-limited top (~18 kHz).
                    bqL[3].setPeak(250.f, 0.8f, 1.0f, sr);
                    bqL[4].setLowPass(18500.f, 0.707f, sr);
                } else {
                    bqL[3].setPeak(1000.f, 0.f, 1.f, sr);
                    bqL[4].setPeak(1000.f, 0.f, 1.f, sr);
                }
            }
            for (int i = 0; i < 5; ++i) bqR[size_t(i)].copyCoeffsFrom(bqL[size_t(i)]);
        }

        const float totalBoost = std::max(0.f, lowDb) + std::max(0.f, midDb) + std::max(0.f, highDb);
        const float tubeDrive = 1.f + totalBoost * 0.035f;

        for (int i = 0; i < n; ++i) {
            float l = L[i], r = R[i];
            for (int b = 0; b < 5; ++b) {
                l = bqL[size_t(b)].tick(l);
                r = bqR[size_t(b)].tick(r);
            }
            if (model == 1) {
                // Gentle asymmetric tube saturation scaling with boost.
                const float satL = fastTanh(l * tubeDrive + 0.12f * l * l) - 0.01f;
                const float satR = fastTanh(r * tubeDrive + 0.12f * r * r) - 0.01f;
                l = l * 0.65f + satL * 0.35f;
                r = r * 0.65f + satR * 0.35f;
            }
            L[i] = clampf(std::isfinite(l) ? l : 0.f, -4.f, 4.f);
            R[i] = clampf(std::isfinite(r) ? r : 0.f, -4.f, 4.f);
        }
    }

private:
    float sr = 48000.f;
    std::array<Biquad, 5> bqL, bqR;
    float prevK[6] {};
    int prevModel = -1;
};

// =================================================================================================
// 2. FX_Compressor — Feed-Forward Compressor / Pumper
// Models: VCA Bus, Opto, Vari-Mu, FET Limiter, Pump
// =================================================================================================
class CompressorUnit final : public FxUnit {
public:
    void prepare(float sampleRate, int maxBlock) override {
        sr = sampleRate; (void)maxBlock;
        bloomL.setLowShelf(90.f, 2.0f, 0.707f, sr);
        bloomR.copyCoeffsFrom(bloomL);
        reset();
    }

    void reset() override {
        envGrDb = 0.f;
        rmsEnergy = 0.f;
        pumpPhase = 0.f;
        pumpEnv = 0.f;
        bloomL.reset(); bloomR.reset();
        dcL = DcBlock{}; dcR = DcBlock{};
    }

    void process(float* L, float* R, int n, const float* k, int model) override {
        const float thresholdDb = fxKnob(FX_Compressor, 0).value(k[0]);
        const float ratio       = fxKnob(FX_Compressor, 1).value(k[1]);
        const float attackMs    = fxKnob(FX_Compressor, 2).value(k[2]);
        const float releaseMs   = fxKnob(FX_Compressor, 3).value(k[3]);
        const float makeupDb    = fxKnob(FX_Compressor, 4).value(k[4]);
        const float pumpRate    = fxKnob(FX_Compressor, 5).value(k[5]);

        const float makeupLin = dbToGain(makeupDb);

        if (model == 4) {
            // Pump: sidechain-style ducking triggered at pumpRate Hz, ignoring input level.
            // Threshold controls depth (-48 dB is deepest squash).
            const float duckDepthDb = -thresholdDb; // 0 to 48 dB
            const float dPhase = pumpRate / sr;
            const float pumpRelAlpha = 1.f - std::exp(-1.f / (std::max(1.f, releaseMs) * 0.001f * sr));

            for (int i = 0; i < n; ++i) {
                pumpPhase += dPhase;
                if (pumpPhase >= 1.f) {
                    pumpPhase -= 1.f;
                    pumpEnv = -duckDepthDb;
                }
                // Exponential recovery toward 0 dB
                pumpEnv += pumpRelAlpha * (0.f - pumpEnv);
                const float g = dbToGain(pumpEnv) * makeupLin;
                L[i] = clampf(std::isfinite(L[i]) ? L[i] * g : 0.f, -4.f, 4.f);
                R[i] = clampf(std::isfinite(R[i]) ? R[i] * g : 0.f, -4.f, 4.f);
            }
            return;
        }

        // Standard compressor parameters per model
        float effAttackMs = attackMs;
        float effReleaseMs = releaseMs;
        float kneeW = 6.f;
        float effRatio = ratio;

        if (model == 1) {
            // Opto: smooth program-dependent leveller (slow attack ~10-20 ms floor, gentle ratio)
            kneeW = 14.f;
            effAttackMs = std::max(15.f, attackMs * 1.5f);
            effRatio = 2.2f + (ratio - 1.f) * (0.8f / 19.f);
        } else if (model == 2) {
            // Vari-Mu: slower ballistics (attack x3, release x2)
            kneeW = 12.f;
            effAttackMs = attackMs * 3.0f;
            effReleaseMs = releaseMs * 2.0f;
        } else if (model == 3) {
            // FET Limiter: attack scaled down 10x, aggressive 4 dB knee
            effAttackMs = std::max(0.02f, attackMs * 0.1f);
            effReleaseMs = std::max(5.f, releaseMs * 0.6f);
            kneeW = 4.f;
            if (ratio >= 18.5f) {
                // "All-buttons-in" aggressive limiter mode
                effRatio = 20.f;
                kneeW = 2.f;
            }
        }

        const float baseAttAlpha = 1.f - std::exp(-1.f / (std::max(0.01f, effAttackMs) * 0.001f * sr));
        const float baseRelAlpha = 1.f - std::exp(-1.f / (std::max(1.f, effReleaseMs) * 0.001f * sr));
        const float rmsAlpha = 1.f - std::exp(-1.f / (0.010f * sr)); // 10ms RMS window

        for (int i = 0; i < n; ++i) {
            float inL = L[i], inR = R[i];
            float detLevel;
            if (model == 0) {
                // VCA Bus: RMS detector
                const float power = 0.5f * (inL * inL + inR * inR);
                rmsEnergy += rmsAlpha * (power - rmsEnergy);
                detLevel = std::sqrt(std::max(1e-10f, rmsEnergy));
            } else {
                // Peak detector
                detLevel = std::max(std::fabs(inL), std::fabs(inR));
            }

            const float detDb = 20.f * std::log10(std::max(1e-5f, detLevel));

            // Vari-Mu ratio increases heavily with level (heavy squash on hot chord)
            float curRatio = effRatio;
            if (model == 2) {
                const float overDb = std::max(0.f, detDb - thresholdDb);
                curRatio = 2.0f + (ratio * 0.5f) + 0.9f * overDb + 0.03f * overDb * overDb;
                curRatio = clampf(curRatio, 2.0f, 24.f);
            }

            // Log-domain soft-knee gain computer
            float targetGrDb = 0.f;
            const float diff = detDb - thresholdDb;
            if (2.f * diff < -kneeW) {
                targetGrDb = 0.f;
            } else if (2.f * std::fabs(diff) <= kneeW) {
                const float term = diff + 0.5f * kneeW;
                targetGrDb = (1.f / curRatio - 1.f) * (term * term) / (2.f * kneeW);
            } else {
                targetGrDb = (1.f / curRatio - 1.f) * diff;
            }
            if (targetGrDb > 0.f) targetGrDb = 0.f;

            // Attack / Release smoothing
            if (targetGrDb < envGrDb) {
                envGrDb += baseAttAlpha * (targetGrDb - envGrDb);
            } else {
                float relAlpha = baseRelAlpha;
                if (model == 1) {
                    // Opto two-stage release: fast initial release then slow tail
                    const float tailFactor = clampf(std::fabs(envGrDb) * 0.35f, 0.f, 1.f);
                    const float effRelTime = lerpf(effReleaseMs * 3.0f, effReleaseMs * 0.45f, tailFactor);
                    relAlpha = 1.f - std::exp(-1.f / (effRelTime * 0.001f * sr));
                }
                envGrDb += relAlpha * (targetGrDb - envGrDb);
            }

            const float gain = dbToGain(envGrDb) * makeupLin;
            float outL = inL * gain;
            float outR = inR * gain;

            if (model == 2) {
                // Vari-Mu: audible warm even-harmonic saturation + slight low-end bloom
                const float vDrive = 1.4f;
                const float dL = outL * vDrive;
                const float dR = outR * vDrive;
                float satL = fastTanh(dL + 0.22f * dL * dL);
                float satR = fastTanh(dR + 0.22f * dR * dR);
                satL = dcL.tick(satL);
                satR = dcR.tick(satR);
                outL = bloomL.tick(satL * 0.95f);
                outR = bloomR.tick(satR * 0.95f);
            } else if (model == 3) {
                // FET Limiter odd-harmonic grit when driven hard
                const float grAbs = std::fabs(envGrDb);
                const float fetDrive = 1.f + grAbs * 0.08f + (ratio >= 18.5f ? 0.35f : 0.f);
                outL = fastTanh(outL * fetDrive) / fetDrive;
                outR = fastTanh(outR * fetDrive) / fetDrive;
            }

            L[i] = clampf(std::isfinite(outL) ? outL : 0.f, -4.f, 4.f);
            R[i] = clampf(std::isfinite(outR) ? outR : 0.f, -4.f, 4.f);
        }
    }

private:
    float sr = 48000.f;
    float envGrDb = 0.f;
    float rmsEnergy = 0.f;
    float pumpPhase = 0.f;
    float pumpEnv = 0.f;
    Biquad bloomL, bloomR;
    DcBlock dcL, dcR;
};

// =================================================================================================
// 3. FX_LoFi — Bit Crusher / Degrader
// Models: Bitcrush, Sample & Hold, Vinyl, Cassette, AM Radio
// =================================================================================================
class LoFiUnit final : public FxUnit {
public:
    void prepare(float sampleRate, int maxBlock) override {
        sr = sampleRate; (void)maxBlock;
        // Allocate worst-case delay buffer for wow/flutter (192 kHz * 50ms)
        delayL.init(int(sr * 0.06f) + 1024);
        delayR.init(int(sr * 0.06f) + 1024);
        reset();
    }

    void reset() override {
        delayL.clear(); delayR.clear();
        heldL = heldR = 0.f;
        holdPhase = 0.f;
        nextHoldPeriod = 1.f;
        wowPhase = 0.f;
        fltPhase = 0.f;
        fadePhase = 0.f;
        whistlePhase = 0.f;
        crackleEnv = 0.f;
        lpL.reset(); lpR.reset();
        hpL.reset(); hpR.reset();
        hissFilt.y = 0.f;
    }

    void process(float* L, float* R, int n, const float* k, int model) override {
        const float bits       = fxKnob(FX_LoFi, 0).value(k[0]);
        const float downsample = fxKnob(FX_LoFi, 1).value(k[1]);
        const float noise      = fxKnob(FX_LoFi, 2).value(k[2]);
        const float filterHz   = fxKnob(FX_LoFi, 3).value(k[3]);
        const float crackle    = fxKnob(FX_LoFi, 4).value(k[4]);
        const float drive      = fxKnob(FX_LoFi, 5).value(k[5]);

        lpL.set(filterHz, 0.707f, sr);
        lpR.set(filterHz, 0.707f, sr);

        if (model == 2) {
            // Vinyl: 40 Hz HP, surface hiss, crackle clicks, gentle wow
            hpL.set(40.f, 0.707f, sr);
            hpR.set(40.f, 0.707f, sr);
            const float crackleProb = crackle * 0.0015f;
            hissFilt.setCutoff(3500.f, sr);

            for (int i = 0; i < n; ++i) {
                wowPhase += 0.55f / sr;
                if (wowPhase >= 1.f) wowPhase -= 1.f;
                const float modSamples = (0.005f + 0.0006f * fsin(wowPhase)) * sr;
                delayL.push(L[i]); delayR.push(R[i]);
                float l = delayL.read(modSamples);
                float r = delayR.read(modSamples);

                // Band-limit ~40 Hz to filterHz
                float lpDummy, hpL_val, hpR_val;
                hpL.tick(l, lpDummy, lpDummy, hpL_val);
                hpR.tick(r, lpDummy, lpDummy, hpR_val);
                l = lpL.lp(hpL_val);
                r = lpR.lp(hpR_val);

                // Surface hiss
                const float hiss = hissFilt.tick(rng.bipolar()) * noise * 0.035f;

                // Random sharp crackle pops
                if (rng.unipolar() < crackleProb) {
                    crackleEnv = (rng.bipolar() > 0.f ? 1.f : -1.f) * (0.15f + 0.85f * rng.unipolar()) * crackle * 1.5f;
                }
                crackleEnv *= 0.82f;

                l += hiss + crackleEnv;
                r += hiss + crackleEnv;

                L[i] = clampf(std::isfinite(l) ? l : 0.f, -4.f, 4.f);
                R[i] = clampf(std::isfinite(r) ? r : 0.f, -4.f, 4.f);
            }
            return;
        }

        if (model == 3) {
            // Cassette: tape saturation, wow/flutter delay, hiss, head-loss filter
            hissFilt.setCutoff(2800.f, sr);
            const float satDrive = 1.f + drive * 2.8f;
            const float steps = std::pow(2.f, std::max(2.f, bits) - 1.f);

            for (int i = 0; i < n; ++i) {
                wowPhase += 0.45f / sr; if (wowPhase >= 1.f) wowPhase -= 1.f;
                fltPhase += 5.8f / sr;  if (fltPhase >= 1.f) fltPhase -= 1.f;
                const float mod = (0.005f + 0.0007f * fsin(wowPhase) + 0.0002f * fsin(fltPhase)) * sr;
                delayL.push(L[i]); delayR.push(R[i]);
                float l = delayL.read(mod);
                float r = delayR.read(mod);

                // Saturation
                l = fastTanh(l * satDrive) / std::sqrt(satDrive);
                r = fastTanh(r * satDrive) / std::sqrt(satDrive);

                // Mild bit reduction only if Bits < 12
                if (bits < 12.f) {
                    l = std::round(l * steps) / steps;
                    r = std::round(r * steps) / steps;
                }

                // Head loss LP filter
                l = lpL.lp(l);
                r = lpR.lp(r);

                // Tape hiss
                const float hiss = hissFilt.tick(rng.bipolar()) * noise * 0.03f;
                l += hiss; r += hiss;

                L[i] = clampf(std::isfinite(l) ? l : 0.f, -4.f, 4.f);
                R[i] = clampf(std::isfinite(r) ? r : 0.f, -4.f, 4.f);
            }
            return;
        }

        if (model == 4) {
            // AM Radio: narrow bandpass (300 Hz HP to filterHz LP), heavy compression/saturation, static
            hpL.set(300.f, 0.707f, sr);
            hpR.set(300.f, 0.707f, sr);
            lpL.set(std::min(filterHz, 4000.f), 0.707f, sr);
            lpR.set(std::min(filterHz, 4000.f), 0.707f, sr);
            const float satDrive = 3.f + drive * 5.f;

            for (int i = 0; i < n; ++i) {
                fadePhase += 0.06f / sr; if (fadePhase >= 1.f) fadePhase -= 1.f;
                whistlePhase += 1150.f / sr; if (whistlePhase >= 1.f) whistlePhase -= 1.f;
                const float fade = 0.85f + 0.15f * fsin(fadePhase);

                float l = L[i], r = R[i];
                float lpDum, hpL_val, hpR_val;
                hpL.tick(l, lpDum, lpDum, hpL_val);
                hpR.tick(r, lpDum, lpDum, hpR_val);
                l = lpL.lp(hpL_val);
                r = lpR.lp(hpR_val);

                // Heavy saturation
                l = fastTanh(l * satDrive) * 0.55f;
                r = fastTanh(r * satDrive) * 0.55f;

                // Radio static + heterodyne tone
                const float staticNoise = rng.bipolar() * noise * 0.06f;
                const float whistle = fsin(whistlePhase) * noise * 0.015f;
                l = (l * fade) + staticNoise + whistle;
                r = (r * fade) + staticNoise + whistle;

                L[i] = clampf(std::isfinite(l) ? l : 0.f, -4.f, 4.f);
                R[i] = clampf(std::isfinite(r) ? r : 0.f, -4.f, 4.f);
            }
            return;
        }

        // Models 0 (Bitcrush) & 1 (Sample & Hold)
        const float satDrive = 1.f + drive * 4.f;
        const float steps = std::pow(2.f, std::max(2.f, bits) - 1.f);
        const bool isSh = (model == 1);

        for (int i = 0; i < n; ++i) {
            float inL = fastTanh(L[i] * satDrive);
            float inR = fastTanh(R[i] * satDrive);

            holdPhase += 1.f;
            const float curTargetHold = isSh ? nextHoldPeriod : downsample;
            if (holdPhase >= curTargetHold) {
                holdPhase = 0.f;
                heldL = inL;
                heldR = inR;
                if (isSh) {
                    // Jitter hold period randomly for aliasing glitter
                    nextHoldPeriod = std::max(1.f, downsample * (0.4f + 1.2f * rng.unipolar()));
                }
            }

            // Quantisation
            float qL = heldL, qR = heldR;
            if (bits < 15.9f) {
                qL = std::round(qL * steps) / steps;
                qR = std::round(qR * steps) / steps;
            }

            // Quantisation dither / noise
            if (noise > 0.001f) {
                qL += rng.bipolar() * noise * 0.04f;
                qR += rng.bipolar() * noise * 0.04f;
            }

            // Post lowpass filter
            float outL = lpL.lp(qL);
            float outR = lpR.lp(qR);

            L[i] = clampf(std::isfinite(outL) ? outL : 0.f, -4.f, 4.f);
            R[i] = clampf(std::isfinite(outR) ? outR : 0.f, -4.f, 4.f);
        }
    }

private:
    float sr = 48000.f;
    DelayLine delayL, delayR;
    float heldL = 0.f, heldR = 0.f;
    float holdPhase = 0.f;
    float nextHoldPeriod = 1.f;
    float wowPhase = 0.f, fltPhase = 0.f;
    float fadePhase = 0.f, whistlePhase = 0.f;
    float crackleEnv = 0.f;
    Svf lpL, lpR, hpL, hpR;
    OnePole hissFilt;
    Rng rng;
};

// =================================================================================================
// 4. FX_RingShift — Ring Modulator & Single-Sideband Frequency Shifter
// Models: Ring Mod, Freq Shift Up, Freq Shift Down, Downshift
// =================================================================================================
class RingShiftUnit final : public FxUnit {
    // 1st-order allpass section: H(z) = (z^-1 - c) / (1 - c z^-1)
    struct Allpass1 {
        float c = 0.f;
        float x1 = 0.f, y1 = 0.f;
        void reset() { x1 = y1 = 0.f; }
        inline float tick(float x) {
            float y = x1 - c * x + c * y1;
            if (std::fabs(y) < 1e-15f) y = 0.f;
            x1 = x; y1 = y;
            return y;
        }
    };

    // 4-stage allpass chain for wideband phase splitting
    struct HilbertBranch {
        std::array<Allpass1, 4> ap;
        void reset() { for (auto& a : ap) a.reset(); }
        void setPoles(const float* poles, float sr) {
            for (int i = 0; i < 4; ++i) {
                float p = poles[i];
                if (sr > 1000.f && sr != 48000.f) {
                    const float lp = std::log(std::max(1e-5f, p));
                    p = std::exp(lp * (48000.f / sr));
                }
                ap[size_t(i)].c = clampf(p, 0.01f, 0.9995f);
            }
        }
        inline float tick(float x) {
            for (int i = 0; i < 4; ++i) x = ap[size_t(i)].tick(x);
            return x;
        }
    };

public:
    void prepare(float sampleRate, int maxBlock) override {
        sr = sampleRate; (void)maxBlock;
        // Bernie Hutchins classic 8-pole 90-degree phase splitter network
        static constexpr float kPolesA[4] = { 0.6923878f, 0.9360654f, 0.9882295f, 0.9987488f };
        static constexpr float kPolesB[4] = { 0.4021921f, 0.8561710f, 0.9722909f, 0.9952885f };
        branchAL.setPoles(kPolesA, sr); branchBL.setPoles(kPolesB, sr);
        branchAR.setPoles(kPolesA, sr); branchBR.setPoles(kPolesB, sr);
        reset();
    }

    void reset() override {
        branchAL.reset(); branchBL.reset();
        branchAR.reset(); branchBR.reset();
        carrierPhL = carrierPhR = 0.f;
        lfoPhase = 0.f;
        fbL = fbR = 0.f;
        subLp.y = 0.f;
        subFilt.y = 0.f;
        prevSubIn = 0.f;
        subFlip = 1.f;
    }

    void process(float* L, float* R, int n, const float* k, int model) override {
        const float freq     = fxKnob(FX_RingShift, 0).value(k[0]);
        const float lfoRate  = fxKnob(FX_RingShift, 1).value(k[1]);
        const float lfoDepth = fxKnob(FX_RingShift, 2).value(k[2]);
        const float feedback = fxKnob(FX_RingShift, 3).value(k[3]);
        const float stereo   = fxKnob(FX_RingShift, 4).value(k[4]);

        const float lfoDPhase = lfoRate / sr;
        subLp.setCutoff(220.f, sr);
        subFilt.setCutoff(260.f, sr);

        for (int i = 0; i < n; ++i) {
            lfoPhase += lfoDPhase;
            if (lfoPhase >= 1.f) lfoPhase -= 1.f;

            // Exponential frequency modulation by LFO (± LFO Depth * 2 octaves)
            const float lfoValL = fsin(lfoPhase);
            const float lfoValR = fsin(lfoPhase + stereo * 0.25f);
            const float fL = freq * std::exp2(lfoValL * lfoDepth * 2.f);
            const float fR = (freq * (1.f + stereo * 0.1f)) * std::exp2(lfoValR * lfoDepth * 2.f);

            carrierPhL += fL / sr; if (carrierPhL >= 1.f) carrierPhL -= 1.f;
            carrierPhR += fR / sr; if (carrierPhR >= 1.f) carrierPhR -= 1.f;

            // Soft-clipped feedback loop
            const float inL = L[i] + fastTanh(fbL * feedback * 1.3f);
            const float inR = R[i] + fastTanh(fbR * feedback * 1.3f);

            float outL = 0.f, outR = 0.f;

            if (model == 0) {
                // Ring Modulator: multiply input by sine carrier
                outL = inL * fsin(carrierPhL);
                outR = inR * fsin(carrierPhR);
            } else {
                // Hilbert transform quadrature decomposition
                const float iL = branchAL.tick(inL);
                const float qL = branchBL.tick(inL);
                const float iR = branchAR.tick(inR);
                const float qR = branchBR.tick(inR);

                const float sinCL = fsin(carrierPhL);
                const float cosCL = fsin(carrierPhL + 0.25f);
                const float sinCR = fsin(carrierPhR);
                const float cosCR = fsin(carrierPhR + 0.25f);

                if (model == 1) {
                    // Freq Shift Up: upper sideband = I*cos - Q*sin
                    outL = iL * cosCL - qL * sinCL;
                    outR = iR * cosCR - qR * sinCR;
                } else if (model == 2) {
                    // Freq Shift Down: lower sideband = I*cos + Q*sin
                    outL = iL * cosCL + qL * sinCL;
                    outR = iR * cosCR + qR * sinCR;
                } else {
                    // Downshift: frequency shift down plus blend of sub-octave divider
                    const float ssbL = iL * cosCL + qL * sinCL;
                    const float ssbR = iR * cosCR + qR * sinCR;

                    const float subIn = subLp.tick(0.5f * (inL + inR));
                    if (subIn > 0.f && prevSubIn <= 0.f) subFlip = -subFlip;
                    prevSubIn = subIn;

                    const float subRaw = subFlip * std::fabs(subIn);
                    const float subOct = subFilt.tick(subRaw);

                    outL = ssbL * 0.75f + subOct * 0.35f;
                    outR = ssbR * 0.75f + subOct * 0.35f;
                }
            }

            fbL = outL; fbR = outR;
            L[i] = clampf(std::isfinite(outL) ? outL : 0.f, -4.f, 4.f);
            R[i] = clampf(std::isfinite(outR) ? outR : 0.f, -4.f, 4.f);
        }
    }

private:
    float sr = 48000.f;
    HilbertBranch branchAL, branchBL, branchAR, branchBR;
    float carrierPhL = 0.f, carrierPhR = 0.f;
    float lfoPhase = 0.f;
    float fbL = 0.f, fbR = 0.f;
    OnePole subLp, subFilt;
    float prevSubIn = 0.f, subFlip = 1.f;
};

// =================================================================================================
// 5. FX_Wah — Envelope-, LFO- or Pedal-Controlled Resonant Filter
// Models: Crying Wah, Auto-Wah, Env Filter LP, Env Filter BP, LFO Filter, Talk Box
// =================================================================================================
class WahUnit final : public FxUnit {
    struct VowelFormants { float f1, f2, f3; };
    static constexpr VowelFormants kVowels[5] = {
        { 700.f, 1200.f, 2600.f }, // A
        { 500.f, 1800.f, 2500.f }, // E
        { 300.f, 2300.f, 3000.f }, // I
        { 450.f,  900.f, 2400.f }, // O
        { 350.f,  800.f, 2200.f }  // U
    };

public:
    void prepare(float sampleRate, int maxBlock) override {
        sr = sampleRate; (void)maxBlock;
        reset();
    }

    void reset() override {
        svfL.reset(); svfR.reset();
        for (auto& b : vowelL) b.reset();
        for (auto& b : vowelR) b.reset();
        envSmooth = 0.f;
        lfoPhase = 0.f;
    }

    void process(float* L, float* R, int n, const float* k, int model) override {
        const float sensitivity = fxKnob(FX_Wah, 0).value(k[0]);
        const float baseFreq    = fxKnob(FX_Wah, 1).value(k[1]);
        const float reso        = fxKnob(FX_Wah, 2).value(k[2]);
        const float attackMs    = fxKnob(FX_Wah, 3).value(k[3]);
        const float releaseMs   = fxKnob(FX_Wah, 4).value(k[4]);
        const float rangeOct    = fxKnob(FX_Wah, 5).value(k[5]);

        const float attAlpha = 1.f - std::exp(-1.f / (std::max(0.1f, attackMs) * 0.001f * sr));
        const float relAlpha = 1.f - std::exp(-1.f / (std::max(1.f, releaseMs) * 0.001f * sr));
        const float lfoRate  = 0.1f + sensitivity * 8.f;
        const float lfoDPhase = lfoRate / sr;

        for (int i = 0; i < n; ++i) {
            const float inAbs = std::max(std::fabs(L[i]), std::fabs(R[i]));
            if (inAbs > envSmooth) {
                envSmooth += attAlpha * (inAbs - envSmooth);
            } else {
                envSmooth += relAlpha * (inAbs - envSmooth);
            }

            float control = 0.f;
            if (model == 4) {
                // LFO Filter: rate from Sensitivity, smoothed by attack/release
                lfoPhase += lfoDPhase;
                if (lfoPhase >= 1.f) lfoPhase -= 1.f;
                control = 0.5f + 0.5f * fsin(lfoPhase);
            } else {
                control = clampf(envSmooth * sensitivity * 2.8f, 0.f, 1.f);
            }

            if (model == 5) {
                // Talk Box: 3 parallel formant bandpasses morphing through vowels A, E, I, O, U
                const float pos = clampf(control * 4.f, 0.f, 3.999f);
                const int idx = int(pos);
                const float frac = pos - float(idx);
                const float f1 = lerpf(kVowels[idx].f1, kVowels[idx + 1].f1, frac);
                const float f2 = lerpf(kVowels[idx].f2, kVowels[idx + 1].f2, frac);
                const float f3 = lerpf(kVowels[idx].f3, kVowels[idx + 1].f3, frac);

                const float q = 3.5f + reso * 10.f;
                vowelL[0].setBandPass(f1, q, sr); vowelR[0].copyCoeffsFrom(vowelL[0]);
                vowelL[1].setBandPass(f2, q, sr); vowelR[1].copyCoeffsFrom(vowelL[1]);
                vowelL[2].setBandPass(f3, q, sr); vowelR[2].copyCoeffsFrom(vowelL[2]);

                float vL = (0.6f * vowelL[0].tick(L[i]) + 0.4f * vowelL[1].tick(L[i]) + 0.25f * vowelL[2].tick(L[i]) + 0.15f * L[i]) * 6.1f;
                float vR = (0.6f * vowelR[0].tick(R[i]) + 0.4f * vowelR[1].tick(R[i]) + 0.25f * vowelR[2].tick(R[i]) + 0.15f * R[i]) * 6.1f;

                L[i] = clampf(std::isfinite(vL) ? vL : 0.f, -4.f, 4.f);
                R[i] = clampf(std::isfinite(vR) ? vR : 0.f, -4.f, 4.f);
                continue;
            }

            float outL = 0.f, outR = 0.f;
            if (model == 0) {
                // Crying Wah: classic inductor wah with narrow peaky Q, retained low end, 350 Hz - 2.2 kHz sweep
                const float fc = clampf((baseFreq * 0.7f) * std::exp2((rangeOct * 0.88f) * control), 350.f, 2200.f);
                const float q = 4.5f + reso * 8.0f;
                svfL.set(fc, q, sr); svfR.set(fc, q, sr);
                float lpDummy, bpL, hpDummy, bpR;
                svfL.tick(L[i], lpDummy, bpL, hpDummy);
                svfR.tick(R[i], lpDummy, bpR, hpDummy);
                const float bpGain = 1.55f / std::sqrt(q);
                outL = bpL * bpGain + L[i] * 0.35f;
                outR = bpR * bpGain + R[i] * 0.35f;
            } else if (model == 1) {
                // Auto-Wah: envelope follower with Attack/Release, broader Q, 1/sqrt(Q) compensation
                const float fc = clampf(baseFreq * std::exp2(rangeOct * control), 40.f, 0.45f * sr);
                const float q = 1.8f + reso * 5.5f;
                svfL.set(fc, q, sr); svfR.set(fc, q, sr);
                float lpDummy, bpL, hpDummy, bpR;
                svfL.tick(L[i], lpDummy, bpL, hpDummy);
                svfR.tick(R[i], lpDummy, bpR, hpDummy);
                const float bpGain = 1.6f / std::sqrt(q);
                outL = bpL * bpGain + L[i] * 0.25f;
                outR = bpR * bpGain + R[i] * 0.25f;
            } else if (model == 2) {
                // Env Filter LP: resonant 2-pole low-pass (Mu-Tron style) with resonance gain compensation
                const float fc = clampf(baseFreq * std::exp2(rangeOct * control), 40.f, 0.45f * sr);
                const float q = 1.0f + reso * 9.0f;
                svfL.set(fc, q, sr); svfR.set(fc, q, sr);
                const float lpGain = 1.f / (0.65f + reso * 2.2f);
                outL = svfL.lp(L[i]) * lpGain;
                outR = svfR.lp(R[i]) * lpGain;
            } else if (model == 3) {
                // Env Filter BP: broader, smoother Mu-Tron style band-pass with wider sweep
                const float fc = clampf((baseFreq * 0.4f) * std::exp2((rangeOct * 1.35f) * control), 60.f, 0.45f * sr);
                const float q = 1.2f + reso * 4.5f;
                svfL.set(fc, q, sr); svfR.set(fc, q, sr);
                const float bpGain = 1.6f / std::sqrt(q);
                outL = svfL.bp(L[i]) * bpGain;
                outR = svfR.bp(R[i]) * bpGain;
            } else {
                // LFO Filter: sweeping resonant LP/BP with gain compensation
                const float fc = clampf(baseFreq * std::exp2(rangeOct * control), 40.f, 0.45f * sr);
                const float q = 2.0f + reso * 8.0f;
                svfL.set(fc, q, sr); svfR.set(fc, q, sr);
                float lpL_val, bpL_val, hpDummy, lpR_val, bpR_val;
                svfL.tick(L[i], lpL_val, bpL_val, hpDummy);
                svfR.tick(R[i], lpR_val, bpR_val, hpDummy);
                const float bpGain = 1.15f / std::sqrt(q);
                const float lpGain = 0.55f / (1.f + reso * 0.5f);
                outL = lpL_val * lpGain + bpL_val * bpGain;
                outR = lpR_val * lpGain + bpR_val * bpGain;
            }

            L[i] = clampf(std::isfinite(outL) ? outL : 0.f, -4.f, 4.f);
            R[i] = clampf(std::isfinite(outR) ? outR : 0.f, -4.f, 4.f);
        }
    }

private:
    float sr = 48000.f;
    Svf svfL, svfR;
    std::array<Biquad, 3> vowelL, vowelR;
    float envSmooth = 0.f;
    float lfoPhase = 0.f;
};

// =================================================================================================
// 6. FX_Tape — Tape Machine Saturation & Transport Wobble
// Models: Studio 15ips, Cassette, Broken Tape
// =================================================================================================
class TapeUnit final : public FxUnit {
public:
    void prepare(float sampleRate, int maxBlock) override {
        sr = sampleRate; (void)maxBlock;
        // Allocate delay line sized for 192 kHz (60ms max)
        delayL.init(int(sr * 0.08f) + 1024);
        delayR.init(int(sr * 0.08f) + 1024);
        reset();
    }

    void reset() override {
        delayL.clear(); delayR.clear();
        wowPh = fltPh = 0.f;
        satPrevL = satPrevR = 0.f;
        preL = preR = 0.f;
        deEmpL.y = deEmpR.y = 0.f;
        hissFilt.y = 0.f;
        bumpL.reset(); bumpR.reset();
        toneL.reset(); toneR.reset();
        dropoutTimer = 0;
        dropoutGain = 1.f;
        smoothDropout = 1.f;
        pitchDiveTimer = 0;
        pitchDiveOffset = 0.f;
    }

    void process(float* L, float* R, int n, const float* k, int model) override {
        const float drive   = fxKnob(FX_Tape, 0).value(k[0]);
        const float wow     = fxKnob(FX_Tape, 1).value(k[1]);
        const float flutter = fxKnob(FX_Tape, 2).value(k[2]);
        const float hiss    = fxKnob(FX_Tape, 3).value(k[3]);
        const float tone    = fxKnob(FX_Tape, 4).value(k[4]);
        const float age     = fxKnob(FX_Tape, 5).value(k[5]);

        // Model parameter calibration
        float driveScale = 1.f + drive * 2.5f;
        float bumpFreq = 90.f;
        float bumpGain = 2.5f;
        float baseHiss = 0.015f;
        float wowScale = 1.f;
        float fltScale = 1.f;

        if (model == 1) {
            // Cassette: darker, more wow/flutter, lower head bump
            driveScale = 1.3f + drive * 3.8f;
            bumpFreq = 65.f;
            bumpGain = 2.0f;
            baseHiss = 0.035f;
            wowScale = 2.2f;
            fltScale = 2.0f;
        } else if (model == 2) {
            // Broken Tape: heavy distortion, severe warble, dropouts
            driveScale = 1.6f + drive * 5.0f;
            bumpFreq = 50.f;
            bumpGain = 3.2f;
            baseHiss = 0.055f;
            wowScale = 4.5f;
            fltScale = 3.5f;
        }

        // Filters setup
        deEmpL.setCutoff(4200.f, sr); deEmpR.setCutoff(4200.f, sr);
        bumpL.setPeak(bumpFreq, bumpGain, 1.6f, sr);
        bumpR.copyCoeffsFrom(bumpL);

        // Tone knob (4-18 kHz) reduced by age and model
        float hfCutoff = 4000.f + tone * 14000.f;
        if (model == 1) hfCutoff = std::min(hfCutoff * 0.72f, 12500.f);
        if (model == 2) hfCutoff = std::min(hfCutoff * 0.50f, 8500.f);
        hfCutoff *= (1.f - age * 0.45f);
        toneL.setLowPass(hfCutoff, 0.707f, sr);
        toneR.copyCoeffsFrom(toneL);

        hissFilt.setCutoff(3200.f, sr);

        const float wowDPhase = (0.5f + (model == 2 ? 0.3f : 0.f)) / sr;
        const float fltDPhase = 6.5f / sr;
        const float baseDelay = 0.006f * sr;
        const float totalWowAmp = (0.0003f + wow * 0.0012f) * (1.f + age * 0.8f) * wowScale * sr;
        const float totalFltAmp = (0.0001f + flutter * 0.0004f) * fltScale * sr;
        const float totalHissAmp = (baseHiss + hiss * 0.04f) * (1.f + age * 1.2f);

        for (int i = 0; i < n; ++i) {
            float inL = L[i], inR = R[i];

            // 1. Input pre-emphasis
            const float preEmpL = inL + 0.35f * (inL - preL); preL = inL;
            const float preEmpR = inR + 0.35f * (inR - preR); preR = inR;

            // 2. Tape saturation with hysteresis-style state memory
            const float sInL = preEmpL * driveScale + 0.12f * age * satPrevL;
            const float sInR = preEmpR * driveScale + 0.12f * age * satPrevR;
            satPrevL = fastTanh(sInL) - 0.04f * sInL * std::fabs(sInL);
            satPrevR = fastTanh(sInR) - 0.04f * sInR * std::fabs(sInR);
            const float satL = satPrevL / std::sqrt(driveScale);
            const float satR = satPrevR / std::sqrt(driveScale);

            // 3. De-emphasis
            float deEmpOutL = deEmpL.tick(satL);
            float deEmpOutR = deEmpR.tick(satR);

            // 4. Head bump (low peak ~80-100 Hz)
            float bumpOutL = bumpL.tick(deEmpOutL);
            float bumpOutR = bumpR.tick(deEmpOutR);

            // 5. Modulated delay (wow & flutter)
            wowPh += wowDPhase; if (wowPh >= 1.f) wowPh -= 1.f;
            fltPh += fltDPhase; if (fltPh >= 1.f) fltPh -= 1.f;

            if (model == 2) {
                // Broken Tape: random pitch dives (tape slows down abruptly)
                pitchDiveTimer--;
                if (pitchDiveTimer <= 0) {
                    if (rng.unipolar() < 0.0003f) {
                        pitchDiveTimer = int(sr * (0.05f + 0.15f * rng.unipolar()));
                        pitchDiveOffset = (0.003f + 0.007f * rng.unipolar()) * sr;
                    } else {
                        pitchDiveOffset = 0.f;
                    }
                }
            }

            const float modL = baseDelay + totalWowAmp * fsin(wowPh) + totalFltAmp * fsin(fltPh) + pitchDiveOffset;
            const float modR = baseDelay + totalWowAmp * fsin(wowPh + 0.12f) + totalFltAmp * fsin(fltPh + 0.25f) + pitchDiveOffset;

            delayL.push(bumpOutL); delayR.push(bumpOutR);
            float delayOutL = delayL.read(modL);
            float delayOutR = delayR.read(modR);

            // 6. High-frequency loss (Tone)
            float toneOutL = toneL.tick(delayOutL);
            float toneOutR = toneR.tick(delayOutR);

            // 7. Dropouts
            dropoutTimer--;
            if (dropoutTimer <= 0) {
                const float prob = (model == 2 ? 0.004f : 0.0006f) * (0.1f + age * 1.5f);
                if (rng.unipolar() < prob) {
                    dropoutTimer = int(sr * (0.015f + 0.05f * rng.unipolar()));
                    dropoutGain = model == 2 ? 0.08f : 0.35f;
                } else {
                    dropoutGain = 1.f;
                }
            }
            smoothDropout += 0.003f * (dropoutGain - smoothDropout);
            toneOutL *= smoothDropout;
            toneOutR *= smoothDropout;

            // 8. Tape hiss
            const float hissNoiseL = hissFilt.tick(rng.bipolar()) * totalHissAmp;
            const float hissNoiseR = hissFilt.tick(rng.bipolar()) * totalHissAmp;

            toneOutL += hissNoiseL;
            toneOutR += hissNoiseR;

            L[i] = clampf(std::isfinite(toneOutL) ? toneOutL : 0.f, -4.f, 4.f);
            R[i] = clampf(std::isfinite(toneOutR) ? toneOutR : 0.f, -4.f, 4.f);
        }
    }

private:
    float sr = 48000.f;
    DelayLine delayL, delayR;
    float wowPh = 0.f, fltPh = 0.f;
    float satPrevL = 0.f, satPrevR = 0.f;
    float preL = 0.f, preR = 0.f;
    OnePole deEmpL, deEmpR, hissFilt;
    Biquad bumpL, bumpR, toneL, toneR;
    Rng rng;
    int dropoutTimer = 0;
    float dropoutGain = 1.f;
    float smoothDropout = 1.f;
    int pitchDiveTimer = 0;
    float pitchDiveOffset = 0.f;
};

// =================================================================================================
// 7. FX_Amp — Guitar / Bass Amp Simulation with Synthetic Speaker Cabinet
// Models: Clean Twin, Bassman, Brit Crunch, Rock Stack, Hi-Gain, Tiny Speaker
// =================================================================================================
class AmpUnit final : public FxUnit {
public:
    void prepare(float sampleRate, int maxBlock) override {
        sr = sampleRate; (void)maxBlock;
        reset();
    }

    void reset() override {
        preHpL.reset(); preHpR.reset();
        bassL.reset(); bassR.reset();
        midL.reset(); midR.reset();
        trebL.reset(); trebR.reset();
        presL.reset(); presR.reset();
        cabHpL.reset(); cabHpR.reset();
        cabPk1L.reset(); cabPk1R.reset();
        cabDipL.reset(); cabDipR.reset();
        cabPk2L.reset(); cabPk2R.reset();
        cabLp1L.reset(); cabLp1R.reset();
        cabLp2L.reset(); cabLp2R.reset();
        prevInL = prevInR = 0.f;
        for (int i = 0; i < 6; ++i) prevK[size_t(i)] = -999.f;
        prevModel = -1;
    }

    void process(float* L, float* R, int n, const float* k, int model) override {
        const float gain     = fxKnob(FX_Amp, 0).value(k[0]);
        const float bassDb   = fxKnob(FX_Amp, 1).value(k[1]);
        const float midDb    = fxKnob(FX_Amp, 2).value(k[2]);
        const float trebleDb = fxKnob(FX_Amp, 3).value(k[3]);
        const float presence = fxKnob(FX_Amp, 4).value(k[4]);
        const float levelDb  = fxKnob(FX_Amp, 5).value(k[5]);

        bool update = (model != prevModel);
        for (int i = 1; i < 5; ++i) {
            if (std::fabs(k[i] - prevK[size_t(i)]) > 1e-5f) update = true;
        }

        if (update) {
            for (int i = 0; i < 6; ++i) prevK[size_t(i)] = k[i];
            prevModel = model;

            // Preamp pre-distortion highpass
            float preHpFreq = 100.f;
            if (model == 0) preHpFreq = 75.f;       // Clean Twin: clear bass
            else if (model == 1) preHpFreq = 55.f;  // Bassman: loose tweed low end
            else if (model == 2) preHpFreq = 120.f; // Brit Crunch: tight crunch
            else if (model == 3) preHpFreq = 160.f; // Rock Stack: tight rhythm
            else if (model == 4) preHpFreq = 220.f; // Hi-Gain: tight pre-distortion bass cut
            else if (model == 5) preHpFreq = 340.f; // Tiny Speaker: no low end

            preHpL.setHighPass(preHpFreq, 0.707f, sr);
            preHpR.copyCoeffsFrom(preHpL);

            // 3-band tone stack
            bassL.setLowShelf(110.f, bassDb, 0.707f, sr);
            bassR.copyCoeffsFrom(bassL);
            midL.setPeak(750.f, midDb, 0.9f, sr);
            midR.copyCoeffsFrom(midL);
            trebL.setHighShelf(2800.f, trebleDb, 0.707f, sr);
            trebR.copyCoeffsFrom(trebL);

            // Power amp Presence (high shelf 4 kHz)
            presL.setHighShelf(4000.f, (presence - 0.5f) * 10.f, 0.707f, sr);
            presR.copyCoeffsFrom(presL);

            // Synthetic cabinet filter configurations (no IRs)
            float cHp = 80.f, cPk1F = 110.f, cPk1G = 3.f, cPk1Q = 2.f;
            float cDipF = 650.f, cDipG = -3.f, cDipQ = 1.5f;
            float cPk2F = 2600.f, cPk2G = 3.5f, cPk2Q = 2.2f;
            float cLp1 = 5200.f, cLp2 = 5800.f;

            if (model == 0) {
                // Clean Twin (2x12 open back): sparkling highs, scooped mids
                cHp = 78.f; cPk1F = 105.f; cPk1G = 3.2f; cPk1Q = 1.8f;
                cDipF = 600.f; cDipG = -3.5f; cDipQ = 1.6f;
                cPk2F = 2850.f; cPk2G = 4.0f; cPk2Q = 2.4f;
                cLp1 = 5500.f; cLp2 = 6200.f;
            } else if (model == 1) {
                // Bassman (4x10 tweed): punchy 120 Hz, tweed bark
                cHp = 60.f; cPk1F = 125.f; cPk1G = 4.0f; cPk1Q = 2.0f;
                cDipF = 850.f; cDipG = -2.5f; cDipQ = 1.3f;
                cPk2F = 2200.f; cPk2G = 3.0f; cPk2Q = 2.0f;
                cLp1 = 5100.f; cLp2 = 5600.f;
            } else if (model == 2) {
                // Brit Crunch (2x12 Greenback): woody midrange resonance
                cHp = 85.f; cPk1F = 115.f; cPk1G = 3.5f; cPk1Q = 2.2f;
                cDipF = 1100.f; cDipG = -2.8f; cDipQ = 1.7f;
                cPk2F = 2450.f; cPk2G = 4.2f; cPk2Q = 2.5f;
                cLp1 = 4800.f; cLp2 = 5300.f;
            } else if (model == 3) {
                // Rock Stack (4x12 Vintage 30): massive cab thump, upper-mid spike
                cHp = 72.f; cPk1F = 112.f; cPk1G = 4.5f; cPk1Q = 2.5f;
                cDipF = 480.f; cDipG = -3.8f; cDipQ = 2.0f;
                cPk2F = 2550.f; cPk2G = 4.5f; cPk2Q = 2.6f;
                cLp1 = 4500.f; cLp2 = 5000.f;
            } else if (model == 4) {
                // Hi-Gain (4x12 Oversized): deep thump, smooth cab top
                cHp = 68.f; cPk1F = 108.f; cPk1G = 5.0f; cPk1Q = 2.8f;
                cDipF = 520.f; cDipG = -4.2f; cDipQ = 2.2f;
                cPk2F = 2650.f; cPk2G = 3.8f; cPk2Q = 2.7f;
                cLp1 = 4300.f; cLp2 = 4700.f;
            } else if (model == 5) {
                // Tiny Speaker (3-4" lo-fi cone): boxy horn resonance, no bass
                cHp = 320.f; cPk1F = 1350.f; cPk1G = 6.0f; cPk1Q = 3.2f;
                cDipF = 2200.f; cDipG = -5.0f; cDipQ = 2.0f;
                cPk2F = 3100.f; cPk2G = 4.5f; cPk2Q = 3.0f;
                cLp1 = 3600.f; cLp2 = 3900.f;
            }

            cabHpL.setHighPass(cHp, 0.707f, sr);  cabHpR.copyCoeffsFrom(cabHpL);
            cabPk1L.setPeak(cPk1F, cPk1G, cPk1Q, sr); cabPk1R.copyCoeffsFrom(cabPk1L);
            cabDipL.setPeak(cDipF, cDipG, cDipQ, sr); cabDipR.copyCoeffsFrom(cabDipL);
            cabPk2L.setPeak(cPk2F, cPk2G, cPk2Q, sr); cabPk2R.copyCoeffsFrom(cabPk2L);
            cabLp1L.setLowPass(cLp1, 0.707f, sr); cabLp1R.copyCoeffsFrom(cabLp1L);
            cabLp2L.setLowPass(cLp2, 0.707f, sr); cabLp2R.copyCoeffsFrom(cabLp2L);
        }

        // Preamp gain drive per model
        float preGain = 1.f;
        float baseModelGain = 1.f;
        if (model == 0) {
            preGain = 1.0f + gain * 3.5f;
            baseModelGain = 1.25f;
        } else if (model == 1) {
            preGain = 1.5f + gain * 9.0f;
            baseModelGain = 1.15f;
        } else if (model == 2) {
            preGain = 2.0f + gain * 18.0f;
            baseModelGain = 1.10f;
        } else if (model == 3) {
            preGain = 2.5f + gain * 32.0f;
            baseModelGain = 1.05f;
        } else if (model == 4) {
            preGain = 3.5f + gain * 55.0f;
            baseModelGain = 0.95f;
        } else {
            preGain = 1.8f + gain * 12.0f;
            baseModelGain = 1.20f;
        }

        // Loudness compensation to keep Level consistent across Gain settings
        const float compGain = (1.1f / std::sqrt(1.f + preGain * 0.45f)) * baseModelGain;
        const float outLevel = dbToGain(levelDb);

        for (int i = 0; i < n; ++i) {
            float inL = L[i], inR = R[i];

            // Pre-EQ high-pass
            inL = preHpL.tick(inL);
            inR = preHpR.tick(inR);

            // 2x oversampled preamp waveshaping
            float sub0L = 0.5f * (inL + prevInL);
            float sub0R = 0.5f * (inR + prevInR);
            float sub1L = inL;
            float sub1R = inR;
            prevInL = inL; prevInR = inR;

            auto shapeSample = [model, preGain](float v) -> float {
                float x = v * preGain;
                if (model == 0) {
                    // Clean Twin: high headroom tube curve
                    return fastTanh(x * 0.65f) * 1.5f;
                } else if (model == 1) {
                    // Bassman: asymmetric tweed breakup
                    return fastTanh(x + 0.16f * x * x) - 0.03f;
                } else if (model == 2) {
                    // Brit Crunch: crunch tube stage
                    float v1 = fastTanh(x);
                    return fastTanh(v1 * 1.6f + 0.1f * v1 * v1);
                } else if (model == 3) {
                    // Rock Stack: cascaded 2 stages
                    float v1 = fastTanh(x * 1.1f);
                    return fastTanh(v1 * 2.2f - 0.14f * v1 * v1);
                } else if (model == 4) {
                    // Hi-Gain: cascaded 3 saturated stages
                    float v1 = fastTanh(x * 1.2f);
                    float v2 = fastTanh(v1 * 1.9f - 0.12f * v1 * v1);
                    return fastTanh(v2 * 2.3f);
                } else {
                    // Tiny Speaker: cone breakup clipping
                    return clampf(x * 1.8f, -0.92f, 0.88f);
                }
            };

            float preOutL = 0.5f * (shapeSample(sub0L) + shapeSample(sub1L)) * compGain;
            float preOutR = 0.5f * (shapeSample(sub0R) + shapeSample(sub1R)) * compGain;

            // Tone Stack (Bass, Mid, Treble)
            float tL = trebL.tick(midL.tick(bassL.tick(preOutL)));
            float tR = trebR.tick(midR.tick(bassR.tick(preOutR)));

            // Power amp Presence & push-pull saturation
            float pL = presL.tick(tL);
            float pR = presR.tick(tR);
            pL = fastTanh(pL * 1.15f);
            pR = fastTanh(pR * 1.15f);

            // Synthetic Speaker Cabinet
            float cL = cabLp2L.tick(cabLp1L.tick(cabPk2L.tick(cabDipL.tick(cabPk1L.tick(cabHpL.tick(pL))))));
            float cR = cabLp2R.tick(cabLp1R.tick(cabPk2R.tick(cabDipR.tick(cabPk1R.tick(cabHpR.tick(pR))))));

            // Output level
            cL *= outLevel;
            cR *= outLevel;

            L[i] = clampf(std::isfinite(cL) ? cL : 0.f, -4.f, 4.f);
            R[i] = clampf(std::isfinite(cR) ? cR : 0.f, -4.f, 4.f);
        }
    }

private:
    float sr = 48000.f;
    Biquad preHpL, preHpR;
    Biquad bassL, bassR;
    Biquad midL, midR;
    Biquad trebL, trebR;
    Biquad presL, presR;
    Biquad cabHpL, cabHpR;
    Biquad cabPk1L, cabPk1R;
    Biquad cabDipL, cabDipR;
    Biquad cabPk2L, cabPk2R;
    Biquad cabLp1L, cabLp1R;
    Biquad cabLp2L, cabLp2R;
    float prevInL = 0.f, prevInR = 0.f;
    float prevK[6] {};
    int prevModel = -1;
};

// =================================================================================================
// Tone & Dynamics Factory
// =================================================================================================
inline std::unique_ptr<FxUnit> makeToneFx(int type) {
    switch (type) {
    case FX_EQ:         return std::make_unique<EqUnit>();
    case FX_Compressor: return std::make_unique<CompressorUnit>();
    case FX_LoFi:       return std::make_unique<LoFiUnit>();
    case FX_RingShift:  return std::make_unique<RingShiftUnit>();
    case FX_Wah:        return std::make_unique<WahUnit>();
    case FX_Tape:       return std::make_unique<TapeUnit>();
    case FX_Amp:        return std::make_unique<AmpUnit>();
    default:            return nullptr;
    }
}

} // namespace op