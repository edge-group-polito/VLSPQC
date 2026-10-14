#!/usr/bin/env python3
"""
make_paper_table.py -- LaTeX tables for the paper, generated from the median-of-runs CSV
(bench/results_median3/results.csv, written by median_runs.py). No number is typed by hand.

Usage: python3 -I tools/bench/make_paper_table.py [--csv bench/results_median3/results.csv] [--out bench/results]
Writes <out>/paper_table.tex       (selected variants per scheme, see SEL below)
       <out>/paper_table_full.tex  (all 39 level-1 variants)
       <out>/paper_table_cells.md  (table cell -> CSV row/column, for paper_numbers.md)
Peak memory = max over {keygen, sign, verify} of (heap peak + stack peak) in KiB, from the same CSV.
"""
import argparse, csv
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
FAMS = ["FAEST", "MAYO", "MQOM", "QR-UOV", "SDitH", "SNOVA", "SQIsign", "UOV"]
OPS = ("keygen", "sign", "verify")


def f(x, nd=None):
    x = float(x)
    if nd is not None:
        return ("%.*f" % (nd, x))
    return "%.4f" % x if x < 0.01 else "%.3f" % x if x < 1 else "%.2f" % x if x < 10 else "%.1f" % x


def peak(r):
    return max(float(r["heap_%s_KiB" % o]) + float(r["stack_%s_KiB" % o]) for o in OPS)


def peak_op(r):
    return max(OPS, key=lambda o: float(r["heap_%s_KiB" % o]) + float(r["stack_%s_KiB" % o]))


def select(rows):
    """FAEST: all four. Others: fastest-sign variant and smallest-signature variant (tie -> faster sign);
    UOV additionally the pkc_skc variant of the smallest-signature level (smallest keys)."""
    sel = {}
    for fam in FAMS:
        rs = [r for r in rows if r["family"] == fam]
        if fam == "FAEST":
            sel[fam] = [(r, "all variants") for r in rs]
            continue
        fast = min(rs, key=lambda r: float(r["sign_ms"]))
        small = min(rs, key=lambda r: (int(r["sig_B"]), float(r["sign_ms"])))
        if fast is small:
            out = [(fast, "fastest sign and smallest sig")]
        else:
            out = [(fast, "fastest sign"), (small, "smallest sig")]
        if fam == "UOV":
            sk = min(rs, key=lambda r: (int(r["pk_B"]) + int(r["sk_B"]), int(r["sig_B"])))
            if all(sk is not o[0] for o in out):
                out.append((sk, "smallest pk+sk"))
        sel[fam] = out
    return sel


def tex_name(v):
    return v.replace("_", "\\_")


def table(rows_sel, caption, label, why=True):
    L = ["\\begin{table*}[t]", "\\centering", "\\caption{%s}" % caption, "\\label{%s}" % label, "\\small",
         "\\begin{tabular}{llrrrrrrr}", "\\toprule",
         "Scheme & Variant & pk (B) & sk (B) & sig (B) & KeyGen (ms) & Sign (ms) & Verify (ms) & Peak mem (KiB) \\\\",
         "\\midrule"]
    cells = []
    for fam, lst in rows_sel:
        for i, (r, why_) in enumerate(lst):
            L.append("%s & %s & %s & %s & %s & %s & %s & %s & %s \\\\" % (
                fam if i == 0 else "", tex_name(r["variant"]), r["pk_B"], r["sk_B"], r["sig_B"],
                f(r["keygen_ms"]), f(r["sign_ms"]), f(r["verify_ms"]), f(peak(r), 1)))
            cells.append((r["variant"], why_, peak_op(r)))
        L.append("\\midrule")
    L[-1] = "\\bottomrule"
    L += ["\\end{tabular}", "\\end{table*}"]
    return "\n".join(L) + "\n", cells


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--csv", default=str(ROOT / "bench" / "results_median3" / "results.csv"))
    ap.add_argument("--out", default=str(ROOT / "bench" / "results"))
    a = ap.parse_args()
    rows = list(csv.DictReader(open(a.csv, newline="")))
    out = Path(a.out)
    sel = select(rows)
    sp = [float(x["rel_spread_pct"]) for x in csv.DictReader(open(Path(a.csv).with_name("spread.csv"), newline=""))
          if x["metric"] in ("keygen_ms", "sign_ms", "verify_ms")]
    sp.sort()
    sp_med = (sp[len(sp) // 2] + sp[(len(sp) - 1) // 2]) / 2
    cap = ("NIST level~1: FAEST versus the other round-3 candidates, reference C (\\texttt{-O2}, plain gcc, one pinned core, "
           "1000-byte message). Times are the per-metric median of three runs (median-of-calls within each run); "
           "run-to-run spread ((max$-$min)/median over the three runs) is %.0f\\%% (median over variants and operations) and up to %.0f\\%%, "
           "so differences below $\\approx$1.5$\\times$ are not claimed. Peak mem is the maximum over the three operations of peak heap plus peak stack. "
           "Per scheme the fastest-signing and the smallest-signature variant are shown (UOV: also the variant with the smallest keys); "
           "all 39 variants are given in the artefact.") % (sp_med, sp[-1])
    t, cells = table([(fam, sel[fam]) for fam in FAMS], cap, "tab:l1")
    (out / "paper_table.tex").write_text(t)
    full = {fam: [(r, "all") for r in rows if r["family"] == fam] for fam in FAMS}
    t2, _ = table([(fam, full[fam]) for fam in FAMS], "NIST level~1, all 39 variants (same conventions as Table~\\ref{tab:l1}).",
                  "tab:l1full")
    (out / "paper_table_full.tex").write_text(t2)
    with open(out / "paper_table_cells.md", "w") as m:
        m.write("Rows of paper_table.tex (variant | why shown | memory column = heap+stack of op):\n\n")
        for v, why_, op in cells:
            m.write("- %s | %s | peak mem from op=%s\n" % (v, why_, op))
    print("wrote", out / "paper_table.tex", out / "paper_table_full.tex", out / "paper_table_cells.md")


if __name__ == "__main__":
    main()
