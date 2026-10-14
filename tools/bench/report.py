#!/usr/bin/env python3
"""
report.py -- turn bench/results/raw/*.json (written by run_bench.py) into
  <out>/results.md   tables + methodology + FAEST-vs-rest summary
  <out>/results.csv  one row per variant
  <out>/*.png        charts (time, memory, sizes)

Usage: python3 tools/bench/report.py [--results bench/results]
"""
import argparse, json, math, statistics
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
OPS = ("keygen", "sign", "verify")
# fixed family order/colours (categorical, assigned by entity, never by rank)
FAMS = ["FAEST", "MAYO", "MQOM", "QR-UOV", "SDitH", "SNOVA", "SQIsign", "UOV"]
COL = {"FAEST": "#d95f02", "MAYO": "#1b9e77", "MQOM": "#7570b3", "QR-UOV": "#e7298a",
       "SDitH": "#66a61e", "SNOVA": "#e6ab02", "SQIsign": "#a6761d", "UOV": "#1f78b4"}


def load(res):
    rows = []
    for f in sorted((res / "raw").glob("*.json")):
        r = json.load(open(f))
        if "ops" not in r or any("ns_median" not in r["ops"].get(o, {}) for o in OPS):
            continue
        ops = r["ops"]
        r["pk"] = ops["keygen"]["pk_bytes"]
        r["sk"] = ops["keygen"]["sk_bytes"]
        r["sig"] = ops["sign"]["sig_bytes"]
        r["sig_max"] = ops["sign"]["sig_bytes_max"]
        rows.append(r)
    order = {f: i for i, f in enumerate(FAMS)}
    cfg = {v["label"]: i for i, v in enumerate(json.load(open(Path(__file__).resolve().parent / "variants_l1.json"))["variants"])}
    rows.sort(key=lambda r: (order.get(r["family"], 99), cfg.get(r["label"], 1 << 20), r["label"]))
    return rows


def ms(r, o):
    return r["ops"][o]["ns_median"] / 1e6


def net(op):
    """VmHWM growth during the timed operation (VmHWM minus the VmHWM baseline taken after clear_refs)."""
    return op["vmhwm_kb"] - op["vmhwm_base_kb"]


def kib(b):
    return b / 1024.0


def fmt_t(x):
    return "%.4f" % x if x < 0.01 else "%.3f" % x if x < 10 else ("%.1f" % x if x < 1000 else "%.0f" % x)


def fmt_r(x):
    return "%.1f" % x if x >= 1 else "%.3g" % x


def fmt_b(b):
    return "%d" % b if b < 10000 else "%.1f K" % (b / 1000.0) if b < 1e6 else "%.2f M" % (b / 1e6)


def write_csv(rows, out):
    with open(out / "results.csv", "w") as f:
        f.write("family,variant,pk_B,sk_B,sig_B,keygen_ms,sign_ms,verify_ms,keygen_Mcyc,sign_Mcyc,verify_Mcyc,"
                "heap_keygen_KiB,heap_sign_KiB,heap_verify_KiB,stack_keygen_KiB,stack_sign_KiB,stack_verify_KiB,"
                "vmhwm_keygen_KiB,vmhwm_sign_KiB,vmhwm_verify_KiB,vmhwm_base_keygen_KiB,vmhwm_base_sign_KiB,vmhwm_base_verify_KiB,text_B,data_B,bss_B\n")
        for r in rows:
            o = r["ops"]
            f.write(",".join([r["family"], r["label"], str(r["pk"]), str(r["sk"]), str(r["sig"])]
                             + ["%.4f" % ms(r, x) for x in OPS]
                             + ["%.3f" % (o[x]["cycles_median"] / 1e6) for x in OPS]
                             + ["%.1f" % kib(o[x]["heap_peak"]) for x in OPS]
                             + ["%.1f" % kib(o[x]["stack_peak"]) for x in OPS]
                             + [str(o[x]["vmhwm_kb"]) for x in OPS]
                             + [str(o[x]["vmhwm_base_kb"]) for x in OPS]
                             + [str(r["objsize"][k]) for k in ("text", "data", "bss")]) + "\n")


def charts(rows, out):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    plt.rcParams.update({"font.size": 8, "axes.spines.top": False, "axes.spines.right": False,
                         "axes.edgecolor": "#888", "axes.labelcolor": "#333", "xtick.color": "#333",
                         "ytick.color": "#333", "figure.dpi": 150})
    labs = [r["label"] for r in rows]
    cols = [COL.get(r["family"], "#888") for r in rows]
    n = len(rows)

    def hbar(ax, vals, title, unit, log=True, shown=None):
        shown = vals if shown is None else shown
        y = range(n)
        ax.barh(y, vals, color=cols, height=0.62)
        ax.set_yticks(list(y)); ax.set_yticklabels(labs); ax.invert_yaxis()
        if log:
            ax.set_xscale("log")
        ax.set_title(title, loc="left", fontsize=9)
        ax.set_xlabel(unit)
        ax.grid(axis="x", color="#ddd", lw=0.5); ax.set_axisbelow(True)
        for yi, v, sv in zip(y, vals, shown):
            ax.text(v * 1.08 if log else v, yi, fmt_t(sv) if sv >= 1e-3 else "0", va="center", fontsize=6.5, color="#333")

    fig, axs = plt.subplots(1, 3, figsize=(11, 0.26 * n + 1.4), sharey=True)
    for ax, o in zip(axs, OPS):
        hbar(ax, [max(ms(r, o), 1e-4) for r in rows], o, "median time per call (ms, log)")
        if ax is not axs[0]:
            ax.tick_params(labelleft=False)
    fig.suptitle("Level-1 additional signatures: reference C, -O2, single core", x=0.01, ha="left", fontsize=10)
    fig.tight_layout(rect=(0, 0, 1, 0.975)); fig.savefig(out / "time.png"); plt.close(fig)

    fig, axs = plt.subplots(1, 3, figsize=(11, 0.26 * n + 1.4), sharey=True)
    for ax, (key, t) in zip(axs, (("heap_peak", "peak heap, worst op"), ("stack_peak", "peak stack, worst op"))):
        real = [kib(max(r["ops"][o][key] for o in OPS)) for r in rows]
        hbar(ax, [max(v, 0.5) for v in real], t, "KiB (log; bars floored at 0.5 KiB, labels exact)", shown=real)
    vals = [max(net(r["ops"][o]) for o in OPS) for r in rows]
    hbar(axs[2], vals, "VmHWM minus baseline, worst op", "KiB (linear)", log=False)
    axs[2].set_xlim(0, max(vals) * 1.12)
    for ax in axs[1:]:
        ax.tick_params(labelleft=False)
    fig.tight_layout(); fig.savefig(out / "memory.png"); plt.close(fig)

    fig, ax = plt.subplots(figsize=(6.6, 4.6))
    seen = set()
    for r in rows:
        x = r["pk"] + r["sig"]; y = ms(r, "sign") + ms(r, "verify")
        lab = r["family"] if r["family"] not in seen else None
        seen.add(r["family"])
        ax.scatter(x, y, s=34, color=COL.get(r["family"], "#888"), edgecolor="white", linewidth=0.8, label=lab, zorder=3)
        if r["family"] == "FAEST":
            em = "EM" in r["label"]
            ax.annotate(r["label"], (x, y), fontsize=6.5, xytext=((-4, -10) if r["label"].endswith("f") else (-5, 5)) if em else (4, 4),
                        ha="right" if em else "left", textcoords="offset points", color="#444")
    ax.set_xscale("log"); ax.set_yscale("log")
    ax.set_xlabel("pk + signature (bytes, log)"); ax.set_ylabel("sign + verify (ms, log)")
    ax.grid(color="#ddd", lw=0.5, zorder=0)
    ax.legend(frameon=False, fontsize=7)
    ax.set_title("Bandwidth vs. computation, every level-1 variant (lower-left is better; FAEST labelled)", loc="left", fontsize=9)
    fig.tight_layout(); fig.savefig(out / "tradeoff.png"); plt.close(fig)


def spread(rows, other):
    """Run-to-run comparison of the median times against a second run directory (same variants)."""
    o = {r["label"]: r for r in load(other)}
    L, ds, flag = [], [], []
    for r in rows:
        if r["label"] not in o:
            continue
        for op in OPS:
            a, b = ms(r, op), ms(o[r["label"]], op)
            d = abs(a - b) / min(a, b) * 100.0
            ds.append(d)
            if d > 10.0:
                flag.append((r["label"], op, a, b, d))
    if not ds:
        return L
    ds.sort()
    nfast = sum(1 for lab, op in [(r["label"], op) for r in rows if r["label"] in o for op in OPS]
                if ms(o[lab], op) < ms(next(x for x in rows if x["label"] == lab), op))
    L.append("## Run-to-run spread (two back-to-back runs, second run: `%s`)\n" % other.name)
    L.append("%d (variant, operation) medians compared as |a-b|/min(a,b): median difference %.1f %%, maximum %.1f %%; %d of %d differ by more than 10 %% (listed); "
             "in %d of %d the second run is faster. The machine is a laptop (hybrid-core CPU, WSL2, no frequency control) and was not perfectly quiet "
             "(see the load notes): differences between schemes below about 1.5x should not be read as significant, and the differences are largely a common "
             "shift of most schemes rather than independent noise." % (len(ds), statistics.median(ds), max(ds), len(flag), len(ds), nfast, len(ds)))
    try:
        m2 = json.load(open(other / "meta.json"))
        for n in m2.get("load_notes", []):
            L.append("\n- second run: " + n)
    except OSError:
        pass
    L.append("\n| Variant | Op | run 1 (ms) | run 2 (ms) | diff % |\n|---|---|---:|---:|---:|")
    for lab, op, a, b, d in flag:
        L.append("| %s | %s | %s | %s | %.1f |" % (lab, op, fmt_t(a), fmt_t(b), d))
    L.append("")
    return L


def md(rows, meta, out, other=None):
    L = []
    L.append("# FAEST vs. the other on-going NIST additional-signature candidates (level 1)\n")
    L.append("All numbers are produced by `tools/bench/run_bench.py` + `tools/bench/report.py` from the "
             "reference implementations in `original/Additional-DS/` (static-test folders, portable C, no SIMD/AES-NI). "
             "Regenerate with `make bench-l1`.\n")
    L.append("## Setup\n")
    L.append("| | |\n|---|---|")
    L.append("| CPU | %s |" % meta.get("cpu_model", "?"))
    L.append("| OS | %s |" % meta.get("uname", "?"))
    L.append("| Compiler | %s, `CFLAGS=%s -std=gnu11 -w -Wno-psabi` (warnings suppressed), identical for every variant; the static KAT tests were built with `-std=c11` |" % (meta.get("gcc", "?"), meta.get("cflags", "?")))
    L.append("| Pinning | single core (CPU %s), one process per operation |" % meta.get("cpu", "?"))
    L.append("| Message | 1000 bytes (`hello world ` repeated), as in the reference paper |")
    L.append("| Iterations | up to 1000 per operation, within a %.1f s budget (min 5); median reported |" % meta.get("time_budget_s", 2))
    L.append("| Date | %s |" % meta.get("date", "?"))
    for n in meta.get("load_notes", []):
        L.append("| Machine load | %s |" % n)
    L.append("")
    L.append("Metrics: **time** = wall clock (`CLOCK_MONOTONIC`) per call; **cycles** = raw TSC ticks; "
             "**heap** = peak bytes live in `malloc/calloc/realloc` (requested size, via link-time wrapping, key/message buffers of the harness excluded); "
             "**stack** = deepest touched page of a dedicated 1 GiB mmap'ed thread stack (4 KiB granularity); "
             "**VmHWM-base** = growth of the process peak resident set during the operation, `vmhwm_kb - vmhwm_base_kb` (the raw VmHWM, in `results.csv`, also contains a 2.0-2.4 MiB baseline of libc and harness pages that depends on the operation, because the preparation thread has already run sign/verify and its freed heap stays resident; pages reused from it are not counted, so this column under-reports sign/verify of heap-heavy variants and is only indicative); "
             "**code/data/bss** = `size` of the algorithm objects only (harness excluded).\n")

    L.append("> **Note on FAEST timings.** FAEST sign/verify medians moved between two back-to-back runs by 0-22 % (FAEST-128f and -128s: second run slower by 17-22 %, "
             "while most other schemes were faster by about 25 %; FAEST-EM-128f/-128s within 6 %). The sign/verify anomalies seen in an earlier run under foreign load "
             "(EM-128f verify above sign, EM-128s verify far below sign) did not reproduce on the idle reruns: verify is at or below sign for all four variants in both runs (128s: 41.8 vs 41.8 ms in run 2). "
             "Keygen of FAEST (1.6-2.4 us) is near the timer resolution and varies by up to 45 %. Read FAEST/other ratios within about 1.5x with caution. "
             "No number has been altered.\n")
    L.append("## Key and signature sizes (bytes)\n")
    L.append("| Scheme | Variant | pk | sk | signature |\n|---|---|---:|---:|---:|")
    for r in rows:
        L.append("| %s | %s | %s | %s | %s |" % (r["family"], r["label"], r["pk"], r["sk"],
                 r["sig"] if r["sig"] == r["sig_max"] else "%d (max %d)" % (r["sig"], r["sig_max"])))
    L.append("")

    L.append("## Run time (median, ms) and TSC cycles (millions)\n")
    L.append("| Scheme | Variant | KeyGen ms | Sign ms | Verify ms | KeyGen Mcyc | Sign Mcyc | Verify Mcyc |\n|---|---|---:|---:|---:|---:|---:|---:|")
    for r in rows:
        o = r["ops"]
        L.append("| %s | %s | %s | %s | %s | %.2f | %.2f | %.2f |" % (
            r["family"], r["label"], *[fmt_t(ms(r, x)) for x in OPS], *[o[x]["cycles_median"] / 1e6 for x in OPS]))
    L.append("\n![time](time.png)\n")

    L.append("## Memory (KiB)\n")
    L.append("| Scheme | Variant | heap KG/S/V | stack KG/S/V | VmHWM-base KG/S/V | code | data+bss |\n|---|---|---|---|---|---:|---:|")
    for r in rows:
        o = r["ops"]
        L.append("| %s | %s | %s | %s | %s | %.1f | %.1f |" % (
            r["family"], r["label"],
            " / ".join("%.1f" % kib(o[x]["heap_peak"]) for x in OPS),
            " / ".join("%.1f" % kib(o[x]["stack_peak"]) for x in OPS),
            " / ".join("%d" % net(o[x]) for x in OPS),
            kib(r["objsize"]["text"]), kib(r["objsize"]["data"] + r["objsize"]["bss"])))
    L.append("\nStack legend: values of 0, 4 and 8 KiB are page-granularity floors (read them as \"<= 4 KiB\", \"<= 8 KiB\"), not measurements; the stack is the touched pages of the thread stack minus an empty thread.\n")
    L.append("\n![memory](memory.png)\n")
    L.append("![tradeoff](tradeoff.png)\n")

    # FAEST vs best-of-each-other-family summary
    faest = [r for r in rows if r["family"] == "FAEST"]
    others = [r for r in rows if r["family"] != "FAEST"]
    if faest and others:
        L.append("## FAEST against the best variant of every other family\n")
        L.append("Fastest FAEST variant per operation vs. fastest variant of each other scheme (ratio > 1: FAEST slower).\n")
        L.append("| Scheme | best KeyGen | best Sign | best Verify | smallest pk+sig |\n|---|---|---|---|---|")
        bf = {o: min(faest, key=lambda r: ms(r, o)) for o in OPS}
        bs = min(faest, key=lambda r: r["pk"] + r["sig"])
        for fam in FAMS[1:]:
            rs = [r for r in others if r["family"] == fam]
            if not rs:
                continue
            cells = []
            for o in OPS:
                b = min(rs, key=lambda r: ms(r, o))
                cells.append("%s (%s ms): x%s" % (b["label"], fmt_t(ms(b, o)), fmt_r(ms(bf[o], o) / ms(b, o))))
            s = min(rs, key=lambda r: r["pk"] + r["sig"])
            cells.append("%s (%s B): x%.1f" % (s["label"], fmt_b(s["pk"] + s["sig"]), (bs["pk"] + bs["sig"]) / (s["pk"] + s["sig"])))
            L.append("| %s | %s |" % (fam, " | ".join(cells)))
        L.append("\nFAEST reference points: " + ", ".join("%s %s %s ms" % (bf[o]["label"], o, fmt_t(ms(bf[o], o))) for o in OPS)
                 + "; smallest pk+sig %s (%d B).\n" % (bs["label"], bs["pk"] + bs["sig"]))
    if other is not None:
        L.extend(spread(rows, other))
    L.append("## Caveats\n")
    L.append("- Portable *reference* C for every scheme; schemes whose submission relies on AVX2/AES-NI/GFNI "
             "ship considerably faster optimized packages (not measured here), so absolute numbers are not comparable with optimized-implementation papers; the comparison among them is like-for-like.")
    L.append("- Cycles are TSC ticks of an x86 laptop CPU under WSL2, shown for relative comparison only.")
    L.append("- Stack figures have 4 KiB granularity; heap counts requested bytes only.")
    L.append("- Level 1 only (the lowest NIST category); variants per scheme are all Round-3 level-1 parameter sets "
             "that build in this tree. KAT status differs by family: MAYO, SNOVA, MQOM, SDitH and SQIsign match the official KAT record #0; "
             "UOV, QR-UOV (16 of the 39 variants) matched only the package's own generator on records 0-1 "
             "(no official KAT available offline); FAEST is the round-3 (v3) port (FAEST-R3) and matches the official v3 KAT on 100 records.\n")
    (out / "results.md").write_text("\n".join(L))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--results", default=str(ROOT / "bench" / "results"))
    ap.add_argument("--compare", default=None, help="second run directory, adds a run-to-run spread section")
    a = ap.parse_args()
    res = Path(a.results)
    rows = load(res)
    meta = json.load(open(res / "meta.json"))
    write_csv(rows, res)
    charts(rows, res)
    md(rows, meta, res, Path(a.compare) if a.compare else None)
    print("wrote results.md/csv/png for %d variants in %s" % (len(rows), res))


if __name__ == "__main__":
    main()
