#!/usr/bin/env python3
"""
analysis/diversity.py - Preset timbral diversity analysis and near-duplicate detection.

Features:
- Loads presets from <render_dir>/index.csv
- Audio pre-processing: non-silent trimming (frames below -60 dBFS)
- Computes comprehensive 69-dimensional feature vector per preset:
  * MFCC (20 coeffs, skipping c0): mean & std over time (40 dims)
  * Spectral centroid (log2 Hz), bandwidth, rolloff (0.85), flatness, contrast (7 bands): mean & std
  * Zero-crossing rate: mean
  * Onset strength mean & onset rate (onsets/sec)
  * RMS envelope: attack time (-40 dB below peak to within 3 dB of peak) & modulation depth
  * Harmonicity: ratio of harmonic to total energy from librosa.effects.hpss
  * Pitch salience: fraction of frames voiced by librosa.pyin
  * Stereo width: 1 - |corr(L,R)| & side/mid energy ratio in dB
- Z-score normalization (StandardScaler) with MFCC block scaled by 1/sqrt(10)
- Computes pairwise Euclidean distance matrix & nearest neighbors
- Flags near-duplicates (distance < 5th percentile)
- Diversity metrics: mean pairwise distance (diversity score) & mean nearest-neighbor distance (coverage)
- Per-category intra- vs. inter-category distance statistics
- 2D PCA projection map (diversity_map.png) & clustered distance heatmap (distance_heatmap.png)
- Outputs features.csv, distances.csv, diversity.json, diversity_map.png, distance_heatmap.png, diversity.html
"""

import argparse
import base64
import csv
import json
import os
import sys
import warnings
from typing import Any, Dict, List, Tuple

import numpy as np

# Use Agg backend for headless / non-GUI environments
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

import librosa
from scipy.cluster.hierarchy import leaves_list, linkage
from scipy.spatial.distance import pdist, squareform
from sklearn.decomposition import PCA
from sklearn.preprocessing import StandardScaler
import soundfile as sf

# Allow imports from the same directory
script_dir = os.path.dirname(os.path.abspath(__file__))
if script_dir not in sys.path:
    sys.path.insert(0, script_dir)

import common


def trim_to_nonsilent(
    y_stereo: np.ndarray,
    y_mono: np.ndarray,
    sr: int,
    threshold_db: float = -60.0,
    frame_length: int = 2048,
    hop_length: int = 512,
) -> Tuple[np.ndarray, np.ndarray]:
    """
    Trim leading and trailing frames where mono RMS is below threshold_db (dBFS).
    Returns trimmed (stereo, mono) audio arrays.
    """
    if len(y_mono) == 0:
        return y_stereo, y_mono

    rms = librosa.feature.rms(y=y_mono, frame_length=frame_length, hop_length=hop_length)[0]
    rms_db = 20.0 * np.log10(np.maximum(rms, 1e-9))
    active_frames = np.where(rms_db >= threshold_db)[0]

    if len(active_frames) == 0:
        # If entire file is below threshold, return original
        return y_stereo, y_mono

    start_frame = active_frames[0]
    end_frame = active_frames[-1]

    start_sample = start_frame * hop_length
    end_sample = min(len(y_mono), (end_frame + 1) * hop_length)

    # Ensure trimmed audio is at least 1 frame length
    if end_sample - start_sample < frame_length:
        return y_stereo, y_mono

    if y_stereo.ndim == 2:
        y_stereo_trimmed = y_stereo[:, start_sample:end_sample]
    else:
        y_stereo_trimmed = y_stereo[start_sample:end_sample]
    y_mono_trimmed = y_mono[start_sample:end_sample]

    return y_stereo_trimmed, y_mono_trimmed


def extract_preset_features(wav_path: str) -> Dict[str, float]:
    """
    Extract the full set of 69 audio features for a single preset WAV file.
    """
    features: Dict[str, float] = {}

    # Read audio with soundfile at native sample rate (48 kHz)
    try:
        data, sr = sf.read(wav_path, dtype="float32")
    except Exception as e:
        warnings.warn(f"Failed to read {wav_path}: {e}")
        return {}

    # Prepare stereo and mono arrays
    if data.ndim == 1:
        y_stereo = np.stack([data, data])
        y_mono = data
    else:
        # soundfile returns (samples, channels), librosa expects (channels, samples)
        y_stereo = data.T
        y_mono = np.mean(y_stereo, axis=0)

    # 1. Trim leading and trailing silence below -60 dBFS
    y_stereo, y_mono = trim_to_nonsilent(y_stereo, y_mono, sr, threshold_db=-60.0)

    # Defensive check: ensure audio length is sufficient for STFT
    if len(y_mono) < 2048:
        pad_len = 2048 - len(y_mono)
        y_mono = np.pad(y_mono, (0, pad_len))
        if y_stereo.ndim == 2:
            y_stereo = np.pad(y_stereo, ((0, 0), (0, pad_len)))
        else:
            y_stereo = np.pad(y_stereo, (0, pad_len))

    # 2. MFCC (20 coeffs, skip c0): mean and std over time (40 dims)
    # Compute 21 coeffs to get c0 .. c20, then slice [1:21] to skip c0
    try:
        mfcc = librosa.feature.mfcc(y=y_mono, sr=sr, n_mfcc=21)
        mfcc_coeffs = mfcc[1:21, :]  # shape: (20, T)
        mfcc_mean = np.mean(mfcc_coeffs, axis=1)
        mfcc_std = np.std(mfcc_coeffs, axis=1)
    except Exception:
        mfcc_mean = np.zeros(20)
        mfcc_std = np.zeros(20)

    for i in range(20):
        features[f"mfcc_{i+1}_mean"] = float(mfcc_mean[i])
        features[f"mfcc_{i+1}_std"] = float(mfcc_std[i])

    # 3. Spectral features:
    # Spectral centroid (in log2 Hz), bandwidth, rolloff (0.85), flatness, contrast (7 bands)
    try:
        cent = librosa.feature.spectral_centroid(y=y_mono, sr=sr)[0]
        cent_log2 = np.log2(np.maximum(cent, 1.0))
        features["spec_centroid_mean"] = float(np.mean(cent_log2))
        features["spec_centroid_std"] = float(np.std(cent_log2))
    except Exception:
        features["spec_centroid_mean"] = 0.0
        features["spec_centroid_std"] = 0.0

    try:
        bw = librosa.feature.spectral_bandwidth(y=y_mono, sr=sr)[0]
        features["spec_bandwidth_mean"] = float(np.mean(bw))
        features["spec_bandwidth_std"] = float(np.std(bw))
    except Exception:
        features["spec_bandwidth_mean"] = 0.0
        features["spec_bandwidth_std"] = 0.0

    try:
        roll = librosa.feature.spectral_rolloff(y=y_mono, sr=sr, roll_percent=0.85)[0]
        features["spec_rolloff_mean"] = float(np.mean(roll))
        features["spec_rolloff_std"] = float(np.std(roll))
    except Exception:
        features["spec_rolloff_mean"] = 0.0
        features["spec_rolloff_std"] = 0.0

    try:
        flat = librosa.feature.spectral_flatness(y=y_mono)[0]
        features["spec_flatness_mean"] = float(np.mean(flat))
        features["spec_flatness_std"] = float(np.std(flat))
    except Exception:
        features["spec_flatness_mean"] = 0.0
        features["spec_flatness_std"] = 0.0

    # Spectral contrast: 7 bands (n_bands=6 returns 7 bands)
    # Ensure fmin * 2^6 <= Nyquist
    nyquist = sr / 2.0
    n_bands = 6
    fmin = 200.0
    if fmin * (2 ** n_bands) > nyquist:
        fmin = (nyquist / (2 ** n_bands)) * 0.95

    try:
        contrast = librosa.feature.spectral_contrast(y=y_mono, sr=sr, n_bands=n_bands, fmin=fmin)
        contrast_mean = np.mean(contrast, axis=1)
        contrast_std = np.std(contrast, axis=1)
        for b in range(7):
            features[f"spec_contrast_b{b}_mean"] = float(contrast_mean[b])
            features[f"spec_contrast_b{b}_std"] = float(contrast_std[b])
    except Exception:
        for b in range(7):
            features[f"spec_contrast_b{b}_mean"] = 0.0
            features[f"spec_contrast_b{b}_std"] = 0.0

    # 4. Zero-crossing rate mean
    try:
        zcr = librosa.feature.zero_crossing_rate(y_mono)[0]
        features["zcr_mean"] = float(np.mean(zcr))
    except Exception:
        features["zcr_mean"] = 0.0

    # 5. Onset features: onset strength mean and onset rate (onsets/sec)
    try:
        onset_env = librosa.onset.onset_strength(y=y_mono, sr=sr)
        features["onset_strength_mean"] = float(np.mean(onset_env))
        onset_frames = librosa.onset.onset_detect(onset_envelope=onset_env, sr=sr)
        duration_sec = len(y_mono) / float(sr)
        features["onset_rate"] = float(len(onset_frames) / duration_sec) if duration_sec > 0 else 0.0
    except Exception:
        features["onset_strength_mean"] = 0.0
        features["onset_rate"] = 0.0

    # 6. RMS envelope: attack time and modulation depth
    try:
        rms_hop = 512
        rms_fl = 2048
        rms = librosa.feature.rms(y=y_mono, frame_length=rms_fl, hop_length=rms_hop)[0]
        peak_rms = float(np.max(rms)) if len(rms) > 0 else 0.0

        if peak_rms <= 1e-9:
            attack_time = 0.0
            mod_depth = 0.0
        else:
            rms_db = 20.0 * np.log10(np.maximum(rms, 1e-9))
            peak_db = 20.0 * np.log10(peak_rms)
            t_start_thresh = peak_db - 40.0
            t_end_thresh = peak_db - 3.0

            idx_start = np.where(rms_db >= t_start_thresh)[0]
            if len(idx_start) == 0:
                i_start = 0
                i_end = int(np.argmax(rms_db))
            else:
                i_start = idx_start[0]
                idx_end = np.where(rms_db[i_start:] >= t_end_thresh)[0]
                if len(idx_end) == 0:
                    i_end = int(np.argmax(rms_db))
                else:
                    i_end = i_start + idx_end[0]

            attack_time = max(0.0, float((i_end - i_start) * rms_hop / float(sr)))

            # Sustained part: from i_end onwards, considering active portion
            sustain_frames = rms_db[i_end:]
            active_sustain = sustain_frames[sustain_frames >= max(peak_db - 20.0, -60.0)]
            if len(active_sustain) >= 2:
                mod_depth = float(np.std(active_sustain))
            elif len(sustain_frames) >= 2:
                mod_depth = float(np.std(sustain_frames))
            else:
                mod_depth = 0.0

        features["attack_time"] = attack_time
        features["modulation_depth"] = mod_depth
    except Exception:
        features["attack_time"] = 0.0
        features["modulation_depth"] = 0.0

    # 7. Harmonicity: ratio of harmonic to total energy from librosa.effects.hpss
    try:
        y_harm, y_perc = librosa.effects.hpss(y_mono)
        harm_energy = float(np.sum(y_harm ** 2))
        tot_energy = float(np.sum(y_mono ** 2))
        harmonicity = float(np.clip(harm_energy / tot_energy if tot_energy > 1e-12 else 0.0, 0.0, 1.0))
        features["harmonicity"] = harmonicity
    except Exception:
        features["harmonicity"] = 0.0

    # 8. Pitch salience: fraction of frames voiced by librosa.pyin (downsample to 22050 Hz for speed)
    try:
        if sr != 22050:
            y_pyin = librosa.resample(y_mono, orig_sr=sr, target_sr=22050)
            sr_pyin = 22050
        else:
            y_pyin = y_mono
            sr_pyin = sr

        if len(y_pyin) < 2048:
            y_pyin = np.pad(y_pyin, (0, 2048 - len(y_pyin)))

        f0, voiced_flag, voiced_probs = librosa.pyin(y_pyin, fmin=40, fmax=2000, sr=sr_pyin)
        if voiced_flag is not None and len(voiced_flag) > 0:
            pitch_salience = float(np.mean(voiced_flag.astype(float)))
        else:
            pitch_salience = 0.0
        features["pitch_salience"] = pitch_salience
    except Exception:
        features["pitch_salience"] = 0.0

    # 9. Stereo width: 1 - |corr(L,R)| and side/mid energy ratio in dB
    try:
        if y_stereo.ndim == 2 and y_stereo.shape[0] >= 2:
            L = y_stereo[0]
            R = y_stereo[1]
            std_L = float(np.std(L))
            std_R = float(np.std(R))
            if std_L > 1e-9 and std_R > 1e-9:
                corr = float(np.corrcoef(L, R)[0, 1])
                if np.isnan(corr):
                    corr = 1.0
                stereo_width = float(np.clip(1.0 - np.abs(corr), 0.0, 1.0))
            else:
                stereo_width = 0.0

            M = 0.5 * (L + R)
            S = 0.5 * (L - R)
            e_m = float(np.sum(M ** 2))
            e_s = float(np.sum(S ** 2))
            ratio = (e_s + 1e-12) / (e_m + 1e-12)
            sm_ratio_db = float(np.clip(10.0 * np.log10(ratio), -60.0, 30.0))
        else:
            stereo_width = 0.0
            sm_ratio_db = -60.0

        features["stereo_width"] = stereo_width
        features["side_mid_ratio_db"] = sm_ratio_db
    except Exception:
        features["stereo_width"] = 0.0
        features["side_mid_ratio_db"] = -60.0

    return features


def generate_html_report(
    html_path: str,
    map_png_path: str,
    heatmap_png_path: str,
    presets: List[Dict[str, Any]],
    diversity_summary: Dict[str, Any],
) -> None:
    """
    Generate a self-contained HTML report with embedded base64 PNGs and styled tables.
    """
    b64_map = ""
    if os.path.isfile(map_png_path):
        with open(map_png_path, "rb") as f:
            b64_map = base64.b64encode(f.read()).decode("utf-8")

    b64_heat = ""
    if os.path.isfile(heatmap_png_path):
        with open(heatmap_png_path, "rb") as f:
            b64_heat = base64.b64encode(f.read()).decode("utf-8")

    mean_pairwise = diversity_summary["mean_pairwise"]
    coverage = diversity_summary["coverage"]
    near_duplicates = diversity_summary["near_duplicates"]
    category_stats = diversity_summary["category_stats"]

    # Category counts
    cat_counts: Dict[str, int] = {}
    preset_cat_map: Dict[str, str] = {}
    for p in presets:
        cat = p["category"]
        cat_counts[cat] = cat_counts.get(cat, 0) + 1
        preset_cat_map[p["name"]] = cat

    # Near-duplicates rows
    nd_rows_html = []
    if near_duplicates:
        for idx, (p_a, p_b, dist) in enumerate(near_duplicates, 1):
            cat_a = preset_cat_map.get(p_a, "Unknown")
            cat_b = preset_cat_map.get(p_b, "Unknown")
            match_class = "same-cat" if cat_a == cat_b else "cross-cat"
            nd_rows_html.append(
                f"<tr>"
                f"<td class='rank-cell'>{idx}</td>"
                f"<td><strong>{p_a}</strong> <span class='badge'>{cat_a}</span></td>"
                f"<td><strong>{p_b}</strong> <span class='badge'>{cat_b}</span></td>"
                f"<td class='dist-cell'>{dist:.3f}</td>"
                f"<td><span class='status-pill {match_class}'>{'Intra-Category' if cat_a == cat_b else 'Cross-Category'}</span></td>"
                f"</tr>"
            )
    else:
        nd_rows_html.append("<tr><td colspan='5' class='empty-cell'>No near-duplicates detected below threshold.</td></tr>")

    # Category stats rows
    cat_rows_html = []
    for cat, stats in sorted(category_stats.items()):
        cnt = cat_counts.get(cat, 0)
        intra = stats["intra"]
        inter = stats["inter"]
        ratio_str = f"{(intra / inter):.2f}" if inter > 0 and intra > 0 else ("N/A" if cnt <= 1 else "0.00")
        cohesion_badge = (
            "<span class='status-pill cohesion-high'>Tight Cluster</span>"
            if (cnt > 1 and inter > 0 and (intra / inter) < 0.75)
            else ("<span class='status-pill cohesion-norm'>Diverse</span>" if cnt > 1 else "<span class='status-pill cohesion-single'>Single Preset</span>")
        )

        cat_rows_html.append(
            f"<tr>"
            f"<td><strong>{cat}</strong></td>"
            f"<td class='num-cell'>{cnt}</td>"
            f"<td class='num-cell'>{intra:.3f}</td>"
            f"<td class='num-cell'>{inter:.3f}</td>"
            f"<td class='num-cell'>{ratio_str}</td>"
            f"<td>{cohesion_badge}</td>"
            f"</tr>"
        )

    html_content = f"""<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Panaudium Preset Timbral Diversity Report</title>
<style>
  :root {{
    --bg-color: #f8fafc;
    --card-bg: #ffffff;
    --text-primary: #0f172a;
    --text-secondary: #475569;
    --accent: #2563eb;
    --accent-light: #dbeafe;
    --border: #e2e8f0;
    --success: #16a34a;
    --warning: #d97706;
  }}
  * {{ box-sizing: border-box; margin: 0; padding: 0; }}
  body {{
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
    background-color: var(--bg-color);
    color: var(--text-primary);
    line-height: 1.5;
    padding: 24px;
  }}
  .container {{ max-width: 1300px; margin: 0 auto; }}
  header {{
    background: var(--card-bg);
    border-radius: 12px;
    padding: 24px 32px;
    margin-bottom: 24px;
    border: 1px solid var(--border);
    box-shadow: 0 1px 3px rgba(0,0,0,0.05);
  }}
  h1 {{ font-size: 26px; font-weight: 700; color: #1e293b; margin-bottom: 6px; }}
  .subtitle {{ color: var(--text-secondary); font-size: 14px; }}
  
  .stats-grid {{
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
    gap: 16px;
    margin-bottom: 24px;
  }}
  .stat-card {{
    background: var(--card-bg);
    border-radius: 10px;
    padding: 18px 20px;
    border: 1px solid var(--border);
    box-shadow: 0 1px 3px rgba(0,0,0,0.04);
  }}
  .stat-label {{ font-size: 12px; font-weight: 600; text-transform: uppercase; letter-spacing: 0.05em; color: var(--text-secondary); margin-bottom: 4px; }}
  .stat-value {{ font-size: 24px; font-weight: 700; color: #1e293b; }}
  .stat-desc {{ font-size: 11px; color: #64748b; margin-top: 2px; }}

  .section {{
    background: var(--card-bg);
    border-radius: 12px;
    padding: 24px 28px;
    margin-bottom: 24px;
    border: 1px solid var(--border);
    box-shadow: 0 1px 3px rgba(0,0,0,0.05);
  }}
  .section-title {{ font-size: 18px; font-weight: 600; color: #1e293b; margin-bottom: 16px; border-bottom: 1px solid var(--border); padding-bottom: 8px; }}

  .images-grid {{
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 20px;
  }}
  @media (max-width: 960px) {{
    .images-grid {{ grid-template-columns: 1fr; }}
  }}
  .image-box {{
    background: #ffffff;
    border: 1px solid var(--border);
    border-radius: 8px;
    overflow: hidden;
    text-align: center;
  }}
  .image-box img {{
    width: 100%;
    height: auto;
    display: block;
  }}
  .image-caption {{
    font-size: 12px;
    color: var(--text-secondary);
    padding: 10px 14px;
    background: #f1f5f9;
    border-top: 1px solid var(--border);
    font-weight: 500;
  }}

  table {{
    width: 100%;
    border-collapse: collapse;
    font-size: 13px;
    text-align: left;
  }}
  th {{
    background: #f8fafc;
    color: var(--text-secondary);
    font-weight: 600;
    padding: 10px 14px;
    border-bottom: 2px solid var(--border);
  }}
  td {{
    padding: 10px 14px;
    border-bottom: 1px solid var(--border);
    color: #334155;
  }}
  tr:hover td {{ background: #f8fafc; }}
  .rank-cell {{ font-weight: bold; width: 40px; color: #94a3b8; }}
  .num-cell {{ text-align: right; font-variant-numeric: tabular-nums; }}
  .dist-cell {{ text-align: right; font-weight: 600; font-family: monospace; font-size: 13px; color: #2563eb; }}
  .badge {{
    display: inline-block;
    padding: 2px 7px;
    background: #f1f5f9;
    color: #475569;
    border-radius: 4px;
    font-size: 11px;
    font-weight: 500;
    margin-left: 6px;
  }}
  .status-pill {{
    display: inline-block;
    padding: 2px 8px;
    border-radius: 12px;
    font-size: 11px;
    font-weight: 600;
  }}
  .same-cat {{ background: #fee2e2; color: #991b1b; }}
  .cross-cat {{ background: #fef3c7; color: #92400e; }}
  .cohesion-high {{ background: #dcfce7; color: #166534; }}
  .cohesion-norm {{ background: #e0f2fe; color: #075985; }}
  .cohesion-single {{ background: #f1f5f9; color: #64748b; }}
  .empty-cell {{ text-align: center; color: #94a3b8; padding: 24px; }}
  footer {{ text-align: center; font-size: 12px; color: #94a3b8; margin-top: 32px; }}
</style>
</head>
<body>
<div class="container">
  <header>
    <h1>Panaudium Synthesizer Preset Timbral Diversity Report</h1>
    <p class="subtitle">Comprehensive analysis of acoustic feature diversity, nearest neighbors, and cluster cohesion across factory presets.</p>
  </header>

  <div class="stats-grid">
    <div class="stat-card">
      <div class="stat-label">Total Presets</div>
      <div class="stat-value">{len(presets)}</div>
      <div class="stat-desc">Across {len(category_stats)} categories</div>
    </div>
    <div class="stat-card">
      <div class="stat-label">Diversity Score</div>
      <div class="stat-value">{mean_pairwise:.2f}</div>
      <div class="stat-desc">Mean pairwise Euclidean distance</div>
    </div>
    <div class="stat-card">
      <div class="stat-label">Coverage Metric</div>
      <div class="stat-value">{coverage:.2f}</div>
      <div class="stat-desc">Mean nearest-neighbor distance</div>
    </div>
    <div class="stat-card">
      <div class="stat-label">Near-Duplicates</div>
      <div class="stat-value">{len(near_duplicates)}</div>
      <div class="stat-desc">Pairs below 5th percentile distance</div>
    </div>
  </div>

  <div class="section">
    <div class="section-title">Visualizations</div>
    <div class="images-grid">
      <div class="image-box">
        <img src="data:image/png;base64,{b64_map}" alt="PCA Timbral Diversity Map">
        <div class="image-caption">PCA 2D Projection of Standardized Feature Space (Colored by Category)</div>
      </div>
      <div class="image-box">
        <img src="data:image/png;base64,{b64_heat}" alt="Clustered Distance Heatmap">
        <div class="image-caption">Hierarchical Average Linkage Clustered Distance Matrix</div>
      </div>
    </div>
  </div>

  <div class="section">
    <div class="section-title">Near-Duplicate Candidates (Distance &lt; 5th Percentile)</div>
    <table>
      <thead>
        <tr>
          <th>#</th>
          <th>Preset A</th>
          <th>Preset B</th>
          <th style="text-align:right;">Euclidean Distance</th>
          <th>Cluster Relationship</th>
        </tr>
      </thead>
      <tbody>
        {"".join(nd_rows_html)}
      </tbody>
    </table>
  </div>

  <div class="section">
    <div class="section-title">Category Cohesion & Separation Analysis</div>
    <table>
      <thead>
        <tr>
          <th>Category</th>
          <th style="text-align:right;">Preset Count</th>
          <th style="text-align:right;">Mean Intra Distance</th>
          <th style="text-align:right;">Mean Inter Distance</th>
          <th style="text-align:right;">Ratio (Intra/Inter)</th>
          <th>Clustering Behavior</th>
        </tr>
      </thead>
      <tbody>
        {"".join(cat_rows_html)}
      </tbody>
    </table>
  </div>

  <footer>
    Generated by Panaudium Timbral Diversity Analysis Suite &bull; Self-contained report
  </footer>
</div>
</body>
</html>
"""
    with open(html_path, "w", encoding="utf-8") as f:
        f.write(html_content)


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Panaudium Preset Timbral Diversity & Near-Duplicate Analysis Tool"
    )
    parser.add_argument(
        "render_dir",
        type=str,
        help="Directory containing rendered preset WAVs and index.csv",
    )
    parser.add_argument(
        "--out",
        type=str,
        default="analysis/out",
        help="Output directory (default: analysis/out)",
    )

    args = parser.parse_args()

    render_dir = os.path.abspath(args.render_dir)
    out_dir = os.path.abspath(args.out)

    if not os.path.isdir(render_dir):
        sys.stderr.write(f"Error: Render directory does not exist: {render_dir}\n")
        sys.exit(1)

    os.makedirs(out_dir, exist_ok=True)

    # 1. Load preset index
    try:
        presets = common.load_preset_index(render_dir)
    except Exception as e:
        sys.stderr.write(f"Error loading index: {e}\n")
        sys.exit(1)

    N = len(presets)
    if N < 2:
        sys.stderr.write(f"Error: Need at least 2 presets for diversity analysis (found {N}).\n")
        sys.exit(1)

    print(f"Extracting 69 timbral features for {N} presets from {render_dir}...")

    # 2. Extract features per preset
    feature_records: List[Dict[str, float]] = []
    valid_presets: List[Dict[str, Any]] = []

    for idx, p in enumerate(presets, 1):
        wav_path = p["wav_path"]
        feats = extract_preset_features(wav_path)
        if not feats:
            print(f"  [{idx}/{N}] Warning: Failed extracting features for {p['name']} ({p['file']})")
            continue
        feature_records.append(feats)
        valid_presets.append(p)

    N_valid = len(valid_presets)
    if N_valid < 2:
        sys.stderr.write(f"Error: Fewer than 2 valid preset audio files analyzed ({N_valid}).\n")
        sys.exit(1)

    feature_names = list(feature_records[0].keys())
    print(f"Successfully extracted {len(feature_names)} features across {N_valid} presets.")

    # 3. Write <out>/features.csv
    features_csv_path = os.path.join(out_dir, "features.csv")
    with open(features_csv_path, "w", encoding="utf-8", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(["file", "name", "category"] + feature_names)
        for p, feats in zip(valid_presets, feature_records):
            row = [p["file"], p["name"], p["category"]] + [f"{feats.get(fn, 0.0):.6g}" for fn in feature_names]
            writer.writerow(row)

    # 4. Assemble feature matrix X
    X = np.zeros((N_valid, len(feature_names)), dtype=np.float64)
    for i, feats in enumerate(feature_records):
        for j, fn in enumerate(feature_names):
            X[i, j] = feats.get(fn, 0.0)

    # Replace any potential NaNs or Infs
    X = np.nan_to_num(X, nan=0.0, posinf=0.0, neginf=0.0)

    # 5. Standardize features (Z-score) & apply MFCC group weighting
    scaler = StandardScaler()
    X_scaled = scaler.fit_transform(X)

    # Group weights: divide 40 MFCC columns by sqrt(10) so they count as ~4 features
    mfcc_indices = [idx for idx, name in enumerate(feature_names) if name.startswith("mfcc_")]
    if mfcc_indices:
        X_scaled[:, mfcc_indices] /= np.sqrt(10.0)

    # 6. Pairwise Euclidean distance matrix
    pairwise_dists = pdist(X_scaled, metric="euclidean")
    dist_matrix = squareform(pairwise_dists)

    names = [p["name"] for p in valid_presets]
    categories = [p["category"] for p in valid_presets]

    # Nearest neighbor for each preset
    dist_no_diag = dist_matrix.copy()
    np.fill_diagonal(dist_no_diag, np.inf)
    nearest_indices = np.argmin(dist_no_diag, axis=1)
    nearest_dists = np.min(dist_no_diag, axis=1)

    nearest_dict: Dict[str, List[Any]] = {
        names[i]: [names[nearest_indices[i]], round(float(nearest_dists[i]), 3)]
        for i in range(N_valid)
    }

    # 7. Flag near-duplicates (distance < 5th percentile of all pairwise distances)
    p5 = float(np.percentile(pairwise_dists, 5.0))
    near_duplicates: List[List[Any]] = []
    for i in range(N_valid):
        for j in range(i + 1, N_valid):
            d = float(dist_matrix[i, j])
            if d < p5:
                near_duplicates.append([names[i], names[j], round(d, 3)])
    near_duplicates.sort(key=lambda item: item[2])

    # 8. Diversity score & coverage
    mean_pairwise = float(round(float(np.mean(pairwise_dists)), 3))
    coverage = float(round(float(np.mean(nearest_dists)), 3))

    # 9. Per-category intra- vs inter-category distance
    unique_cats = sorted(list(set(categories)))
    category_stats: Dict[str, Dict[str, float]] = {}

    for cat in unique_cats:
        cat_idxs = [i for i, c in enumerate(categories) if c == cat]
        other_idxs = [i for i, c in enumerate(categories) if c != cat]

        if len(cat_idxs) > 1:
            intra_list = [dist_matrix[i, j] for i in cat_idxs for j in cat_idxs if i < j]
            intra_val = float(np.mean(intra_list))
        else:
            intra_val = 0.0

        if len(other_idxs) > 0:
            inter_list = [dist_matrix[i, j] for i in cat_idxs for j in other_idxs]
            inter_val = float(np.mean(inter_list))
        else:
            inter_val = 0.0

        category_stats[cat] = {
            "intra": round(intra_val, 3),
            "inter": round(inter_val, 3),
        }

    # 10. Write <out>/distances.csv
    distances_csv_path = os.path.join(out_dir, "distances.csv")
    with open(distances_csv_path, "w", encoding="utf-8", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(["preset"] + names)
        for i, name_i in enumerate(names):
            row = [name_i] + [f"{dist_matrix[i, j]:.4f}" for j in range(N_valid)]
            writer.writerow(row)

    # 11. Write <out>/diversity.json
    diversity_json_path = os.path.join(out_dir, "diversity.json")
    diversity_data = {
        "mean_pairwise": mean_pairwise,
        "coverage": coverage,
        "near_duplicates": near_duplicates,
        "nearest": nearest_dict,
        "category_stats": category_stats,
    }
    with open(diversity_json_path, "w", encoding="utf-8") as f:
        json.dump(diversity_data, f, indent=2)

    # 12. Plot 2D PCA Map: <out>/diversity_map.png
    map_png_path = os.path.join(out_dir, "diversity_map.png")
    pca = PCA(n_components=min(2, X_scaled.shape[1]))
    coords = pca.fit_transform(X_scaled)
    if coords.shape[1] == 1:
        coords = np.column_stack([coords, np.zeros(N_valid)])

    var_ratio = pca.explained_variance_ratio_ * 100.0 if len(pca.explained_variance_ratio_) >= 2 else np.array([100.0, 0.0])

    fig, ax = plt.subplots(figsize=(13, 9), dpi=130)

    # Assign distinct colors to categories
    try:
        cmap = matplotlib.colormaps["tab20"]
    except Exception:
        cmap = plt.cm.get_cmap("tab20")

    cat_colors = {cat: cmap(i % 20) for i, cat in enumerate(unique_cats)}

    for cat in unique_cats:
        idxs = [i for i, c in enumerate(categories) if c == cat]
        ax.scatter(
            coords[idxs, 0],
            coords[idxs, 1],
            c=[cat_colors[cat]],
            label=f"{cat} ({len(idxs)})",
            s=65,
            edgecolors="black",
            linewidth=0.6,
            alpha=0.9,
            zorder=3,
        )

    # Label every point with small font
    for i, name in enumerate(names):
        ax.annotate(
            name,
            (coords[i, 0], coords[i, 1]),
            xytext=(4, 4),
            textcoords="offset points",
            fontsize=6.5,
            color="#1e293b",
            bbox=dict(boxstyle="round,pad=0.15", fc="white", ec="none", alpha=0.65),
            zorder=4,
        )

    ax.set_title(
        f"Panaudium Preset Timbral Diversity Map (PCA 2D Projection)\n"
        f"Diversity Score (Mean Pairwise): {mean_pairwise:.2f} | Coverage (Mean NN): {coverage:.2f}",
        fontsize=12,
        fontweight="bold",
        pad=12,
    )
    ax.set_xlabel(f"PCA Component 1 ({var_ratio[0]:.1f}% variance)", fontsize=10)
    ax.set_ylabel(f"PCA Component 2 ({var_ratio[1]:.1f}% variance)", fontsize=10)
    ax.grid(True, linestyle="--", alpha=0.4, zorder=1)
    ax.legend(
        title="Category",
        bbox_to_anchor=(1.02, 1),
        loc="upper left",
        fontsize=8,
        title_fontsize=9,
        frameon=True,
    )
    plt.tight_layout()
    plt.savefig(map_png_path, dpi=130, bbox_inches="tight")
    plt.close(fig)

    # 13. Plot Clustered Heatmap: <out>/distance_heatmap.png
    heatmap_png_path = os.path.join(out_dir, "distance_heatmap.png")
    Z = linkage(pairwise_dists, method="average")
    order = leaves_list(Z)

    ordered_dist = dist_matrix[order, :][:, order]
    ordered_names = [names[i] for i in order]

    fig_dim = max(11, int(N_valid * 0.16) + 5)
    fig, ax = plt.subplots(figsize=(fig_dim, fig_dim), dpi=130)
    im = ax.imshow(ordered_dist, cmap="viridis_r", aspect="equal")
    cbar = fig.colorbar(im, ax=ax, fraction=0.046, pad=0.04)
    cbar.set_label("Euclidean Feature Distance (Standardized)", fontsize=10)

    tick_font = 6 if N_valid <= 50 else 4.5
    ax.set_xticks(np.arange(N_valid))
    ax.set_yticks(np.arange(N_valid))
    ax.set_xticklabels(ordered_names, rotation=90, fontsize=tick_font, ha="center")
    ax.set_yticklabels(ordered_names, fontsize=tick_font)
    ax.set_title(
        "Clustered Preset Timbral Distance Matrix (Average Linkage)",
        fontsize=12,
        fontweight="bold",
        pad=12,
    )
    plt.tight_layout()
    plt.savefig(heatmap_png_path, dpi=130, bbox_inches="tight")
    plt.close(fig)

    # 14. Generate <out>/diversity.html
    html_path = os.path.join(out_dir, "diversity.html")
    generate_html_report(html_path, map_png_path, heatmap_png_path, valid_presets, diversity_data)

    # 15. Print summary to console
    print("\n" + "=" * 90)
    print("PANAUDIUM PRESET TIMBRAL DIVERSITY SUMMARY:")
    print("=" * 90)
    print(f"  Total Presets Analyzed    : {N_valid}")
    print(f"  Diversity Score (Mean Dist): {mean_pairwise:.3f}")
    print(f"  Coverage (Mean NN Dist)   : {coverage:.3f}")
    print(f"  5th Percentile Threshold  : {p5:.3f}")
    print(f"  Near-Duplicate Pairs (<p5): {len(near_duplicates)}")
    print("-" * 90)

    if near_duplicates:
        print("  Top Near-Duplicate Pairs (Closest First):")
        nd_table_headers = ["Rank", "Preset A", "Category A", "Preset B", "Category B", "Distance"]
        nd_table_rows = []
        cat_lookup = {p["name"]: p["category"] for p in valid_presets}
        for rank, (pa, pb, d) in enumerate(near_duplicates[:10], 1):
            nd_table_rows.append([
                rank,
                pa,
                cat_lookup.get(pa, ""),
                pb,
                cat_lookup.get(pb, ""),
                f"{d:.3f}",
            ])
        print(common.format_table(nd_table_headers, nd_table_rows))
        if len(near_duplicates) > 10:
            print(f"  ... and {len(near_duplicates) - 10} more pairs (see diversity.html / diversity.json)")
    else:
        print("  No pairs below the 5th percentile threshold.")

    print("\n  Per-Category Cluster Statistics:")
    cat_table_headers = ["Category", "Count", "Mean Intra Dist", "Mean Inter Dist", "Ratio (Intra/Inter)"]
    cat_table_rows = []
    cat_cnts = {cat: categories.count(cat) for cat in unique_cats}
    for cat in unique_cats:
        st = category_stats[cat]
        cnt = cat_cnts[cat]
        ratio_str = f"{(st['intra'] / st['inter']):.2f}" if st['inter'] > 0 and st['intra'] > 0 else ("N/A" if cnt <= 1 else "0.00")
        cat_table_rows.append([
            cat,
            cnt,
            f"{st['intra']:.3f}",
            f"{st['inter']:.3f}",
            ratio_str,
        ])
    print(common.format_table(cat_table_headers, cat_table_rows))
    print("=" * 90)
    print(f"Generated outputs in {out_dir}:")
    print(f"  - {features_csv_path}")
    print(f"  - {distances_csv_path}")
    print(f"  - {diversity_json_path}")
    print(f"  - {map_png_path}")
    print(f"  - {heatmap_png_path}")
    print(f"  - {html_path}\n")


if __name__ == "__main__":
    main()
