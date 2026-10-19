// Panaudium polyphonic two-layer synth. AGPL-3.0 (see LICENSE).
#pragma once
#include <array>
#include "Engines.h"
#include "fx/FxRack.h"

namespace op {

constexpr int kBlock = 16;      // control-rate sub-block
constexpr int kMaxUnison = 8;
constexpr int kMaxVoices = 16;

class LayerVoice {
public:
    void prepare(float rate) {
        sr = rate;
        for (auto& o : osc) o.prepare(sr);
        combL.init(int(sr / 15.f)); combR.init(int(sr / 15.f));
    }
    void noteOn(float note, float vel, const float* P, uint32_t seed) {
        rng = Rng(seed);
        nUni = int(clampf(P[L_UniVoices], 1, kMaxUnison));
        engine = int(P[L_Engine]);
        amp.set(P[L_AA], P[L_AD], P[L_AS], P[L_AR], sr); amp.level = 0; amp.gate(true);
        fenv.set(P[L_FA], P[L_FD], P[L_FS], P[L_FR], sr); fenv.level = 0; fenv.gate(true);
        lfo1 = 0; lfo2 = rng.unipolar();
        velocity = vel;
        for (auto& s : filt) s.reset();
        combL.clear(); combR.clear(); combLpL = combLpR = 0;
        for (auto& d : drift) d = Drift{};
        OscControl c = makeControl(note, P, 0, 0);
        for (int u = 0; u < nUni; ++u) osc[u].start(engine, c, rng.next());
        gate = true;
    }
    void noteOff() { gate = false; amp.gate(false); fenv.gate(false); }
    bool active() const { return amp.active(); }
    float level() const { return amp.level; }

    // Adds n (<= kBlock) samples of this layer into L/R.
    void render(float* L, float* R, int n, float note, const float* P) {
        amp.set(P[L_AA], P[L_AD], P[L_AS], P[L_AR], sr);
        fenv.set(P[L_FA], P[L_FD], P[L_FS], P[L_FR], sr);
        const float a = amp.advance(n), fe = fenv.advance(n);
        lfo1 += P[L_L1Rate] * float(n) / sr; lfo1 -= std::floor(lfo1);
        lfo2 += P[L_L2Rate] * float(n) / sr; lfo2 -= std::floor(lfo2);
        const float l1 = fsin(lfo1), l2 = fsin(lfo2);

        float tmp[kBlock], bufL[kBlock] = {}, bufR[kBlock] = {};
        const float detune = P[L_UniDetune], spread = P[L_UniSpread];
        for (int u = 0; u < nUni; ++u) {
            const float pos = nUni > 1 ? 2.f * float(u) / float(nUni - 1) - 1.f : 0.f;
            const float dr = drift[u].advance(n, rng, sr);
            const float cents = detune * pos + P[L_Drift] * 14.f * dr;
            OscControl c = makeControl(note + cents * 0.01f + l1 * P[L_L1Pitch], P, l2, u);
            osc[u].render(tmp, n, c);
            const float pan = clampf(spread * pos + l2 * P[L_L2Pan], -1.f, 1.f);
            const float gl = std::cos((pan + 1.f) * kPi * 0.25f), gr = std::sin((pan + 1.f) * kPi * 0.25f);
            for (int i = 0; i < n; ++i) { bufL[i] += tmp[i] * gl; bufR[i] += tmp[i] * gr; }
        }
        const float uniNorm = 1.f / std::sqrt(float(nUni));

        // Drive (pre-filter saturation)
        const float drive = P[L_Drive];
        if (drive > 0.001f) {
            const float k = 1.f + drive * 9.f, nrm = 1.f / fastTanh(k * 0.5f) * 0.5f;
            for (int i = 0; i < n; ++i) { bufL[i] = fastTanh(bufL[i] * uniNorm * k) * nrm / uniNorm; bufR[i] = fastTanh(bufR[i] * uniNorm * k) * nrm / uniNorm; }
        }

        applyFilter(bufL, bufR, n, note, fe, l1, P);

        const float velGain = 1.f - P[L_VelSens] * (1.f - velocity);
        const float g = a * P[L_Level] * velGain * uniNorm;
        const float pan = P[L_Pan];
        const float pl = pan > 0 ? 1.f - pan : 1.f, pr = pan < 0 ? 1.f + pan : 1.f;
        for (int i = 0; i < n; ++i) { L[i] += bufL[i] * g * pl; R[i] += bufR[i] * g * pr; }
    }

private:
    float sr = 48000;
    EngineVoice osc[kMaxUnison];
    Drift drift[kMaxUnison];
    Adsr amp, fenv;
    Svf filt[6];
    DelayLine combL, combR; float combLpL = 0, combLpR = 0;
    Rng rng;
    int nUni = 1, engine = 0;
    float lfo1 = 0, lfo2 = 0, velocity = 1;
    bool gate = false;

    OscControl makeControl(float note, const float* P, float l2, int) const {
        OscControl c;
        c.freq = clampf(midiToHz(note + 12.f * P[L_Octave] + P[L_Semi] + P[L_Fine] * 0.01f), 8.f, sr * 0.45f);
        c.timbre = clampf(P[L_Timbre] + l2 * P[L_L2Timbre], 0.f, 1.f);
        c.shape = P[L_Shape]; c.chr = P[L_Char]; c.noise = P[L_Noise];
        c.fmDepth = P[L_FMDepth]; c.fmRatio = P[L_FMRatio]; c.anim = P[L_Drift];
        c.vel = velocity; c.gate = gate;
        return c;
    }

    void applyFilter(float* L, float* R, int n, float note, float fe, float l1, const float* P) {
        const int type = int(P[L_FiltType]);
        if (type == F_Off) return;
        const float oct = P[L_KeyTrack] * (note - 60.f) / 12.f + P[L_FiltEnv] * fe + l1 * P[L_L1Cut];
        const float fc = clampf(P[L_Cutoff] * std::exp2(oct), 20.f, sr * 0.45f);
        const float reso = P[L_Reso];
        const float q = 0.5f + reso * reso * 18.f;
        if (type == F_Comb) {
            const float d = sr / fc, fb = reso * 0.97f;
            for (int i = 0; i < n; ++i) {
                combLpL += 0.7f * (combL.read(d) - combLpL); combLpR += 0.7f * (combR.read(d) - combLpR);
                const float yl = L[i] + fb * combLpL, yr = R[i] + fb * combLpR;
                combL.push(yl); combR.push(yr);
                L[i] = yl * (1.f - fb * 0.6f); R[i] = yr * (1.f - fb * 0.6f);
            }
            return;
        }
        if (type == F_Formant) {
            // Cutoff knob sweeps the vowel (A..U); resonance sharpens the formants.
            float fo[5], go[5], bo[5];
            const float pos = clampf(std::log2(fc / 100.f) / std::log2(80.f), 0.f, 1.f);
            VowelTable::get().lookup(pos, fo, go, bo);
            float kq[3];
            for (int k = 0; k < 3; ++k) {
                const float qq = fo[k] / bo[k] * (0.5f + reso * 1.5f);
                filt[k].set(fo[k], qq, sr); filt[k + 3].set(fo[k], qq, sr); kq[k] = 1.f / qq;
            }
            for (int i = 0; i < n; ++i) {
                float yl = 0, yr = 0;
                for (int k = 0; k < 3; ++k) { yl += go[k] * kq[k] * filt[k].bp(L[i]); yr += go[k] * kq[k] * filt[k + 3].bp(R[i]); }
                L[i] = yl * 2.f; R[i] = yr * 2.f;
            }
            return;
        }
        filt[0].set(fc, q, sr); filt[1].set(fc, q, sr);
        if (type == F_LP24) { filt[2].set(fc, 0.5f + reso * 0.6f, sr); filt[3].set(fc, 0.5f + reso * 0.6f, sr); }
        for (int i = 0; i < n; ++i) {
            float lp, bp, hp, lp2, bp2, hp2;
            filt[0].tick(L[i], lp, bp, hp); filt[1].tick(R[i], lp2, bp2, hp2);
            switch (type) {
            case F_LP12: L[i] = lp; R[i] = lp2; break;
            case F_LP24: L[i] = filt[2].lp(lp); R[i] = filt[3].lp(lp2); break;
            case F_HP12: L[i] = hp; R[i] = hp2; break;
            case F_BP12: L[i] = bp * filt[0].k * 1.5f; R[i] = bp2 * filt[1].k * 1.5f; break;
            default: L[i] = lp + hp; R[i] = lp2 + hp2; break;  // notch
            }
        }
    }
};

class Synth {
public:
    Synth() {
        const auto defs = buildParamDefs();
        for (int i = 0; i < kNumParams; ++i) params[size_t(i)] = defs[size_t(i)].def;
    }
    void prepare(float rate) {
        sr = rate;
        for (auto& v : voices) for (auto& l : v.layer) l.prepare(sr);
        for (int k = 0; k < kNumLayers; ++k) { busL[k].assign(size_t(kChunk), 0.f); busR[k].assign(size_t(kChunk), 0.f); }
        for (auto& s : slots) s.prepare(sr, kChunk);
        masterDcL = DcBlock{}; masterDcR = DcBlock{}; masterDcL.R = masterDcR.R = 1.f - kTwoPi * 5.f / sr;
        preloadFx(params.data());
    }
    void setParam(int idx, float v) { if (idx >= 0 && idx < kNumParams) params[size_t(idx)] = v; }
    float getParam(int idx) const { return params[size_t(idx)]; }
    const std::array<float, kNumParams>& allParams() const { return params; }

    void noteOn(int note, float vel) {
        Voice* v = pickVoice();
        const float glide = params[size_t(gp(G_Glide))];
        v->note = note; v->target = float(note);
        v->cur = (glide > 0.001f && lastNote >= 0) ? float(lastNote) : float(note);
        v->age = ++ageCounter; v->held = true; v->sustained = false;
        for (int l = 0; l < kNumLayers; ++l) {
            v->layerOn[l] = params[size_t(lp(l, L_On))] > 0.5f;
            if (v->layerOn[l]) v->layer[l].noteOn(v->cur, vel, &params[size_t(lp(l, 0))], seedRng.next());
        }
        lastNote = note;
    }
    void noteOff(int note) {
        for (auto& v : voices) if (v.held && v.note == note) {
            v.held = false;
            if (sustain) v.sustained = true; else release(v);
        }
    }
    void setSustain(bool on) {
        sustain = on;
        if (!on) for (auto& v : voices) if (v.sustained) { v.sustained = false; release(v); }
    }
    void allNotesOff() { for (auto& v : voices) { v.held = v.sustained = false; release(v); } }

    // Create the FX units a parameter set needs ahead of time (call from a non-audio thread, e.g. on
    // preset load), so the audio thread never allocates when a preset switches FX types.
    void preloadFx(const float* p) {
        for (int s = 0; s < kNumFxSlots; ++s) slots[s].preload(int(p[fxp(s, S_Type)] + 0.5f));
    }

    // Like preloadFx, plus let units precompute heavy state (Innerspace IRs) for these settings. Blocking; never call
    // from the audio thread.
    void primeFx(const float* p) {
        for (int s = 0; s < kNumFxSlots; ++s) slots[s].prime(&p[fxp(s, S_Type)]);
    }

    void process(float* L, float* R, int n) {
        int done = 0;
        while (done < n) {
            const int m = std::min(kChunk, n - done);
            processChunk(L + done, R + done, m);
            done += m;
        }
    }

private:
    static constexpr int kChunk = 512;
    void processChunk(float* L, float* R, int n) {
        for (int k = 0; k < kNumLayers; ++k) { std::fill_n(busL[k].data(), n, 0.f); std::fill_n(busR[k].data(), n, 0.f); }
        int done = 0;
        while (done < n) {
            const int m = std::min(kBlock, n - done);
            const float glide = params[size_t(gp(G_Glide))];
            const float gc = glide > 0.001f ? 1.f - std::exp(-float(m) / (glide * sr)) : 1.f;
            for (auto& v : voices) {
                if (!v.active()) continue;
                v.cur += (v.target - v.cur) * gc;
                for (int k = 0; k < kNumLayers; ++k)
                    if (v.layerOn[k] && v.layer[k].active())
                        v.layer[k].render(busL[k].data() + done, busR[k].data() + done, m, v.cur, &params[size_t(lp(k, 0))]);
            }
            done += m;
        }
        const float* P = params.data();
        // Per-layer racks, then sum, then the common rack.
        for (int k = 0; k < kNumLayers; ++k)
            for (int i = 0; i < kRackSlots[k]; ++i) {
                const int s = fxSlot(k, i);
                slots[s].process(busL[k].data(), busR[k].data(), n, &P[fxp(s, S_Type)]);
            }
        for (int i = 0; i < n; ++i) { L[i] = busL[0][size_t(i)] + busL[1][size_t(i)]; R[i] = busR[0][size_t(i)] + busR[1][size_t(i)]; }
        for (int i = 0; i < kRackSlots[R_Common]; ++i) {
            const int s = fxSlot(R_Common, i);
            slots[s].process(L, R, n, &P[fxp(s, S_Type)]);
        }
        // Master: DC blocker (safety net for engine/filter DC) -> gain -> soft limiter.
        const float g = dbToGain(P[gp(G_Gain)]);
        for (int i = 0; i < n; ++i) { L[i] = fastTanh(masterDcL.tick(L[i]) * g); R[i] = fastTanh(masterDcR.tick(R[i]) * g); }
    }
public:

private:
    struct Voice {
        LayerVoice layer[kNumLayers];
        bool layerOn[kNumLayers] = {};
        int note = -1; float cur = 60, target = 60; uint64_t age = 0; bool held = false, sustained = false;
        bool active() const {
            for (int k = 0; k < kNumLayers; ++k) if (layerOn[k] && layer[k].active()) return true;
            return false;
        }
        float level() const { float m = 0; for (int k = 0; k < kNumLayers; ++k) if (layerOn[k]) m = std::max(m, layer[k].level()); return m; }
    };
    std::array<float, kNumParams> params {};
    Voice voices[kMaxVoices];
    FxSlot slots[kNumFxSlots];
    DcBlock masterDcL, masterDcR;
    std::vector<float> busL[kNumLayers], busR[kNumLayers];
    Rng seedRng { 0xC0FFEEu };
    float sr = 48000;
    uint64_t ageCounter = 0;
    int lastNote = -1;
    bool sustain = false;

    static void release(Voice& v) { for (auto& l : v.layer) l.noteOff(); }
    Voice* pickVoice() {
        for (auto& v : voices) if (!v.active()) return &v;
        Voice* best = nullptr;  // prefer the quietest released voice, else the oldest
        for (auto& v : voices) if (!v.held && !v.sustained && (!best || v.level() < best->level())) best = &v;
        if (best) return best;
        for (auto& v : voices) if (!best || v.age < best->age) best = &v;
        return best;
    }
};

} // namespace op
