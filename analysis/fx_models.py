"""Check that each FX unit's models sound different: compares the --fxtest default-knob renders
(fx_<type>_<model>.wav) per unit with log-mel spectrogram + modulation features.

usage: python analysis/fx_models.py renders/fxtest [type ...]
"""
import glob, os, re, sys, itertools
import numpy as np, soundfile as sf, librosa

d = sys.argv[1]
only = {int(t) for t in sys.argv[2:]}
files = {}
for f in glob.glob(os.path.join(d, "fx_*_*.wav")):
    t, m = map(int, re.findall(r"fx_(\d+)_(\d+)\.wav", os.path.basename(f))[0])
    if not only or t in only:
        files.setdefault(t, {})[m] = f

def feat(path):
    y, sr = sf.read(path)
    m = librosa.resample(y.mean(axis=1), orig_sr=sr, target_sr=22050)
    S = librosa.power_to_db(librosa.feature.melspectrogram(y=m, sr=22050, n_mels=48), ref=1.0)
    side = (y[:, 0] - y[:, 1]); width = 10 * np.log10((side ** 2).mean() + 1e-12) - 10 * np.log10((y.mean(axis=1) ** 2).mean() + 1e-12)
    env = S.mean(axis=0)
    return np.concatenate([S.mean(axis=1), S.std(axis=1) * 0.5, [np.std(np.diff(env)) * 2, width * 0.3]])

worst = []
for t in sorted(files):
    ms = files[t]
    fs = {m: feat(p) for m, p in ms.items()}
    pairs = [(a, b, float(np.sqrt(np.mean((fs[a] - fs[b]) ** 2)))) for a, b in itertools.combinations(sorted(fs), 2)]
    if not pairs:
        continue
    lo = min(pairs, key=lambda x: x[2])
    print(f"type {t:2d}: {len(ms)} models, min pair distance {lo[2]:5.2f} dB (models {lo[0]} vs {lo[1]}), mean {np.mean([p[2] for p in pairs]):5.2f} dB")
    worst += [(t, a, b, x) for a, b, x in pairs if x < 1.0]
print("near-identical model pairs (<1 dB):", worst if worst else "none")
