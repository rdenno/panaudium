// Panaudium sound engines — every engine is purely synthetic (no recorded samples). AGPL-3.0 (see LICENSE).
#pragma once
#include "Dsp.h"
#include "Params.h"
#include <cstdlib>

namespace op {

// Control-rate inputs to one oscillator instance (one unison voice of one layer).
struct OscControl {
    float freq = 220, timbre = 0.5f, shape = 0.5f, chr = 0.5f, noise = 0, fmDepth = 0, fmRatio = 1;
    float anim = 0, vel = 1;
    bool gate = true;
};

// Band-limited "rich" wavetable used by the granular engine (8 harmonics, 1/k).
struct RichTable {
    static constexpr int N = 2048;
    float t[N + 1];
    RichTable() {
        for (int i = 0; i <= N; ++i) {
            float s = 0;
            for (int k = 1; k <= 8; ++k) s += std::sin(kTwoPi * float(k * i) / float(N)) / float(k);
            t[i] = s * 0.55f;
        }
    }
    static const RichTable& get() { static RichTable r; return r; }
    inline float operator()(float ph) const {
        ph -= std::floor(ph);
        const float f = ph * N; const int i = int(f); const float fr = f - float(i);
        return t[i] + (t[i + 1] - t[i]) * fr;
    }
};

// Mode-ratio sets for the modal engine; Timbre morphs continuously across them.
struct ModalSets {
    static constexpr int M = 16, S = 4;
    float r[S][M];
    ModalSets() {
        for (int k = 0; k < M; ++k) r[0][k] = float(k + 1);                                    // ideal string / tube
        for (int k = 0; k < M; ++k) { const float b = float(2 * k + 3) / 3.0112f; r[1][k] = b * b; } // free-free bar (1, 2.756, 5.404 ...)
        // Bell partials (hum, prime, tierce, quint, nominal ...) normalised so the hum sits on the played note;
        // otherwise morphing from the bar set would drag the lowest mode to 0.75x and detune the patch by a fourth.
        const float bell[M] = { 1.f, 2.f, 2.366f, 3.012f, 4.f, 5.028f, 5.324f, 6.022f, 8.332f, 10.866f, 13.592f, 16.43f, 19.466f, 22.66f, 26.f, 29.4f };
        const float membrane[M] = { 1.f, 1.594f, 2.136f, 2.296f, 2.653f, 2.918f, 3.156f, 3.501f, 3.6f, 3.652f, 4.06f, 4.154f, 4.601f, 4.832f, 4.903f, 5.131f };
        for (int k = 0; k < M; ++k) { r[2][k] = bell[k]; r[3][k] = membrane[k]; }
    }
    static const ModalSets& get() { static ModalSets m; return m; }
};

// Five-formant vowel table (tenor, A E I O U): freq Hz, gain dB, bandwidth Hz.
struct VowelTable {
    static constexpr int V = 5, F = 5;
    float f[V][F] = { { 650, 1080, 2650, 2900, 3250 }, { 400, 1700, 2600, 3200, 3580 }, { 290, 1870, 2800, 3250, 3540 },
                      { 400, 800, 2600, 2800, 3000 }, { 350, 600, 2700, 2900, 3300 } };
    float db[V][F] = { { 0, -6, -7, -8, -22 }, { 0, -14, -12, -14, -20 }, { 0, -15, -18, -20, -30 },
                       { 0, -10, -12, -12, -26 }, { 0, -20, -17, -14, -26 } };
    float bw[V][F] = { { 80, 90, 120, 130, 140 }, { 70, 80, 100, 120, 120 }, { 40, 90, 100, 120, 120 },
                       { 40, 80, 100, 120, 120 }, { 40, 60, 100, 120, 120 } };
    // Interpolated lookup, like Omnisphere's 128-step morph tables (pos in [0,1]).
    void lookup(float pos, float* fo, float* go, float* bo) const {
        const float x = clampf(pos, 0.f, 1.f) * float(V - 1);
        const int i = std::min(int(x), V - 2); const float t = x - float(i);
        for (int k = 0; k < F; ++k) {
            fo[k] = lerpf(f[i][k], f[i + 1][k], t);
            go[k] = dbToGain(lerpf(db[i][k], db[i + 1][k], t));
            bo[k] = lerpf(bw[i][k], bw[i + 1][k], t);
        }
    }
    static const VowelTable& get() { static VowelTable v; return v; }
};

class EngineVoice {
public:
    void prepare(float sampleRate) {
        sr = sampleRate;
        const int maxDelay = int(sr / 12.f) + 8;
        dlA.init(maxDelay); dlB.init(maxDelay);
        exc.assign(size_t(maxDelay), 0.f);
    }

    void start(int engineType, const OscControl& c, uint32_t seed) {
        engine = engineType;
        rng = Rng(seed);
        ph = rng.unipolar(); ph2 = 0; phMod = 0; gPrev = 0;
        lastOutA = lastOutB = 0; lpState = 0; dc = DcBlock{}; breath = 0;
        for (auto& s : fsv) s.reset();
        bodyA.reset(); bodyB.reset(); bodyC.reset(); colour.reset();
        for (int k = 0; k < kPartials; ++k) { pPh[k] = rng.unipolar(); aPh[k] = rng.unipolar(); aRate[k] = 0.05f + 0.6f * rng.unipolar(); }
        for (auto& g : grains) g.on = false;
        grainCountdown = 0;
        for (int k = 0; k < ModalSets::M; ++k) { my1[k] = my2[k] = 0; }
        gust = Drift{};
        if (engine == E_Pluck || engine == E_Atmos || engine == E_Pipe || engine == E_Bowed) { dlA.clear(); dlB.clear(); }
        pipePhase = 0; pipePrev = pipeCur = toneLp = 0;
        pipeSub = std::max(1, int(std::ceil(sr / (c.freq * 0.66666f) / 480.f)));
        if (engine == E_Pluck) buildPluckExcitation(c);
        if (engine == E_Modal) {
            // Mallet contact time: hard = short. Capped below half a period so the fundamental is always struck.
            const float period = sr / c.freq;
            pulseLen = std::max(2, int(std::min(sr * (0.0002f + (1.f - c.shape) * 0.004f), period * (0.05f + 0.4f * (1.f - c.shape)))));
            pulsePos = 0;
        }
    }

    // Renders n mono samples (n <= block size) into out (overwrites).
    void render(float* out, int n, const OscControl& c) {
        switch (engine) {
        case E_VA: renderVA(out, n, c); break;
        case E_Spectral: renderSpectral(out, n, c); break;
        case E_Granular: renderGranular(out, n, c); break;
        case E_Pluck: renderPluck(out, n, c); break;
        case E_Modal: renderModal(out, n, c); break;
        case E_Vocal: renderVocal(out, n, c); break;
        case E_Pipe: renderPipe(out, n, c); break;
        case E_Bowed: renderBowed(out, n, c); break;
        default: renderAtmos(out, n, c); break;
        }
    }

private:
    static constexpr int kPartials = 32;
    static constexpr int kGrains = 32;
    float sr = 48000;
    int engine = 0;
    Rng rng;
    float ph = 0, ph2 = 0, phMod = 0, gPrev = 0;
    // waveguides
    DelayLine dlA, dlB;
    std::vector<float> exc;
    int excLen = 0, excPos = 0;
    float lastOutA = 0, lastOutB = 0, lpState = 0, breath = 0;
    int pipeSub = 1, pipePhase = 0; float pipePrev = 0, pipeCur = 0, toneLp = 0;
    DcBlock dc;
    // additive
    float pPh[kPartials] {}, aPh[kPartials] {}, aRate[kPartials] {};
    // granular
    struct Grain { bool on = false; float ph = 0, inc = 0, env = 0, envInc = 0, amp = 0; };
    Grain grains[kGrains];
    int grainCountdown = 0;
    // modal
    float my1[ModalSets::M] {}, my2[ModalSets::M] {};
    int pulseLen = 0, pulsePos = 0;
    // filters (formants, bodies, atmos colour)
    Svf fsv[VowelTable::F];
    Svf bodyA, bodyB, bodyC, colour;
    Drift gust;

    // ---------------------------------------------------------------- Analog / FM / sync
    void renderVA(float* out, int n, const OscControl& c) {
        const float dt = c.freq / sr;
        const float sk = c.chr < 0.35f ? 0.f : (c.chr - 0.35f) / 0.65f;  // hard-sync dead zone below 0.35
        const float syncR = 1.f + sk * sk * 7.f;
        const float sdt = std::min(0.5f, dt * syncR);
        const float w = 0.5f + 0.45f * (c.shape - 0.5f) * 2.f * 0.98f;  // pulse width / symmetry (0.06..0.94)
        const float pwm = clampf(w, 0.06f, 0.94f);
        const float modInc = c.freq * c.fmRatio / sr;
        const float pmAmt = c.fmDepth * c.fmDepth * 2.5f;
        const float morph = c.timbre * 3.f;
        const int seg = std::min(2, int(morph));
        const float mt = morph - float(seg);
        for (int i = 0; i < n; ++i) {
            ph2 += dt;
            bool wrap = false;
            if (ph2 >= 1.f) { ph2 -= 1.f; wrap = true; }
            if (syncR > 1.001f && wrap) ph = ph2 * syncR;
            else { ph += sdt; if (ph >= 1.f) ph -= 1.f; }
            float p = ph;
            if (pmAmt > 0.f) { p += pmAmt * fsin(phMod); phMod += modInc; if (phMod >= 1.f) phMod -= 1.f; p -= std::floor(p); }
            float a, b;
            auto saw = [&] { return 2.f * p - 1.f - polyBlep(p, sdt); };
            auto sq = [&] { float q = p < pwm ? 1.f : -1.f; float t2 = p + 1.f - pwm; t2 -= std::floor(t2); return q + polyBlep(p, sdt) - polyBlep(t2, sdt); };
            auto tri = [&] { return p < pwm ? -1.f + 2.f * p / pwm : 1.f - 2.f * (p - pwm) / (1.f - pwm); };
            auto sine = [&] { float q = p < pwm ? 0.5f * p / pwm : 0.5f + 0.5f * (p - pwm) / (1.f - pwm); return fsin(q); };
            if (seg == 0) { a = saw(); b = sq(); }
            else if (seg == 1) { a = sq(); b = tri(); }
            else { a = tri(); b = sine(); }
            out[i] = (a + (b - a) * mt) * 0.5f + c.noise * rng.bipolar() * 0.5f;
        }
    }

    // ---------------------------------------------------------------- Additive "spectral" (timbre-shift style)
    void renderSpectral(float* out, int n, const OscControl& c) {
        const float tilt = 2.6f - 2.4f * c.timbre;      // dark (1/k^2.6) .. bright (1/k^0.2)
        const float evenGain = 1.f - c.shape;            // 0 = full, 1 = hollow / odd-only
        const float B = c.chr * c.chr * 0.012f;          // stiffness / inharmonic stretch
        const float anim = c.anim * 0.9f;
        const float fmSpread = c.fmDepth;                // spreads partials into a chorus of detuned pairs
        float amp[kPartials], inc[kPartials];
        float sumSq = 0;
        for (int k = 0; k < kPartials; ++k) {
            const float kk = float(k + 1);
            float f = c.freq * kk * std::sqrt(1.f + B * kk * kk);
            f *= 1.f + fmSpread * 0.01f * ((k & 1) ? 1.f : -1.f) * std::sqrt(kk);
            float a = std::pow(kk, -tilt) * (((k + 1) % 2 == 0) ? evenGain : 1.f);
            if (f > sr * 0.45f) a = 0;
            aPh[k] += aRate[k] * float(n) / sr; aPh[k] -= std::floor(aPh[k]);
            a *= 1.f + anim * fsin(aPh[k]) * (k == 0 ? 0.2f : 1.f);
            amp[k] = a; inc[k] = f / sr; sumSq += a * a;
        }
        const float g = 0.35f / std::sqrt(std::max(1e-6f, sumSq * 0.5f)) * 0.5f;
        std::fill(out, out + n, 0.f);
        for (int k = 0; k < kPartials; ++k) {
            if (amp[k] <= 0.f) continue;
            const float a = amp[k] * g, d = inc[k];
            float p = pPh[k];
            for (int i = 0; i < n; ++i) { out[i] += a * fsin(p); p += d; }
            pPh[k] = p - std::floor(p);
        }
        if (c.noise > 0) for (int i = 0; i < n; ++i) out[i] += c.noise * 0.5f * rng.bipolar();
    }

    // ---------------------------------------------------------------- Granular cloud of synthetic grains
    void renderGranular(float* out, int n, const OscControl& c) {
        const float rate = 4.f + c.timbre * c.timbre * 220.f;          // grains / second
        const float dur = 0.01f * std::pow(35.f, c.shape);             // 10 ms .. 350 ms
        const float gain = 0.6f / std::sqrt(std::max(1.f, rate * dur));
        const auto& sine = SineTable::get();
        const auto& rich = RichTable::get();
        std::fill(out, out + n, 0.f);
        for (int i = 0; i < n; ++i) {
            if (--grainCountdown <= 0) {
                grainCountdown = std::max(1, int(sr / rate * (0.5f + rng.unipolar())));
                for (auto& g : grains) if (!g.on) {
                    float st = rng.bipolar() * c.chr;                   // +-1 semitone scatter
                    if (rng.unipolar() < c.chr * c.chr * 0.5f) st += (rng.unipolar() < 0.5f ? -12.f : 12.f);
                    if (rng.unipolar() < c.chr * c.chr * 0.25f) st += 7.f;  // fifths sparkle
                    g.on = true; g.ph = rng.unipolar(); g.inc = c.freq * semitoneRatio(st) / sr;
                    g.env = 0; g.envInc = 1.f / (dur * sr * (0.7f + 0.6f * rng.unipolar()));
                    g.amp = 0.6f + 0.4f * rng.unipolar();
                    break;
                }
            }
        }
        for (auto& g : grains) {
            if (!g.on) continue;
            const bool useRich = g.inc * 8.f < 0.45f;
            const float mix = useRich ? c.fmDepth : 0.f;
            for (int i = 0; i < n; ++i) {
                const float w = 0.5f - 0.5f * sine(g.env + 0.25f);    // Hann window
                const float s = sine(g.ph) * (1.f - mix) + rich(g.ph) * mix;
                out[i] += s * w * g.amp * gain;
                g.ph += g.inc; g.env += g.envInc;
                if (g.env >= 1.f) { g.on = false; break; }
            }
            g.ph -= std::floor(g.ph);
        }
        if (c.noise > 0) for (int i = 0; i < n; ++i) out[i] += c.noise * 0.5f * rng.bipolar();
    }

    // ---------------------------------------------------------------- Karplus-Strong plucked string
    void buildPluckExcitation(const OscControl& c) {
        excLen = std::min(int(exc.size()) - 1, std::max(2, int(sr / c.freq)));
        excPos = 0;
        OnePole lp; lp.setCutoff(300.f + 16000.f * c.timbre * (0.4f + 0.6f * c.vel), sr);
        for (int i = 0; i < excLen; ++i) exc[size_t(i)] = lp.tick(rng.bipolar());
        const int pick = std::max(1, int(float(excLen) * (0.04f + 0.46f * c.shape)));
        for (int i = excLen - 1; i >= pick; --i) exc[size_t(i)] -= exc[size_t(i - pick)];
    }
    void renderPluck(float* out, int n, const OscControl& c) {
        const float damp = 0.15f + 0.85f * c.timbre;            // loop lowpass amount (1 = none)
        const float pole = 1.f - damp;
        const float L = std::max(2.f, sr / c.freq - 0.5f - pole / std::max(0.05f, damp));
        const float t60 = c.gate ? 0.25f + c.chr * c.chr * 25.f : 0.12f;
        const float g = std::pow(10.f, -3.f / (t60 * c.freq));
        for (int i = 0; i < n; ++i) {
            float x = excPos < excLen ? exc[size_t(excPos++)] : 0.f;
            x += c.noise * 0.08f * rng.bipolar();
            const float y = dlA.read(L);
            const float avg = 0.5f * (y + lastOutA);              // classic KS two-point average
            lastOutA = y;
            lpState += damp * (avg - lpState);
            const float v = x + g * lpState;
            dlA.push(v);
            out[i] = dc.tick(v) * 0.8f;
        }
    }

    // ---------------------------------------------------------------- Modal resonator bank (bars, bells, plates)
    void renderModal(float* out, int n, const OscControl& c) {
        const auto& sets = ModalSets::get();
        constexpr int M = ModalSets::M;
        const float pos = c.timbre * float(ModalSets::S - 1);
        const int s0 = std::min(ModalSets::S - 2, int(pos));
        const float st = pos - float(s0);
        const float t0 = 0.15f + c.chr * c.chr * 14.f;
        const float hard = 1.6f - 1.4f * c.shape;
        float a1[M], a2[M], gi[M], sumSq = 0;
        for (int k = 0; k < M; ++k) {
            const float ratio = lerpf(sets.r[s0][k], sets.r[s0 + 1][k], st) * (1.f + c.fmDepth * 0.03f * float((k * 7) % 5 - 2));
            const float f = c.freq * ratio;
            if (f > sr * 0.45f) { a1[k] = a2[k] = gi[k] = 0; continue; }
            const float t = t0 / std::pow(std::max(0.25f, ratio), 0.4f + 0.6f * (1.f - c.chr));
            const float r = std::exp(-6.9078f / (t * sr));
            const float w = kTwoPi * f / sr;
            const float amp = std::pow(std::max(0.25f, ratio), -hard);
            a1[k] = 2.f * r * std::cos(w); a2[k] = -r * r; gi[k] = amp * std::sin(w);
            sumSq += amp * amp;
        }
        const float norm = 0.5f / std::sqrt(std::max(1e-6f, sumSq));
        const float pulseScale = kPi / (2.f * float(pulseLen)) * (0.3f + 0.7f * c.vel);
        for (int i = 0; i < n; ++i) {
            float x = 0;
            if (pulsePos < pulseLen) { x = std::sin(kPi * float(pulsePos) / float(pulseLen)) * pulseScale; ++pulsePos; }
            x += c.noise * 0.02f * rng.bipolar();
            float acc = 0;
            for (int k = 0; k < M; ++k) {
                const float y = a1[k] * my1[k] + a2[k] * my2[k] + gi[k] * x;
                my2[k] = my1[k]; my1[k] = y; acc += y;
            }
            out[i] = acc * norm;
        }
    }

    // ---------------------------------------------------------------- Glottal source + 5 formant resonators
    void renderVocal(float* out, int n, const OscControl& c) {
        float fo[VowelTable::F], go[VowelTable::F], bo[VowelTable::F];
        VowelTable::get().lookup(c.timbre, fo, go, bo);
        const float shift = std::exp2((c.shape - 0.5f) * 1.2f);  // vocal-tract size (child .. giant)
        float kq[VowelTable::F];
        // Formant tuning: like trained sopranos, raise F1 above f0 on high notes so the voice doesn't vanish.
        fo[0] = std::max(fo[0], c.freq * 1.15f / shift);
        for (int k = 0; k < VowelTable::F; ++k) {
            const float f = fo[k] * shift;
            const float q = f / std::max(20.f, bo[k] * (1.f + c.chr * 0.8f));
            fsv[k].set(f, q, sr); kq[k] = 1.f / q;
            if (f > sr * 0.45f) go[k] = 0;
        }
        const float dt = c.freq / sr;
        const float breathAmt = c.chr;
        const float Tp = 0.4f, Tn = 0.16f;
        const float diffNorm = 1.f / std::max(1e-4f, dt * 8.f);
        for (int i = 0; i < n; ++i) {
            ph += dt; if (ph >= 1.f) ph -= 1.f;
            float g;
            if (ph < Tp) g = 0.5f * (1.f - std::cos(kPi * ph / Tp));
            else if (ph < Tp + Tn) g = std::cos(kPi * (ph - Tp) / (2.f * Tn));
            else g = 0.f;
            const float src = (g - gPrev) * diffNorm; gPrev = g;
            const float asp = rng.bipolar() * (0.25f + g);
            const float x = src * (1.f - 0.7f * breathAmt) + asp * (breathAmt * 0.9f + c.noise);
            float y = 0;
            for (int k = 0; k < VowelTable::F; ++k) y += go[k] * kq[k] * fsv[k].bp(x);
            out[i] = y * 0.9f;
        }
    }

    // ---------------------------------------------------------------- Jet-driven flute waveguide (after STK Flute)
    void renderPipe(float* out, int n, const OscControl& c) {
        // Pitch sweeps (analysis/pitchsweep.py) showed two limits of this model:
        //  * the jet ratio changes both tuning and register, so it is fixed at 0.35 (Shape is a tone control instead);
        //  * once the bore exceeds ~480 samples the jet locks to the octave below. Low notes therefore run the
        //    waveguide at sr/pipeSub (chosen per note) and linearly interpolate the output back up.
        const float rate = sr / float(pipeSub);
        const float loopF = c.freq * 0.66666f;                    // STK overblows a 1.5x-long bore
        const float bore = std::max(4.f, rate / loopF - 2.f);
        const float jet = std::max(2.f, bore * 0.35f);
        const float maxP = (0.9f + 0.25f * c.chr) * (0.85f + 0.15f * c.vel);
        const float noiseGain = 0.02f + 0.35f * c.timbre + c.noise * 0.3f;
        const float pole = 0.7f - 0.1f * 22050.f / rate;
        const float target = c.gate ? maxP : 0.f;
        const float envC = std::exp(-1.f / ((c.gate ? 0.03f : 0.08f) * rate));
        const float toneC = std::exp(-kTwoPi * (1500.f * std::pow(12.f, c.shape)) / sr);  // 1.5 kHz .. 18 kHz
        for (int i = 0; i < n; ++i) {
            if (pipePhase == 0) {
                breath = target + (breath - target) * envC;
                const float rnd = noiseGain * rng.bipolar() * breath;
                const float bp = breath + rnd;
                lpState = -(1.f - pole) * lastOutA + pole * lpState;    // OnePole with gain -1
                const float temp = dc.tick(lpState);
                float pd = bp - 0.5f * temp;
                const float jd = dlB.read(jet); dlB.push(pd); pd = jd;
                float jt = pd * (pd * pd - 1.f); jt = clampf(jt, -1.f, 1.f);
                pd = jt + 0.5f * temp;
                pipePrev = pipeCur;
                lastOutA = dlA.read(bore); dlA.push(pd);
                pipeCur = lastOutA;
            }
            const float y = pipePrev + (pipeCur - pipePrev) * float(pipePhase + 1) / float(pipeSub);
            pipePhase = (pipePhase + 1) % pipeSub;
            toneLp = y + (toneLp - y) * toneC;
            out[i] = 0.9f * toneLp;
        }
    }

    // ---------------------------------------------------------------- Bowed string (after STK Bowed)
    void renderBowed(float* out, int n, const OscControl& c) {
        const float base = std::max(4.f, sr / c.freq - 3.4f);  // STK uses -4; -3.4 measured in tune at 48 kHz
        // Parameter ranges come from a sweep (analysis/sweep.py): outside beta ~0.085-0.115, low string loss,
        // or bow velocity above ~0.09 the model falls into double-slip and plays an octave high.
        const float beta = 0.095f + 0.02f * c.shape;            // bow position
        const float slope = 5.f - 4.f * c.timbre;               // bow pressure
        const float loss = 0.985f + 0.01f * c.chr;
        const float pole = 0.75f - 0.2f * 22050.f / sr;
        const float maxV = 0.03f + 0.03f * c.vel;
        const float target = c.gate ? maxV : 0.f;
        const float envC = std::exp(-1.f / ((c.gate ? 0.06f : 0.05f) * sr));
        bodyA.set(280.f, 2.f, sr); bodyB.set(560.f, 3.f, sr); bodyC.set(2900.f, 1.2f, sr);
        for (int i = 0; i < n; ++i) {
            breath = target + (breath - target) * envC;          // bow velocity envelope
            const float bowV = breath * (1.f + c.noise * 0.3f * rng.bipolar());
            lpState = loss * (1.f - pole) * lastOutB + pole * lpState;
            const float bridgeRefl = -lpState;
            const float nutRefl = -lastOutA;
            const float dv = bowV - (bridgeRefl + nutRefl);
            float bt = std::fabs((dv + 0.001f) * slope) + 0.75f;
            bt = clampf(std::pow(bt, -4.f), 0.01f, 0.98f);
            const float nv = dv * bt;
            lastOutA = dlA.read(base * (1.f - beta)); dlA.push(bridgeRefl + nv);
            lastOutB = dlB.read(base * beta); dlB.push(nutRefl + nv);
            const float s = lastOutB;
            const float bm = c.fmDepth;                           // "FM Depth" knob = wooden body resonance amount
            const float body = s + bm * (0.6f * bodyA.k * bodyA.bp(s) + 0.4f * bodyB.k * bodyB.bp(s) + 0.3f * bodyC.k * bodyC.bp(s));
            out[i] = body * 8.f;
        }
    }

    // ---------------------------------------------------------------- Atmospheric noise through a tuned comb
    void renderAtmos(float* out, int n, const OscControl& c) {
        const float g = gust.advance(n, rng, sr * 0.35f);
        const float turb = c.chr;
        const float fc = 150.f * std::pow(100.f, c.timbre) * std::exp2(turb * g * 2.5f);
        colour.set(fc, 0.7f + c.fmDepth * 4.f, sr);
        const float fb = c.shape * 0.985f;
        const float L = std::max(2.f, sr / c.freq);
        const float amp = (1.f + turb * g * 0.8f) * (1.f - fb * 0.92f);
        for (int i = 0; i < n; ++i) {
            const float x = colour.lp(rng.bipolar());
            const float d = dlA.read(L);
            lpState += 0.6f * (d - lpState);
            const float y = x + fb * lpState;
            dlA.push(y);
            out[i] = y * amp * 0.9f;
        }
    }
};

} // namespace op
