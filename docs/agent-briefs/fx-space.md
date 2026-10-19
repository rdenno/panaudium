# Task: implement the "Space" FX group in `src/dsp/fx/Space.h`

First read `src/dsp/fx/AGY_COMMON.md` and follow it exactly. Replace the stub in `src/dsp/fx/Space.h`
(keep `makeSpaceFx(int type)` as the factory) with real implementations of these four units. Model lists
and knob ranges are in `fxTable()` in `src/dsp/fx/FxUnit.h`.

## FX_Delay — knobs Time(s), Feedback, Tone(Hz, low-pass in the feedback path), Wow, Spread, Ducking
Max time 2 s (allocate for 192 kHz). Smooth Time changes (tape-style pitch glide for Tape Echo, a fast
crossfade between old/new read positions for Digital so it doesn't zipper). Feedback <= ~1.0 with fastTanh
in the loop so "maxed" self-oscillates musically but never explodes. Ducking: reduce the wet level while the
input is loud (envelope follower on the input, ~10 ms attack / 250 ms release).
- Digital: clean stereo delay (L and R times equal, Spread offsets R by up to +25%).
- Tape Echo: saturation in the loop, Wow (slow ~0.5 Hz) and flutter (~6 Hz, random) delay modulation,
  progressively darker repeats, a little hiss.
- Ping-Pong: mono-summed input into L, repeats alternate L <-> R (cross-feedback); Spread = width.
- Dual Tap: two taps at Time and Time*0.75 (dotted feel) panned by Spread, shared feedback.
- Diffuse: echoes smeared through a short allpass diffuser chain inside the loop so repeats turn into a wash.

## FX_ProVerb — knobs Size, Decay(s), Predelay(ms), Diffusion, Lows(dB), Highs(dB)
Build on the 8-line Hadamard FDN in `src/dsp/Fx.h` (input diffusion allpasses, modulated lines, per-line
damping, decay gains computed from the RT60 = Decay knob per line length). Lows/Highs are shelving EQs inside
the loop (low-shelf ~250 Hz, high-shelf ~4 kHz) so decay time is frequency dependent. Diffusion controls the
input allpass coefficients. Stereo output: decorrelated taps for L/R. Models differ by line-length set,
diffusion, modulation and early reflections:
- Room: short lines, strong early reflections (a sparse tapped delay), little modulation.
- Hall: medium/long lines, moderate modulation, smooth.
- Plate: very dense from the start (high input diffusion, no distinct early reflections), bright.
- Cathedral: longest lines, slow build-up, extra low-mid warmth, decay scaled up x1.5.
- Dark Space: strong high damping, deep slow modulation (chorused tail).
- Bloom: input passes through a swell (the tail level rises over ~0.5-1 s, e.g. a second FDN fed by the first
  or an envelope that fades the late reverb in) — ambient, "blooming".

## FX_Innerspace — knobs Size(x stretch), Decay, Tone(dB tilt), Predelay(ms), Width
Convolution reverb whose impulse responses are SYNTHESIZED in code when the model (or Size/Decay) changes —
the sample-free answer to Omnisphere's Innerspace. Implement uniformly partitioned FFT convolution
(block size 256 or 512, overlap-save or overlap-add) with your own iterative radix-2 complex FFT
(precompute twiddles/bit-reversal in prepare). Max IR length 4 s at the current sample rate (allocate all
partitions in prepare for that worst case). Generate separate L/R IRs (decorrelated by Width).
IR regeneration must not allocate: write into preallocated buffers. Regenerating on the audio thread is
allowed but must be throttled: only when model/Size/Decay change by more than a small threshold, at most
every ~100 ms, and crossfade old->new output over one block to avoid clicks (keeping two spectra sets is fine).
Synthesize each IR (then scale by Size = time stretch and Decay = envelope slope, normalise energy):
- Coin Dropper: sparse, accelerating metallic clicks (each click = a few decaying high inharmonic partials),
  bouncing-coin timing (intervals shrinking geometrically).
- Spring Tank: dispersive chirps — repeated echoes (~30-40 ms apart) of a downward chirp (allpass-dispersed
  impulse), each echo darker; the boingy spring sound.
- Metal Box: dense modal ring — sum of ~40 decaying sinusoids at inharmonic box-mode frequencies 200 Hz-6 kHz.
- Glass Jar: a few strong, long, high-Q resonances (~500 Hz-3 kHz) plus a short bright noise burst.
- Piano Lid: sympathetic strings — decaying partials at the harmonic series of several notes (e.g. 12 pitches)
  with slow beating, very long decay, soft attack.
- Cave Drip: long dark diffuse noise tail plus occasional random high "drips" (short pitched blips) inside it.
- Paper Tube: tube resonance — comb-like harmonic series around ~150 Hz with fast HF damping, short (~0.5 s).
- Gong Wash: dense inharmonic shimmer that swells (energy builds over ~0.3 s then decays slowly).

## FX_Shimmer — knobs Feedback, Shift Mix, Size, Decay(s), Tone(Hz), Detune(cents)
A reverb (smaller FDN or reuse the ProVerb structure) with a pitch shifter in its feedback path: reverb
output -> pitch shift -> scaled by Feedback -> back into the reverb input, so each pass climbs in pitch.
Pitch shifter: granular/delay-line shifter with two (or more) crossfaded taps (window ~50-80 ms), Detune adds
small opposite detunes to a pair of shifters for width. Shift Mix blends shifted vs unshifted feedback.
Tone = low-pass in the loop. Keep it stable at maxed knobs (fastTanh + gain < 1 in the loop).
- Octave Up: +12 semitones. Fifth Up: +7. Octave Down: -12. Detune Doubler: ±Detune only (no transposition;
  a lush chorused reverb). Crystal: +12 and +19 (octave + fifth) shifters in parallel.
