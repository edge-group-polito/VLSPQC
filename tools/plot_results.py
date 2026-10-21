#!/usr/bin/env python3
"""
plot_results.py -- figures from results/benchmark.csv (written by tools/collect_results.py) into docs/figures/.

  docs/figures/time_per_operation.png   median wall time per operation and variant, log scale, grouped by family
  docs/figures/sizes.png                pk / sk / ciphertext-or-signature bytes per variant, one panel per family
  README.md                             the compact table between the `results:root` markers

Three categorical series only (keygen / sign|encaps / verify|decaps and pk / sk / ct|sig), so the first three slots of
the validated palette are enough; the series also differ in marker shape, so identity never relies on colour alone.
Host, compiler and flags are read from results/benchmark_meta.json and printed under each figure.

Usage (from the repo root): python3 -I tools/plot_results.py
"""
import csv, json
from collections import OrderedDict
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.lines import Line2D
from matplotlib.ticker import FuncFormatter

ROOT = Path(__file__).resolve().parent.parent
FIG = ROOT / "docs" / "figures"

SURFACE, INK, INK2, GRID = "#fcfcfb", "#0b0b0b", "#52514e", "#e4e3df"
SLOTS = ["#2a78d6", "#eb6834", "#1baf7a"]      # validated categorical slots 1-3 (blue, orange, aqua)
MARKS = ["o", "s", "^"]

FAMILY_ORDER = ["ML-KEM (FIPS 203)", "HQC (v5.0.0)", "ML-DSA (FIPS 204)", "SLH-DSA (FIPS 205)",
                "Falcon (round 3)", "FN-DSA (c-fn-dsa, experimental)"]
STATUS = {"ML-KEM (FIPS 203)": "final FIPS", "ML-DSA (FIPS 204)": "final FIPS", "SLH-DSA (FIPS 205)": "final FIPS",
          "HQC (v5.0.0)": "standard pending", "Falcon (round 3)": "FN-DSA pending",
          "FN-DSA (c-fn-dsa, experimental)": "experimental, pre-standard"}


def load():
    rows = list(csv.DictReader(open(ROOT / "results" / "benchmark.csv")))
    # variants of a family are shown in order of NIST security category, then by name
    rows.sort(key=lambda r: (int(r["nist_category"]), r["variant"]))
    meta = json.load(open(ROOT / "results" / "benchmark_meta.json"))
    return rows, meta


def style(ax):
    ax.set_facecolor(SURFACE)
    for s in ("top", "right"):
        ax.spines[s].set_visible(False)
    for s in ("left", "bottom"):
        ax.spines[s].set_color(GRID)
    ax.tick_params(colors=INK2, labelsize=8, length=0)
    ax.grid(True, color=GRID, linewidth=0.6)
    ax.set_axisbelow(True)


def footer(fig, meta, extra=""):
    txt = "%s; %s; flags %s; 1 CPU pinned; median per operation; %s%s" % (
        meta.get("cpu_model", "?"), meta.get("gcc", "?").replace("(Ubuntu ", "(").split(" ")[0] + " " + meta.get("gcc", "?").split(")")[-1].strip(),
        meta.get("cflags", "?"), meta.get("date", "?")[:10], extra)
    fig.text(0.01, 0.005, txt + " - portable reference C, not optimized code", fontsize=6.5, color=INK2, ha="left", va="bottom")


def time_figure(rows, meta):
    by = OrderedDict((f, OrderedDict()) for f in FAMILY_ORDER)
    for r in rows:
        by[r["family"]].setdefault(r["variant"], {})[r["operation"]] = int(r["ns_median"]) / 1e6
    n = sum(len(v) for v in by.values()) + len(by)
    fig, ax = plt.subplots(figsize=(8.2, 0.215 * n + 1.5), facecolor=SURFACE)
    style(ax)
    y, yticks, ylabels = 0, [], []
    for fam, vs in by.items():
        ax.text(0.0, y, "  " + fam + " - " + STATUS[fam], transform=ax.get_yaxis_transform(), fontsize=8, fontweight="bold",
                color=INK, va="center", ha="left")
        y += 1
        for var, ops in vs.items():
            names = list(ops)
            for i, nm in enumerate(names):
                # small vertical offsets keep markers of equal time visible
                ax.plot(ops[nm], y + (i - 1) * 0.2, MARKS[i], color=SLOTS[i], markersize=5.5, markeredgecolor=SURFACE,
                        markeredgewidth=1.0, zorder=3)
            yticks.append(y); ylabels.append(var)
            y += 1
    ax.set_yticks(yticks); ax.set_yticklabels(ylabels)
    ax.set_ylim(y - 0.4, -0.8)
    ax.set_xscale("log")
    ax.set_xlabel("median time per call (ms), log scale", color=INK2, fontsize=8.5)
    ax.tick_params(axis="y", labelsize=7)
    legend = [Line2D([0], [0], marker=MARKS[i], color="none", markerfacecolor=SLOTS[i], markeredgecolor=SURFACE, markersize=7,
                     label=l) for i, l in enumerate(["key generation", "sign / encaps", "verify / decaps"])]
    ax.legend(handles=legend, loc="lower right", frameon=False, fontsize=8, labelcolor=INK)
    ax.set_title("Operation time per variant (KEM rows: encaps / decaps)", color=INK, fontsize=10, loc="left")
    fig.tight_layout(rect=(0, 0.03, 1, 1))
    footer(fig, meta)
    FIG.mkdir(parents=True, exist_ok=True)
    fig.savefig(FIG / "time_per_operation.png", dpi=160, facecolor=SURFACE)
    plt.close(fig)


def size_figure(rows, meta):
    seen = OrderedDict((f, OrderedDict()) for f in FAMILY_ORDER)
    for r in rows:
        if r["operation"] == "keygen":
            seen[r["family"]][r["variant"]] = (int(r["pk_B"]), int(r["sk_B"]), int(r["ct_or_sig_B"]) if r["ct_or_sig_B"] else 0, r["nist_category"])
    fig, axes = plt.subplots(2, 3, figsize=(10, 6.3), facecolor=SURFACE)
    for ax, (fam, vs) in zip(axes.flat, seen.items()):
        style(ax)
        xs = range(len(vs))
        for i in range(3):
            ax.plot([x + (i - 1) * 0.16 for x in xs], [v[i] for v in vs.values()], MARKS[i], color=SLOTS[i], markersize=5.5,
                    markeredgecolor=SURFACE, markeredgewidth=1.0, zorder=3)
        ax.set_yscale("log")
        ax.yaxis.set_major_formatter(FuncFormatter(lambda v, _: "{:,.0f}".format(v)))
        ax.yaxis.set_minor_formatter(FuncFormatter(lambda v, _: "{:,.0f}".format(v) if v in (2, 3, 5, 20, 30, 50, 200, 300, 500, 2000, 3000, 5000, 20000, 30000, 50000) else ""))
        ax.tick_params(axis="y", which="minor", labelsize=6, labelcolor=INK2, length=0)
        ax.set_xticks(list(xs))
        ax.set_xticklabels([k.replace(fam.split(" ")[0] + "-", "").replace("exp-", "") for k in vs], rotation=60, ha="right", fontsize=6.5)
        ax.set_title(fam.split(" (")[0] + "  (" + STATUS[fam] + ")", color=INK, fontsize=8, loc="left")
        ax.set_xlim(-0.6, len(vs) - 0.4)
    kem = [Line2D([0], [0], marker=MARKS[i], color="none", markerfacecolor=SLOTS[i], markeredgecolor=SURFACE, markersize=7, label=l)
           for i, l in enumerate(["public key", "secret key", "ciphertext (KEM) / signature (DS, CRYPTO_BYTES)"])]
    fig.legend(handles=kem, loc="upper center", ncol=3, frameon=False, fontsize=8.5, labelcolor=INK, bbox_to_anchor=(0.5, 0.995))
    for ax in axes[:, 0]:
        ax.set_ylabel("bytes (log scale)", color=INK2, fontsize=8)
    fig.tight_layout(rect=(0, 0.03, 1, 0.94))
    footer(fig, meta, "; sizes from the compiled api.h constants")
    fig.savefig(FIG / "sizes.png", dpi=160, facecolor=SURFACE)
    plt.close(fig)


def update_readme_table(rows, meta):
    """Write the compact results table between <!-- BEGIN results:root --> / <!-- END results:root --> in README.md."""
    by = OrderedDict()
    for r in rows:
        by.setdefault((r["family"], r["variant"]), {})[r["operation"]] = r
    lines = ["| Variant | NIST cat. | keygen (ms) | sign / encaps (ms) | verify / decaps (ms) | peak stack (B) | peak heap (B) | code (B) |",
             "|---|---:|---:|---:|---:|---:|---:|---:|"]
    for fam in FAMILY_ORDER:
        for (f, var), ops in by.items():
            if f != fam:
                continue
            ms = [int(ops[o]["ns_median"]) / 1e6 for o in ops]
            first = next(iter(ops.values()))
            lines.append("| %s | %s | %s | %s | %s | %d | %d | %s |" % (
                var, first["nist_category"], *["%.3f" % x if x < 10 else "%.1f" % x for x in ms],
                max(int(o["stack_peak_B"]) for o in ops.values()), max(int(o["heap_peak_B"]) for o in ops.values()), first["code_B"]))
    p = ROOT / "README.md"
    s = p.read_text()
    b, e = "<!-- BEGIN results:root -->", "<!-- END results:root -->"
    if b in s and e in s:
        i, j = s.index(b) + len(b), s.index(e)
        p.write_text(s[:i] + "\n" + "\n".join(lines) + "\n" + s[j:])
        print("updated results:root in README.md")


def main():
    rows, meta = load()
    time_figure(rows, meta)
    size_figure(rows, meta)
    update_readme_table(rows, meta)
    print("wrote", FIG / "time_per_operation.png", "and", FIG / "sizes.png")


if __name__ == "__main__":
    main()
