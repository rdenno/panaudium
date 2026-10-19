// Panaudium — sample-free layered synth. DSP core is AGPL-3.0 (see LICENSE).
#pragma once
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>
#include "fx/FxUnit.h"

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

enum GlobalParam { G_Gain, G_Glide, G_NumGlobal };

// FX racks, as in Omnisphere: one per layer plus a common rack on the summed output.
enum FxRackId { R_A, R_B, R_Common, R_NumRacks };
constexpr int kRackSlots[R_NumRacks] = { 3, 3, 4 };
constexpr int kRackFirstSlot[R_NumRacks] = { 0, 3, 6 };
constexpr int kNumFxSlots = 10;
enum FxSlotParam { S_Type, S_On, S_Mix, S_Model, S_K1, S_K2, S_K3, S_K4, S_K5, S_K6, S_NumSlotParams };

constexpr int kNumLayers = 2;
constexpr int kFxParamBase = kNumLayers * L_NumLayerParams + G_NumGlobal;
constexpr int kNumParams = kFxParamBase + kNumFxSlots * S_NumSlotParams;
constexpr int lp(int layer, int p) { return layer * L_NumLayerParams + p; }
constexpr int gp(int g) { return kNumLayers * L_NumLayerParams + g; }
constexpr int fxSlot(int rack, int i) { return kRackFirstSlot[rack] + i; }
constexpr int fxp(int slot, int p) { return kFxParamBase + slot * S_NumSlotParams + p; }

struct ParamDef {
    std::string id, name;
    float min, max, def;
    float centre = 0.f;               // >0: log-ish skew so that this value sits mid-knob
    bool isInt = false;
    std::vector<std::string> choices; // non-empty: choice parameter (value = index)
    std::string unit = "";            // display unit for formatValue(): "%", "Hz", "s", "dB", "ct", "st", "oct", "x", "pan"
};

// Human-readable value text shared by the plugin (host automation readouts) and the GUI.
inline std::string formatValue(float v, const std::string& unit) {
    char b[48];
    if (unit == "%") std::snprintf(b, sizeof b, "%.0f %%", v * 100.f);
    else if (unit == "Hz") {
        if (v >= 1000.f) std::snprintf(b, sizeof b, "%.2f kHz", v / 1000.f);
        else if (v >= 100.f) std::snprintf(b, sizeof b, "%.0f Hz", v);
        else std::snprintf(b, sizeof b, "%.2f Hz", v);
    }
    else if (unit == "s") { if (v < 1.f) std::snprintf(b, sizeof b, "%.0f ms", v * 1000.f); else std::snprintf(b, sizeof b, "%.2f s", v); }
    else if (unit == "ms") std::snprintf(b, sizeof b, "%.1f ms", v);
    else if (unit == "dB") std::snprintf(b, sizeof b, "%+.1f dB", v);
    else if (unit == "ct") std::snprintf(b, sizeof b, "%+.0f ct", v);
    else if (unit == "st") std::snprintf(b, sizeof b, "%.2f st", v);
    else if (unit == "oct") std::snprintf(b, sizeof b, "%+.2f oct", v);
    else if (unit == "x") std::snprintf(b, sizeof b, "%.2fx", v);
    else if (unit == "deg") std::snprintf(b, sizeof b, "%.0f deg", v);
    else if (unit == "pan") {
        if (v < -0.005f) std::snprintf(b, sizeof b, "L %.0f", -v * 100.f);
        else if (v > 0.005f) std::snprintf(b, sizeof b, "R %.0f", v * 100.f);
        else std::snprintf(b, sizeof b, "C");
    }
    else std::snprintf(b, sizeof b, "%.2f", v);
    return b;
}

// Inverse of formatValue for typed-in values: understands "350 ms", "2.4 kHz", "45 %", "L 20".
inline float parseValue(const std::string& text, const std::string& unit) {
    float v = std::strtof(text.c_str(), nullptr);
    auto has = [&](const char* s) { return text.find(s) != std::string::npos; };
    if (unit == "%") return v / 100.f;
    if (unit == "s" && has("ms")) return v / 1000.f;
    if (unit == "Hz" && (has("k") || has("K"))) return v * 1000.f;
    if (unit == "pan") { if (has("L") || has("l")) return -std::fabs(v) / 100.f; if (has("R") || has("r")) return std::fabs(v) / 100.f; return v; }
    return v;
}

inline const std::vector<std::string>& engineNames() {
    static const std::vector<std::string> n = { "Analog/FM", "Spectral (additive)", "Granular", "Plucked String",
        "Modal (bar/bell/plate)", "Vocal Formant", "Blown Pipe", "Bowed String", "Atmos Noise" };
    return n;
}
inline const std::vector<std::string>& filterNames() {
    static const std::vector<std::string> n = { "Off", "LPF 12", "LPF 24", "HPF 12", "Bandpass", "Notch", "Formant", "Comb (Metal Pipe)" };
    return n;
}

// What the generic Timbre / Shape / Character / FM Depth knobs do for each engine (for UI labels).
struct EngineLabels { const char* timbre; const char* shape; const char* chr; const char* fmDepth; const char* fmRatio; };
inline const EngineLabels& engineLabels(int engine) {
    static const EngineLabels t[E_NumEngines] = {
        { "Wave Morph", "Pulse/Sym", "Hard Sync", "FM Depth", "FM Ratio" },        // VA
        { "Brightness", "Odd/Even", "Stretch", "Spread", "-" },                    // Spectral
        { "Density", "Grain Size", "Scatter", "Grain Tone", "-" },                 // Granular
        { "Brightness", "Pick Pos", "Sustain", "-", "-" },                         // Pluck
        { "Material", "Hardness", "Decay", "Inharm", "-" },                        // Modal
        { "Vowel", "Tract Size", "Breath", "-", "-" },                             // Vocal
        { "Breath Noise", "Tone", "Pressure", "-", "-" },                          // Pipe
        { "Bow Pressure", "Bow Position", "Sustain", "Body", "-" },                // Bowed
        { "Colour", "Pitch Focus", "Turbulence", "Resonance", "-" },               // Atmos
    };
    return t[(engine < 0 || engine >= E_NumEngines) ? 0 : engine];
}

inline const std::vector<std::string>& fxTypeNames() {
    static const std::vector<std::string> n = [] {
        std::vector<std::string> v;
        for (const auto& f : fxTable()) v.push_back(f.name);
        return v;
    }();
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
        static const std::pair<int, const char*> units[] = {
            { L_Level, "%" }, { L_Pan, "pan" }, { L_Fine, "ct" }, { L_Timbre, "%" }, { L_Shape, "%" }, { L_Char, "%" },
            { L_Noise, "%" }, { L_FMDepth, "%" }, { L_FMRatio, "x" }, { L_UniDetune, "ct" }, { L_UniSpread, "%" },
            { L_Drift, "%" }, { L_Cutoff, "Hz" }, { L_Reso, "%" }, { L_KeyTrack, "%" }, { L_FiltEnv, "oct" }, { L_Drive, "%" },
            { L_AA, "s" }, { L_AD, "s" }, { L_AS, "%" }, { L_AR, "s" }, { L_FA, "s" }, { L_FD, "s" }, { L_FS, "%" }, { L_FR, "s" },
            { L_L1Rate, "Hz" }, { L_L1Pitch, "st" }, { L_L1Cut, "oct" }, { L_L2Rate, "Hz" }, { L_L2Timbre, "%" }, { L_L2Pan, "%" },
            { L_VelSens, "%" } };
        for (const auto& u : units) d[lp(l, u.first)].unit = u.second;
    }
    auto g = [&](int p, const char* id, const char* name, float mn, float mx, float df, float centre = 0.f) {
        d[gp(p)] = ParamDef{ std::string("g_") + id, name, mn, mx, df, centre, false, {} };
    };
    g(G_Gain, "gain", "Master Gain (dB)", -36, 18, -6);
    g(G_Glide, "glide", "Glide (s)", 0, 2, 0, 0.2f);
    d[gp(G_Gain)].unit = "dB"; d[gp(G_Glide)].unit = "s";
    // FX slots: ids like "fxA1_type", "fxC4_k3"; knob meaning depends on the slot's unit type (see fx/FxUnit.h).
    static const char* rackTag[R_NumRacks] = { "A", "B", "C" };
    static const char* rackName[R_NumRacks] = { "FX A", "FX B", "FX Common " };
    for (int r = 0; r < R_NumRacks; ++r)
        for (int i = 0; i < kRackSlots[r]; ++i) {
            const int s = fxSlot(r, i);
            const std::string id = std::string("fx") + rackTag[r] + std::to_string(i + 1) + "_";
            const std::string nm = std::string(rackName[r]) + std::to_string(i + 1) + " ";
            d[fxp(s, S_Type)] = ParamDef{ id + "type", nm + "Type", 0, float(fxTypeNames().size() - 1), 0, 0, true, fxTypeNames() };
            d[fxp(s, S_On)] = ParamDef{ id + "on", nm + "On", 0, 1, 1, 0, true, {} };
            d[fxp(s, S_Mix)] = ParamDef{ id + "mix", nm + "Mix", 0, 1, 1, 0, false, {}, "%" };
            d[fxp(s, S_Model)] = ParamDef{ id + "model", nm + "Model", 0, 7, 0, 0, true, {} };
            for (int k = 0; k < 6; ++k)
                d[fxp(s, S_K1 + k)] = ParamDef{ id + "k" + std::to_string(k + 1), nm + "Knob " + std::to_string(k + 1), 0, 1, 0.5f, 0, false, {} };
        }
    // Init patch: a Hall reverb in the last common slot.
    const int rv = fxSlot(R_Common, 3);
    d[fxp(rv, S_Type)].def = float(FX_ProVerb);
    d[fxp(rv, S_Mix)].def = fxInfo(FX_ProVerb).defaultMix;
    d[fxp(rv, S_Model)].def = 1;
    for (int k = 0; k < 6; ++k) d[fxp(rv, S_K1 + k)].def = fxKnob(FX_ProVerb, k).defNorm();
    return d;
}

} // namespace op
