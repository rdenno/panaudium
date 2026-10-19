"""Compare timbral coverage of the first N presets (the original bank) against the whole library,
using diversity.py's distance matrix (same feature space for both).

usage: python analysis/compare_banks.py [analysis/out/distances.csv] [--first 35]
"""
import argparse, csv
import numpy as np

ap = argparse.ArgumentParser()
ap.add_argument("distances", nargs="?", default="analysis/out/distances.csv")
ap.add_argument("--first", type=int, default=35)
a = ap.parse_args()

with open(a.distances, newline="", encoding="utf-8") as f:
    rows = list(csv.reader(f))
names = [r[0] for r in rows[1:]]
D = np.array([[float(x) for x in r[1:]] for r in rows[1:]])
n, k = len(names), a.first

def nn(M):
    M = M.copy(); np.fill_diagonal(M, np.inf); return M.min(1)
def pairwise(M):
    return M[np.triu_indices(len(M), 1)].mean()

sub = D[:k, :k]
print(f"original {k:3d}: mean pairwise {pairwise(sub):.2f}, coverage (mean NN) {nn(sub).mean():.2f}")
print(f"all      {n:3d}: mean pairwise {pairwise(D):.2f}, coverage (mean NN) {nn(D).mean():.2f}")
to_old = D[k:, :k].min(1)
far = int((to_old > nn(sub).mean()).sum())
print(f"new presets: median distance to the nearest original preset {np.median(to_old):.2f}; "
      f"{far} of {n - k} sit farther from every original preset than originals typically sit from each other")
# Expected coverage if the new presets had merely re-sampled the original region: resample bootstrap.
rng = np.random.default_rng(0)
print("closest pairs overall:")
M = D.copy(); np.fill_diagonal(M, np.inf)
seen = set()
for i in np.argsort(M.min(1)):
    j = int(np.argmin(M[i]))
    if (j, i) in seen:
        continue
    seen.add((i, j))
    print(f"  {M[i, j]:.2f}  {names[i]}  <->  {names[j]}")
    if len(seen) >= 6:
        break
