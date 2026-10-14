#!/usr/bin/env python3
"""
run_bench.py -- build and run the single-operation benchmark (tools/bench/bench.c)
for a list of variants, using the flattened static-test folders produced by
tools/gen_static_test.py (kat/static-style, plain `gcc`, no external libs).

For every variant it
  1. (re)generates the static-test folder (1 KAT vector) under build/bench/static/<target>,
  2. compiles every algorithm .c (main.c excluded) to an object with one common
     CFLAGS set, links it with bench.c + memtrack.c (heap tracker),
  3. runs keygen / sign / verify, each in its own process pinned to one CPU,
  4. stores one JSON file per variant in <out>/raw/<label>.json (timings, heap,
     stack, VmHWM, key/signature sizes, object-code size).

Usage (from the repo root):
  python3 tools/bench/run_bench.py                       # all variants in variants_l1.json
  python3 tools/bench/run_bench.py --only FAEST MAYO     # label/family substrings
  python3 tools/bench/run_bench.py --cflags "-O3" --time 5 --cpu 3

Fairness notes: every variant is built from its portable reference C (no
SIMD/AES-NI paths) with the SAME compiler flags; wall time is
CLOCK_MONOTONIC, "cycles" are raw TSC ticks (x86), not core cycles.
"""
import argparse, json, os, re, shlex, subprocess, sys, time
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
WRAPS = ",".join("--wrap=" + f for f in
                 ("malloc", "calloc", "realloc", "free", "posix_memalign", "aligned_alloc", "memalign"))


def sh(cmd, **kw):
    return subprocess.run(cmd, capture_output=True, text=True, **kw)


def gen_static(target, outdir, regen):
    d = outdir / target
    if d.exists() and not regen:
        return d
    r = sh([sys.executable, "-I", str(ROOT / "tools" / "gen_static_test.py"),
            "--target", target, "--kat-num", "1", "--outdir", str(outdir)])
    if r.returncode != 0 or not d.exists():
        raise RuntimeError("static generation failed for %s:\n%s%s" % (target, r.stdout[-1500:], r.stderr[-1500:]))
    return d


def detect(d):
    """Find the header declaring randombytes_init and the smlen type used by the variant's main.c."""
    main = (d / "main.c").read_text(errors="replace")
    hdr = None
    for m in re.finditer(r'#include\s+"([^"]+\.h)"', main):
        h = d / m.group(1)
        if h.exists() and "randombytes_init" in h.read_text(errors="replace"):
            hdr = m.group(1)
            break
    if hdr is None:
        for h in sorted(d.glob("*.h")):
            if "randombytes_init" in h.read_text(errors="replace"):
                hdr = h.name
                break
    if hdr is None:
        raise RuntimeError("no header with randombytes_init in %s" % d)
    t = "unsigned long long"
    m = re.search(r'\b(unsigned long long|size_t)\s+smlen\b', main)
    if m:
        t = m.group(1)
    return hdr, t


def build(v, d, bdir, cflags, extra):
    bdir.mkdir(parents=True, exist_ok=True)
    hdr, lent = detect(d)
    srcs = sorted(p for p in d.glob("*.c") if p.name != "main.c")
    objs = []
    base = shlex.split(cflags) + ["-std=gnu11", "-Wno-psabi", "-w", "-I" + str(d)] + extra
    for s in srcs:
        o = bdir / (s.stem + ".o")
        r = sh(["gcc", "-c"] + base + [str(s), "-o", str(o)])
        if r.returncode != 0:
            raise RuntimeError("compile %s failed:\n%s" % (s.name, r.stderr[-2000:]))
        objs.append(o)
    defs = ["-DBENCH_RNG_HDR=\"%s\"" % hdr, "-DBENCH_LEN_T=%s" % lent, "-DBENCH_NAME=\"%s\"" % v["label"]]
    hobjs = []
    for s in (HERE / "bench.c", HERE / "memtrack.c"):
        o = bdir / ("_h_" + s.stem + ".o")
        r = sh(["gcc", "-c"] + base + defs + [str(s), "-o", str(o)])
        if r.returncode != 0:
            raise RuntimeError("compile %s failed:\n%s" % (s.name, r.stderr[-2000:]))
        hobjs.append(o)
    exe = bdir / "bench"
    r = sh(["gcc"] + [str(o) for o in objs + hobjs] + ["-Wl,%s" % WRAPS, "-lm", "-lpthread", "-o", str(exe)])
    if r.returncode != 0:
        raise RuntimeError("link failed:\n" + r.stderr[-2000:])
    # object-code footprint of the algorithm only (harness excluded)
    text = data = bss = 0
    r = sh(["size"] + [str(o) for o in objs])
    for line in r.stdout.splitlines()[1:]:
        f = line.split()
        if len(f) >= 3 and f[0].isdigit():
            text += int(f[0]); data += int(f[1]); bss += int(f[2])
    return exe, {"text": text, "data": data, "bss": bss}


def run_ops(exe, cpu, env_extra):
    env = dict(os.environ, **env_extra)
    res = {}
    for op in ("keygen", "sign", "verify"):
        def pin():
            if cpu >= 0:
                os.sched_setaffinity(0, {cpu})
        r = subprocess.run([str(exe), op], capture_output=True, text=True, env=env, preexec_fn=pin, timeout=3600)
        if r.returncode != 0:
            res[op] = {"error": (r.stderr or r.stdout)[-500:].strip()}
            continue
        res[op] = json.loads(r.stdout.strip().splitlines()[-1])
    return res


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--config", default=str(HERE / "variants_l1.json"))
    ap.add_argument("--only", nargs="*", default=[], help="keep variants whose label/family/target contains one of these")
    ap.add_argument("--out", default=str(ROOT / "bench" / "results"))
    ap.add_argument("--cflags", default="-O2")
    ap.add_argument("--time", type=float, default=2.0, help="wall-clock budget per operation (s)")
    ap.add_argument("--min-iters", type=int, default=5)
    ap.add_argument("--max-iters", type=int, default=1000)
    ap.add_argument("--cpu", type=int, default=2, help="CPU to pin to (-1 = no pinning)")
    ap.add_argument("--jobs", type=int, default=4, help="parallel compile jobs")
    ap.add_argument("--regen", action="store_true", help="regenerate static-test folders")
    a = ap.parse_args()

    cfg = json.load(open(a.config))
    vs = cfg["variants"]
    if a.only:
        vs = [v for v in vs if any(k.lower() in (v["label"] + v["family"] + v["target"]).lower() for k in a.only)]
    out = Path(a.out); (out / "raw").mkdir(parents=True, exist_ok=True)
    broot = ROOT / "build" / "bench"
    sroot = broot / "static"; sroot.mkdir(parents=True, exist_ok=True)

    print("[bench] %d variants, CFLAGS=%s" % (len(vs), a.cflags))
    built = {}

    def prep(v):
        try:
            d = gen_static(v["target"], sroot, a.regen)
            exe, size = build(v, d, broot / v["target"], a.cflags, v.get("cflags_extra", []))
            return v["label"], (exe, size, None)
        except Exception as e:  # keep going, report at the end
            return v["label"], (None, None, str(e))

    # static generation shells out to make (shared build/ dir) -> keep it sequential,
    # compilation inside build() is per-variant and cheap to serialize as well.
    for v in vs:
        lab, r = prep(v)
        built[lab] = r
        print("[build] %-28s %s" % (lab, "ok" if r[2] is None else "FAILED: " + r[2].splitlines()[0]))

    env = {"BENCH_TIME_S": str(a.time), "BENCH_MIN_ITERS": str(a.min_iters), "BENCH_MAX_ITERS": str(a.max_iters)}
    meta = {"cflags": a.cflags, "cpu": a.cpu, "time_budget_s": a.time, "date": time.strftime("%Y-%m-%d %H:%M:%S"),
            "gcc": sh(["gcc", "--version"]).stdout.splitlines()[0], "uname": " ".join(os.uname()),
            "cpu_model": next((l.split(":", 1)[1].strip() for l in open("/proc/cpuinfo") if l.startswith("model name")), "?")}
    json.dump(meta, open(out / "meta.json", "w"), indent=1)

    for v in vs:
        exe, size, err = built[v["label"]]
        rec = dict(v)
        if err:
            rec["error"] = err
        else:
            t0 = time.time()
            rec["ops"] = run_ops(exe, a.cpu, env)
            rec["objsize"] = size
            print("[run]   %-28s %.1fs  keygen=%s sign=%s verify=%s" % (
                v["label"], time.time() - t0,
                *[("%.3f ms" % (rec["ops"][o]["ns_median"] / 1e6)) if "ns_median" in rec["ops"][o] else "ERR"
                  for o in ("keygen", "sign", "verify")]))
        json.dump(rec, open(out / "raw" / (v["label"] + ".json"), "w"), indent=1)


if __name__ == "__main__":
    main()
