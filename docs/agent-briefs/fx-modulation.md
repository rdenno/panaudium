# Task: implement the "Modulation & Drive" FX group in `src/dsp/fx/Modulation.h`

First read `src/dsp/fx/AGY_COMMON.md` and follow it exactly. Replace the stub in `src/dsp/fx/Modulation.h`
(keep `makeModulationFx(int type)` as the factory) with real implementations of these six units. Model lists
and knob ranges are in `fxTable()` in `src/dsp/fx/FxUnit.h`.

## FX_Stompbox — knobs Drive, Tone, Level(dB), Bias
Oversample the nonlinearity 2x (simple polyphase halfband or linear-interp up + 2-pole lowpass down is fine)
to limit aliasing. Per model:
- Tube Screamer: input high-pass ~720 Hz feeding a soft symmetric clipper (diode pair ≈ tanh) blended back with
  the clean low end (the "mid hump"), gain up to ~+40 dB, tone = post low-pass 700 Hz..6 kHz.
- Fuzz Face: germanium-style asymmetric hard-ish clipping, gain up to ~+50 dB, slightly sputtery: bias knob
  shifts the operating point (gated, broken-up fuzz at high bias); tone = gentle low-pass.
- Rat: op-amp gain up to ~+60 dB with gain-dependent bandwidth (low-pass that falls as drive rises), hard
  clipping, "filter" tone control = low-pass 600 Hz..15 kHz.
- Big Muff: two cascaded soft clipping stages, then the famous tone stack: crossfade between low-pass (~1 kHz)
  and high-pass (~1 kHz) paths, giving a mid scoop at noon.
- Blues Driver: mild asymmetric clipping with light compression, smoother/more transparent; tone = treble shelf.
- Octave Fuzz: full-wave rectification (|x|, an octave up) blended with fuzz; Bias controls the rectifier mix.
Level is output gain in dB after a rough loudness compensation so Drive doesn't massively change volume.

## FX_Phaser — knobs Rate(Hz), Color(feedback), Depth, Center(Hz), Stereo(LFO phase offset 0..180°)
Cascaded first-order allpasses with an LFO-swept break frequency (log sweep around Center, ±Depth*2 octaves),
feedback from the last stage to the input (Color, keep < 0.95, sign per model).
- Phase 90: 4 stages, sine-ish LFO, modest feedback.
- Small Stone: 4 stages but stronger feedback ("color" switch character), slightly faster sweep.
- Uni-Vibe: 4 stages with deliberately staggered (unequal) break frequencies, a skewed, lamp-like LFO shape
  (fast rise, slow fall), mixed with a little dry inside the unit for the throb (it's a "chorale").
- Twelve Stage: 12 stages, deeper notches, slower and smoother.
- Barber Pole: endless rising sweep — two or three phaser chains with sawtooth LFOs offset in phase and
  crossfaded with raised-cosine windows so the sweep seems to rise forever.

## FX_Chorus — knobs Rate, Depth, Width, Delay(ms), Tone
Modulated fractional delay lines (use DelayLine.read with linear interpolation or better, e.g. cubic).
- CE-2: single voice, triangle LFO, warm (low-pass on the wet ~ Tone), mono-ish (Width spreads).
- Dimension: two voices with LFOs in antiphase, very subtle depth, wide stereo, almost no pitch wobble.
- Solina Ensemble: three delay lines modulated by three LFOs 120° apart, each LFO = slow (~0.6 Hz) + fast
  (~6 Hz) components (Rate scales both) — the classic string-machine ensemble.
- Juno: two voices, triangle LFOs in quadrature, "mode II" style faster rate, slightly noisy/BBD darker.
- Tri-Chorus: three voices panned left/centre/right with independent random-ish (smoothed noise) modulation.

## FX_Flanger — knobs Rate, Depth, Feedback(-0.95..0.95), Manual(ms), Stereo
Short delay around Manual swept by the LFO, feedback (sign from the knob), mix dry+delayed inside the unit
(a flanger IS dry+delay, so output the summed comb result as the wet signal).
- Jet: classic positive feedback, triangle LFO, sweeping 0.3..12 ms.
- Through-Zero: two delay lines (one fixed at Manual, one swept through it) so the comb passes through zero
  delay — the tape-flange cancellation effect.
- Barber Pole: endless sweep like the phaser version (crossfaded sawtooth-swept lines).
- Metallic: very short delays (0.3..2 ms), high feedback, resonant/robotic.

## FX_Tremolo — knobs Rate, Depth, Shape, Stereo(deg)
- Sine Trem: amplitude LFO, Shape morphs sine -> triangle-ish.
- Square Trem: LFO square with Shape controlling edge smoothing (avoid clicks: min ~2 ms slew).
- Harmonic Trem: split signal at ~800 Hz with a crossover; modulate lows and highs in antiphase (Fender "harmonic vibrato").
- Auto-Pan: equal-power pan between channels driven by the LFO; Stereo knob unused/limits width as you see fit.
- Stutter Gate: rhythmic gate pattern (e.g. 16-step pattern derived from Shape, at Rate steps/s) with short
  attack/release ramps.

## FX_Rotary — knobs Speed (0 = slow/chorale, 1 = fast/tremolo, continuous), Accel(s), Drive, Horn/Drum, Mic Dist
Leslie model: crossover at ~800 Hz; horn (highs) and drum (lows) each rotate at their own speed (horn slow
~0.8 Hz / fast ~6.7 Hz; drum slow ~0.7 Hz / fast ~5.7 Hz), with inertia: rotation speed slews to the target
with time constant Accel (drum slower than horn). Each rotor: Doppler (modulated short delay), amplitude
modulation and a little filtering vs angle; two virtual mics (L/R at ±90°, Mic Dist reduces modulation depth
and adds a bit of room). Pre-drive = tube-ish soft clipping by Drive. Models: Leslie 122 (classic), Leslie 147
(slightly brighter/louder horn), Vibratone (single rotor, no horn/drum split, guitar-cab voiced band-pass ~250 Hz-4 kHz).
