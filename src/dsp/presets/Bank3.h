// Panaudium factory bank 3. AGPL-3.0 (see LICENSE).
#pragma once
#include "../Presets.h"

namespace op {

inline void addBank3(std::vector<Preset>& b) {
    using PB = PresetBuilder;

    // =========================================================================
    // Keys (6 presets)
    // =========================================================================

    // Classic Rhodes-style tine electric piano with stereo harmonic tremolo
    b.push_back(PB("Suitcase Seventy-Three", "Keys")
        .oscA(E_VA, 0.95f, 0.5f, 0.0f).A(L_FMDepth, 0.35f).A(L_FMRatio, 14.f).filtA(F_LP12, 3500.f, 0.1f).A(L_FiltEnv, 1.8f).A(L_FD, 0.8f).A(L_FS, 0.2f).ampA(0.002f, 2.2f, 0.2f, 0.5f)
        .oscB(E_Modal, 0.1f, 0.8f, 0.2f).B(L_Octave, 1).B(L_Level, 0.35f).filtB(F_Off, 20000.f, 0.f).ampB(0.001f, 0.25f, 0.0f, 0.2f)
        .fx(R_A, 0, FX_Tremolo, 2, 0.85f, { 4.8f, 0.65f, 0.5f, 90.f })
        .chorus(0.35f, 0.8f).rev(0.2f, 0.5f).done());

    // Barking 200A reed piano pushed through a vintage tube combo amp and tremolo
    b.push_back(PB("Wurlitzer Reed", "Keys")
        .oscA(E_VA, 0.35f, 0.35f, 0.0f).A(L_FMDepth, 0.25f).A(L_FMRatio, 1.f).filtA(F_LP12, 3200.f, 0.15f).A(L_FiltEnv, 1.5f).A(L_FD, 0.6f).A(L_FS, 0.25f).ampA(0.002f, 1.8f, 0.25f, 0.4f)
        .oscB(E_Spectral, 0.4f, 0.8f, 0.1f).B(L_Level, 0.4f).filtB(F_Off, 20000.f, 0.f).ampB(0.005f, 1.5f, 0.2f, 0.4f)
        .fx(R_A, 0, FX_Amp, 1, 0.85f, { 0.45f, 1.f, 3.f, -1.f, 0.4f, -3.f })
        .fx(R_A, 1, FX_Tremolo, 0, 0.5f, { 5.5f, 0.55f, 0.5f, 0.f })
        .fx(R_Common, 0, FX_Tape, 0, 0.55f, { 0.35f, 0.12f, 0.15f, 0.04f, 0.65f })
        .rev(0.22f, 0.4f).done());

    // Sharp percussive clavinet with pick attack running through an envelope auto-wah
    b.push_back(PB("Funky D6 Clav", "Keys")
        .oscA(E_Pluck, 0.88f, 0.05f, 0.25f).filtA(F_HP12, 120.f, 0.1f).ampA(0.001f, 1.2f, 0.1f, 0.15f)
        .oscB(E_VA, 0.3f, 0.2f, 0.0f).B(L_Level, 0.3f).filtB(F_LP12, 4000.f, 0.2f).ampB(0.001f, 0.4f, 0.05f, 0.1f)
        .fx(R_A, 0, FX_Wah, 1, 0.95f, { 0.7f, 800.f, 0.65f, 8.f, 120.f, 3.2f })
        .fx(R_A, 1, FX_Stompbox, 0, 0.5f, { 0.35f, 0.6f, -2.f })
        .rev(0.15f, 0.35f).done());

    // Full drawbar tonewheel organ spinning through a dual-rotor Leslie cabinet
    b.push_back(PB("B3 Gospel Spinner", "Keys")
        .oscA(E_Spectral, 0.6f, 0.2f, 0.0f).A(L_Drift, 0.05f).filtA(F_Off, 20000.f, 0.f).ampA(0.005f, 0.5f, 1.f, 0.08f)
        .oscB(E_VA, 0.8f, 0.5f, 0.0f).B(L_Level, 0.35f).filtB(F_HP12, 1000.f, 0.1f).ampB(0.001f, 0.06f, 0.0f, 0.04f)
        .fx(R_A, 0, FX_Rotary, 0, 1.f, { 1.f, 1.2f, 0.35f, 0.55f, 0.5f })
        .rev(0.28f, 0.65f).done());

    // Resonant metallic toy piano tines heard through a resonant metal box
    b.push_back(PB("Cellar Toy Piano", "Keys")
        .oscA(E_Modal, 0.35f, 0.95f, 0.45f).filtA(F_Off, 20000.f, 0.f).ampA(0.001f, 3.0f, 0.85f, 0.8f)
        .oscB(E_Modal, 0.65f, 0.9f, 0.3f).B(L_Octave, 1).B(L_Level, 0.4f).filtB(F_Off, 20000.f, 0.f).ampB(0.001f, 1.5f, 0.8f, 0.5f)
        .fx(R_A, 0, FX_Amp, 5, 0.75f, { 0.3f, -6.f, 4.f, 2.f, 0.6f, -2.f })
        .fx(R_Common, 0, FX_Innerspace, 2, 0.45f, { 0.8f, 0.5f, 2.f })
        .rev(0.25f, 0.45f).done());

    // Intimate felt upright with soft hammer damping and tape warmth
    b.push_back(PB("Nordic Felt Piano", "Keys")
        .oscA(E_Spectral, 0.22f, 0.4f, 0.35f).filtA(F_LP12, 4500.f, 0.05f).ampA(0.008f, 3.5f, 0.15f, 0.6f)
        .oscB(E_Modal, 0.15f, 0.25f, 0.5f).B(L_Level, 0.45f).filtB(F_LP12, 3500.f, 0.05f).ampB(0.005f, 2.5f, 0.2f, 0.5f)
        .fx(R_A, 0, FX_EQ, 1, 1.f, { 3.5f, 110.f, -2.5f, 650.f, -4.f, 5000.f })
        .fx(R_Common, 0, FX_Tape, 1, 0.55f, { 0.25f, 0.18f, 0.12f, 0.06f, 0.55f })
        .rev(0.32f, 0.7f).done());

    // =========================================================================
    // Bass (7 presets)
    // =========================================================================

    // Sub-heavy 808 kick bass with pitch drop, saturated bite and bus compression
    b.push_back(PB("Deep 808 Boom", "Bass")
        .oscA(E_VA, 1.0f, 0.5f, 0.0f).A(L_Octave, -2).filtA(F_LP24, 180.f, 0.25f).A(L_FiltEnv, 4.5f).A(L_FA, 0.001f).A(L_FD, 0.26f).A(L_FS, 0.0f).ampA(0.001f, 1.2f, 0.7f, 0.2f)
        .oscB(E_Modal, 0.95f, 0.8f, 0.15f).B(L_Octave, -1).B(L_Level, 0.3f).filtB(F_LP12, 1200.f, 0.1f).ampB(0.001f, 0.15f, 0.0f, 0.08f)
        .fx(R_A, 0, FX_Stompbox, 4, 0.65f, { 0.4f, 0.35f, 1.f })
        .fx(R_Common, 0, FX_Compressor, 0, 1.f, { -16.f, 4.5f, 15.f, 120.f, 3.f })
        .noFx(R_Common, 3).done());

    // Detuned multi-saw reese bass carved by flanging and sweeping notch phaser
    b.push_back(PB("Neuro Reese Bass", "Bass")
        .oscA(E_VA, 0.05f, 0.5f, 0.0f).A(L_Octave, -1).uniA(4, 24.f, 0.8f).filtA(F_LP24, 950.f, 0.25f).A(L_Drive, 0.3f).ampA(0.01f, 0.8f, 0.9f, 0.2f)
        .oscB(E_VA, 0.4f, 0.5f, 0.0f).B(L_Octave, -2).B(L_Level, 0.6f).filtB(F_LP12, 350.f, 0.1f).ampB(0.01f, 0.8f, 1.f, 0.2f)
        .fx(R_A, 0, FX_Flanger, 0, 0.55f, { 0.35f, 0.75f, 0.6f, 2.5f, 0.7f })
        .fx(R_A, 1, FX_Phaser, 3, 0.7f, { 0.22f, 0.6f, 0.8f, 450.f, 0.8f })
        .fx(R_Common, 0, FX_EQ, 0, 1.f, { 4.f, 90.f, -3.f, 400.f, 1.f, 4000.f })
        .noFx(R_Common, 3).done());

    // Natural upright wooden bass combining plucked string with bowed body resonance
    b.push_back(PB("Acoustic Double Bass", "Bass")
        .oscA(E_Pluck, 0.32f, 0.15f, 0.55f).A(L_Octave, -1).filtA(F_LP12, 2800.f, 0.05f).ampA(0.001f, 3.2f, 0.9f, 0.35f)
        .oscB(E_Bowed, 0.3f, 0.1f, 0.5f).B(L_Octave, -1).B(L_FMDepth, 0.75f).B(L_Level, 0.45f).filtB(F_LP12, 1800.f, 0.05f).ampB(0.02f, 2.5f, 0.7f, 0.3f)
        .fx(R_A, 0, FX_Amp, 1, 0.6f, { 0.3f, 3.f, 1.5f, -2.f, 0.3f, 0.f })
        .fx(R_Common, 0, FX_Compressor, 1, 0.85f, { -14.f, 3.5f, 25.f, 220.f, 2.f })
        .rev(0.18f, 0.4f).done());

    // Snappy FM slap bass with percussive thumb pop and biting envelope wah
    b.push_back(PB("Slap Funk Thumb", "Bass")
        .oscA(E_VA, 0.85f, 0.5f, 0.0f).A(L_Octave, -1).A(L_FMDepth, 0.42f).A(L_FMRatio, 2.f).filtA(F_LP12, 3200.f, 0.2f).A(L_FiltEnv, 2.5f).A(L_FD, 0.2f).A(L_FS, 0.15f).ampA(0.002f, 0.8f, 0.3f, 0.15f)
        .oscB(E_Pluck, 0.9f, 0.08f, 0.2f).B(L_Level, 0.45f).filtB(F_HP12, 300.f, 0.1f).ampB(0.001f, 0.4f, 0.1f, 0.1f)
        .fx(R_A, 0, FX_Wah, 2, 0.85f, { 0.75f, 450.f, 0.7f, 5.f, 90.f, 3.0f })
        .fx(R_Common, 0, FX_Compressor, 3, 1.f, { -18.f, 8.f, 1.5f, 80.f, 4.f })
        .fx(R_Common, 1, FX_Chorus, 3, 0.25f, { 0.5f, 0.4f })
        .rev(0.12f, 0.35f).done());

    // Screaming silver-box bass with resonant 4-pole filter sweep and stompbox distortion
    b.push_back(PB("Acid Squelch 303", "Bass")
        .oscA(E_VA, 0.0f, 0.5f, 0.0f).A(L_Octave, -1).filtA(F_LP24, 450.f, 0.82f).A(L_FiltEnv, 4.5f).A(L_FA, 0.002f).A(L_FD, 0.24f).A(L_FS, 0.08f).A(L_Drive, 0.35f).ampA(0.002f, 0.6f, 0.8f, 0.12f)
        .fx(R_A, 0, FX_Stompbox, 2, 0.75f, { 0.55f, 0.5f, -3.f })
        .dly(0.28f, 0.25f, 0.45f).noFx(R_Common, 3).done());

    // Singing fretless electric bass with expressive pitch glide and lush analog chorus
    b.push_back(PB("Fretless Warmth", "Bass")
        .oscA(E_VA, 0.28f, 0.5f, 0.0f).A(L_Octave, -1).filtA(F_LP24, 1600.f, 0.25f).A(L_FiltEnv, 1.2f).A(L_FD, 0.4f).A(L_FS, 0.6f).ampA(0.01f, 1.2f, 0.85f, 0.25f)
        .oscB(E_Bowed, 0.45f, 0.12f, 0.65f).B(L_Octave, -1).B(L_Level, 0.4f).filtB(F_LP12, 2200.f, 0.1f).ampB(0.04f, 1.0f, 0.75f, 0.3f)
        .fx(R_A, 0, FX_Chorus, 1, 0.65f, { 0.45f, 0.6f, 0.85f })
        .fx(R_Common, 0, FX_EQ, 1, 1.f, { 2.5f, 120.f, 3.5f, 850.f, -3.f, 4500.f })
        .rev(0.2f, 0.55f).G(G_Glide, 0.07f).done());

    // Crushed sub bass combined with a modulated vocal formant growl
    b.push_back(PB("Subterranean Growl", "Bass")
        .oscA(E_VA, 1.0f, 0.5f, 0.0f).A(L_Octave, -2).A(L_Level, 0.85f).filtA(F_LP12, 250.f, 0.1f).ampA(0.005f, 0.8f, 0.95f, 0.15f)
        .oscB(E_Vocal, 0.2f, 0.15f, 0.3f).B(L_Octave, -1).B(L_Level, 0.5f).B(L_L2Timbre, 0.6f).B(L_L2Rate, 0.35f).filtB(F_LP12, 3500.f, 0.2f).ampB(0.01f, 0.8f, 0.9f, 0.2f)
        .fx(R_B, 0, FX_LoFi, 0, 0.7f, { 7.5f, 2.f, 0.05f, 3200.f })
        .fx(R_Common, 0, FX_Compressor, 4, 0.85f, { -15.f, 5.f, 10.f, 150.f, 2.5f, 2.f })
        .noFx(R_Common, 3).done());

    // =========================================================================
    // Lead (6 presets)
    // =========================================================================

    // Aggressive hard-sync lead pushed through a roaring British high-gain amplifier
    b.push_back(PB("Screaming Sync Lead", "Lead")
        .oscA(E_VA, 0.0f, 0.5f, 0.82f).uniA(2, 8.f, 0.4f).filtA(F_LP24, 7500.f, 0.2f).A(L_FiltEnv, 2.0f).A(L_FD, 0.5f).A(L_FS, 0.6f).ampA(0.005f, 0.8f, 0.9f, 0.25f)
        .oscB(E_VA, 0.5f, 0.5f, 0.0f).B(L_Octave, -1).B(L_Level, 0.35f).filtB(F_LP12, 1500.f, 0.1f).ampB(0.005f, 0.8f, 0.9f, 0.25f)
        .fx(R_A, 0, FX_Amp, 4, 0.8f, { 0.65f, 1.f, 3.5f, 2.f, 0.7f, -4.f })
        .fx(R_Common, 2, FX_Delay, 1, 0.32f, { 0.375f, 0.4f, 4000.f, 0.15f })
        .rev(0.22f, 0.6f).G(G_Glide, 0.05f).done());

    // High-register overblown pipe lead with breath flutter and tape delay
    b.push_back(PB("Whistling Shepherd", "Lead")
        .oscA(E_Pipe, 0.18f, 0.75f, 0.65f).A(L_Octave, 1).A(L_L1Pitch, 0.18f).A(L_L1Rate, 5.2f).filtA(F_Off, 20000.f, 0.f).ampA(0.02f, 1.0f, 1.0f, 0.2f)
        .fx(R_A, 0, FX_Tape, 0, 0.7f, { 0.3f, 0.14f, 0.18f, 0.03f, 0.8f })
        .dly(0.35f, 0.333f, 0.5f).rev(0.3f, 0.7f).G(G_Glide, 0.04f).done());

    // Authentic vintage chiptune pulse lead degraded through 8-bit downsampling
    b.push_back(PB("Arcade Hero Square", "Lead")
        .oscA(E_VA, 0.33f, 0.25f, 0.0f).A(L_L1Pitch, 0.2f).A(L_L1Rate, 6.0f).filtA(F_Off, 20000.f, 0.f).ampA(0.001f, 0.5f, 0.8f, 0.08f)
        .fx(R_A, 0, FX_LoFi, 0, 1.f, { 5.f, 4.f, 0.08f, 8000.f })
        .fx(R_Common, 2, FX_Delay, 0, 0.25f, { 0.187f, 0.35f, 7000.f })
        .noFx(R_Common, 3).G(G_Glide, 0.03f).done());

    // Sweeping vocal vowel lead morphing from deep O to brilliant A through a hall
    b.push_back(PB("Operatic Formant Lead", "Lead")
        .oscA(E_Vocal, 0.25f, 0.65f, 0.2f).A(L_L1Pitch, 0.22f).A(L_L1Rate, 5.4f).A(L_L2Timbre, 0.45f).A(L_L2Rate, 0.25f).filtA(F_Off, 20000.f, 0.f).ampA(0.05f, 1.0f, 1.0f, 0.35f)
        .fx(R_A, 0, FX_Phaser, 1, 0.5f, { 0.4f, 0.5f, 0.65f, 1200.f })
        .fx(R_Common, 1, FX_Chorus, 4, 0.35f, { 0.6f, 0.5f })
        .fx(R_Common, 3, FX_ProVerb, 3, 0.42f, { 0.85f, 4.0f })
        .G(G_Glide, 0.06f).done());

    // Expressive solo bowed cello lead with warm vibrato and gentle wood warmth
    b.push_back(PB("Solitary Cello Lead", "Lead")
        .oscA(E_Bowed, 0.52f, 0.15f, 0.8f).A(L_FMDepth, 0.65f).A(L_L1Pitch, 0.25f).A(L_L1Rate, 5.0f).filtA(F_LP12, 5500.f, 0.05f).ampA(0.08f, 1.0f, 1.0f, 0.35f)
        .fx(R_A, 0, FX_EQ, 1, 1.f, { 2.f, 180.f, 1.5f, 1200.f, -2.f, 6000.f })
        .fx(R_Common, 0, FX_Innerspace, 4, 0.3f, { 1.0f, 0.6f, 0.f })
        .rev(0.35f, 0.75f).G(G_Glide, 0.05f).done());

    // Futuristic metallic synth lead shifting frequencies with an interactive wah
    b.push_back(PB("Ring Mod Cry Baby", "Lead")
        .oscA(E_VA, 0.4f, 0.5f, 0.0f).uniA(2, 6.f, 0.4f).filtA(F_LP12, 5000.f, 0.3f).ampA(0.005f, 0.8f, 0.85f, 0.2f)
        .oscB(E_Spectral, 0.7f, 0.5f, 0.4f).B(L_Level, 0.35f).filtB(F_Off, 20000.f, 0.f).ampB(0.005f, 0.8f, 0.85f, 0.2f)
        .fx(R_A, 0, FX_RingShift, 1, 0.45f, { 120.f, 0.25f, 0.3f, 0.2f, 0.4f })
        .fx(R_A, 1, FX_Wah, 0, 0.75f, { 0.6f, 900.f, 0.65f, 10.f, 150.f, 2.5f })
        .dly(0.3f, 0.35f, 0.45f)
        .fx(R_Common, 3, FX_ProVerb, 2, 0.25f, { 0.5f, 2.0f })
        .G(G_Glide, 0.04f).done());

    // =========================================================================
    // Pluck (5 presets)
    // =========================================================================

    // Twangy five-string banjo with sharp fingerpick and resonant comb body
    b.push_back(PB("Appalachian Banjo", "Pluck")
        .oscA(E_Pluck, 0.92f, 0.04f, 0.2f).filtA(F_Comb, 261.63f, 0.65f).A(L_KeyTrack, 1.f).ampA(0.001f, 1.5f, 0.85f, 0.3f)
        .fx(R_A, 0, FX_Amp, 0, 0.7f, { 0.35f, -4.f, 3.f, 2.5f, 0.6f, -1.f })
        .fx(R_Common, 0, FX_Compressor, 3, 0.75f, { -15.f, 4.f, 4.f, 60.f, 2.f })
        .rev(0.2f, 0.35f).done());

    // Palm-muted electric guitar chugging through an American Clean Twin amplifier
    b.push_back(PB("Muted Nashville Twin", "Pluck")
        .oscA(E_Pluck, 0.28f, 0.12f, 0.12f).filtA(F_LP12, 2400.f, 0.1f).ampA(0.001f, 0.6f, 0.3f, 0.12f)
        .fx(R_A, 0, FX_Amp, 0, 0.9f, { 0.4f, 1.5f, 0.f, 3.f, 0.5f, 0.f })
        .fx(R_Common, 2, FX_Delay, 1, 0.25f, { 0.25f, 0.3f, 3500.f })
        .rev(0.2f, 0.45f).done());

    // Sparkling nylon concert harp with warm decay and glistening pitch shimmer
    b.push_back(PB("Celtic Concert Harp", "Pluck")
        .oscA(E_Pluck, 0.6f, 0.22f, 0.55f).filtA(F_Off, 20000.f, 0.f).ampA(0.001f, 5.0f, 0.9f, 1.2f)
        .oscB(E_Modal, 0.15f, 0.4f, 0.45f).B(L_Level, 0.35f).filtB(F_Off, 20000.f, 0.f).ampB(0.001f, 3.0f, 0.85f, 1.0f)
        .fx(R_A, 0, FX_Chorus, 0, 0.3f, { 0.5f, 0.35f })
        .fx(R_Common, 3, FX_Shimmer, 0, 0.32f, { 0.45f, 0.65f, 0.75f, 5.5f, 7000.f })
        .done());

    // Tight orchestral string pizzicato with crisp bow-wood snap and concert hall bloom
    b.push_back(PB("Orchestral Pizzicato", "Pluck")
        .oscA(E_Pluck, 0.7f, 0.18f, 0.18f).filtA(F_LP12, 5500.f, 0.1f).ampA(0.001f, 0.8f, 0.2f, 0.15f)
        .oscB(E_Modal, 0.3f, 0.5f, 0.12f).B(L_Level, 0.4f).filtB(F_Off, 20000.f, 0.f).ampB(0.001f, 0.4f, 0.1f, 0.1f)
        .fx(R_A, 0, FX_EQ, 0, 1.f, { -2.f, 150.f, 2.f, 2200.f, 1.5f, 6000.f })
        .rev(0.35f, 0.8f).done());

    // Shimmering dual-layer hybrid combining crisp Karplus pluck with modal bell resonance
    b.push_back(PB("Glassy Prism Pluck", "Pluck")
        .oscA(E_Pluck, 0.85f, 0.3f, 0.6f).filtA(F_Off, 20000.f, 0.f).ampA(0.001f, 4.0f, 0.85f, 1.0f)
        .oscB(E_Modal, 0.66f, 0.7f, 0.6f).B(L_Octave, 1).B(L_Level, 0.5f).filtB(F_Off, 20000.f, 0.f).ampB(0.001f, 3.5f, 0.8f, 1.2f)
        .fx(R_A, 0, FX_Flanger, 3, 0.45f, { 0.15f, 0.6f, 0.5f, 1.8f })
        .dly(0.3f, 0.375f, 0.45f)
        .fx(R_Common, 3, FX_ProVerb, 2, 0.3f, { 0.65f, 3.0f })
        .done());

    // =========================================================================
    // Mallet (4 presets)
    // =========================================================================

    // Dry acoustic rosewood xylophone with sharp wooden mallet impact
    b.push_back(PB("Rosewood Xylophone", "Mallet")
        .oscA(E_Modal, 0.33f, 0.85f, 0.22f).filtA(F_LP12, 8000.f, 0.1f).ampA(0.001f, 1.5f, 0.85f, 0.4f)
        .oscB(E_Modal, 0.1f, 0.95f, 0.08f).B(L_Octave, 1).B(L_Level, 0.3f).filtB(F_Off, 20000.f, 0.f).ampB(0.001f, 0.2f, 0.0f, 0.1f)
        .fx(R_A, 0, FX_Compressor, 1, 0.7f, { -12.f, 3.f, 8.f, 80.f, 1.5f })
        .rev(0.22f, 0.4f).done());

    // Piercing steel glockenspiel struck with hard brass mallets in a grand auditorium
    b.push_back(PB("Orchestral Glockenspiel", "Mallet")
        .oscA(E_Modal, 0.35f, 0.98f, 0.7f).A(L_Octave, 1).filtA(F_Off, 20000.f, 0.f).ampA(0.001f, 6.0f, 0.9f, 2.5f)
        .oscB(E_Spectral, 0.5f, 0.9f, 0.1f).B(L_Octave, 2).B(L_Level, 0.35f).filtB(F_Off, 20000.f, 0.f).ampB(0.001f, 2.0f, 0.5f, 1.0f)
        .fx(R_A, 0, FX_Shimmer, 4, 0.28f, { 0.4f, 0.5f, 0.8f, 6.0f, 9000.f })
        .fx(R_Common, 3, FX_ProVerb, 3, 0.38f, { 0.85f, 3.8f })
        .done());

    // Caribbean steel pan with vibrant tuned membrane bounce and tropical chorus
    b.push_back(PB("Trinidad Steel Drum", "Mallet")
        .oscA(E_Modal, 0.88f, 0.6f, 0.45f).A(L_FMDepth, 0.2f).filtA(F_LP12, 6500.f, 0.15f).ampA(0.001f, 3.5f, 0.9f, 0.8f)
        .oscB(E_VA, 0.9f, 0.5f, 0.0f).B(L_Level, 0.3f).filtB(F_LP12, 1200.f, 0.1f).ampB(0.001f, 1.2f, 0.2f, 0.3f)
        .fx(R_A, 0, FX_Chorus, 2, 0.4f, { 0.7f, 0.5f, 0.8f, 8.f, 0.75f })
        .dly(0.22f, 0.25f, 0.3f).rev(0.28f, 0.6f).done());

    // Resonant steel hang drum with warm central ding and acoustic impulse space
    b.push_back(PB("Handpan Meditation", "Mallet")
        .oscA(E_Modal, 0.92f, 0.4f, 0.65f).A(L_FMDepth, 0.15f).filtA(F_LP12, 5000.f, 0.1f).ampA(0.002f, 5.0f, 0.9f, 1.5f)
        .oscB(E_Atmos, 0.3f, 0.1f, 0.4f).B(L_Level, 0.2f).filtB(F_LP12, 2000.f, 0.1f).ampB(0.001f, 0.3f, 0.0f, 0.2f)
        .fx(R_A, 0, FX_Innerspace, 7, 0.35f, { 1.2f, 0.7f, -2.f })
        .fx(R_Common, 3, FX_ProVerb, 4, 0.35f, { 0.8f, 4.5f })
        .done());

    // =========================================================================
    // World (5 presets)
    // =========================================================================

    // Traditional Indian sitar with buzzing jawari bridge and sympathetic piano-lid resonance
    b.push_back(PB("Varanasi Sitar Drone", "World")
        .oscA(E_Pluck, 0.95f, 0.02f, 0.65f).filtA(F_Comb, 523.25f, 0.72f).A(L_KeyTrack, 1.f).ampA(0.001f, 5.0f, 0.9f, 1.2f)
        .oscB(E_Spectral, 0.5f, 0.6f, 0.2f).B(L_Drift, 0.2f).B(L_Level, 0.35f).filtB(F_Off, 20000.f, 0.f).ampB(0.01f, 4.0f, 0.8f, 1.5f)
        .fx(R_A, 0, FX_Innerspace, 4, 0.45f, { 1.1f, 0.65f, 1.f })
        .fx(R_Common, 2, FX_Delay, 1, 0.25f, { 0.375f, 0.4f, 4500.f })
        .rev(0.25f, 0.65f).done());

    // Japanese 13-string zither with ivory plectrum snap and woody soundboard
    b.push_back(PB("Kyoto Silk Koto", "World")
        .oscA(E_Pluck, 0.78f, 0.06f, 0.45f).filtA(F_LP12, 7000.f, 0.1f).ampA(0.001f, 4.0f, 0.9f, 0.8f)
        .oscB(E_Modal, 0.2f, 0.5f, 0.3f).B(L_Level, 0.4f).filtB(F_Off, 20000.f, 0.f).ampB(0.001f, 1.5f, 0.7f, 0.5f)
        .fx(R_A, 0, FX_EQ, 2, 1.f, { 2.f, 100.f, 1.5f, 1800.f, 2.5f, 7000.f })
        .dly(0.2f, 0.28f, 0.35f).rev(0.28f, 0.6f).done());

    // Meditative Japanese end-blown bamboo flute with breath noise and expressive pitch dip
    b.push_back(PB("Bamboo Shakuhachi Zen", "World")
        .oscA(E_Pipe, 0.68f, 0.38f, 0.45f).A(L_L1Pitch, 0.28f).A(L_L1Rate, 4.2f).filtA(F_LP12, 5500.f, 0.1f).ampA(0.08f, 1.0f, 1.0f, 0.45f)
        .oscB(E_Atmos, 0.5f, 0.3f, 0.6f).B(L_Level, 0.3f).filtB(F_LP12, 4000.f, 0.1f).ampB(0.05f, 0.8f, 0.5f, 0.4f)
        .fx(R_A, 0, FX_Tape, 0, 0.6f, { 0.25f, 0.16f, 0.14f, 0.05f, 0.7f })
        .fx(R_Common, 3, FX_ProVerb, 3, 0.4f, { 0.85f, 3.5f })
        .G(G_Glide, 0.08f).done());

    // Deep Australian eucalyptus didgeridoo with throat vocal resonance and rhythmic auto-wah
    b.push_back(PB("Outback Didgeridoo", "World")
        .oscA(E_Vocal, 0.15f, 0.05f, 0.4f).A(L_Octave, -2).A(L_Level, 0.7f).A(L_L2Timbre, 0.5f).A(L_L2Rate, 1.4f).filtA(F_LP12, 2200.f, 0.2f).ampA(0.05f, 1.0f, 1.0f, 0.3f)
        .oscB(E_Pipe, 0.45f, 0.2f, 0.5f).B(L_Octave, -2).B(L_Level, 0.6f).filtB(F_LP12, 1800.f, 0.15f).ampB(0.04f, 1.0f, 1.0f, 0.3f)
        .fx(R_A, 0, FX_Wah, 4, 0.85f, { 0.5f, 350.f, 0.75f, 20.f, 200.f, 2.8f })
        .fx(R_A, 1, FX_Stompbox, 0, 0.45f, { 0.4f, 0.45f, 0.f })
        .rev(0.25f, 0.5f).done());

    // Piercing Scottish chanter with sustained dual harmonic pipe drones through tube drive
    b.push_back(PB("Highland Bagpipe Drone", "World")
        .oscA(E_Spectral, 0.75f, 1.0f, 0.05f).A(L_Drift, 0.15f).uniA(2, 6.f, 0.6f).filtA(F_Off, 20000.f, 0.f).ampA(0.02f, 1.0f, 1.0f, 0.15f)
        .oscB(E_VA, 0.25f, 0.5f, 0.0f).B(L_Octave, -1).B(L_Level, 0.5f).filtB(F_LP12, 2500.f, 0.1f).ampB(0.02f, 1.0f, 1.0f, 0.2f)
        .fx(R_A, 0, FX_Stompbox, 4, 0.4f, { 0.35f, 0.6f, -1.f })
        .chorus(0.35f, 0.5f).rev(0.32f, 0.7f).done());

    // =========================================================================
    // Brass (3 presets)
    // =========================================================================

    // Punchy analog synthesizer brass section with sweeping resonant lowpass envelope
    b.push_back(PB("Vintage Brass Stab", "Brass")
        .oscA(E_VA, 0.0f, 0.5f, 0.0f).uniA(5, 18.f, 0.8f).filtA(F_LP24, 600.f, 0.28f).A(L_FiltEnv, 3.8f).A(L_FA, 0.035f).A(L_FD, 0.65f).A(L_FS, 0.35f).A(L_FR, 0.3f).ampA(0.02f, 0.7f, 0.8f, 0.25f)
        .oscB(E_Spectral, 0.6f, 0.3f, 0.0f).B(L_Level, 0.4f).filtB(F_LP12, 3500.f, 0.1f).ampB(0.02f, 0.6f, 0.7f, 0.25f)
        .fx(R_A, 0, FX_Chorus, 3, 0.45f, { 0.65f, 0.55f })
        .fx(R_Common, 0, FX_EQ, 0, 1.f, { 2.f, 140.f, 2.5f, 1500.f, 1.5f, 7000.f })
        .rev(0.3f, 0.65f).done());

    // Intimate harmon-muted jazz trumpet with acoustic air flutter and subtle frequency shift
    b.push_back(PB("Miles Muted Trumpet", "Brass")
        .oscA(E_Pipe, 0.2f, 0.85f, 0.5f).A(L_L1Pitch, 0.15f).A(L_L1Rate, 5.5f).filtA(F_Formant, 1800.f, 0.6f).A(L_KeyTrack, 0.4f).ampA(0.015f, 0.8f, 0.9f, 0.2f)
        .fx(R_A, 0, FX_RingShift, 1, 0.25f, { 45.f, 0.1f, 0.1f, 0.1f })
        .fx(R_Common, 0, FX_Tape, 1, 0.45f, { 0.3f, 0.12f, 0.1f, 0.04f, 0.6f })
        .rev(0.28f, 0.45f).done());

    // Majestic orchestral French horn ensemble with warm attack and expansive hall reverb
    b.push_back(PB("Noble French Horns", "Brass")
        .oscA(E_Spectral, 0.45f, 0.1f, 0.0f).A(L_Drift, 0.2f).uniA(3, 10.f, 0.85f).filtA(F_LP24, 1800.f, 0.15f).A(L_FiltEnv, 1.5f).A(L_FA, 0.12f).A(L_FD, 1.2f).A(L_FS, 0.7f).ampA(0.09f, 1.2f, 0.85f, 0.6f)
        .oscB(E_VA, 0.1f, 0.5f, 0.0f).B(L_Octave, -1).B(L_Level, 0.4f).filtB(F_LP12, 1200.f, 0.08f).ampB(0.1f, 1.2f, 0.8f, 0.6f)
        .fx(R_A, 0, FX_EQ, 1, 1.f, { 3.f, 160.f, 1.f, 800.f, -3.f, 5000.f })
        .fx(R_Common, 1, FX_Chorus, 1, 0.35f, { 0.4f, 0.4f })
        .fx(R_Common, 3, FX_ProVerb, 3, 0.42f, { 0.85f, 3.5f })
        .done());

    // =========================================================================
    // Percussion (4 presets)
    // =========================================================================

    // Punchy acoustic-electronic hybrid kick with fast pitch drop and bus compression
    b.push_back(PB("Sub Kick Drum", "Percussion")
        .oscA(E_Modal, 1.0f, 0.75f, 0.1f).A(L_Octave, -2).filtA(F_LP24, 160.f, 0.3f).A(L_FiltEnv, 5.0f).A(L_FA, 0.001f).A(L_FD, 0.06f).A(L_FS, 0.0f).ampA(0.001f, 0.35f, 0.0f, 0.1f)
        .oscB(E_Pluck, 0.9f, 0.05f, 0.08f).B(L_Level, 0.4f).filtB(F_LP12, 3000.f, 0.1f).ampB(0.001f, 0.04f, 0.0f, 0.02f)
        .fx(R_A, 0, FX_Stompbox, 0, 0.45f, { 0.35f, 0.4f, 2.f })
        .fx(R_Common, 0, FX_Compressor, 3, 1.f, { -14.f, 8.f, 3.f, 60.f, 3.f })
        .noFx(R_Common, 3).done());

    // Snappy wooden snare drum blending tuned membrane pop with fizzy snare noise
    b.push_back(PB("Acoustic Wood Snare", "Percussion")
        .oscA(E_Modal, 0.96f, 0.8f, 0.18f).filtA(F_LP12, 3500.f, 0.2f).ampA(0.001f, 0.22f, 0.0f, 0.08f)
        .oscB(E_Atmos, 0.75f, 0.0f, 0.8f).B(L_Level, 0.7f).filtB(F_HP12, 1200.f, 0.1f).ampB(0.001f, 0.28f, 0.0f, 0.12f)
        .fx(R_Common, 0, FX_Compressor, 0, 0.9f, { -16.f, 6.f, 5.f, 90.f, 3.5f })
        .fx(R_Common, 1, FX_Innerspace, 1, 0.35f, { 0.9f, 0.4f, 2.f })
        .rev(0.25f, 0.4f).done());

    // Razor-sharp metallic closed hi-hat shaped by filtered noise and high ring modulation
    b.push_back(PB("Crisp Studio Hi-Hat", "Percussion")
        .oscA(E_Atmos, 0.88f, 0.0f, 0.9f).filtA(F_HP12, 6500.f, 0.35f).ampA(0.001f, 0.08f, 0.0f, 0.04f)
        .oscB(E_Modal, 0.65f, 0.95f, 0.15f).B(L_Octave, 2).B(L_Level, 0.35f).filtB(F_Off, 20000.f, 0.f).ampB(0.001f, 0.06f, 0.0f, 0.03f)
        .fx(R_A, 0, FX_RingShift, 0, 0.4f, { 3200.f, 0.1f, 0.f })
        .fx(R_Common, 0, FX_EQ, 0, 1.f, { -12.f, 300.f, 1.f, 4000.f, 4.f, 12000.f })
        .noFx(R_Common, 3).done());

    // Resonant Afro-Cuban conga with expressive rim slap and skin overtones
    b.push_back(PB("Tribal Conga Slap", "Percussion")
        .oscA(E_Modal, 0.95f, 0.7f, 0.28f).filtA(F_LP12, 4200.f, 0.22f).ampA(0.001f, 0.45f, 0.0f, 0.15f)
        .oscB(E_Atmos, 0.4f, 0.2f, 0.5f).B(L_Level, 0.45f).filtB(F_LP12, 2000.f, 0.1f).ampB(0.001f, 0.08f, 0.0f, 0.04f)
        .fx(R_A, 0, FX_Rotary, 2, 0.35f, { 0.2f, 1.5f, 0.25f })
        .fx(R_Common, 0, FX_Compressor, 1, 0.8f, { -14.f, 4.f, 8.f, 110.f, 2.f })
        .rev(0.22f, 0.45f).done());
}

} // namespace op
