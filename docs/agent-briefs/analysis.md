# Task: preset loudness + timbral-diversity analysis scripts (Python)

Project: Panaudium, a sample-free software synth. An offline renderer writes one stereo 48 kHz
32-bit-float WAV per factory preset into a folder, plus `index.csv` with header
`file,name,category,realtime_factor` (one row per preset, `file` is the WAV filename).
Example data already exists in `renders/presets/` (35 presets). Expect ~100 later; categories
include Pad, Pluck, Mallet, Keys, Bass, Strings, Lead, Wind, Vocal, Atmos and more may be added.

Installed and allowed libraries: numpy, scipy, librosa, soundfile, matplotlib, pyloudnorm, scikit-learn.
No other dependencies. Python 3.13 on Windows.

## 1. `analysis/loudness.py`

CLI: `python analysis/loudness.py <render_dir> [--target -16] [--out analysis/out]`

For each WAV in index.csv:
- integrated loudness (ITU-R BS.1770 via `pyloudnorm.Meter(rate)`), LUFS;
- max short-term loudness (3 s window, 0.1 s hop, BS.1770 K-weighting via pyloudnorm's filters, or
  compute with pyloudnorm on sliding windows);
- sample peak dBFS and approximate true peak (4x oversampling with `scipy.signal.resample_poly`);
- suggested trim in dB = target - integrated, clamped to [-18, +18].

Write `<out>/loudness.csv` (columns: file,name,category,lufs_i,lufs_st_max,peak_db,true_peak_db,trim_db)
and `<out>/trims.json` = `{ "<preset name>": trim_db, ... }` with trim rounded to 0.1 dB.
Print a table sorted by lufs_i plus the spread (max-min LUFS, and std dev).
Handle silent/near-silent files without crashing (report -inf / trim clamped).

## 2. `analysis/diversity.py`

CLI: `python analysis/diversity.py <render_dir> [--out analysis/out]`

Goal: quantify how different the presets sound from each other and flag near-duplicates.

Per preset, compute a feature vector from the mono mix (and stereo where noted), using only the
non-silent part (trim leading/trailing frames below -60 dBFS):
- MFCC (20 coeffs, skip c0): mean and std over time (40 dims);
- spectral centroid, bandwidth, rolloff(0.85), flatness, contrast (7 bands): mean + std, centroid in log2(Hz);
- zero-crossing rate mean;
- onset strength mean and onset rate (onsets/sec) from librosa;
- RMS envelope: attack time (time from -40 dB below peak to within 3 dB of peak on the first note),
  and modulation depth (std of RMS in dB over the sustained part);
- harmonicity: ratio of harmonic to total energy from `librosa.effects.hpss`;
- pitch salience: fraction of frames voiced by `librosa.pyin` (fmin 40, fmax 2000; downsample to
  22050 Hz first for speed);
- stereo width: 1 - |corr(L,R)| and side/mid energy ratio in dB.

Then:
- z-score each feature column (sklearn StandardScaler); group weights so the 40 MFCC dims together
  count as much as ~4 other features (divide MFCC block by sqrt(10));
- pairwise Euclidean distance matrix; for each preset its nearest neighbour and distance;
- flag "near-duplicates": pairs whose distance is below the 5th percentile of all pairwise distances;
- a diversity score: mean pairwise distance, and the "coverage" = mean nearest-neighbour distance;
- per-category mean intra-category distance vs inter-category distance;
- 2D map: PCA (2 components) scatter, colour by category, every point labelled with preset name
  (small font); also a clustered distance-matrix heatmap ordered by hierarchical clustering
  (scipy.cluster.hierarchy, average linkage) with preset names on axes.

Outputs in `<out>/`:
- `features.csv` (raw features per preset), `distances.csv` (matrix),
- `diversity.json` with keys: mean_pairwise, coverage, near_duplicates [[a,b,dist],...],
  nearest {name: [neighbour, dist]}, category_stats {cat: {intra, inter}},
- `diversity_map.png`, `distance_heatmap.png` (dpi 130, readable with 100 labels),
- `diversity.html`: one self-contained page (images embedded as base64 PNG) with a short summary,
  the two images, the near-duplicate table and the per-category table. Plain HTML + inline CSS.

Performance: ~100 files of ~10 s must finish in under ~5 minutes. Use `librosa.load(sr=22050, mono=False)`
for features where 48k isn't needed.

## Constraints
- Only create/modify files under `analysis/` (the two scripts; you may add `analysis/common.py`).
  Do not touch `src/`, `tools/`, CMake files, or `renders/`.
- Make both scripts robust to a different number of presets and unknown category names.
- Test both scripts on `renders/presets` and make sure they run end to end without errors.

## Report back
1. Files created. 2. The printed loudness table summary (spread) and the top-5 near-duplicate pairs from
the real run. 3. Any caveats.
