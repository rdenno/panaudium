// Panaudium factory bank 2: Cinematic, Textures & Atmospheres. AGPL-3.0 (see LICENSE).
#pragma once
#include "../Presets.h"

namespace op {

inline void addBank2(std::vector<Preset>& b) {
    using PB = PresetBuilder;

    // =========================================================================
    // Pads (8 presets)
    // =========================================================================

    // Evolving vowel-choir pad with LFO-swept formant vowels through Solina ensemble chorus.
    b.push_back(PB("Vocal Choir Osmosis", "Pad")
        .oscA(E_Vocal, 0.15f, 0.5f, 0.25f).A(L_L2Timbre, 0.5f).A(L_L2Rate, 0.12f).uniA(4, 12, 0.9f).A(L_Drift, 0.35f).ampA(1.5f, 1.f, 1.f, 3.5f)
        .oscB(E_Spectral, 0.35f, 0.6f, 0.1f).B(L_Level, 0.4f).B(L_Drift, 0.4f).ampB(2.f, 1.f, 1.f, 4.f)
        .fx(R_Common, 1, FX_Chorus, 2, 0.6f, { 0.8f, 0.7f, 0.9f })
        .rev(0.45f, 0.85f).done());

    // Glassy friction pad combining bowed string with continuous modal resonance.
    b.push_back(PB("Bowed Crystal Glass", "Pad")
        .oscA(E_Bowed, 0.35f, 0.12f, 0.85f).A(L_FMDepth, 0.5f).uniA(2, 6, 0.7f).filtA(F_LP12, 6500, 0.1f).ampA(1.2f, 1.f, 1.f, 3.f)
        .oscB(E_Modal, 0.67f, 0.2f, 0.95f).B(L_Noise, 0.45f).B(L_Level, 0.45f).filtB(F_Off, 20000, 0).ampB(2.f, 1.f, 1.f, 4.f)
        .dly(0.25f, 0.5f, 0.45f).rev(0.5f, 0.9f).done());

    // Warm analog sawtooth pad swept with a deep twelve-stage phaser and subtle drift.
    b.push_back(PB("Solar Flare Phaser", "Pad")
        .oscA(E_VA, 0.0f, 0.5f, 0.f).uniA(6, 24, 0.95f).A(L_Drift, 0.35f).filtA(F_LP24, 3200, 0.2f).ampA(0.9f, 1.f, 1.f, 2.8f)
        .oscB(E_VA, 0.33f, 0.65f, 0.f).B(L_Octave, -1).B(L_Level, 0.4f).filtB(F_LP12, 2000, 0.1f).ampB(1.2f, 1.f, 1.f, 3.f)
        .fx(R_A, 0, FX_Phaser, 3, 0.8f, { 0.15f, 0.6f, 0.85f, 800.f, 0.7f })
        .rev(0.4f, 0.75f).done());

    // Evolving granular texture bathed in dark algorithmic cathedral reverberation.
    b.push_back(PB("Granular Shimmer Clouds", "Pad")
        .oscA(E_Granular, 0.45f, 0.65f, 0.4f).A(L_FMDepth, 0.35f).uniA(2, 8, 0.8f).filtA(F_LP12, 7000, 0.1f).ampA(1.8f, 1.f, 1.f, 4.f)
        .oscB(E_Spectral, 0.25f, 0.4f, 0.05f).B(L_Octave, 1).B(L_Level, 0.35f).ampB(2.5f, 1.f, 1.f, 4.5f)
        .fx(R_Common, 3, FX_ProVerb, 4, 0.55f, { 0.85f, 12.f, 30.f, 0.8f, 2.f, -4.f }).done());

    // Heavy spectral additive pad warmed by cassette tape saturation and tube EQ.
    b.push_back(PB("Dark Additive Obelisk", "Pad")
        .oscA(E_Spectral, 0.2f, 0.7f, 0.2f).A(L_Drift, 0.4f).A(L_Octave, -1).uniA(2, 6, 0.6f).filtA(F_LP24, 4500, 0.15f).ampA(1.4f, 1.f, 1.f, 3.2f)
        .oscB(E_VA, 0.8f, 0.5f, 0.f).B(L_Level, 0.4f).ampB(1.6f, 1.f, 1.f, 3.f)
        .fx(R_A, 0, FX_EQ, 1, 1.f, { 4.f, 90.f, -2.f, 1200.f, 3.f, 6000.f })
        .fx(R_A, 1, FX_Tape, 1, 0.75f, { 0.45f, 0.2f, 0.15f, 0.05f, 0.55f, 0.3f })
        .rev(0.35f, 0.8f).done());

    // Pitched modal bell pad reflecting through octave-up shimmer reverb and ping-pong delay.
    b.push_back(PB("Crystalline Shimmer Spire", "Pad")
        .oscA(E_Modal, 0.67f, 0.8f, 0.85f).A(L_Noise, 0.1f).filtA(F_LP12, 10000, 0.1f).ampA(0.02f, 5.f, 0.85f, 4.5f)
        .oscB(E_Spectral, 0.5f, 0.2f, 0.3f).B(L_Octave, 1).B(L_Level, 0.35f).ampB(1.5f, 1.f, 1.f, 4.f)
        .fx(R_Common, 2, FX_Delay, 2, 0.35f, { 0.42f, 0.45f, 6000.f })
        .fx(R_Common, 3, FX_Shimmer, 0, 0.45f, { 0.6f, 0.7f, 0.8f, 8.f, 9000.f, 12.f }).done());

    // Nostalgic detuned pad degraded by cassette lo-fi and broken tape flutter.
    b.push_back(PB("Cassette Memory Tape", "Pad")
        .oscA(E_VA, 0.6f, 0.5f, 0.f).uniA(4, 18, 0.85f).A(L_Drift, 0.6f).filtA(F_LP24, 2800, 0.2f).ampA(0.8f, 1.f, 1.f, 2.5f)
        .oscB(E_Vocal, 0.4f, 0.6f, 0.15f).B(L_Octave, 1).B(L_Level, 0.35f).ampB(1.5f, 1.f, 1.f, 2.8f)
        .fx(R_A, 0, FX_LoFi, 3, 0.8f, { 12.f, 1.f, 0.15f, 8000.f, 0.1f, 0.35f })
        .fx(R_Common, 0, FX_Tape, 2, 0.7f, { 0.4f, 0.35f, 0.3f, 0.1f, 0.5f, 0.45f })
        .rev(0.35f, 0.65f).done());

    // Silky multi-layered bowed string ensemble enriched by Solina ensemble chorus.
    b.push_back(PB("Celestial String Solina", "Pad")
        .oscA(E_Bowed, 0.42f, 0.18f, 0.8f).uniA(4, 14, 0.9f).A(L_Drift, 0.3f).filtA(F_LP12, 7500, 0.05f).ampA(0.7f, 1.f, 1.f, 2.2f)
        .oscB(E_VA, 0.0f, 0.5f, 0.f).B(L_Octave, 1).B(L_Level, 0.3f).filtB(F_LP24, 4000, 0.1f).ampB(0.9f, 1.f, 1.f, 2.5f)
        .fx(R_Common, 1, FX_Chorus, 2, 0.7f, { 0.7f, 0.85f, 1.f, 12.f, 0.75f })
        .rev(0.4f, 0.8f).done());

    // =========================================================================
    // Atmos (6 presets)
    // =========================================================================

    // Cold atmospheric gale through a broken window with whistling turbulence and comb resonance.
    b.push_back(PB("Broken Window Howl", "Atmos")
        .oscA(E_Atmos, 0.65f, 0.94f, 0.85f).A(L_FMDepth, 0.6f).uniA(2, 10, 1.f).filtA(F_HP12, 180, 0.2f).ampA(2.f, 1.f, 1.f, 3.5f)
        .oscB(E_Pipe, 0.8f, 0.25f, 0.2f).B(L_Level, 0.35f).filtB(F_BP12, 1200, 0.4f).ampB(1.5f, 1.f, 1.f, 3.f)
        .dly(0.2f, 0.6f, 0.5f).rev(0.55f, 0.95f).done());

    // Industrial low-frequency machinery hum filtered with tuned comb and room resonance.
    b.push_back(PB("Distant Factory Drone", "Atmos")
        .oscA(E_VA, 0.2f, 0.85f, 0.1f).A(L_Octave, -1).filtA(F_Comb, 523.25f, 0.85f).A(L_KeyTrack, 1.f).ampA(1.5f, 1.f, 1.f, 3.f)
        .oscB(E_Atmos, 0.35f, 0.3f, 0.6f).B(L_Level, 0.6f).filtB(F_LP24, 1800, 0.3f).ampB(2.f, 1.f, 1.f, 3.f)
        .fx(R_A, 0, FX_Stompbox, 3, 0.5f, { 0.35f, 0.3f, -6.f })
        .fx(R_Common, 0, FX_EQ, 0, 1.f, { 2.f, 70.f, 2.f, 700.f, -4.f, 5000.f })
        .rev(0.35f, 0.7f).done());

    // Pitch-shifted underwater rumble with dark lowpass filter and frequency downshifter.
    b.push_back(PB("Submerged Ocean Trench", "Atmos")
        .oscA(E_Atmos, 0.15f, 0.4f, 0.6f).A(L_Octave, -1).filtA(F_LP24, 450, 0.4f).ampA(1.5f, 1.f, 1.f, 3.f)
        .oscB(E_Modal, 1.0f, 0.1f, 0.95f).B(L_Noise, 0.6f).B(L_Level, 0.4f).filtB(F_LP12, 350, 0.2f).ampB(2.f, 1.f, 1.f, 3.5f)
        .fx(R_A, 0, FX_RingShift, 2, 0.65f, { 45.f, 0.08f, 0.3f, 0.2f, 0.5f })
        .rev(0.6f, 0.9f).done());

    // Alien vocal transmission captured through degraded AM radio lo-fi filters.
    b.push_back(PB("Shortwave Ghost Radios", "Atmos")
        .oscA(E_Vocal, 0.35f, 0.3f, 0.7f).A(L_L2Timbre, 0.6f).A(L_L2Rate, 0.35f).filtA(F_BP12, 1400, 0.6f).ampA(0.8f, 1.f, 1.f, 2.5f)
        .oscB(E_Atmos, 0.75f, 0.15f, 0.8f).B(L_Level, 0.4f).ampB(1.f, 1.f, 1.f, 2.5f)
        .fx(R_A, 0, FX_LoFi, 4, 0.9f, { 7.f, 3.f, 0.4f, 3200.f, 0.3f, 0.4f })
        .dly(0.3f, 0.33f, 0.55f).rev(0.4f, 0.75f).done());

    // Sparse modal impacts resonating through a subterranean dripstone cave IR.
    b.push_back(PB("Cavern of Cold Drips", "Atmos")
        .oscA(E_Modal, 0.3f, 0.85f, 0.4f).A(L_Noise, 0.05f).filtA(F_HP12, 300, 0.1f).ampA(0.001f, 3.f, 0.9f, 2.5f)
        .oscB(E_Pipe, 0.4f, 0.1f, 0.2f).B(L_Octave, 1).B(L_Level, 0.35f).ampB(0.8f, 1.f, 1.f, 3.f)
        .dly(0.35f, 0.45f, 0.6f)
        .fx(R_Common, 3, FX_Innerspace, 5, 0.6f, { 1.2f, 0.85f, 2.f, 40.f, 0.8f }).done());

    // High bowed glass whistle spinning through rotary cabinet modulation and atmospheric gusts.
    b.push_back(PB("Glass Harmonica Squall", "Atmos")
        .oscA(E_Spectral, 0.5f, 0.85f, 0.3f).A(L_Drift, 0.6f).A(L_Octave, 1).uniA(2, 6, 0.7f).filtA(F_LP12, 8000, 0.1f).ampA(1.2f, 1.f, 1.f, 3.f)
        .oscB(E_Atmos, 0.8f, 0.95f, 0.7f).B(L_FMDepth, 0.5f).B(L_Level, 0.4f).ampB(1.5f, 1.f, 1.f, 3.f)
        .fx(R_Common, 1, FX_Rotary, 2, 0.65f, { 0.4f, 1.5f, 0.1f, 0.7f, 0.6f })
        .rev(0.5f, 0.85f).done());

    // =========================================================================
    // Drone (5 presets)
    // =========================================================================

    // Endless Indian tanpura drone with buzzing plucked strings and tuned comb filter.
    b.push_back(PB("Tanpura Resonance Drone", "Drone")
        .oscA(E_Pluck, 0.85f, 0.05f, 0.92f).A(L_Octave, -1).filtA(F_Comb, 146.83f, 0.75f).A(L_KeyTrack, 1.f).ampA(0.002f, 12.f, 0.95f, 4.f)
        .oscB(E_Spectral, 0.35f, 0.2f, 0.15f).B(L_Drift, 0.3f).B(L_Level, 0.45f).ampB(2.f, 1.f, 1.f, 4.f)
        .chorus(0.35f, 0.4f).rev(0.45f, 0.8f).done());

    // Tibetan throat chant drone resonating inside an immense Gothic stone cathedral.
    b.push_back(PB("Monastery Crypt Chant", "Drone")
        .oscA(E_Vocal, 0.75f, 0.1f, 0.4f).A(L_Octave, -2).A(L_L2Timbre, 0.3f).A(L_L2Rate, 0.08f).uniA(3, 8, 0.6f).ampA(1.8f, 1.f, 1.f, 4.f)
        .oscB(E_Modal, 0.0f, 0.3f, 0.85f).B(L_Noise, 0.3f).B(L_Octave, -1).B(L_Level, 0.4f).ampB(2.f, 1.f, 1.f, 4.5f)
        .fx(R_Common, 3, FX_ProVerb, 3, 0.6f, { 0.95f, 22.f, 45.f, 0.85f, 1.f, -2.f }).done());

    // Evolving metallic wash drone convolved through procedural gong wash impulse.
    b.push_back(PB("Gong Wash Horizon", "Drone")
        .oscA(E_Modal, 1.0f, 0.35f, 0.98f).A(L_Noise, 0.5f).A(L_Octave, -1).filtA(F_LP12, 6000, 0.1f).ampA(1.5f, 1.f, 1.f, 5.f)
        .oscB(E_Spectral, 0.4f, 0.5f, 0.4f).B(L_Drift, 0.5f).B(L_Level, 0.4f).ampB(2.f, 1.f, 1.f, 5.f)
        .fx(R_Common, 3, FX_Innerspace, 7, 0.65f, { 1.4f, 0.9f, 0.f, 10.f, 0.9f }).done());

    // Seismic sub-bass drone with barber-pole phaser sweeping across analog octaves.
    b.push_back(PB("Abyssal Sub Resonator", "Drone")
        .oscA(E_VA, 0.1f, 0.5f, 0.f).A(L_Octave, -2).uniA(3, 14, 0.5f).filtA(F_LP24, 280, 0.3f).ampA(0.8f, 1.f, 1.f, 2.5f)
        .oscB(E_VA, 0.7f, 0.5f, 0.f).B(L_Octave, -1).B(L_Level, 0.5f).filtB(F_LP12, 400, 0.2f).ampB(1.f, 1.f, 1.f, 2.5f)
        .fx(R_A, 0, FX_Phaser, 4, 0.75f, { 0.05f, 0.5f, 0.9f, 220.f, 0.6f })
        .rev(0.25f, 0.5f).done());

    // Continuous friction rubbing a giant steel plate excited by modal noise modes.
    b.push_back(PB("Singing Steel Plate", "Drone")
        .oscA(E_Modal, 0.92f, 0.15f, 0.96f).A(L_Noise, 0.8f).uniA(2, 5, 0.7f).filtA(F_BP12, 1800, 0.5f).ampA(1.5f, 1.f, 1.f, 4.f)
        .oscB(E_Bowed, 0.3f, 0.15f, 0.85f).B(L_FMDepth, 0.4f).B(L_Level, 0.4f).ampB(1.8f, 1.f, 1.f, 4.f)
        .dly(0.25f, 0.5f, 0.5f).rev(0.5f, 0.85f).done());

    // =========================================================================
    // Texture (6 presets)
    // =========================================================================

    // Microscopic high-frequency buzzing insect texture modulated by fast sine tremolo.
    b.push_back(PB("Insectoid Swarm", "Texture")
        .oscA(E_Granular, 0.9f, 0.1f, 0.95f).A(L_FMDepth, 0.7f).A(L_Octave, 1).filtA(F_HP12, 1200, 0.2f).ampA(0.5f, 1.f, 1.f, 2.f)
        .oscB(E_Spectral, 0.8f, 0.8f, 0.5f).B(L_Octave, 2).B(L_Level, 0.35f).ampB(0.8f, 1.f, 1.f, 2.f)
        .fx(R_A, 0, FX_Tremolo, 0, 0.85f, { 14.f, 0.8f, 0.5f, 90.f })
        .rev(0.4f, 0.7f).done());

    // Fragile acoustic crackle and particulate dust bed with vinyl degradation.
    b.push_back(PB("Dust and Vinyl Embers", "Texture")
        .oscA(E_Granular, 0.25f, 0.25f, 0.6f).A(L_Noise, 0.2f).filtA(F_BP12, 2400, 0.3f).ampA(0.8f, 1.f, 1.f, 2.5f)
        .oscB(E_Atmos, 0.5f, 0.05f, 0.5f).B(L_Level, 0.4f).ampB(1.f, 1.f, 1.f, 2.5f)
        .fx(R_A, 0, FX_LoFi, 2, 0.9f, { 14.f, 1.f, 0.25f, 9000.f, 0.65f, 0.15f })
        .rev(0.3f, 0.6f).done());

    // Creaking hull timbers of an old sailing ship modeled by strained bowed string and auto-wah.
    b.push_back(PB("Galleon Timber Creak", "Texture")
        .oscA(E_Bowed, 0.12f, 0.08f, 0.65f).A(L_Octave, -1).filtA(F_LP12, 1800, 0.3f).ampA(0.4f, 1.f, 1.f, 1.8f)
        .oscB(E_Modal, 0.33f, 0.9f, 0.2f).B(L_Noise, 0.15f).B(L_Level, 0.45f).ampB(0.001f, 1.5f, 0.7f, 1.f)
        .fx(R_A, 0, FX_Wah, 1, 0.85f, { 0.75f, 450.f, 0.65f, 20.f, 300.f, 2.5f })
        .rev(0.45f, 0.8f).done());

    // Hyper-realistic rain droplets spattering across metal sheet via sparse modal resonators.
    b.push_back(PB("Rain on Zinc Roof", "Texture")
        .oscA(E_Modal, 1.0f, 0.75f, 0.35f).A(L_Noise, 0.3f).filtA(F_HP12, 600, 0.2f).ampA(0.001f, 2.f, 0.8f, 1.5f)
        .oscB(E_Atmos, 0.6f, 0.1f, 0.7f).B(L_Level, 0.35f).ampB(0.5f, 1.f, 1.f, 2.f)
        .dly(0.3f, 0.18f, 0.4f).rev(0.4f, 0.75f).done());

    // Icy needle-like granular crystals drifting over vintage studio tape warmth.
    b.push_back(PB("Polar Needle Frost", "Texture")
        .oscA(E_Granular, 0.6f, 0.4f, 0.7f).A(L_FMDepth, 0.6f).A(L_Octave, 1).filtA(F_HP12, 1500, 0.1f).ampA(1.f, 1.f, 1.f, 3.f)
        .oscB(E_Spectral, 0.6f, 0.1f, 0.2f).B(L_Level, 0.35f).ampB(1.2f, 1.f, 1.f, 3.f)
        .fx(R_A, 0, FX_Tape, 0, 0.75f, { 0.35f, 0.1f, 0.1f, 0.02f, 0.8f, 0.1f })
        .rev(0.5f, 0.85f).done());

    // Gritty metallic friction scrape blending Karplus-Strong string with metal box convolution.
    b.push_back(PB("Bowed Wire Friction", "Texture")
        .oscA(E_Pluck, 0.95f, 0.02f, 0.75f).A(L_Noise, 0.4f).filtA(F_BP12, 3500, 0.5f).ampA(0.01f, 4.f, 0.85f, 2.f)
        .oscB(E_Bowed, 0.25f, 0.14f, 0.7f).B(L_Level, 0.4f).ampB(0.3f, 1.f, 1.f, 2.f)
        .fx(R_Common, 3, FX_Innerspace, 2, 0.55f, { 0.8f, 0.65f, 3.f, 10.f, 0.75f }).done());

    // =========================================================================
    // Cinematic (5 presets)
    // =========================================================================

    // Earth-shattering cinematic trailer braam brass hit driven by Rat distortion and hi-gain amp.
    b.push_back(PB("Braam of Doom", "Cinematic")
        .oscA(E_VA, 0.05f, 0.55f, 0.65f).A(L_Octave, -1).uniA(4, 18, 0.8f).filtA(F_LP24, 800, 0.35f).A(L_FiltEnv, 3.5f).A(L_FA, 0.02f).A(L_FD, 1.2f).A(L_FS, 0.2f).ampA(0.005f, 2.f, 0.6f, 1.2f)
        .oscB(E_VA, 0.1f, 0.5f, 0.f).B(L_Octave, -2).B(L_Level, 0.6f).filtB(F_LP12, 450, 0.1f).ampB(0.005f, 2.f, 0.6f, 1.f)
        .fx(R_A, 0, FX_Stompbox, 2, 0.85f, { 0.7f, 0.45f, -4.f, 0.1f })
        .fx(R_A, 1, FX_Amp, 4, 0.75f, { 0.6f, 3.f, 1.f, -2.f, 0.6f, -3.f })
        .rev(0.4f, 0.8f).done());

    // Dissonant suspense horror cluster strings fed into a tinny speaker amplifier.
    b.push_back(PB("Tension String Cluster", "Cinematic")
        .oscA(E_Bowed, 0.55f, 0.15f, 0.75f).uniA(4, 35, 1.f).A(L_Drift, 0.6f).filtA(F_HP12, 350, 0.2f).ampA(0.4f, 1.f, 1.f, 2.f)
        .oscB(E_Spectral, 0.65f, 0.4f, 0.6f).B(L_Octave, 1).B(L_Level, 0.4f).ampB(0.6f, 1.f, 1.f, 2.2f)
        .fx(R_A, 0, FX_Amp, 5, 0.7f, { 0.5f, -6.f, 4.f, 2.f, 0.7f, -2.f })
        .dly(0.3f, 0.375f, 0.5f).rev(0.5f, 0.9f).done());

    // Bloodcurdling bowed string screech sculpted by Pultec EQ and sharp comb filtering.
    b.push_back(PB("Horror Scrape Agony", "Cinematic")
        .oscA(E_Bowed, 0.85f, 0.05f, 0.9f).filtA(F_Comb, 880.f, 0.8f).A(L_KeyTrack, 1.f).ampA(0.05f, 1.f, 1.f, 1.5f)
        .oscB(E_Modal, 0.67f, 0.9f, 0.7f).B(L_Octave, 1).B(L_Level, 0.4f).ampB(0.02f, 2.f, 0.7f, 1.5f)
        .fx(R_A, 0, FX_EQ, 2, 1.f, { -4.f, 100.f, 5.f, 3500.f, 4.f, 8000.f })
        .rev(0.55f, 0.9f).done());

    // Massive cinematic sub hit slamming a modal membrane through Tube Screamer and FET limiter.
    b.push_back(PB("Titan Colossus Impact", "Cinematic")
        .oscA(E_Modal, 1.0f, 0.85f, 0.8f).A(L_Octave, -2).filtA(F_LP24, 1200, 0.2f).A(L_FiltEnv, 3.f).A(L_FA, 0.001f).A(L_FD, 0.35f).A(L_FS, 0.1f).ampA(0.001f, 3.5f, 0.3f, 1.8f)
        .oscB(E_VA, 0.9f, 0.5f, 0.f).B(L_Octave, -2).B(L_Level, 0.6f).ampB(0.001f, 1.2f, 0.f, 0.8f)
        .fx(R_A, 0, FX_Stompbox, 0, 0.7f, { 0.55f, 0.5f, 0.f, 0.f })
        .fx(R_A, 1, FX_Compressor, 3, 1.f, { -12.f, 12.f, 0.5f, 80.f, 6.f, 2.f })
        .fx(R_Common, 0, FX_EQ, 0, 1.f, { 6.f, 50.f, -2.f, 300.f, 1.f, 5000.f })
        .rev(0.45f, 0.85f).done());

    // Epic cinematic crescendo swell rising through jet flanger and crystal shimmer reverb.
    b.push_back(PB("Rising Ascension Swell", "Cinematic")
        .oscA(E_VA, 0.05f, 0.5f, 0.f).uniA(6, 20, 0.9f).filtA(F_LP24, 400, 0.3f).A(L_FiltEnv, 5.f).A(L_FA, 4.f).A(L_FD, 1.f).A(L_FS, 1.f).ampA(3.5f, 1.f, 1.f, 3.f)
        .oscB(E_Vocal, 0.0f, 0.7f, 0.2f).B(L_Octave, 1).B(L_Level, 0.45f).ampB(3.5f, 1.f, 1.f, 3.5f)
        .fx(R_Common, 1, FX_Flanger, 0, 0.6f, { 0.1f, 0.8f, 0.7f, 1.5f, 0.7f })
        .fx(R_Common, 3, FX_Shimmer, 4, 0.55f, { 0.65f, 0.75f, 0.9f, 10.f, 8000.f, 15.f }).done());

    // =========================================================================
    // Pulse (5 presets)
    // =========================================================================

    // Rhythmic choir trance pulse sliced by stutter gate tremolo with stereo delay.
    b.push_back(PB("Stutter Gate Choir", "Pulse")
        .oscA(E_Vocal, 0.2f, 0.45f, 0.3f).uniA(4, 16, 0.9f).filtA(F_LP12, 5000, 0.1f).ampA(0.05f, 1.f, 1.f, 0.5f)
        .oscB(E_Spectral, 0.4f, 0.5f, 0.1f).B(L_Level, 0.4f).ampB(0.05f, 1.f, 1.f, 0.5f)
        .fx(R_Common, 0, FX_Tremolo, 4, 1.f, { 6.f, 0.95f, 0.8f, 45.f })
        .dly(0.3f, 0.25f, 0.4f).rev(0.3f, 0.6f).done());

    // Four-on-the-floor EDM supersaw sidechain pumped by bus compressor pump model.
    b.push_back(PB("Pumping Saw Thruster", "Pulse")
        .oscA(E_VA, 0.0f, 0.5f, 0.f).uniA(7, 28, 1.f).A(L_Drift, 0.3f).filtA(F_LP24, 3800, 0.2f).ampA(0.01f, 1.f, 1.f, 0.4f)
        .oscB(E_VA, 0.2f, 0.5f, 0.f).B(L_Octave, -1).B(L_Level, 0.5f).filtB(F_LP12, 1200, 0.1f).ampB(0.01f, 1.f, 1.f, 0.3f)
        .fx(R_Common, 0, FX_Compressor, 4, 1.f, { -24.f, 8.f, 5.f, 120.f, 4.f, 2.5f })
        .rev(0.35f, 0.65f).done());

    // Syncopated granular raindrops rhythmicized by dual-tap delay.
    b.push_back(PB("Stuttering Granular Cascade", "Pulse")
        .oscA(E_Granular, 0.7f, 0.15f, 0.85f).A(L_FMDepth, 0.5f).filtA(F_BP12, 2800, 0.4f).ampA(0.05f, 1.f, 1.f, 0.6f)
        .oscB(E_Modal, 0.33f, 0.6f, 0.5f).B(L_Level, 0.35f).ampB(0.001f, 1.5f, 0.8f, 0.5f)
        .fx(R_Common, 2, FX_Delay, 3, 0.5f, { 0.25f, 0.55f, 7000.f, 0.1f, 0.8f, 0.f })
        .rev(0.35f, 0.7f).done());

    // Surf-rock rhythmic plucked guitar through clean twin amp and square tremolo.
    b.push_back(PB("Tremolo Telecaster Pluck", "Pulse")
        .oscA(E_Pluck, 0.75f, 0.2f, 0.6f).filtA(F_Off, 20000, 0).ampA(0.001f, 4.f, 0.9f, 0.8f)
        .oscB(E_VA, 0.1f, 0.4f, 0.f).B(L_Octave, -1).B(L_Level, 0.3f).ampB(0.002f, 1.f, 0.5f, 0.5f)
        .fx(R_A, 0, FX_Tremolo, 1, 0.85f, { 5.5f, 0.85f, 0.5f, 0.f })
        .fx(R_A, 1, FX_Amp, 0, 0.8f, { 0.45f, 2.f, -1.f, 3.f, 0.6f, 0.f })
        .rev(0.35f, 0.65f).done());

    // Pulsing tonewheel organ spinning vigorously through dual Leslie 122 rotary cabinet.
    b.push_back(PB("Rotary Pulsar Organ", "Pulse")
        .oscA(E_Spectral, 0.3f, 0.95f, 0.f).A(L_Drift, 0.1f).ampA(0.01f, 1.f, 1.f, 0.15f)
        .oscB(E_Spectral, 0.5f, 0.1f, 0.f).B(L_Octave, 1).B(L_Level, 0.45f).ampB(0.01f, 1.f, 1.f, 0.15f)
        .fx(R_A, 0, FX_Rotary, 0, 0.9f, { 0.85f, 0.8f, 0.35f, 0.6f, 0.5f })
        .rev(0.3f, 0.6f).done());

    // =========================================================================
    // FX (5 presets)
    // =========================================================================

    // Blinding laser beam zap with ultra-fast pitch modulation and metallic ring modulator.
    b.push_back(PB("Hyper Zap Annihilator", "FX")
        .oscA(E_VA, 0.1f, 0.5f, 0.8f).A(L_L1Rate, 18.f).A(L_L1Pitch, 8.f).filtA(F_LP24, 9000, 0.4f).A(L_FA, 0.001f).A(L_FD, 0.25f).A(L_FS, 0.05f).A(L_FiltEnv, 4.f).ampA(0.001f, 0.4f, 0.1f, 0.3f)
        .oscB(E_Atmos, 0.8f, 0.5f, 0.9f).B(L_Level, 0.4f).ampB(0.001f, 0.3f, 0.f, 0.2f)
        .fx(R_A, 0, FX_RingShift, 0, 0.75f, { 650.f, 4.f, 0.4f, 0.3f, 0.6f })
        .dly(0.35f, 0.2f, 0.5f).rev(0.3f, 0.6f).done());

    // Demonic cybernetic speech synth mutilated by Fuzz Face distortion and robot talkbox wah.
    b.push_back(PB("Cyber Berserker", "FX")
        .oscA(E_Vocal, 0.5f, 0.25f, 0.6f).A(L_L2Timbre, 0.8f).A(L_L2Rate, 2.5f).ampA(0.02f, 1.f, 1.f, 0.3f)
        .oscB(E_VA, 0.0f, 0.5f, 0.9f).B(L_Octave, -1).B(L_Level, 0.5f).ampB(0.02f, 1.f, 1.f, 0.3f)
        .fx(R_A, 0, FX_Stompbox, 1, 0.8f, { 0.8f, 0.6f, -3.f, 0.2f })
        .fx(R_A, 1, FX_Wah, 5, 0.85f, { 0.7f, 900.f, 0.7f, 15.f, 200.f, 3.f })
        .rev(0.3f, 0.5f).done());

    // Futuristic computer mainframe calculation bleeps processed through metallic flanger.
    b.push_back(PB("Sci-Fi Comm Bleeps", "FX")
        .oscA(E_VA, 0.95f, 0.5f, 0.f).A(L_FMDepth, 0.6f).A(L_FMRatio, 3.5f).ampA(0.001f, 0.18f, 0.f, 0.15f)
        .oscB(E_Modal, 0.67f, 0.9f, 0.2f).B(L_Octave, 1).B(L_Level, 0.4f).ampB(0.001f, 0.2f, 0.f, 0.15f)
        .fx(R_A, 1, FX_Flanger, 3, 0.75f, { 1.2f, 0.8f, 0.85f, 0.8f, 0.5f })
        .dly(0.3f, 0.15f, 0.45f).rev(0.25f, 0.5f).done());

    // Frequency-shifting infinite spiral feedback creating an extraterrestrial radio vortex.
    b.push_back(PB("Alien Carrier Spiral", "FX")
        .oscA(E_Granular, 0.85f, 0.3f, 0.9f).A(L_FMDepth, 0.7f).filtA(F_BP12, 2000, 0.5f).ampA(0.4f, 1.f, 1.f, 1.5f)
        .oscB(E_Atmos, 0.7f, 0.8f, 0.6f).B(L_Level, 0.4f).ampB(0.8f, 1.f, 1.f, 2.f)
        .fx(R_Common, 0, FX_RingShift, 1, 0.7f, { 120.f, 0.2f, 0.4f, 0.65f, 0.8f })
        .dly(0.35f, 0.38f, 0.6f).rev(0.45f, 0.8f).done());

    // Dramatic cinematic transition downlifter crashing into white noise and deep reverb tail.
    b.push_back(PB("Vortex Downlifter", "FX")
        .oscA(E_Atmos, 0.9f, 0.95f, 0.9f).A(L_FMDepth, 0.8f).filtA(F_LP24, 14000, 0.35f).A(L_FiltEnv, -6.f).A(L_FA, 0.05f).A(L_FD, 4.f).A(L_FS, 0.05f).ampA(0.05f, 4.f, 0.1f, 2.5f)
        .oscB(E_VA, 0.0f, 0.5f, 0.5f).B(L_Octave, -1).B(L_Level, 0.45f).filtB(F_LP24, 8000, 0.2f).B(L_FiltEnv, -5.f).B(L_FA, 0.05f).B(L_FD, 3.5f).B(L_FS, 0.05f).ampB(0.05f, 3.5f, 0.1f, 2.f)
        .chorus(0.4f, 0.5f)
        .fx(R_Common, 3, FX_ProVerb, 5, 0.6f, { 0.9f, 8.f, 10.f, 0.8f, 0.f, 2.f }).done());
}

} // namespace op
