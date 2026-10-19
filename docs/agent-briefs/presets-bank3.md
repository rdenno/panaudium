# Task: write factory bank 3 — "Instruments, Keys, Bass & Percussion" (40 presets)

Read `src/dsp/presets/AGY_PRESETS_COMMON.md` first and follow it. Edit only `src/dsp/presets/Bank3.h`
(function `addBank3`). Write 40 new presets spread over these categories (use exactly these category
strings): "Keys" (6), "Bass" (7), "Lead" (6), "Pluck" (5), "Mallet" (4), "World" (5), "Brass" (3),
"Percussion" (4).

Ideas to cover (go beyond these; every preset must be clearly distinct from each other and from bank 1):
- Keys: Rhodes-like FM electric piano through tremolo, Wurlitzer-ish (FM + tube amp), clavinet (pluck,
  short, bright + wah), tonewheel organ with Rotary (Leslie), toy piano (modal, hard mallet, Tiny Speaker
  amp), harpsichord (pluck with pick near the end, bright), felt/soft piano (spectral stretch + modal).
- Bass: 808-style sine bass with drive, reese bass (detuned saws + phaser/flanger), acoustic upright
  (pluck + bowed body), FM slap bass, acid squelch (VA + resonant LP + env + Stompbox), sub + growl hybrid,
  fretless (bowed/pluck with glide G_Glide).
- Lead: screaming sync lead through Amp Hi-Gain, whistling lead (pipe, high pressure), chiptune square (Lo-Fi
  bitcrush), vocal lead with formant, singing bowed lead with vibrato, soft sine lead with tape delay.
- Pluck: banjo-like (pluck + comb + bright), muted guitar through Amp Clean Twin, harp glissando-friendly harp,
  pizzicato strings, glassy pluck (modal + pluck layers).
- Mallet: xylophone (bar, hard), glockenspiel (bar, high, very hard), steel pan, hang drum.
- World: sitar with sympathetic drone (pluck + comb + Innerspace Piano Lid), koto, shakuhachi breathy vibrato
  variant (different from bank 1), didgeridoo (vocal giant tract + pipe), bagpipe drone (Spectral hollow +
  stompbox), gamelan (modal bell set, beating).
- Brass: synth brass stab (VA saws, filter env), muted trumpet-ish (pipe + formant), french horn pad.
- Percussion (pitched/unpitched one-shots — note length short, amp decay sets length): kick drum (modal
  membrane low with pitch... use L_FiltEnv / short pluck sub), snare (atmos noise + membrane modal), closed
  hi-hat (atmos bright noise + ring mod, very short), tom/conga (modal membrane, medium).
