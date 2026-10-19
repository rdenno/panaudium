// Panaudium FX units: shared contract + metadata. AGPL-3.0 (see LICENSE).
//
// Every effect is a FxUnit. A rack slot owns one unit per type (created on demand), feeds it the
// slot's six normalised knob values (0..1) and a model index, and handles on/off and dry/wet mix.
// The FxInfo table below is the single source of truth for names, models and knob ranges: DSP code
// converts knobs with fxKnob(type, i).value(norm), the GUI uses the same table for labels/readouts,
// and presets use fxKnob(type, i).norm(real) to store real-world values.
#pragma once
#include <array>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

namespace op {

enum FxType {
    FX_Off,
    // group "Modulation & Drive" (fx/Modulation.h)
    FX_Stompbox, FX_Phaser, FX_Chorus, FX_Flanger, FX_Tremolo, FX_Rotary,
    // group "Space" (fx/Space.h)
    FX_Delay, FX_ProVerb, FX_Innerspace, FX_Shimmer,
    // group "Tone & Dynamics" (fx/Tone.h)
    FX_EQ, FX_Compressor, FX_LoFi, FX_RingShift, FX_Wah, FX_Tape, FX_Amp,
    FX_NumTypes
};

constexpr int kFxKnobs = 6;
constexpr int kFxMaxModels = 8;

struct FxKnob {
    const char* name = nullptr;   // nullptr = knob unused by this unit
    float min = 0, max = 1, def = 0.5f;
    bool log = false;             // exponential mapping (min must be > 0)
    const char* unit = "%";       // "%", "Hz", "s", "ms", "dB", "x", "st", "ct", "deg", "" (plain number)
    float value(float n) const {
        n = n < 0 ? 0 : (n > 1 ? 1 : n);
        return log ? min * std::pow(max / min, n) : min + (max - min) * n;
    }
    float norm(float v) const {
        float n = log ? std::log(v / min) / std::log(max / min) : (v - min) / (max - min);
        return n < 0 ? 0 : (n > 1 ? 1 : n);
    }
    // Default as a normalised knob position.
    float defNorm() const { return norm(def); }
};

struct FxInfo {
    const char* name;           // shown on the slot button, e.g. "STOMPBOX"
    const char* description;
    std::vector<std::string> models;  // <= kFxMaxModels entries; at least one
    std::array<FxKnob, kFxKnobs> knobs;
    float defaultMix;           // dry/wet the slot starts with when this unit is chosen
};

// Percent knobs store 0..1 and display x100.
inline FxKnob pct(const char* n, float def) { return FxKnob{ n, 0.f, 1.f, def, false, "%" }; }
inline FxKnob lin(const char* n, float mn, float mx, float def, const char* unit) { return FxKnob{ n, mn, mx, def, false, unit }; }
inline FxKnob lg(const char* n, float mn, float mx, float def, const char* unit) { return FxKnob{ n, mn, mx, def, true, unit }; }

inline const std::vector<FxInfo>& fxTable() {
    static const std::vector<FxInfo> t = {
        { "OFF", "Empty slot", { "-" }, {}, 1.f },
        // ---------------------------------------------------------------- Modulation & Drive
        { "STOMPBOX", "Guitar-pedal overdrive / distortion / fuzz",
          { "Tube Screamer", "Fuzz Face", "Rat", "Big Muff", "Blues Driver", "Octave Fuzz" },
          { pct("Drive", 0.5f), pct("Tone", 0.5f), lin("Level", -24, 12, 0, "dB"), pct("Bias", 0.f) }, 1.f },
        { "ANALOG PHASER", "Allpass-chain phaser",
          { "Phase 90", "Small Stone", "Uni-Vibe", "Twelve Stage", "Barber Pole" },
          { lg("Rate", 0.02f, 10, 0.5f, "Hz"), pct("Color", 0.4f), pct("Depth", 0.8f), lg("Center", 100, 4000, 700, "Hz"), pct("Stereo", 0.5f) }, 1.f },
        { "CHORUS", "Chorus / ensemble",
          { "CE-2", "Dimension", "Solina Ensemble", "Juno", "Tri-Chorus" },
          { lg("Rate", 0.05f, 8, 0.6f, "Hz"), pct("Depth", 0.5f), pct("Width", 0.8f), lg("Delay", 2, 30, 8, "ms"), pct("Tone", 0.7f) }, 0.5f },
        { "FLANGER", "Short modulated delay with feedback",
          { "Jet", "Through-Zero", "Barber Pole", "Metallic" },
          { lg("Rate", 0.02f, 8, 0.25f, "Hz"), pct("Depth", 0.7f), lin("Feedback", -0.95f, 0.95f, 0.5f, "%"), lg("Manual", 0.3f, 12, 2, "ms"), pct("Stereo", 0.5f) }, 0.5f },
        { "TREMOLO", "Amplitude modulation / auto-pan",
          { "Sine Trem", "Square Trem", "Harmonic Trem", "Auto-Pan", "Stutter Gate" },
          { lg("Rate", 0.1f, 20, 4, "Hz"), pct("Depth", 0.6f), pct("Shape", 0.5f), lin("Stereo", 0, 180, 0, "deg") }, 1.f },
        { "ROTARY", "Rotating speaker cabinet",
          { "Leslie 122", "Leslie 147", "Vibratone" },
          { pct("Speed", 0.f), lg("Accel", 0.1f, 5, 1, "s"), pct("Drive", 0.2f), pct("Horn/Drum", 0.5f), pct("Mic Dist", 0.5f) }, 1.f },
        // ---------------------------------------------------------------- Space
        { "DELAY", "Echo unit",
          { "Digital", "Tape Echo", "Ping-Pong", "Dual Tap", "Diffuse" },
          { lg("Time", 0.02f, 2, 0.375f, "s"), pct("Feedback", 0.35f), lg("Tone", 500, 16000, 5000, "Hz"), pct("Wow", 0.1f), pct("Spread", 0.5f), pct("Ducking", 0.f) }, 0.25f },
        { "PRO-VERB", "Algorithmic reverb (8x8 FDN)",
          { "Room", "Hall", "Plate", "Cathedral", "Dark Space", "Bloom" },
          { pct("Size", 0.5f), lg("Decay", 0.2f, 30, 2.5f, "s"), lin("Predelay", 0, 250, 20, "ms"), pct("Diffusion", 0.7f), lin("Lows", -12, 12, 0, "dB"), lin("Highs", -12, 12, -2, "dB") }, 0.25f },
        { "INNERSPACE", "Convolution with procedurally synthesised impulse responses",
          { "Coin Dropper", "Spring Tank", "Metal Box", "Glass Jar", "Piano Lid", "Cave Drip", "Paper Tube", "Gong Wash" },
          { lg("Size", 0.25f, 2, 1, "x"), pct("Decay", 0.6f), lin("Tone", -12, 12, 0, "dB"), lin("Predelay", 0, 200, 0, "ms"), pct("Width", 0.7f) }, 0.35f },
        { "SHIMMER", "Pitch-shifted feedback reverb",
          { "Octave Up", "Fifth Up", "Octave Down", "Detune Doubler", "Crystal" },
          { pct("Feedback", 0.5f), pct("Shift Mix", 0.6f), pct("Size", 0.7f), lg("Decay", 0.5f, 20, 6, "s"), lg("Tone", 1000, 16000, 8000, "Hz"), lin("Detune", 0, 50, 8, "ct") }, 0.35f },
        // ---------------------------------------------------------------- Tone & Dynamics
        { "STUDIO EQ", "4-band equaliser",
          { "Clean", "Tube", "Pultec Trick" },
          { lin("Low", -15, 15, 0, "dB"), lg("Low Freq", 30, 400, 100, "Hz"), lin("Mid", -15, 15, 0, "dB"), lg("Mid Freq", 200, 8000, 1000, "Hz"), lin("High", -15, 15, 0, "dB"), lg("High Freq", 2000, 16000, 8000, "Hz") }, 1.f },
        { "COMPRESSOR", "Dynamics / pumping",
          { "VCA Bus", "Opto", "Vari-Mu", "FET Limiter", "Pump" },
          { lin("Threshold", -48, 0, -18, "dB"), lg("Ratio", 1, 20, 4, "x"), lg("Attack", 0.1f, 100, 10, "ms"), lg("Release", 10, 1000, 150, "ms"), lin("Makeup", 0, 24, 0, "dB"), lg("Pump Rate", 0.5f, 8, 2, "Hz") }, 1.f },
        { "LO-FI", "Bit crusher / degrader",
          { "Bitcrush", "Sample & Hold", "Vinyl", "Cassette", "AM Radio" },
          { lin("Bits", 2, 16, 10, ""), lg("Downsample", 1, 64, 1, "x"), pct("Noise", 0.1f), lg("Filter", 500, 20000, 12000, "Hz"), pct("Crackle", 0.f), pct("Drive", 0.2f) }, 1.f },
        { "RING / SHIFT", "Ring modulator and frequency shifter",
          { "Ring Mod", "Freq Shift Up", "Freq Shift Down", "Downshift" },
          { lg("Freq", 0.1f, 5000, 200, "Hz"), lg("LFO Rate", 0.01f, 10, 0.3f, "Hz"), pct("LFO Depth", 0.f), pct("Feedback", 0.f), pct("Stereo", 0.2f) }, 0.5f },
        { "WAH / ENV FILTER", "Envelope-, LFO- or pedal-controlled filter",
          { "Crying Wah", "Auto-Wah", "Env Filter LP", "Env Filter BP", "LFO Filter", "Talk Box" },
          { pct("Sensitivity", 0.6f), lg("Freq", 100, 4000, 500, "Hz"), pct("Reso", 0.6f), lg("Attack", 1, 200, 10, "ms"), lg("Release", 10, 1000, 150, "ms"), lin("Range", 0, 5, 3, "oct") }, 1.f },
        { "TAPE", "Tape machine saturation and transport wobble",
          { "Studio 15ips", "Cassette", "Broken Tape" },
          { pct("Drive", 0.3f), pct("Wow", 0.15f), pct("Flutter", 0.15f), pct("Hiss", 0.05f), pct("Tone", 0.7f), pct("Age", 0.2f) }, 1.f },
        { "AMP SIM", "Guitar/bass amp with synthetic cabinet",
          { "Clean Twin", "Bassman", "Brit Crunch", "Rock Stack", "Hi-Gain", "Tiny Speaker" },
          { pct("Gain", 0.4f), lin("Bass", -12, 12, 0, "dB"), lin("Mid", -12, 12, 0, "dB"), lin("Treble", -12, 12, 0, "dB"), pct("Presence", 0.5f), lin("Level", -24, 12, 0, "dB") }, 1.f },
    };
    return t;
}

inline const FxInfo& fxInfo(int type) { const auto& t = fxTable(); return t[size_t(type < 0 || type >= int(t.size()) ? 0 : type)]; }
inline const FxKnob& fxKnob(int type, int k) { return fxInfo(type).knobs[size_t(k)]; }

class FxUnit {
public:
    virtual ~FxUnit() = default;
    // Called once before use (not on the audio thread in the plugin). Allocate everything here.
    virtual void prepare(float sampleRate, int maxBlock) = 0;
    // Clear internal state (delay lines, envelopes) without reallocating.
    virtual void reset() = 0;
    // In-place stereo processing of n <= maxBlock samples. k[0..5] are normalised knobs (convert with
    // fxKnob(type, i).value(k[i])), model is clamped to the unit's model list. Output must be the
    // fully WET signal (the slot mixes dry/wet). Must be real-time safe: no allocation, no locks,
    // no I/O, never output NaN/Inf, keep peaks sane (soft-limit internally if feedback can blow up).
    virtual void process(float* L, float* R, int n, const float* k, int model) = 0;
    // Optional, NEVER called on the audio thread: precompute expensive state (e.g. synthesised impulse responses)
    // for these settings and block until it is ready. Called on preset load and before offline renders.
    virtual void prime(const float* k, int model) { (void)k; (void)model; }
};

// Group factories (one per header) return nullptr for types they don't own.
inline std::unique_ptr<FxUnit> makeModulationFx(int type);
inline std::unique_ptr<FxUnit> makeSpaceFx(int type);
inline std::unique_ptr<FxUnit> makeToneFx(int type);

} // namespace op
