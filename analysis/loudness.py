#!/usr/bin/env python3
"""
analysis/loudness.py - Preset loudness analysis and trim calculation.

Features:
- Reads presets from <render_dir>/index.csv
- Computes integrated loudness (ITU-R BS.1770 via pyloudnorm.Meter)
- Computes max short-term loudness (3 s window, 0.1 s hop, BS.1770 K-weighting)
- Computes sample peak dBFS and approximate true peak (4x oversampling with scipy.signal.resample_poly)
- Computes suggested trim in dB = target - integrated, clamped to [-18, +18]
- Outputs <out>/loudness.csv and <out>/trims.json (trims rounded to 0.1 dB)
- Prints a sorted loudness table and spread statistics (max-min LUFS, std dev)
- Fully handles silent/near-silent files without crashing (reports -inf / clamped trim)
"""

import argparse
import csv
import json
import os
import sys
from typing import Any, Dict, List

import numpy as np
import pyloudnorm as pyln
import scipy.signal
import soundfile as sf

# Allow imports from the same directory (e.g. analysis.common)
script_dir = os.path.dirname(os.path.abspath(__file__))
if script_dir not in sys.path:
    sys.path.insert(0, script_dir)

import common


def analyze_file_loudness(
    wav_path: str,
    meter_dict: Dict[int, pyln.Meter],
    target_lufs: float = -16.0,
) -> Dict[str, Any]:
    """
    Analyze loudness and peaks for a single audio file.
    Returns dictionary with keys:
      lufs_i, lufs_st_max, peak_db, true_peak_db, trim_db
    """
    if not os.path.isfile(wav_path):
        return {
            "lufs_i": -np.inf,
            "lufs_st_max": -np.inf,
            "peak_db": -np.inf,
            "true_peak_db": -np.inf,
            "trim_db": 18.0,
            "status": "missing_file",
        }

    try:
        data, rate = sf.read(wav_path, dtype="float32")
    except Exception as e:
        return {
            "lufs_i": -np.inf,
            "lufs_st_max": -np.inf,
            "peak_db": -np.inf,
            "true_peak_db": -np.inf,
            "trim_db": 18.0,
            "status": f"read_error: {e}",
        }

    # Convert to 64-bit float for stable DSP filtering and math
    data_64 = data.astype(np.float64)

    # Ensure shape is (samples, channels)
    if data_64.ndim == 1:
        data_64 = np.column_stack([data_64, data_64])

    total_samples, num_channels = data_64.shape

    # Check for empty or silent audio
    max_amp = float(np.max(np.abs(data_64))) if total_samples > 0 else 0.0
    if total_samples == 0 or max_amp < 1e-9:
        return {
            "lufs_i": -np.inf,
            "lufs_st_max": -np.inf,
            "peak_db": -np.inf,
            "true_peak_db": -np.inf,
            "trim_db": 18.0,
            "status": "silent",
        }

    # 1. Sample Peak dBFS
    peak_db = float(20.0 * np.log10(max_amp))

    # 2. Approximate True Peak dBFS (4x oversampling with resample_poly)
    try:
        resampled = scipy.signal.resample_poly(data_64, up=4, down=1, axis=0)
        tp_max = float(np.max(np.abs(resampled)))
        true_peak_db = float(20.0 * np.log10(tp_max)) if tp_max > 1e-9 else -np.inf
    except Exception:
        true_peak_db = peak_db

    # 3. Integrated Loudness
    if rate not in meter_dict:
        meter_dict[rate] = pyln.Meter(rate)
    meter = meter_dict[rate]

    try:
        lufs_i = float(meter.integrated_loudness(data_64))
        if np.isnan(lufs_i) or np.isneginf(lufs_i) or lufs_i < -120.0:
            lufs_i = -np.inf
    except Exception:
        lufs_i = -np.inf

    # Suggested trim in dB = target - integrated, clamped to [-18, +18]
    if np.isneginf(lufs_i) or np.isnan(lufs_i):
        trim_db = 18.0
    else:
        trim_db = float(np.clip(target_lufs - lufs_i, -18.0, 18.0))

    # 4. Max Short-Term Loudness (3s window, 0.1s hop, BS.1770 K-weighting)
    win_samples = int(round(3.0 * rate))
    hop_samples = int(round(0.1 * rate))

    try:
        filtered = common.get_k_weighted_signal(meter, data_64, rate)
        # Sum squares across channels (stereo weights GL = 1.0, GR = 1.0)
        if filtered.ndim == 2:
            ch_sq = np.sum(filtered ** 2, axis=1)
        else:
            ch_sq = filtered ** 2

        if total_samples <= win_samples:
            mean_sq = float(np.mean(ch_sq))
            if mean_sq > 1e-12:
                lufs_st_max = float(-0.691 + 10.0 * np.log10(mean_sq))
            else:
                lufs_st_max = -np.inf
        else:
            cumsum = np.cumsum(np.insert(ch_sq, 0, 0.0))
            starts = np.arange(0, total_samples - win_samples + 1, hop_samples)
            if (total_samples - win_samples) % hop_samples != 0:
                starts = np.append(starts, total_samples - win_samples)

            win_sums = cumsum[starts + win_samples] - cumsum[starts]
            win_means = win_sums / float(win_samples)
            max_mean = float(np.max(win_means))
            if max_mean > 1e-12:
                lufs_st_max = float(-0.691 + 10.0 * np.log10(max_mean))
            else:
                lufs_st_max = -np.inf
    except Exception:
        # Fallback to sliding window via meter directly
        st_vals = []
        starts = np.arange(0, max(1, total_samples - win_samples + 1), hop_samples)
        for s in starts:
            chunk = data_64[s : s + win_samples]
            try:
                val = meter.integrated_loudness(chunk)
                if np.isfinite(val):
                    st_vals.append(val)
            except Exception:
                pass
        lufs_st_max = float(max(st_vals)) if st_vals else -np.inf

    return {
        "lufs_i": lufs_i,
        "lufs_st_max": lufs_st_max,
        "peak_db": peak_db,
        "true_peak_db": true_peak_db,
        "trim_db": trim_db,
        "status": "ok",
    }


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Panaudium Preset Loudness & True Peak Analysis Tool"
    )
    parser.add_argument(
        "render_dir",
        type=str,
        help="Directory containing rendered preset WAVs and index.csv",
    )
    parser.add_argument(
        "--target",
        type=float,
        default=-16.0,
        help="Target integrated loudness in LUFS (default: -16)",
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
    target_lufs = args.target

    if not os.path.isdir(render_dir):
        sys.stderr.write(f"Error: Render directory does not exist: {render_dir}\n")
        sys.exit(1)

    os.makedirs(out_dir, exist_ok=True)

    # Load presets
    try:
        presets = common.load_preset_index(render_dir)
    except Exception as e:
        sys.stderr.write(f"Error loading index: {e}\n")
        sys.exit(1)

    meter_dict: Dict[int, pyln.Meter] = {}
    results: List[Dict[str, Any]] = []
    trims_json: Dict[str, float] = {}

    for p in presets:
        metrics = analyze_file_loudness(p["wav_path"], meter_dict, target_lufs)
        entry = {
            "file": p["file"],
            "name": p["name"],
            "category": p["category"],
            "lufs_i": metrics["lufs_i"],
            "lufs_st_max": metrics["lufs_st_max"],
            "peak_db": metrics["peak_db"],
            "true_peak_db": metrics["true_peak_db"],
            "trim_db": metrics["trim_db"],
            "status": metrics["status"],
        }
        results.append(entry)
        trims_json[p["name"]] = round(metrics["trim_db"], 1)

    # 1. Write <out>/loudness.csv
    csv_path = os.path.join(out_dir, "loudness.csv")
    csv_columns = [
        "file",
        "name",
        "category",
        "lufs_i",
        "lufs_st_max",
        "peak_db",
        "true_peak_db",
        "trim_db",
    ]
    with open(csv_path, mode="w", encoding="utf-8", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(csv_columns)
        for r in results:
            writer.writerow([
                r["file"],
                r["name"],
                r["category"],
                "-inf" if np.isneginf(r["lufs_i"]) else f"{r['lufs_i']:.2f}",
                "-inf" if np.isneginf(r["lufs_st_max"]) else f"{r['lufs_st_max']:.2f}",
                "-inf" if np.isneginf(r["peak_db"]) else f"{r['peak_db']:.2f}",
                "-inf" if np.isneginf(r["true_peak_db"]) else f"{r['true_peak_db']:.2f}",
                f"{r['trim_db']:.1f}",
            ])

    # 2. Write <out>/trims.json
    json_path = os.path.join(out_dir, "trims.json")
    with open(json_path, mode="w", encoding="utf-8") as f:
        json.dump(trims_json, f, indent=2)

    # 3. Print sorted table and spread summary
    # Sort by lufs_i descending (loudest first, -inf at bottom)
    results_sorted = sorted(
        results,
        key=lambda r: (1 if np.isneginf(r["lufs_i"]) else 0, -r["lufs_i"]),
    )

    table_headers = [
        "File",
        "Preset Name",
        "Category",
        "LUFS (Int)",
        "LUFS (ST Max)",
        "Peak dB",
        "True Peak dB",
        "Trim dB",
    ]
    table_rows = []
    for r in results_sorted:
        table_rows.append([
            r["file"],
            r["name"],
            r["category"],
            "-inf" if np.isneginf(r["lufs_i"]) else f"{r['lufs_i']:>7.2f}",
            "-inf" if np.isneginf(r["lufs_st_max"]) else f"{r['lufs_st_max']:>7.2f}",
            "-inf" if np.isneginf(r["peak_db"]) else f"{r['peak_db']:>7.2f}",
            "-inf" if np.isneginf(r["true_peak_db"]) else f"{r['true_peak_db']:>7.2f}",
            f"{r['trim_db']:>+6.1f}",
        ])

    print("\n" + "=" * 90)
    print(f"PANAUDIUM PRESET LOUDNESS ANALYSIS (Target: {target_lufs:.1f} LUFS)")
    print("=" * 90)
    print(common.format_table(table_headers, table_rows))

    # Spread calculation
    valid_lufs = [r["lufs_i"] for r in results if np.isfinite(r["lufs_i"])]
    print("\n" + "=" * 90)
    print("LOUDNESS SUMMARY & SPREAD STATISTICS:")
    print(f"  Total Presets Analyzed : {len(results)}")
    if valid_lufs:
        min_lufs = float(np.min(valid_lufs))
        max_lufs = float(np.max(valid_lufs))
        mean_lufs = float(np.mean(valid_lufs))
        spread = max_lufs - min_lufs
        std_dev = float(np.std(valid_lufs))
        silent_cnt = len(results) - len(valid_lufs)

        print(f"  Loudest Preset         : {max_lufs:.2f} LUFS")
        print(f"  Softest Preset         : {min_lufs:.2f} LUFS")
        print(f"  Mean Loudness          : {mean_lufs:.2f} LUFS")
        print(f"  Loudness Spread (Max-Min): {spread:.2f} LU")
        print(f"  Standard Deviation     : {std_dev:.2f} LU")
        if silent_cnt > 0:
            print(f"  Silent/Gated Presets   : {silent_cnt} (excluded from spread)")
    else:
        print("  All presets were silent or below gating threshold.")
    print("=" * 90)
    print(f"Wrote outputs to:\n  {csv_path}\n  {json_path}\n")


if __name__ == "__main__":
    main()
