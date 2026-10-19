"""Sweep engine params through the --probe renderer; report which harmonic dominates (detects octave-jump regimes)."""
import itertools, subprocess, sys, os
import numpy as np, soundfile as sf

exe, outdir, engine = sys.argv[1], sys.argv[2], sys.argv[3]
notes = [int(x) for x in sys.argv[4].split(",")]
grid = [[float(v) for v in a.split(",")] for a in sys.argv[5:8]]  # timbre shape char
os.makedirs(outdir, exist_ok=True)

def harm(path, n):
    y, sr = sf.read(path); m = y.mean(axis=1)[int(0.5 * sr):int(1.4 * sr)]
    f0 = 440 * 2 ** ((n - 69) / 12)
    S = np.abs(np.fft.rfft(m * np.hanning(len(m)))); fr = np.fft.rfftfreq(len(m), 1 / sr)
    a = lambda h: S[(fr > f0 * h * 0.97) & (fr < f0 * h * 1.03)].max() + 1e-9
    return 20 * np.log10(a(1) / max(a(0.5), a(2), a(1.5))), float(np.sqrt(np.mean(m ** 2)))

for t, s, c in itertools.product(*grid):
    res = []
    for n in notes:
        subprocess.run([exe, outdir, "--probe", engine, str(n), str(t), str(s), str(c)], check=True)
        res.append(harm(os.path.join(outdir, f"probe_n{n}.wav"), n))
    ok = all(r[0] > 0 for r in res)
    print(f"t={t:.2f} s={s:.2f} c={c:.2f} | " + " ".join(f"{n}:{r[0]:+5.1f}dB/{r[1]:.2f}" for n, r in zip(notes, res)) + ("  OK" if ok else ""))
