// Panaudium FX rack: slots, racks and unit creation. AGPL-3.0 (see LICENSE).
#pragma once
#include <atomic>
#include <cstring>
#include "../Dsp.h"
#include "FxUnit.h"
#include "Modulation.h"
#include "Space.h"
#include "Tone.h"

namespace op {

// One unit instance per (slot, type), created on first use and never destroyed while the synth
// lives, so the audio thread can switch types without freeing memory. Creation normally happens on
// a non-audio thread via preload(); if the audio thread meets a type that isn't loaded yet it creates
// it itself (one-off allocation). Installation is a compare-exchange, so two threads racing to create
// the same unit is safe.
class FxSlot {
public:
    ~FxSlot() { for (auto& u : units) delete u.load(); }

    void prepare(float sampleRate, int maxBlockSize) {
        sr = sampleRate; maxBlock = maxBlockSize;
        for (auto& u : units) if (FxUnit* p = u.load()) { p->prepare(sr, maxBlock); p->reset(); }
        dryL.assign(size_t(maxBlock), 0.f); dryR.assign(size_t(maxBlock), 0.f);
        dcL = DcBlock{}; dcR = DcBlock{}; dcL.R = dcR.R = 1.f - kTwoPi * 5.f / sr;
    }

    FxUnit* preload(int type) {
        if (type <= FX_Off || type >= FX_NumTypes) return nullptr;
        FxUnit* p = units[size_t(type)].load(std::memory_order_acquire);
        if (p) return p;
        std::unique_ptr<FxUnit> made = makeModulationFx(type);
        if (!made) made = makeSpaceFx(type);
        if (!made) made = makeToneFx(type);
        if (!made) return nullptr;
        made->prepare(sr, maxBlock); made->reset();
        FxUnit* expected = nullptr;
        if (units[size_t(type)].compare_exchange_strong(expected, made.get(), std::memory_order_acq_rel)) return made.release();
        return expected;  // someone else won; `made` is freed here
    }

    // Non-audio thread: create the unit for p's type and let it precompute heavy state for p's settings.
    void prime(const float* p) {
        const int type = int(p[0] + 0.5f);
        if (FxUnit* u = preload(type)) {
            const int nModels = int(fxInfo(type).models.size());
            u->prime(p + 4, std::min(std::max(0, int(p[3] + 0.5f)), nModels - 1));
        }
    }

    // p = this slot's 10 parameters: type, on, mix, model, k1..k6
    void process(float* L, float* R, int n, const float* p) {
        const int type = int(p[0] + 0.5f);
        const bool on = p[1] > 0.5f;
        if (type != lastType) {  // flush the outgoing unit's tail so switching back later starts clean
            if (FxUnit* old = (lastType > FX_Off && lastType < FX_NumTypes) ? units[size_t(lastType)].load() : nullptr) old->reset();
            lastType = type;
        }
        if (!on || type <= FX_Off || type >= FX_NumTypes) return;
        FxUnit* u = preload(type);
        if (!u) return;
        const float mix = clampf(p[2], 0.f, 1.f);
        const int nModels = int(fxInfo(type).models.size());
        const int model = std::min(std::max(0, int(p[3] + 0.5f)), nModels - 1);
        std::memcpy(dryL.data(), L, sizeof(float) * size_t(n));
        std::memcpy(dryR.data(), R, sizeof(float) * size_t(n));
        u->process(L, R, n, p + 4, model);
        // Asymmetric nonlinearities (fuzz bias, tube stages, rectifiers) create DC; strip it from every wet signal.
        for (int i = 0; i < n; ++i) { L[i] = dcL.tick(L[i]); R[i] = dcR.tick(R[i]); }
        for (int i = 0; i < n; ++i) {
            float l = dryL[size_t(i)] + (L[i] - dryL[size_t(i)]) * mix;
            float r = dryR[size_t(i)] + (R[i] - dryR[size_t(i)]) * mix;
            if (!std::isfinite(l)) l = 0;   // a misbehaving unit must never poison the bus
            if (!std::isfinite(r)) r = 0;
            L[i] = l; R[i] = r;
        }
    }

private:
    std::array<std::atomic<FxUnit*>, FX_NumTypes> units {};
    std::vector<float> dryL, dryR;
    DcBlock dcL, dcR;
    float sr = 48000; int maxBlock = 512; int lastType = FX_Off;
};

} // namespace op
