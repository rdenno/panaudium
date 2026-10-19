"""Like sweep.py but scores with pYIN f0 (cents error vs. the played note) — catches 12th/octave regime jumps."""
import itertools, subprocess, sys, os
import numpy as np, soundfile as sf, librosa

exe, outdir, engine = sys.argv[1], sys.argv[2], sys.argv[3]
notes = [int(x) for x in sys.argv[4].split(",")]
grid = [[float(v) for v in a.split(",")] for a in sys.argv[5:8]]  # timbre shape char
os.makedirs(outdir, exist_ok=True)

def cents(path, n):
    y, sr = sf.read(path); m = y.mean(axis=1)[int(0.4 * sr):int(1.4 * sr)]
    f0, v, _ = librosa.pyin(m, fmin=40, fmax=2500, sr=sr, frame_length=4096)
    f0 = f0[v & np.isfinite(f0)]
    return 1200 * np.log2(np.median(f0) / (440 * 2 ** ((n - 69) / 12))) if len(f0) else float("nan")

bad = 0
for t, s, c in itertools.product(*grid):
    res = []
    for n in notes:
        subprocess.run([exe, outdir, "--probe", engine, str(n), str(t), str(s), str(c)], check=True)
        res.append(cents(os.path.join(outdir, f"probe_n{n}.wav"), n))
    ok = all(abs(r) < 50 for r in res)
    bad += not ok
    print(f"t={t:.2f} s={s:.2f} c={c:.2f} | " + " ".join(f"{n}:{r:+6.0f}c" for n, r in zip(notes, res)) + ("  OK" if ok else "  <-- off"))
print("failing combos:", bad)
