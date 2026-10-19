// Offline renderer: plays a demo phrase through every factory preset and writes WAVs.
// Also renders engine tuning probes (--tuning). Links only the JUCE-free DSP core.
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
#include "../src/dsp/Presets.h"
#include "../src/dsp/Synth.h"

using namespace op;

static void writeWav(const std::string& path, const std::vector<float>& L, const std::vector<float>& R, int sr) {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) { std::fprintf(stderr, "cannot write %s\n", path.c_str()); return; }
    const uint32_t frames = uint32_t(L.size()), dataBytes = frames * 2 * 4;
    auto u32 = [&](uint32_t v) { std::fwrite(&v, 4, 1, f); };
    auto u16 = [&](uint16_t v) { std::fwrite(&v, 2, 1, f); };
    std::fwrite("RIFF", 1, 4, f); u32(36 + dataBytes); std::fwrite("WAVEfmt ", 1, 8, f);
    u32(16); u16(3); u16(2); u32(uint32_t(sr)); u32(uint32_t(sr) * 8); u16(8); u16(32);
    std::fwrite("data", 1, 4, f); u32(dataBytes);
    for (uint32_t i = 0; i < frames; ++i) { std::fwrite(&L[i], 4, 1, f); std::fwrite(&R[i], 4, 1, f); }
    std::fclose(f);
}

struct Ev { double t; int note; bool on; };

static std::vector<Ev> phraseFor(const std::string& cat, double& length) {
    std::vector<Ev> ev;
    auto note = [&](double t, double dur, int n) { ev.push_back({ t, n, true }); ev.push_back({ t + dur, n, false }); };
    if (cat == "Percussion") {  // a small groove across the keyboard
        const int pat[] = { 36, 42, 38, 42, 36, 36, 38, 42, 45, 42, 38, 47, 36, 42, 38, 50 };
        for (int i = 0; i < 16; ++i) note(i * 0.25, 0.12, pat[i]);
        length = 5.5;
        return ev;
    }
    if (cat == "Pulse" || cat == "Drone") {  // long held chord; gates/LFOs provide the motion
        for (int n : { 45, 52, 57, 60 }) note(0.0, 6.0, n);
        length = 9.0;
        return ev;
    }
    if (cat == "Pad" || cat == "Atmos" || cat == "Texture" || cat == "Cinematic") {
        for (int n : { 48, 52, 55, 59 }) note(0.0, 4.0, n);
        for (int n : { 53, 57, 60, 64 }) note(4.0, 4.0, n);
        length = 12.0;
    } else if (cat == "Pluck" || cat == "Mallet" || cat == "Keys" || cat == "World") {
        const int arp[] = { 48, 55, 60, 64, 67, 72, 67, 64, 53, 60, 65, 69, 72, 77, 72, 69 };
        for (int i = 0; i < 16; ++i) note(i * 0.3, 0.27, arp[i]);
        for (int n : { 48, 55, 64, 71 }) note(4.8, 2.5, n);
        length = 10.0;
    } else if (cat == "Bass") {
        const int bl[] = { 36, 36, 43, 41, 36, 48, 46, 43 };
        for (int r = 0; r < 2; ++r) for (int i = 0; i < 8; ++i) note(r * 2.0 + i * 0.25, 0.2, bl[i]);
        note(4.0, 1.5, 36);
        length = 7.0;
    } else {  // Lead, Wind, Vocal, Strings: legato-ish melody
        const int m[] = { 60, 62, 64, 67, 69, 67, 64, 62 };
        for (int i = 0; i < 8; ++i) note(i * 0.5, 0.5, m[i]);
        note(4.0, 2.5, 60);
        length = 9.5;
    }
    return ev;
}

static double renderEvents(Synth& s, std::vector<Ev> ev, double length, int sr, std::vector<float>& L, std::vector<float>& R) {
    std::sort(ev.begin(), ev.end(), [](const Ev& a, const Ev& b) { return a.t < b.t || (a.t == b.t && !a.on && b.on); });
    const size_t total = size_t(length * sr);
    L.assign(total, 0.f); R.assign(total, 0.f);
    size_t pos = 0, e = 0;
    const auto t0 = std::chrono::steady_clock::now();
    while (pos < total) {
        while (e < ev.size() && size_t(ev[e].t * sr) <= pos) {
            if (ev[e].on) s.noteOn(ev[e].note, 0.85f); else s.noteOff(ev[e].note);
            ++e;
        }
        size_t next = total;
        if (e < ev.size()) next = std::min(next, std::max(pos + 1, size_t(ev[e].t * sr)));
        const size_t n = std::min<size_t>(next - pos, 256);
        s.process(&L[pos], &R[pos], int(n));
        pos += n;
    }
    const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    return length / std::max(1e-9, secs);  // realtime factor
}

static std::string slug(std::string s) {
    for (auto& c : s) if (!std::isalnum((unsigned char)c)) c = '_';
    return s;
}

int main(int argc, char** argv) {
    if (argc < 2) { std::printf("usage: panaudium_render <outdir> [--tuning]\n"); return 1; }
    const std::string out = argv[1];
    const bool tuning = argc > 2 && std::strcmp(argv[2], "--tuning") == 0;
    const int sr = 48000;

    if (argc >= 3 && std::strcmp(argv[2], "--fxtest") == 0) {
        // Every FX unit x model x knob setting on a test signal: impulse, noise burst, then a sustained chord.
        // Reports NaN/Inf, peak, output/input RMS and realtime factor; writes one WAV per unit/model (default knobs).
        const int len = sr * 3;
        std::vector<float> inL(size_t(len), 0.f), inR(size_t(len), 0.f);
        Rng rng(7);
        inL[100] = inR[100] = 0.9f;
        for (int i = sr / 4; i < sr / 2; ++i) { inL[size_t(i)] = 0.3f * rng.bipolar(); inR[size_t(i)] = 0.3f * rng.bipolar(); }
        for (int i = sr; i < len - sr / 2; ++i) {
            const float t = float(i) / float(sr);
            float s = 0; for (float f : { 110.f, 220.f, 277.2f, 329.6f, 880.f }) s += 0.08f * std::sin(kTwoPi * f * t);
            inL[size_t(i)] = s; inR[size_t(i)] = s * 0.9f;
        }
        double inSq = 0; for (float v : inL) inSq += double(v) * v;
        const float inRms = float(std::sqrt(inSq / len));
        FILE* csv = std::fopen((out + "/fxtest.csv").c_str(), "w");
        if (csv) std::fprintf(csv, "type,unit,model,setting,nonfinite,peak,rms_ratio,realtime_factor\n");
        int failures = 0;
        for (int type = 1; type < FX_NumTypes; ++type) {
            const FxInfo& info = fxInfo(type);
            for (int model = 0; model < int(info.models.size()); ++model) {
                for (int setting = 0; setting < 4; ++setting) {
                    FxSlot slot; slot.prepare(float(sr), 256);
                    float p[S_NumSlotParams] = { float(type), 1.f, 1.f, float(model) };
                    Rng kr(uint32_t(type * 131 + model * 17 + setting));
                    for (int k = 0; k < kFxKnobs; ++k)
                        p[S_K1 + k] = setting == 0 ? 0.f : setting == 1 ? fxKnob(type, k).defNorm() : setting == 2 ? 1.f : kr.unipolar();
                    std::vector<float> L = inL, R = inR;
                    const auto t0 = std::chrono::steady_clock::now();
                    for (int pos = 0; pos < len; pos += 256) slot.process(&L[size_t(pos)], &R[size_t(pos)], std::min(256, len - pos), p);
                    const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
                    int nonfinite = 0; float peak = 0; double sq = 0;
                    for (int i = 0; i < len; ++i) for (float v : { L[size_t(i)], R[size_t(i)] }) {
                        if (!std::isfinite(v)) ++nonfinite; else { peak = std::max(peak, std::fabs(v)); sq += double(v) * v; }
                    }
                    const float ratio = float(std::sqrt(sq / (2.0 * len))) / inRms;
                    const double rtf = 3.0 / std::max(1e-9, secs);
                    const char* names[] = { "zeros", "defaults", "maxed", "random" };
                    const bool bad = nonfinite > 0 || peak > 8.f || rtf < 20.0 || (setting == 1 && ratio < 0.01f);
                    failures += bad;
                    std::printf("%-17s %-18s %-8s nonfinite=%d peak=%6.2f rms_ratio=%6.2f rt=%7.0fx%s\n", info.name, info.models[size_t(model)].c_str(),
                                names[setting], nonfinite, peak, ratio, rtf, bad ? "   <-- CHECK" : "");
                    if (csv) std::fprintf(csv, "%d,%s,%s,%s,%d,%.3f,%.3f,%.1f\n", type, info.name, info.models[size_t(model)].c_str(), names[setting], nonfinite, peak, ratio, rtf);
                    if (setting == 1) {
                        char file[256]; std::snprintf(file, sizeof file, "%s/fx_%02d_%d.wav", out.c_str(), type, model);
                        writeWav(file, L, R, sr);
                    }
                }
            }
        }
        if (csv) std::fclose(csv);
        std::printf("FX test done, %d settings flagged\n", failures);
        return failures ? 2 : 0;
    }
    if (argc >= 8 && std::strcmp(argv[2], "--probe") == 0) {
        // --probe <engine> <note> <timbre> <shape> <char> [vel]: one dry note to <outdir>/probe.wav
        auto s = std::make_unique<Synth>();
        s->prepare(float(sr));
        s->setParam(lp(0, L_Engine), float(std::atof(argv[3])));
        s->setParam(lp(0, L_Timbre), float(std::atof(argv[5])));
        s->setParam(lp(0, L_Shape), float(std::atof(argv[6])));
        s->setParam(lp(0, L_Char), float(std::atof(argv[7])));
        s->setParam(lp(0, L_FiltType), F_Off); s->setParam(lp(0, L_Drift), 0); s->setParam(lp(0, L_AR), 0.05f);
        s->setParam(fxp(fxSlot(R_Common, 3), S_Type), float(FX_Off)); s->setParam(gp(G_Gain), 0);
        std::vector<float> L, R;
        const int note = std::atoi(argv[4]);
        renderEvents(*s, { { 0.0, note, true }, { 1.5, note, false } }, 1.8, sr, L, R);
        writeWav(out + "/probe_n" + std::to_string(note) + ".wav", L, R, sr);
        return 0;
    }
    if (tuning) {
        // Single-layer, dry, drift-free probes for pitch verification.
        for (int e = 0; e < E_NumEngines; ++e) for (int note : { 45, 57, 69, 81 }) {
            auto s = std::make_unique<Synth>();
            s->prepare(float(sr));
            s->setParam(lp(0, L_Engine), float(e)); s->setParam(lp(0, L_FiltType), F_Off);
            s->setParam(lp(0, L_Drift), 0); s->setParam(lp(0, L_AR), 0.05f);
            s->setParam(lp(0, L_Char), e == E_Pluck || e == E_Modal ? 0.6f : 0.3f);
            s->setParam(lp(0, L_Shape), e == E_Atmos ? 0.95f : 0.5f);
            s->setParam(fxp(fxSlot(R_Common, 3), S_Type), float(FX_Off)); s->setParam(gp(G_Gain), 0);
            std::vector<float> L, R;
            renderEvents(*s, { { 0.0, note, true }, { 1.5, note, false } }, 1.8, sr, L, R);
            char name[256]; std::snprintf(name, sizeof name, "%s/tune_e%d_n%d.wav", out.c_str(), e, note);
            writeWav(name, L, R, sr);
        }
        std::printf("tuning probes written\n");
        return 0;
    }

    const auto& bank = factoryPresets();
    FILE* idx = std::fopen((out + "/index.csv").c_str(), "w");
    if (idx) std::fprintf(idx, "file,name,category,realtime_factor\n");
    for (size_t i = 0; i < bank.size(); ++i) {
        const auto& p = bank[i];
        auto s = std::make_unique<Synth>();
        s->prepare(float(sr));
        for (int k = 0; k < kNumParams; ++k) s->setParam(k, p.v[size_t(k)]);
        s->primeFx(p.v.data());   // build heavy FX state (Innerspace IRs) up front: deterministic renders
        double len = 0;
        auto ev = phraseFor(p.category, len);
        std::vector<float> L, R;
        const double rtf = renderEvents(*s, ev, len, sr, L, R);
        char file[256]; std::snprintf(file, sizeof file, "%02d_%s.wav", int(i + 1), slug(p.name).c_str());
        writeWav(out + "/" + file, L, R, sr);
        std::printf("%-26s %-8s %6.1fx realtime\n", p.name.c_str(), p.category.c_str(), rtf);
        if (idx) std::fprintf(idx, "%s,%s,%s,%.2f\n", file, p.name.c_str(), p.category.c_str(), rtf);
    }
    if (idx) std::fclose(idx);
    return 0;
}
