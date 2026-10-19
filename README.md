# Panaudium: a sample-free, Omnisphere-style layered synth (demo)

**Build:** `cmake -S . -B build -G "Visual Studio 17 2022" -A x64 && cmake --build build --config Release`
→ `build/Panaudium_artefacts/Release/VST3/Panaudium.vst3` (about 5 MB) and a Standalone exe.
Renderer only: `-DPANAUDIUM_BUILD_PLUGIN=OFF`. Then run `panaudium_render <dir>` (presets), `--tuning`, or `--probe`.

**License:** AGPL-3.0 (see `LICENSE`), which is compatible with the JUCE 8 dependency. CMake uses a local `./JUCE` checkout if one exists and otherwise fetches JUCE 8.0.8.

## What reverse engineering showed (Omnisphere.vst3, Ghidra via REA, plus strings and RTTI)
- Engine classes: `RhLayeredSynthEngine`, `RhLayeredNote`, `SynthOsc`/`SynthOscKernel`, `SampledWaveOsc`, `CWavetableFPOsc`, `NoiseOsc`, `AtmoSynth`.
- Each layer stores this parameter block: OscType, Timbre, Shape, Symmetry, FM (depth/freq/shape/sync), Ring mod, Granular (depth, intensity, spread, pitch, position), Unison (depth, detune, spread, drift), a dual filter (F1/F2, 30+ named models, plus Formant, Metal Pipe and Resonator), an envelope for amp/filter/pitch, 8 LFOs, a mod matrix, and Orb/Flex mods.
- FX rack: more than 100 `Eff*` classes, including Sinevibes Matrix Reverb 8x8 (an FDN), granular reverb, amps and EQs.
- Decompiled `VowelFilter` (vtable 0x18095b068, process routine FUN_1805ddc10): the vowel morph indexes 128-step formant tables, scaled by size (0.75 to 1.25). It feeds 5 two-pole resonators per channel, bracketed by Butterworth HP/LP filters, with LCG-noise dither (0x19660d) and a soft-clip at the output. Panaudium's Vocal engine copies this structure.

## Panaudium structure
- 2 layers, 16 voices, and 9 synthetic engines: Analog/FM/sync, additive Spectral, Granular, Karplus-Strong pluck, Modal (morphs bar → bell → membrane), glottal+formant Vocal, STK-style flute, STK-style bowed string, and Atmos noise-comb.
- Signal path: unison with drift → drive → filters (SVF, Formant, Comb) → ADSRs and 2 LFOs → chorus, ping-pong delay, 8×8 Hadamard FDN reverb.
- 35 factory presets live in `src/dsp/Presets.h`. Run `panaudium_render renders/presets` to render them; WAVs are not committed.

## Status and known issues
- **Pitch checks:** `analysis/tuning.py` uses pYIN. Every engine is in tune within pYIN's ~10-cent resolution at A2, A3, A4 and A5. The exceptions are the two stochastic engines: Granular, whose grain pitch scatter is intended, and Spectral, which is about 19 cents sharp at the default Character because of its inharmonic stretch.
- **Parameter sweeps:** `analysis/pitchsweep.py` checks a parameter grid with pYIN, and `analysis/sweep.py` compares harmonic levels.
  - **Bowed string:** in tune at every tested setting from C2 to E6. The bow-position, string-loss and bow-velocity ranges are limited to the region where the STK model stays in Helmholtz motion. Outside it, the model falls into double-slip and plays an octave high.
  - **Flute:** in tune from C2 to A6 except C3 at maximum blowing pressure with minimum breath noise, where it overblows. The jet ratio is fixed at 0.35, because it moved both tuning and register, and Shape is now a tone control. Low notes run the waveguide at a reduced internal rate, because the jet locks an octave low once the bore exceeds about 480 samples.
  - **Modal:** the bell mode set is normalised so its lowest mode sits on the played note. Before, morphing toward it pulled the patch a fourth flat. The mallet contact time is capped below half a period, so high notes keep their level.
  - **Vocal:** F1 is raised above f0 on high notes, the way sopranos tune their formants.
- **Not done yet:** a diversity analysis (MFCC distances), loudness normalisation across presets, a custom GUI, and testing in a DAW.