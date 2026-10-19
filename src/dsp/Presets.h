// Panaudium factory bank — every sound is generated from first principles, no samples. AGPL-3.0 (see LICENSE).
#pragma once
#include <array>
#include <functional>
#include <initializer_list>
#include <string>
#include <vector>
#include "Params.h"
#include "Dsp.h"

namespace op {

struct Preset {
    std::string name, category;
    std::array<float, kNumParams> v;
};

class PresetBuilder {
public:
    PresetBuilder(std::string name, std::string cat) {
        p.name = std::move(name); p.category = std::move(cat);
        const auto defs = buildParamDefs();
        for (int i = 0; i < kNumParams; ++i) p.v[size_t(i)] = defs[size_t(i)].def;
    }
    PresetBuilder& A(int param, float val) { p.v[size_t(lp(0, param))] = val; return *this; }
    PresetBuilder& B(int param, float val) { if (param != L_On) p.v[size_t(lp(1, L_On))] = 1; p.v[size_t(lp(1, param))] = val; return *this; }
    PresetBuilder& G(int param, float val) { p.v[size_t(gp(param))] = val; return *this; }
    PresetBuilder& ampA(float a, float d, float s, float r) { return A(L_AA, a).A(L_AD, d).A(L_AS, s).A(L_AR, r); }
    PresetBuilder& ampB(float a, float d, float s, float r) { return B(L_AA, a).B(L_AD, d).B(L_AS, s).B(L_AR, r); }
    PresetBuilder& filtA(int type, float cut, float res) { return A(L_FiltType, float(type)).A(L_Cutoff, cut).A(L_Reso, res); }
    PresetBuilder& filtB(int type, float cut, float res) { return B(L_FiltType, float(type)).B(L_Cutoff, cut).B(L_Reso, res); }
    PresetBuilder& uniA(int n, float det, float spread) { return A(L_UniVoices, float(n)).A(L_UniDetune, det).A(L_UniSpread, spread); }
    PresetBuilder& uniB(int n, float det, float spread) { return B(L_UniVoices, float(n)).B(L_UniDetune, det).B(L_UniSpread, spread); }
    PresetBuilder& oscA(int engine, float timbre, float shape, float chr) { return A(L_Engine, float(engine)).A(L_Timbre, timbre).A(L_Shape, shape).A(L_Char, chr); }
    PresetBuilder& oscB(int engine, float timbre, float shape, float chr) { return B(L_Engine, float(engine)).B(L_Timbre, timbre).B(L_Shape, shape).B(L_Char, chr); }
    // Put a unit in an FX slot. `real` gives knob values in real-world units (as listed in
    // fx/FxUnit.h, e.g. Hz, dB, s; percent knobs as 0..1); missing or NaN entries keep the unit's default.
    // Common-rack convention used by the helpers below: slot 0 free (drive/EQ/etc.), 1 chorus/mod,
    // 2 delay, 3 reverb.
    PresetBuilder& fx(int rack, int idx, int type, int model, float mix, std::initializer_list<float> real = {}) {
        const int s = fxSlot(rack, idx);
        p.v[size_t(fxp(s, S_Type))] = float(type);
        p.v[size_t(fxp(s, S_On))] = 1.f;
        p.v[size_t(fxp(s, S_Mix))] = mix;
        p.v[size_t(fxp(s, S_Model))] = float(model);
        for (int k = 0; k < kFxKnobs; ++k) {
            const FxKnob& kn = fxKnob(type, k);
            float n = kn.name ? kn.defNorm() : 0.5f;
            if (size_t(k) < real.size()) { const float v = *(real.begin() + k); if (v == v) n = kn.norm(v); }
            p.v[size_t(fxp(s, S_K1 + k))] = n;
        }
        return *this;
    }
    PresetBuilder& noFx(int rack, int idx) { p.v[size_t(fxp(fxSlot(rack, idx), S_Type))] = float(FX_Off); return *this; }
    // Legacy helpers (the first bank was written against a fixed chorus -> delay -> reverb chain).
    PresetBuilder& rev(float mix, float size, float damp = 0.4f) {
        if (mix <= 0.f) return noFx(R_Common, 3);
        return fx(R_Common, 3, FX_ProVerb, 1, mix, { size, 0.4f + size * size * 14.f, 20.f, 0.7f, 0.f, lerpf(2.f, -10.f, damp) });
    }
    PresetBuilder& dly(float mix, float time, float fb) { return fx(R_Common, 2, FX_Delay, 2, mix, { time, fb }); }
    PresetBuilder& chorus(float mix, float rate = 0.6f) { return fx(R_Common, 1, FX_Chorus, 0, mix, { rate }); }
    Preset done() const { return p; }
private:
    Preset p;
};

// Bank 1: the original 35 presets.
inline void addBank1(std::vector<Preset>& b) {
    {
        using PB = PresetBuilder;
        // ---------------------------------------------------------------- Pads
        b.push_back(PB("Glass Cathedral", "Pad")
            .oscA(E_Modal, 0.66f, 0.6f, 0.75f).A(L_Octave, -1).A(L_Noise, 0.12f).uniA(3, 6, 0.8f).filtA(F_LP24, 9000, 0.1f).ampA(0.01f, 4, 0.6f, 4)
            .oscB(E_Spectral, 0.4f, 0.3f, 0.1f).B(L_Drift, 0.7f).uniB(4, 15, 0.9f).B(L_Level, 0.55f).filtB(F_LP12, 7000, 0.1f).ampB(2.5f, 1, 1, 5)
            .chorus(0.3f).rev(0.45f, 0.85f).done());
        b.push_back(PB("Choir of Ghosts", "Pad")
            .oscA(E_Vocal, 0.0f, 0.45f, 0.35f).A(L_L2Timbre, 0.45f).A(L_L2Rate, 0.08f).uniA(5, 18, 1).A(L_Drift, 0.4f).A(L_FiltType, F_Off).ampA(1.2f, 1, 1, 3)
            .oscB(E_Atmos, 0.55f, 0.9f, 0.6f).B(L_Level, 0.3f).B(L_FiltType, F_Off).ampB(2, 1, 1, 3)
            .rev(0.5f, 0.9f).done());
        b.push_back(PB("Aurora Granules", "Pad")
            .oscA(E_Granular, 0.55f, 0.7f, 0.55f).A(L_FMDepth, 0.3f).uniA(2, 8, 1).filtA(F_LP12, 6000, 0.1f).ampA(1.5f, 1, 1, 4)
            .oscB(E_Spectral, 0.2f, 0.5f, 0).B(L_Octave, -1).B(L_Level, 0.4f).ampB(2, 1, 1, 4)
            .dly(0.25f, 0.5f, 0.5f).rev(0.5f, 0.8f).done());
        b.push_back(PB("Analog Dreams", "Pad")
            .oscA(E_VA, 0.0f, 0.5f, 0).uniA(7, 22, 0.9f).A(L_Drift, 0.3f).filtA(F_LP24, 1800, 0.25f).A(L_FiltEnv, 2).A(L_FA, 1.5f).A(L_FD, 3).A(L_FS, 0.4f).ampA(0.8f, 1, 1, 2.5f)
            .chorus(0.5f).rev(0.3f, 0.6f).done());
        b.push_back(PB("Singing Bowl Drone", "Pad")
            .oscA(E_Bowed, 0.4f, 0.15f, 0.9f).uniA(3, 7, 0.7f).filtA(F_LP12, 5000, 0.1f).ampA(1.2f, 1, 1, 3).A(L_Level, 0.6f)
            .oscB(E_Modal, 0.95f, 0.2f, 0.95f).B(L_Noise, 0.7f).B(L_Octave, 1).filtB(F_Off, 20000, 0).ampB(2, 1, 1, 5)
            .rev(0.5f, 0.9f).done());
        b.push_back(PB("Hyper Saw Supernova", "Pad")
            .oscA(E_VA, 0.0f, 0.5f, 0).uniA(8, 35, 1).A(L_Drift, 0.25f).filtA(F_LP24, 8000, 0.15f).ampA(0.05f, 1, 1, 1.5f)
            .oscB(E_Spectral, 0.15f, 0.2f, 0).B(L_Octave, -1).B(L_Level, 0.5f).ampB(0.05f, 1, 1, 1.5f)
            .chorus(0.4f).rev(0.4f, 0.7f).done());
        b.push_back(PB("Glass Harmonica", "Pad")
            .oscA(E_Spectral, 0.45f, 0.8f, 0.35f).A(L_Drift, 0.5f).uniA(2, 4, 0.6f).filtA(F_Off, 20000, 0).ampA(0.35f, 1, 1, 2)
            .oscB(E_Modal, 0.2f, 0.3f, 0.9f).B(L_Noise, 0.5f).B(L_Level, 0.5f).filtB(F_Off, 20000, 0).ampB(0.4f, 1, 1, 2)
            .rev(0.45f, 0.75f).done());
        b.push_back(PB("Alien Choir", "Pad")
            .oscA(E_Vocal, 0.5f, 0.2f, 0.25f).A(L_L2Timbre, 0.5f).A(L_L2Rate, 0.15f).uniA(6, 30, 1).A(L_FiltType, F_Off).ampA(1.5f, 1, 1, 4)
            .oscB(E_Granular, 0.3f, 0.8f, 0.8f).B(L_Level, 0.4f).B(L_FMDepth, 0.5f).ampB(2, 1, 1, 4)
            .rev(0.65f, 0.95f).done());
        // ---------------------------------------------------------------- Plucks / keys / mallets
        b.push_back(PB("Harp of Wire", "Pluck")
            .oscA(E_Pluck, 0.7f, 0.2f, 0.5f).A(L_FiltType, F_Off).ampA(0.001f, 6, 0.9f, 1.5f)
            .dly(0.2f, 0.375f, 0.35f).rev(0.3f, 0.6f).done());
        b.push_back(PB("Nylon Pluck", "Pluck")
            .oscA(E_Pluck, 0.35f, 0.15f, 0.35f).filtA(F_LP12, 6000, 0.05f).ampA(0.001f, 4, 0.9f, 0.6f)
            .rev(0.2f, 0.4f).done());
        b.push_back(PB("Koto Steel", "Pluck")
            .oscA(E_Pluck, 0.85f, 0.08f, 0.55f).A(L_FiltType, F_Off).ampA(0.001f, 5, 0.9f, 1)
            .oscB(E_Modal, 0.0f, 0.7f, 0.2f).B(L_Level, 0.35f).filtB(F_Off, 20000, 0).ampB(0.001f, 1, 0.8f, 0.6f)
            .rev(0.3f, 0.5f).done());
        b.push_back(PB("Sitar Buzz", "Pluck")
            .oscA(E_Pluck, 0.9f, 0.03f, 0.6f).filtA(F_Comb, 523.25f, 0.6f).A(L_KeyTrack, 1).ampA(0.001f, 5, 0.9f, 1.2f)
            .rev(0.3f, 0.6f).done());
        b.push_back(PB("Marimba Hall", "Mallet")
            .oscA(E_Modal, 0.33f, 0.45f, 0.35f).A(L_FiltType, F_Off).ampA(0.001f, 3, 0.9f, 0.8f)
            .rev(0.3f, 0.6f).done());
        b.push_back(PB("Vibraphone", "Mallet")
            .oscA(E_Modal, 0.31f, 0.3f, 0.6f).A(L_FiltType, F_Off).A(L_L2Pan, 0.6f).A(L_L2Rate, 5).ampA(0.001f, 5, 0.9f, 1.5f)
            .rev(0.3f, 0.5f).done());
        b.push_back(PB("Church Bells", "Mallet")
            .oscA(E_Modal, 0.667f, 0.75f, 0.8f).A(L_Octave, -2).A(L_FiltType, F_Off).ampA(0.001f, 8, 0.9f, 6)
            .rev(0.55f, 0.9f).done());
        b.push_back(PB("Tubular Chimes", "Mallet")
            .oscA(E_Modal, 0.2f, 0.8f, 0.7f).A(L_FiltType, F_Off).ampA(0.001f, 6, 0.9f, 4)
            .rev(0.4f, 0.7f).done());
        b.push_back(PB("Tibetan Bowl", "Mallet")
            .oscA(E_Modal, 1.0f, 0.4f, 0.95f).A(L_Noise, 0.25f).A(L_FiltType, F_Off).ampA(0.02f, 10, 0.9f, 6)
            .rev(0.5f, 0.85f).done());
        b.push_back(PB("Kalimba", "Mallet")
            .oscA(E_Modal, 0.3f, 0.6f, 0.25f).A(L_FiltType, F_Off).ampA(0.001f, 2, 0.9f, 0.6f)
            .oscB(E_Pluck, 0.6f, 0.3f, 0.15f).B(L_Level, 0.35f).B(L_Octave, 1).B(L_FiltType, F_Off).ampB(0.001f, 1, 0.9f, 0.3f)
            .dly(0.15f, 0.25f, 0.3f).rev(0.25f, 0.5f).done());
        b.push_back(PB("FM Tine Piano", "Keys")
            .oscA(E_VA, 1.0f, 0.5f, 0).A(L_FMDepth, 0.38f).A(L_FMRatio, 1).filtA(F_LP12, 2500, 0.05f).A(L_FiltEnv, 2.5f).A(L_FD, 1.2f).A(L_FS, 0.1f).ampA(0.002f, 2.5f, 0.15f, 0.5f)
            .oscB(E_VA, 1.0f, 0.5f, 0).B(L_FMDepth, 0.3f).B(L_FMRatio, 7).B(L_Level, 0.35f).B(L_FiltType, F_Off).ampB(0.001f, 0.25f, 0, 0.2f)
            .chorus(0.35f).rev(0.25f, 0.5f).done());
        b.push_back(PB("Hollow Organ", "Keys")
            .oscA(E_Spectral, 0.3f, 0.9f, 0).uniA(2, 4, 0.5f).A(L_FiltType, F_Off).A(L_Drift, 0.05f).ampA(0.01f, 1, 1, 0.15f)
            .oscB(E_Spectral, 0.5f, 0.0f, 0).B(L_Octave, 1).B(L_Level, 0.35f).B(L_L2Pan, 0.8f).B(L_L2Rate, 6).B(L_FiltType, F_Off).ampB(0.01f, 1, 1, 0.15f)
            .rev(0.3f, 0.6f).done());
        // ---------------------------------------------------------------- Bass
        b.push_back(PB("Ladder Bass", "Bass")
            .oscA(E_VA, 0.1f, 0.5f, 0).A(L_Octave, -1).uniA(2, 8, 0.3f).filtA(F_LP24, 250, 0.4f).A(L_FiltEnv, 4).A(L_FD, 0.25f).A(L_FS, 0.1f).A(L_Drive, 0.3f)
            .ampA(0.002f, 0.6f, 0.8f, 0.1f).rev(0.05f, 0.3f).done());
        b.push_back(PB("Plucked Upright", "Bass")
            .oscA(E_Pluck, 0.4f, 0.12f, 0.45f).A(L_Octave, -1).filtA(F_LP12, 3000, 0.05f).ampA(0.001f, 3, 0.9f, 0.3f)
            .oscB(E_VA, 1.0f, 0.5f, 0).B(L_Octave, -1).B(L_Level, 0.4f).B(L_FiltType, F_Off).ampB(0.003f, 1.5f, 0.3f, 0.2f)
            .rev(0.1f, 0.3f).done());
        b.push_back(PB("Formant Growl", "Bass")
            .oscA(E_VA, 0.05f, 0.5f, 0).A(L_Octave, -1).uniA(3, 12, 0.5f).filtA(F_Formant, 600, 0.7f).A(L_L1Rate, 2).A(L_L1Cut, 2.5f).A(L_Drive, 0.5f).A(L_KeyTrack, 0)
            .ampA(0.005f, 1, 1, 0.15f).rev(0.1f, 0.3f).done());
        b.push_back(PB("Bowed Cello", "Strings")
            .oscA(E_Bowed, 0.5f, 0.13f, 0.7f).A(L_Octave, -1).A(L_L1Pitch, 0.12f).A(L_L1Rate, 5).filtA(F_LP12, 6000, 0.05f).ampA(0.1f, 1, 1, 0.4f)
            .rev(0.3f, 0.6f).done());
        b.push_back(PB("String Ensemble", "Strings")
            .oscA(E_Bowed, 0.45f, 0.2f, 0.75f).uniA(4, 10, 0.9f).A(L_Drift, 0.3f).filtA(F_LP12, 5000, 0.05f).ampA(0.4f, 1, 1, 1)
            .chorus(0.3f).rev(0.4f, 0.75f).done());
        // ---------------------------------------------------------------- Leads, winds, voices
        b.push_back(PB("Sync Lead", "Lead")
            .oscA(E_VA, 0.05f, 0.5f, 0.75f).uniA(2, 6, 0.4f).filtA(F_LP24, 6000, 0.15f).A(L_L1Pitch, 0.15f).A(L_L1Rate, 5.5f).ampA(0.005f, 1, 0.9f, 0.3f)
            .G(G_Glide, 0.08f).dly(0.25f, 0.375f, 0.4f).rev(0.2f, 0.5f).done());
        b.push_back(PB("Breathy Flute", "Wind")
            .oscA(E_Pipe, 0.25f, 0.5f, 0.2f).A(L_L1Pitch, 0.15f).A(L_L1Rate, 5).A(L_FiltType, F_Off).ampA(0.02f, 1, 1, 0.25f)
            .rev(0.3f, 0.6f).done());
        b.push_back(PB("Shakuhachi", "Wind")
            .oscA(E_Pipe, 0.6f, 0.4f, 0.4f).A(L_L1Pitch, 0.2f).A(L_L1Rate, 4.5f).A(L_FiltType, F_Off).ampA(0.05f, 1, 1, 0.4f)
            .rev(0.4f, 0.7f).done());
        b.push_back(PB("Pan Pipes", "Wind")
            .oscA(E_Pipe, 0.4f, 0.32f, 0.1f).uniA(2, 5, 0.6f).A(L_FiltType, F_Off).ampA(0.03f, 1, 1, 0.3f)
            .dly(0.2f, 0.3f, 0.3f).rev(0.35f, 0.6f).done());
        b.push_back(PB("Soprano Aah", "Vocal")
            .oscA(E_Vocal, 0.0f, 0.72f, 0.2f).A(L_L1Pitch, 0.25f).A(L_L1Rate, 5.5f).A(L_FiltType, F_Off).ampA(0.15f, 1, 1, 0.6f)
            .rev(0.4f, 0.7f).done());
        b.push_back(PB("Robot Vowels", "Vocal")
            .oscA(E_Vocal, 0.5f, 0.35f, 0.0f).A(L_L2Timbre, 0.5f).A(L_L2Rate, 1.2f).A(L_FiltType, F_Off).ampA(0.01f, 1, 1, 0.2f)
            .dly(0.2f, 0.25f, 0.3f).rev(0.2f, 0.4f).done());
        // ---------------------------------------------------------------- Atmospheres / textures
        b.push_back(PB("Ocean Wind", "Atmos")
            .oscA(E_Atmos, 0.45f, 0.0f, 0.9f).uniA(2, 0, 1).A(L_FiltType, F_Off).ampA(2, 1, 1, 3)
            .rev(0.5f, 0.9f).done());
        b.push_back(PB("Whistling Wind", "Atmos")
            .oscA(E_Atmos, 0.7f, 0.93f, 0.7f).A(L_L1Pitch, 0.4f).A(L_L1Rate, 0.2f).uniA(2, 9, 1).A(L_FiltType, F_Off).ampA(1.5f, 1, 1, 3)
            .rev(0.55f, 0.9f).done());
        b.push_back(PB("Metal Pipe Drone", "Atmos")
            .oscA(E_VA, 0.0f, 0.5f, 0).uniA(3, 10, 0.8f).A(L_Drift, 0.5f).filtA(F_Comb, 261.63f, 0.92f).A(L_KeyTrack, 1).ampA(1, 1, 1, 3)
            .rev(0.45f, 0.8f).done());
        b.push_back(PB("Granular Swarm", "Atmos")
            .oscA(E_Granular, 1.0f, 0.15f, 0.9f).A(L_FMDepth, 0.8f).uniA(2, 0, 1).A(L_L2Pan, 0.7f).A(L_L2Rate, 0.4f).filtA(F_LP12, 9000, 0.1f).ampA(0.8f, 1, 1, 3)
            .dly(0.3f, 0.45f, 0.55f).rev(0.5f, 0.85f).done());
    }
}

} // namespace op

#include "presets/Bank2.h"
#include "presets/Bank3.h"
#include "presets/Trims.h"

namespace op {

inline const std::vector<Preset>& factoryPresets() {
    static const std::vector<Preset> bank = [] {
        std::vector<Preset> b;
        addBank1(b); addBank2(b); addBank3(b);
        applyLoudnessTrims(b);
        return b;
    }();
    return bank;
}

} // namespace op
