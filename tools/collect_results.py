#!/usr/bin/env python3
"""
collect_results.py -- gather measured results into tracked CSVs under results/.

  results/benchmark.csv          long format, one row per (variant, operation), from the raw JSON files
                                 written by tools/bench/run_bench.py (host benchmark, `make bench-fips`)
  results/benchmark_meta.json    host / compiler / flags / date of that run (copied from its meta.json)
  results/benchmark_onramp_l1.csv (+ _spread.csv)
                                 the per-metric median of 3 runs of the level-1 on-ramp benchmark (`make bench-l1`, 39 variants of the
                                 Round-3 families), copied from its directory with --onramp-l1; also renders the table between the
                                 `bench:onramp-l1` markers of the on-ramp README
  results/static_kat_status.csv  pass/fail per variant, parsed from the log files written by
                                 tools/run_all_static_kat.sh (the static, no-file-I/O KAT tests; the "time" there is
                                 compile + generation + run, not a benchmark)

Nothing is invented: a figure that was not measured simply has no row. Cycle counts on the CVA6 / RISC-V targets
need the hardware flow (tools/gen_static_test.py --profiling, see docs/static-tests.md) and are NOT produced here.

Usage (from the repo root):
  python3 -I tools/collect_results.py --bench bench/results_fips_20261020 \\
      --static-log bench/results_fips_20261020/logs/p2final_static_standard.log [--static-log ...]
"""
import argparse, csv, json, re, shutil
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "results"


def bench_rows(bench_dir):
    rows = []
    for f in sorted((bench_dir / "raw").glob("*.json")):
        v = json.load(open(f))
        if "ops" not in v:
            continue
        kem = v["shape"].startswith("kem")
        names = {"keygen": "keygen", "sign": "encaps" if kem else "sign", "verify": "decaps" if kem else "verify"}
        k = v["ops"].get("keygen", {})
        for op, nm in names.items():
            o = v["ops"].get(op, {})
            if "ns_median" not in o:
                continue
            rows.append({
                "family": v["family"], "variant": v["label"], "nist_category": v["level"], "kind": "kem" if kem else "sig",
                "operation": nm, "ns_median": int(o["ns_median"]), "ns_min": int(o["ns_min"]), "iters": o["iters"],
                "heap_peak_B": o["heap_peak"], "stack_peak_B": o["stack_peak"],
                "vmhwm_net_kB": o["vmhwm_kb"] - o["vmhwm_base_kb"],
                "pk_B": k.get("pk_bytes", ""), "sk_B": k.get("sk_bytes", ""),
                "ct_or_sig_B": v["ops"].get("sign", {}).get("sig_bytes", ""),
                "code_B": v["objsize"]["text"], "data_B": v["objsize"]["data"], "bss_B": v["objsize"]["bss"],
                "note": v.get("note", ""), "source": str(bench_dir.relative_to(ROOT)),
            })
    return rows


LINE = re.compile(r"^\[(PASS|FAIL|SKIP|GEN_FAIL|COMPILE_FAIL)\s*\]\s+(\S+)\s+\((\S+)\s*\)\s+([\d.]+)s\s+(.*)$")


def static_rows(logs):
    rows = []
    for lg in logs:
        for line in open(lg, errors="replace"):
            m = LINE.match(line.strip())
            if m:
                rows.append({"status": m.group(1), "variant": m.group(2), "family": m.group(3),
                             "gen_compile_run_s": m.group(4), "detail": m.group(5)[:80], "log": str(Path(lg).relative_to(ROOT))})
    return rows


def write_csv(path, rows):
    if not rows:
        return
    with open(path, "w", newline="") as fh:
        w = csv.DictWriter(fh, fieldnames=list(rows[0]))
        w.writeheader()
        w.writerows(rows)
    print("wrote", path.relative_to(ROOT), "(%d rows)" % len(rows))


def onramp_l1(src):
    """Copy the earlier level-1 on-ramp benchmark (median of 3 runs) into results/ and render its README table."""
    rows = list(csv.DictReader(open(src / "results.csv")))
    shutil.copy(src / "results.csv", OUT / "benchmark_onramp_l1.csv")
    shutil.copy(src / "spread.csv", OUT / "benchmark_onramp_l1_spread.csv")
    sp = [float(r["rel_spread_pct"]) for r in csv.DictReader(open(src / "spread.csv"))
          if r["metric"] in ("keygen_ms", "sign_ms", "verify_ms")]
    sp.sort()
    med = sp[len(sp) // 2] if sp else float("nan")
    L = ["| Scheme | Variant | pk (B) | sk (B) | sig (B) | keygen (ms) | sign (ms) | verify (ms) | heap, max of ops (KiB) | stack, max of ops (KiB) |",
         "|---|---|---:|---:|---:|---:|---:|---:|---:|---:|"]
    f = lambda x: ("%.3f" % float(x)) if float(x) < 10 else ("%.1f" % float(x))
    for r in rows:
        L.append("| %s | %s | %s | %s | %s | %s | %s | %s | %s | %s |" % (
            r["family"], r["variant"], r["pk_B"], r["sk_B"], r["sig_B"], f(r["keygen_ms"]), f(r["sign_ms"]), f(r["verify_ms"]),
            max(float(r["heap_%s_KiB" % o]) for o in ("keygen", "sign", "verify")),
            max(float(r["stack_%s_KiB" % o]) for o in ("keygen", "sign", "verify"))))
    p = ROOT / "algorithms" / "signatures" / "onramp" / "README.md"
    t = p.read_text()
    b, e = "<!-- BEGIN bench:onramp-l1 -->", "<!-- END bench:onramp-l1 -->"
    if b in t and e in t:
        i, j = t.index(b) + len(b), t.index(e)
        p.write_text(t[:i] + "\n" + "\n".join(L) + "\n" + t[j:])
        print("updated bench:onramp-l1 in", p.relative_to(ROOT), "(median timing spread between runs: %.1f %%)" % med)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--bench", type=Path, help="results directory of tools/bench/run_bench.py (contains raw/ and meta.json)")
    ap.add_argument("--onramp-l1", type=Path, help="results_median3 directory of the level-1 on-ramp benchmark (results.csv, spread.csv)")
    ap.add_argument("--static-log", type=Path, action="append", default=[], help="run_all_static_kat.sh log file (repeatable)")
    a = ap.parse_args()
    OUT.mkdir(exist_ok=True)
    if a.bench:
        b = a.bench if a.bench.is_absolute() else ROOT / a.bench
        write_csv(OUT / "benchmark.csv", bench_rows(b))
        shutil.copy(b / "meta.json", OUT / "benchmark_meta.json")
    if a.onramp_l1:
        onramp_l1(a.onramp_l1 if a.onramp_l1.is_absolute() else ROOT / a.onramp_l1)
    if a.static_log:
        logs = [p if p.is_absolute() else ROOT / p for p in a.static_log]
        write_csv(OUT / "static_kat_status.csv", static_rows(logs))


if __name__ == "__main__":
    main()
