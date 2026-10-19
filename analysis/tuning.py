"""Pitch-accuracy check for every engine: pYIN f0 vs. expected MIDI note."""
import glob, os, re, sys
import numpy as np, librosa, soundfile as sf

ENGINES = ["Analog/FM", "Spectral", "Granular", "Pluck", "Modal", "Vocal", "Pipe", "Bowed", "Atmos"]
d = sys.argv[1]
rows = []
for f in sorted(glob.glob(os.path.join(d, "tune_e*_n*.wav"))):
    e, n = map(int, re.findall(r"e(\d+)_n(\d+)", f)[0])
    y, sr = sf.read(f)
    m = y.mean(axis=1)
    bad = not np.all(np.isfinite(m))
    seg = m[int(0.25 * sr):int(1.4 * sr)]
    f0, vflag, _ = librosa.pyin(seg, fmin=40, fmax=2000, sr=sr, frame_length=4096)
    f0 = f0[vflag & np.isfinite(f0)]
    exp = 440 * 2 ** ((n - 69) / 12)
    if len(f0):
        med = float(np.median(f0)); cents = 1200 * np.log2(med / exp)
    else:
        med, cents = float("nan"), float("nan")
    rows.append((ENGINES[e], n, exp, med, cents, float(np.sqrt(np.mean(seg ** 2))), float(np.abs(m).max()), bad, len(f0)))
print(f"{'engine':10} {'note':>4} {'exp Hz':>8} {'f0 Hz':>8} {'cents':>7} {'rms':>6} {'peak':>6} voiced")
for r in rows:
    print(f"{r[0]:10} {r[1]:4d} {r[2]:8.1f} {r[3]:8.1f} {r[4]:7.1f} {r[5]:6.3f} {r[6]:6.3f} {r[8]:4d}{'  NaN!' if r[7] else ''}")
