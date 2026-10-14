#!/usr/bin/env python3
"""
median_runs.py -- per-metric median of several benchmark runs (same variants,
same CSV layout as report.py's results.csv), plus a min/max spread file.

Usage: python3 -I tools/bench/median_runs.py [--out DIR] RUN_DIR [RUN_DIR ...]
  RUN_DIR holds results.csv (e.g. bench/results bench/results_run2 bench/results_run3)
Writes  <out>/results.csv  (median per metric; default out = bench/results_median3)
        <out>/spread.csv   (long format: variant,metric,median,min,max,rel_spread_pct,deterministic)
Sizes (pk_B, sk_B, sig_B) must be identical across runs, otherwise the script stops.
rel_spread_pct = 100*(max-min)/median.
"""
import argparse, csv, statistics, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
TEXT = ("family", "variant")
SIZES = ("pk_B", "sk_B", "sig_B")


def fmt(x):
    # integers stay integers; medians of an even count never occur with 3 runs
    return str(int(x)) if float(x).is_integer() else repr(round(x, 6))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=str(ROOT / "bench" / "results_median3"))
    ap.add_argument("runs", nargs="+")
    a = ap.parse_args()
    tabs = []
    for d in a.runs:
        with open(Path(d) / "results.csv", newline="") as f:
            rd = csv.DictReader(f)
            tabs.append((rd.fieldnames, {r["variant"]: r for r in rd}))
    cols = tabs[0][0]
    for fn, t in tabs:
        if fn != cols or set(t) != set(tabs[0][1]):
            sys.exit("runs differ in columns or variants")
    out = Path(a.out)
    out.mkdir(parents=True, exist_ok=True)
    rows, spread = [], []
    for v in tabs[0][1]:
        row = {c: tabs[0][1][v][c] for c in TEXT}
        for c in cols:
            if c in TEXT:
                continue
            vals = [float(t[v][c]) for _, t in tabs]
            if c in SIZES and len(set(vals)) != 1:
                sys.exit("size %s of %s differs between runs: %s" % (c, v, vals))
            m = statistics.median(vals)
            row[c] = fmt(m)
            rel = 100.0 * (max(vals) - min(vals)) / m if m else 0.0
            spread.append([v, c, fmt(m), fmt(min(vals)), fmt(max(vals)), "%.1f" % rel,
                           "yes" if len(set(vals)) == 1 else "no"])
        rows.append(row)
    with open(out / "results.csv", "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=cols, lineterminator="\n")
        w.writeheader()
        w.writerows(rows)
    with open(out / "spread.csv", "w", newline="") as f:
        w = csv.writer(f, lineterminator="\n")
        w.writerow(["variant", "metric", "median", "min", "max", "rel_spread_pct", "identical_in_all_runs"])
        w.writerows(spread)
    print("wrote", out / "results.csv", "and", out / "spread.csv", "(%d runs, %d variants)" % (len(tabs), len(rows)))


if __name__ == "__main__":
    main()
