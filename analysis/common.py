"""
analysis/common.py - Shared utilities for Panaudium preset audio analysis.

Provides robust index loading, metadata parsing, table formatting, and
standard BS.1770 audio weighting helpers.
"""

import csv
import os
import sys
from typing import Any, Dict, List, Optional, Tuple
import numpy as np


def load_preset_index(render_dir: str) -> List[Dict[str, Any]]:
    """
    Load preset metadata from <render_dir>/index.csv.

    Expected header: file,name,category,realtime_factor
    Returns a list of dictionaries with keys:
      - file: filename of WAV
      - name: preset display name
      - category: preset category string
      - realtime_factor: float (if present)
      - wav_path: absolute or relative path to WAV file
    """
    index_path = os.path.join(render_dir, "index.csv")
    if not os.path.isfile(index_path):
        raise FileNotFoundError(f"index.csv not found in render directory: {render_dir}")

    presets: List[Dict[str, Any]] = []
    with open(index_path, mode="r", encoding="utf-8-sig", newline="") as f:
        reader = csv.DictReader(f)
        for row_num, row in enumerate(reader, start=2):
            if not row or not any(row.values()):
                continue
            filename = (row.get("file") or "").strip()
            if not filename:
                continue
            name = (row.get("name") or os.path.splitext(filename)[0]).strip()
            category = (row.get("category") or "Unknown").strip()
            rtf_raw = (row.get("realtime_factor") or "").strip()
            try:
                rtf = float(rtf_raw) if rtf_raw else None
            except ValueError:
                rtf = None

            wav_path = os.path.join(render_dir, filename)
            presets.append({
                "file": filename,
                "name": name,
                "category": category,
                "realtime_factor": rtf,
                "wav_path": wav_path,
            })

    if not presets:
        raise ValueError(f"No preset entries found in {index_path}")

    return presets


def format_table(
    headers: List[str],
    rows: List[List[Any]],
    alignments: Optional[List[str]] = None,
) -> str:
    """
    Format a clean, readable ASCII table.

    headers: list of column header strings
    rows: list of row lists containing cell values
    alignments: list of '<', '>', or '^' for each column (default '<' for strings, '>' for numbers)
    """
    if not headers:
        return ""

    num_cols = len(headers)
    str_rows = [[str(cell) for cell in row] for row in rows]

    # Compute column widths
    col_widths = [len(h) for h in headers]
    for row in str_rows:
        for col_idx in range(min(num_cols, len(row))):
            col_widths[col_idx] = max(col_widths[col_idx], len(row[col_idx]))

    # Default alignments
    if alignments is None or len(alignments) < num_cols:
        inferred = []
        for c in range(num_cols):
            # If all rows have numeric or inf values in this column, align right
            numeric_col = True
            for r in str_rows:
                if c < len(r):
                    val = r[c].strip()
                    try:
                        float(val)
                    except ValueError:
                        if val not in ("-inf", "inf", "nan", "N/A"):
                            numeric_col = False
                            break
            inferred.append(">" if numeric_col else "<")
        alignments = inferred

    # Build divider lines
    border_line = "+" + "+".join("-" * (w + 2) for w in col_widths) + "+"
    header_cells = [
        f" {headers[i]:^{col_widths[i]}} " for i in range(num_cols)
    ]
    header_line = "|" + "|".join(header_cells) + "|"

    lines = [border_line, header_line, border_line]

    for row in str_rows:
        cells = []
        for i in range(num_cols):
            val = row[i] if i < len(row) else ""
            align = alignments[i]
            if align == ">":
                cell_str = f" {val:>{col_widths[i]}} "
            elif align == "^":
                cell_str = f" {val:^{col_widths[i]}} "
            else:
                cell_str = f" {val:<{col_widths[i]}} "
            cells.append(cell_str)
        lines.append("|" + "|".join(cells) + "|")

    lines.append(border_line)
    return "\n".join(lines)


def get_k_weighted_signal(meter: Any, data: np.ndarray, rate: int) -> np.ndarray:
    """
    Apply BS.1770 K-weighting filtering to audio data.
    Attempts to use pyloudnorm's internal filters if available;
    falls back to ITU-R BS.1770-4 48 kHz / general IIR filter coefficients.
    """
    # 1. Check if meter has filters
    if hasattr(meter, "_filters") and meter._filters:
        filtered = data.copy()
        stages = meter._filters.values() if isinstance(meter._filters, dict) else meter._filters
        for stage in stages:
            if hasattr(stage, "apply_filter"):
                filtered = stage.apply_filter(filtered)
            elif callable(stage):
                filtered = stage(filtered)
        return filtered

    if hasattr(meter, "apply_filter"):
        return meter.apply_filter(data.copy())

    # 2. Fallback: ITU-R BS.1770-4 standard filter coefficients
    import scipy.signal

    if rate == 48000:
        # Pre-filter (high shelf)
        b1 = np.array([1.53512485958697, -2.69169618940638, 1.19839281085285], dtype=np.float64)
        a1 = np.array([1.0, -1.69065929318241, 0.73248077421585], dtype=np.float64)
        # RLB weighting (high pass)
        b2 = np.array([1.0, -2.0, 1.0], dtype=np.float64)
        a2 = np.array([1.0, -1.99004745483398, 0.99007225034111], dtype=np.float64)
    else:
        # Design 2nd-order filters for arbitrary sample rate
        # Stage 1: High shelf (approx +4 dB gain, fc = 1682 Hz)
        f0 = 1681.974450955533
        gain_db = 3.999843853973347
        q = 0.7071752369274193
        k = np.tan(np.pi * f0 / rate)
        vh = 10.0 ** (gain_db / 20.0)
        vb = vh ** 0.4996667741545416
        denom = 1.0 + k / q + k * k
        b1 = np.array([
            (vh + vb * k / q + k * k) / denom,
            2.0 * (k * k - vh) / denom,
            (vh - vb * k / q + k * k) / denom,
        ], dtype=np.float64)
        a1 = np.array([
            1.0,
            2.0 * (k * k - 1.0) / denom,
            (1.0 - k / q + k * k) / denom,
        ], dtype=np.float64)

        # Stage 2: High pass (fc = 38 Hz, Q = 0.5)
        f0_hp = 38.13547087602444
        q_hp = 0.5003270373238773
        k_hp = np.tan(np.pi * f0_hp / rate)
        denom_hp = 1.0 + k_hp / q_hp + k_hp * k_hp
        b2 = np.array([
            1.0 / denom_hp,
            -2.0 / denom_hp,
            1.0 / denom_hp,
        ], dtype=np.float64)
        a2 = np.array([
            1.0,
            2.0 * (k_hp * k_hp - 1.0) / denom_hp,
            (1.0 - k_hp / q_hp + k_hp * k_hp) / denom_hp,
        ], dtype=np.float64)

    filtered = scipy.signal.lfilter(b1, a1, data, axis=0)
    filtered = scipy.signal.lfilter(b2, a2, filtered, axis=0)
    return filtered
