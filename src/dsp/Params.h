// Panaudium — sample-free layered synth. DSP core is AGPL-3.0 (see LICENSE).
#pragma once
#include <string>
#include <vector>

namespace op {

enum Engine { E_VA, E_Spectral, E_Granular, E_Pluck, E_Modal, E_Vocal, E_Pipe, E_Bowed, E_Atmos, E_NumEngines };
enum FilterType { F_Off, F_LP12, F_LP24, F_HP12, F_BP12, F_Notch, F_Formant, F_Comb, F_NumFilters };

// Per-layer parameters (mirrors the per-layer block Omnisphere stores: osc type,
// timbre/shape/symmetry, FM/ring, unison+drift, dual filter, amp/filter envelopes, LFOs).
enum LayerParam {
    L_On, L_Engine, L_Level, L_Pan, L_Octave, L_Semi, L_Fine,
    L_Timbre, L_Shape, L_Char, L_Noise, L_FMDepth, L_FMRatio,
    L_UniVoices, L_UniDetune, L_UniSpread, L_Drift,
    L_FiltType, L_Cutoff, L_Reso, L_KeyTrack, L_FiltEnv, L_Drive,
    L_AA, L_AD, L_AS, L_AR, L_FA, L_FD, L_FS, L_FR,
    L_L1Rate, L_L1Pitch, L_L1Cut, L_L2Rate, L_L2Timbre, L_L2Pan,
    L_VelSens,
    L_NumLayerParams
};

enum GlobalParam {
    G_Gain, G_Glide, G_ChorusMix, G_ChorusRate, G_DelayTime, G_DelayFB, G_DelayMix,
    G_RevSize, G_RevDamp, G_RevMix,
    G_NumGlobal
};

constexpr int kNumLayers = 2;
constexpr int kNumParams = kNumLayers * L_NumLayerParams + G_NumGlobal;
constexpr int lp(int layer, int p) { return layer * L_NumLayerParams + p; }
constexpr int gp(int g) { return kNumLayers * L_NumLayerParams + g; }

struct ParamDef {
    std::string id, name;
    float min, max, def;
    float centre = 0.f;               // >0: log-ish skew so that this value sits mid-knob
    bool isInt = false;
    std::vector<std::string> choices; // non-empty: choice parameter (value = index)
};

inline const std::vector<std::string>& engineNames() {
    static const std::vector<std::string> n = { "Analog/FM", "Spectral (additive)", "Granular", "Plucked String",
        "Modal (bar/bell/plate)", "Vocal Formant", "Blown Pipe", "Bowed String", "Atmos Noise" };
    return n;
}
inline const std::vector<std::string>& filterNames() {
    static const std::vector<std::string> n = { "Off", "LPF 12", "LPF 24", "HPF 12", "Bandpass", "Notch", "Formant", "Comb (Metal Pipe)" };
    return n;
}

inline std::vector<ParamDef> buildParamDefs() {
    std::vector<ParamDef> d(kNumParams);
    for (int l = 0; l < kNumLayers; ++l) {
        const std::string P = (l == 0 ? "A_" : "B_"), N = (l == 0 ? "A " : "B ");
        auto set = [&](int p, const char* id, const char* name, float mn, float mx, float df, float centre = 0.f, bool isInt = false) {
            d[lp(l, p)] = ParamDef{ P + id, N + name, mn, mx, df, centre, isInt, {} };
        };
        set(L_On, "on", "On", 0, 1, l == 0 ? 1.f : 0.f, 0, true);
        d[lp(l, L_Engine)] = ParamDef{ P + "engine", N + "Engine", 0, float(E_NumEngines - 1), 0, 0, true, engineNames() };
        set(L_Level, "level", "Level", 0, 1, 0.8f);
        set(L_Pan, "pan", "Pan", -1, 1, 0);
        set(L_Octave, "oct", "Octave", -3, 3, 0, 0, true);
        set(L_Semi, "semi", "Semitone", -12, 12, 0, 0, true);
        set(L_Fine, "fine", "Fine (cents)", -100, 100, 0);
        set(L_Timbre, "timbre", "Timbre", 0, 1, 0.5f);
        set(L_Shape, "shape", "Shape", 0, 1, 0.5f);
        set(L_Char, "char", "Character", 0, 1, 0.3f);
        set(L_Noise, "noise", "Noise", 0, 1, 0);
        set(L_FMDepth, "fmdepth", "FM Depth", 0, 1, 0);
        set(L_FMRatio, "fmratio", "FM Ratio", 0.25f, 8, 1, 1.5f);
        set(L_UniVoices, "univoices", "Unison Voices", 1, 8, 1, 0, true);
        set(L_UniDetune, "unidetune", "Unison Detune (cents)", 0, 100, 12, 20);
        set(L_UniSpread, "unispread", "Unison Spread", 0, 1, 0.6f);
        set(L_Drift, "drift", "Drift", 0, 1, 0.15f);
        d[lp(l, L_FiltType)] = ParamDef{ P + "ftype", N + "Filter Type", 0, float(F_NumFilters - 1), float(F_LP24), 0, true, filterNames() };
        set(L_Cutoff, "cutoff", "Cutoff (Hz)", 20, 20000, 12000, 1000);
        set(L_Reso, "reso", "Resonance", 0, 1, 0.1f);
        set(L_KeyTrack, "keytrack", "Key Tracking", 0, 1, 0.3f);
        set(L_FiltEnv, "fenvamt", "Filter Env (oct)", -8, 8, 0);
        set(L_Drive, "drive", "Drive", 0, 1, 0);
        set(L_AA, "aa", "Amp Attack", 0.001f, 20, 0.005f, 0.5f);
        set(L_AD, "ad", "Amp Decay", 0.001f, 20, 0.5f, 0.5f);
        set(L_AS, "as", "Amp Sustain", 0, 1, 0.8f);
        set(L_AR, "ar", "Amp Release", 0.001f, 20, 0.4f, 0.5f);
        set(L_FA, "fa", "Filt Attack", 0.001f, 20, 0.005f, 0.5f);
        set(L_FD, "fd", "Filt Decay", 0.001f, 20, 0.6f, 0.5f);
        set(L_FS, "fs", "Filt Sustain", 0, 1, 0.3f);
        set(L_FR, "fr", "Filt Release", 0.001f, 20, 0.5f, 0.5f);
        set(L_L1Rate, "l1rate", "LFO1 Rate", 0.01f, 20, 5, 1);
        set(L_L1Pitch, "l1pitch", "LFO1 > Pitch (st)", 0, 12, 0, 0.5f);
        set(L_L1Cut, "l1cut", "LFO1 > Cutoff (oct)", 0, 4, 0);
        set(L_L2Rate, "l2rate", "LFO2 Rate", 0.01f, 20, 0.3f, 1);
        set(L_L2Timbre, "l2timbre", "LFO2 > Timbre", 0, 1, 0);
        set(L_L2Pan, "l2pan", "LFO2 > Pan", 0, 1, 0);
        set(L_VelSens, "velsens", "Velocity Sens", 0, 1, 0.5f);
    }
    auto g = [&](int p, const char* id, const char* name, float mn, float mx, float df, float centre = 0.f) {
        d[gp(p)] = ParamDef{ std::string("g_") + id, name, mn, mx, df, centre, false, {} };
    };
    g(G_Gain, "gain", "Master Gain (dB)", -36, 6, -6);
    g(G_Glide, "glide", "Glide (s)", 0, 2, 0, 0.2f);
    g(G_ChorusMix, "chmix", "Chorus Mix", 0, 1, 0);
    g(G_ChorusRate, "chrate", "Chorus Rate", 0.05f, 5, 0.6f, 0.8f);
    g(G_DelayTime, "dltime", "Delay Time (s)", 0.02f, 1.5f, 0.375f, 0.35f);
    g(G_DelayFB, "dlfb", "Delay Feedback", 0, 0.95f, 0.35f);
    g(G_DelayMix, "dlmix", "Delay Mix", 0, 1, 0);
    g(G_RevSize, "rvsize", "Reverb Size", 0, 1, 0.5f);
    g(G_RevDamp, "rvdamp", "Reverb Damping", 0, 1, 0.4f);
    g(G_RevMix, "rvmix", "Reverb Mix", 0, 1, 0.2f);
    return d;
}

} // namespace op
