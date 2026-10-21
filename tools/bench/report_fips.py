#!/usr/bin/env python3
"""
report_fips.py -- turn <results>/raw/*.json written by run_bench.py (variants_fips.json) into
<results>/results.csv and <results>/results.md. Only measured numbers are written.
For KEM variants "sign" is encaps and "verify" is decaps; the "sig/ct" column is the ciphertext size.

Usage: python3 -I tools/bench/report_fips.py --results bench/results_fips_<date>
"""
import argparse, csv, json
from pathlib import Path

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--results", required=True)
    a = ap.parse_args()
    R = Path(a.results)
    meta = json.load(open(R / "meta.json"))
    rows = []
    for f in sorted((R / "raw").glob("*.json")):
        v = json.load(open(f))
        kem = v["shape"].startswith("kem")
        names = ("keygen", "encaps" if kem else "sign", "decaps" if kem else "verify")
        base = {"label": v["label"], "family": v["family"], "level": v["level"], "kem": int(kem)}
        if "error" in v:
            rows.append(dict(base, error=v["error"].splitlines()[0])); continue
        ops = v["ops"]
        r = dict(base)
        for op, nm in zip(("keygen", "sign", "verify"), names):
            o = ops.get(op, {})
            if "ns_median" not in o:
                r[nm + "_error"] = o.get("error", "?"); continue
            r[nm + "_ms_median"] = o["ns_median"] / 1e6
            r[nm + "_heap_B"] = o["heap_peak"]
            r[nm + "_stack_B"] = o["stack_peak"]
            r[nm + "_vmhwm_net_kB"] = o["vmhwm_kb"] - o["vmhwm_base_kb"]
            r[nm + "_iters"] = o["iters"]
        k = ops.get("keygen", {})
        r["pk_B"], r["sk_B"] = k.get("pk_bytes"), k.get("sk_bytes")
        s = ops.get("sign", {})
        r["sig_or_ct_B"] = s.get("sig_bytes")
        r["code_B"], r["data_B"], r["bss_B"] = v["objsize"]["text"], v["objsize"]["data"], v["objsize"]["bss"]
        rows.append(r)
    keys = []
    for r in rows:
        for k in r:
            if k not in keys: keys.append(k)
    with open(R / "results.csv", "w", newline="") as fh:
        w = csv.DictWriter(fh, fieldnames=keys); w.writeheader(); w.writerows(rows)

    def g(r, k, fmt="%.3f"):
        return (fmt % r[k]) if k in r and r[k] is not None else "n/a"
    L = ["# FIPS-migration benchmark (portable reference C, x86-64)", "",
         "Host: %s; compiler: %s; flags: `%s`; CPU pinned: %s; date: %s." % (
             meta.get("cpu_model"), meta.get("gcc"), meta.get("cflags"), meta.get("cpu"), meta.get("date")),
         "Per-operation median wall time (`CLOCK_MONOTONIC`) over up to the iteration cap within a budget of %s s per operation; "
         "heap = peak malloc'ed bytes, stack = touched stack pages (4 KiB granularity), net VmHWM growth. "
         "KEM rows: sign = encaps, verify = decaps. Raw per-variant data in `raw/`, full table in `results.csv`." % meta.get("time_budget_s"), "",
         "| Variant | NIST cat. | keygen ms | sign/encaps ms | verify/decaps ms | pk B | sk B | sig/ct B | heap B (kg/sg/vf) | stack B (kg/sg/vf) | code B |",
         "|---|---|---|---|---|---|---|---|---|---|---|"]
    for r in rows:
        if "error" in r:
            L.append("| %s | %s | ERROR: %s |||||||||" % (r["label"], r["level"], r["error"])); continue
        n = ("keygen", "encaps" if r["kem"] else "sign", "decaps" if r["kem"] else "verify")
        L.append("| %s | %s | %s | %s | %s | %s | %s | %s | %s/%s/%s | %s/%s/%s | %s |" % (
            r["label"], r["level"], *[g(r, x + "_ms_median") for x in n],
            r["pk_B"], r["sk_B"], r["sig_or_ct_B"],
            *[g(r, x + "_heap_B", "%d") for x in n], *[g(r, x + "_stack_B", "%d") for x in n], r["code_B"]))
    (R / "results.md").write_text("\n".join(L) + "\n")
    print("wrote", R / "results.csv", "and", R / "results.md", "(%d rows)" % len(rows))

if __name__ == "__main__":
    main()
