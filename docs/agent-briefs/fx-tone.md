# Task: implement the "Tone & Dynamics" FX group in `src/dsp/fx/Tone.h`

First read `src/dsp/fx/AGY_COMMON.md` and follow it exactly. Replace the stub in `src/dsp/fx/Tone.h`
(keep `makeToneFx(int type)` as the factory) with real implementations of these seven units. Model lists
and knob ranges are in `fxTable()` in `src/dsp/fx/FxUnit.h`.

Use RBJ "Audio EQ Cookbook" biquads (write a small `Biquad` struct: lowShelf/highShelf/peak/lowpass/
highpass/bandpass setters + transposed direct form II tick) where you need EQ curves.

## FX_EQ — knobs Low(dB), Low Freq(Hz), Mid(dB), Mid Freq(Hz), High(dB), High Freq(Hz)
Low shelf, peaking mid (Q ~0.9), high shelf. Recompute coefficients only when knobs change.
- Clean: exactly the curves above.
- Tube: same curves, plus gentle asymmetric tube saturation (amount grows with total boost) and a subtle
  low-mid warmth; slightly band-limited top.
- Pultec Trick: the low band applies BOTH a boost (shelf/peak at Low Freq) and a cut slightly above it
  (attenuation knob tied to the boost) giving the classic bump-and-dip; high band is a broad peak boost
  at High Freq with a separate gentle high-shelf cut above 10 kHz.

## FX_Compressor — knobs Threshold(dB), Ratio, Attack(ms), Release(ms), Makeup(dB), Pump Rate(Hz)
Stereo-linked feed-forward compressor with soft knee (~6 dB), log-domain gain computer, attack/release
one-pole smoothing of the gain reduction. Per model:
- VCA Bus: clean, fast, RMS detector (~10 ms window).
- Opto: program-dependent release (fast-ish initial release then slow tail; release lengthens with longer
  gain reduction), peak detector with softer knee; ratio feel ~3-4.
- Vari-Mu: ratio increases with level (soft, curving), slow, slight even-harmonic saturation.
- FET Limiter: very fast attack (Attack knob scaled down 10x), aggressive, adds odd-harmonic grit when
  driven hard; "all-buttons-in" feel when Ratio is maxed.
- Pump: ignores the input level; ducks the signal with a sidechain-style envelope triggered at Pump Rate
  (sharp duck then exponential recovery shaped by Release, depth from Threshold: -48 dB knob = deepest).

## FX_LoFi — knobs Bits(2..16), Downsample(1..64x), Noise, Filter(Hz), Crackle, Drive
- Bitcrush: quantise to Bits (with optional TPDF dither off), sample-rate reduction by holding samples
  for Downsample samples (fractional hold allowed), pre-drive, post low-pass Filter.
- Sample & Hold: like Bitcrush but the hold period is jittered randomly (gritty, aliasing glitter).
- Vinyl: band-limit (~40 Hz-Filter), Crackle = random clicks/pops with sharp transients (Poisson-distributed,
  random amplitude, short decays), Noise = surface hiss, slow wow; bits ignored (16).
- Cassette: tape hiss (Noise), Filter as the head-loss low-pass, gentle saturation (Drive), slight wow/flutter
  via a short modulated delay, mild bit depth loss only if Bits < 12.
- AM Radio: band-pass ~300 Hz-Filter (narrow), heavy compression/saturation, Noise = static, optional
  occasional fading.

## FX_RingShift — knobs Freq(Hz), LFO Rate, LFO Depth, Feedback, Stereo
LFO modulates Freq exponentially (±LFO Depth*2 octaves). Stereo offsets the R channel carrier frequency
(+Stereo*10%) and/or phase.
- Ring Mod: multiply by a sine carrier.
- Freq Shift Up / Freq Shift Down: single-sideband frequency shifting using a Hilbert transformer (a pair of
  allpass-filter chains with 90° phase difference — e.g. the standard 2x4 or 2x6 polyphase IIR design), then
  cos/sin quadrature oscillator, take the upper or lower sideband. Feedback feeds the shifted output back
  into the input (barber-pole spirals), soft-clipped.
- Downshift: frequency shift down by Freq plus a sub-octave component (half-wave rectify + low-pass, or a
  flip-flop divider) blended in for a dark, detuned "downshifted" texture.

## FX_Wah — knobs Sensitivity, Freq(Hz), Reso, Attack(ms), Release(ms), Range(oct)
Resonant filter whose cutoff = Freq * 2^(Range * control), control in 0..1:
- Crying Wah: classic wah — peaking band-pass (inductor-wah voicing: resonant peak Q 4..10 from Reso,
  a little low-end retained); control = envelope follower * Sensitivity (so playing louder opens it).
- Auto-Wah: same filter, envelope follower with Attack/Release, broader Q.
- Env Filter LP: resonant 2-pole low-pass (use Svf) swept by the envelope (Mu-Tron style).
- Env Filter BP: same with band-pass.
- LFO Filter: control = sine LFO with rate = 0.1 + Sensitivity*8 Hz (repurpose Sensitivity as LFO rate),
  Attack/Release smooth it.
- Talk Box: formant (vowel) filter: three parallel band-passes at vowel formants (A, E, I, O, U table: tenor
  F1-F3), control morphs through the vowels; Reso sharpens.

## FX_Tape — knobs Drive, Wow, Flutter, Hiss, Tone, Age
Signal chain: input pre-emphasis -> saturation (tanh-ish with slight hysteresis feel, e.g. a soft clipper
whose curve depends on the previous output) -> de-emphasis -> head bump (low peak ~80-100 Hz) -> modulated
delay (wow ~0.4-1 Hz smoothed random + flutter ~5-8 Hz) -> high-frequency loss low-pass (Tone, 4-18 kHz)
-> hiss (Hiss, pink-ish). Age adds dropouts (random brief level dips), more HF loss and wow.
- Studio 15ips: subtle, clean, head bump.
- Cassette: darker, more wow/flutter and hiss, narrower band.
- Broken Tape: severe: random pitch dives (tape stops briefly slowing), heavy dropouts, warble.

## FX_Amp — knobs Gain, Bass(dB), Mid(dB), Treble(dB), Presence, Level(dB)
Preamp (2x oversampled) -> tone stack -> power amp -> synthetic cabinet. No IR files: the cabinet is a
filter model (high-pass ~70-100 Hz, a few resonant peaks/notches typical of a 1x12 / 4x12 cab, steep
low-pass ~4-6 kHz). Presence = high shelf in the power amp section.
- Clean Twin: high headroom, bright, 2x12 cab. Bassman: tweed breakup, loose low end, 4x10.
- Brit Crunch: mid-forward, medium gain, 2x12 greenback-ish. Rock Stack: high gain, scooped options, 4x12.
- Hi-Gain: cascaded gain stages with tight pre-high-pass (~200 Hz before distortion), very saturated, 4x12.
- Tiny Speaker: small 3-4" speaker: band-pass 300 Hz-3.5 kHz, cone breakup distortion, lo-fi.
Loudness-compensate Gain so Level stays roughly consistent.
