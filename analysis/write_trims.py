"""Fold analysis/out/trims.json into src/dsp/presets/Trims.h.

Renders already include the trims currently in Trims.h, so the new trim for each preset is
old + measured offset. Run render -> loudness.py -> write_trims.py two or three times: the output
soft-limiter makes loud presets compress, so it converges over a couple of passes.

usage: python analysis/write_trims.py [analysis/out/trims.json] [--max-step 12]
"""
import json, re, sys, argparse, pathlib

ap = argparse.ArgumentParser()
ap.add_argument("trims", nargs="?", default="analysis/out/trims.json")
ap.add_argument("--header", default="src/dsp/presets/Trims.h")
ap.add_argument("--max-step", type=float, default=12.0)
a = ap.parse_args()

hdr = pathlib.Path(a.header)
text = hdr.read_text(encoding="utf-8")
old = {m.group(1).replace('\\"', '"'): float(m.group(2)) for m in re.finditer(r'\{\s*"((?:[^"\\]|\\.)*)",\s*(-?[0-9.]+)f\s*\}', text)}
new = json.loads(pathlib.Path(a.trims).read_text(encoding="utf-8"))

merged = dict(old)
for name, delta in new.items():
    if delta is None or delta != delta:
        continue
    delta = max(-a.max_step, min(a.max_step, float(delta)))
    merged[name] = round(old.get(name, 0.0) + delta, 1)

body = "\n".join(f'        {{ "{n.replace(chr(34), chr(92) + chr(34))}", {v:.1f}f }},' for n, v in sorted(merged.items()))
start = text.index("static const std::unordered_map<std::string, float> t = {") + len("static const std::unordered_map<std::string, float> t = {")
end = text.index("    };", start)
hdr.write_text(text[:start] + "\n" + body + "\n" + text[end:], encoding="utf-8")
changed = sum(1 for n in merged if abs(merged[n] - old.get(n, 0.0)) >= 0.05)
print(f"wrote {len(merged)} trims to {hdr} ({changed} changed)")
