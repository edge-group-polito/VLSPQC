#!/usr/bin/env python3
"""
gen_size_table.py -- pk / sk / ciphertext-or-signature sizes of every variant in
tools/algo_registry.json, taken from the CRYPTO_*BYTES constants of the variant's own api.h
(a tiny C program that includes it is compiled and run, nothing is typed by hand).

  CRYPTO_PUBLICKEYBYTES  -> pk
  CRYPTO_SECRETKEYBYTES  -> sk
  CRYPTO_CIPHERTEXTBYTES -> ct   (KEM shapes: kem_*)
  CRYPTO_BYTES           -> sig  (signature shapes; for several schemes this is the
                                  maximum signature expansion, i.e. the "signature
                                  overhead" of the sm = sig || m format)

Outputs
  results/sizes.csv                    one row per variant (empty size cells = does not compile standalone)
  <readme> between marker lines       with --update-readmes: tables regenerated in place, see MARKERS below

Usage (from the repo root):
  python3 -I tools/gen_size_table.py                    # write results/sizes.csv
  python3 -I tools/gen_size_table.py --update-readmes   # also rewrite the marked tables in the READMEs
"""
import argparse, csv, json, shlex, subprocess, sys, tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
REG = ROOT / "tools" / "algo_registry.json"

# README tables kept up to date by --update-readmes:  <!-- BEGIN sizes:<id> --> ... <!-- END sizes:<id> -->
# id -> (readme path, list of registry families, level of detail)
MARKERS = {
    "root":     ("README.md", ["mlkem", "hqc2025", "mldsa", "slhdsa", "falcon", "fndsa-exp"], "variants"),
    "kem":      ("algorithms/kem/README.md", ["mlkem", "hqc2025"], "variants"),
    "standard": ("algorithms/signatures/standard/README.md", ["mldsa", "slhdsa", "falcon", "fndsa-exp"], "variants"),
    "legacy":   ("algorithms/legacy/README.md", ["slhdsa-haraka", "slhdsa-haraka-aes"], "variants"),
    "stateful": ("algorithms/signatures/stateful/README.md", ["xmss", "lms", "hss"], "variants"),
    "onramp":   ("algorithms/signatures/onramp/README.md", None, "families"),
}


def c_program(kem):
    third = "CRYPTO_CIPHERTEXTBYTES" if kem else "CRYPTO_BYTES"
    return ('#include <stdio.h>\n#include "api.h"\n'
            'int main(void) { printf("%%lu %%lu %%lu\\n", (unsigned long)CRYPTO_PUBLICKEYBYTES, '
            '(unsigned long)CRYPTO_SECRETKEYBYTES, (unsigned long)%s); return 0; }\n' % third)


def sizes_of(name, v, tmp):
    kem = v["shape"].startswith("kem_")
    src = tmp / "sz.c"
    src.write_text(c_program(kem))
    incs = [v["include_dir"], v["src_dir"]] + list(v.get("extra_dirs", []))
    cmd = ["gcc", "-std=gnu11", "-w"] + ["-I" + str(ROOT / d) for d in incs]
    cmd += shlex.split(v.get("static_gcc_flags", "")) + [str(src), "-o", str(tmp / "sz"), "-lm"]
    r = subprocess.run(cmd, capture_output=True, text=True, cwd=ROOT)
    if r.returncode != 0:
        return None, "compile: " + (r.stderr.strip().splitlines() or ["?"])[0][:120]
    out = subprocess.run([str(tmp / "sz")], capture_output=True, text=True).stdout.split()
    if len(out) != 3:
        return None, "run failed"
    return [int(x) for x in out], ""


def collect():
    reg = json.load(open(REG))
    rows = []
    with tempfile.TemporaryDirectory() as t:
        tmp = Path(t)
        for name, v in reg["variants"].items():
            if v["shape"] in ("aead_direct", "hash_direct"):
                continue
            sz, err = sizes_of(name, v, tmp)
            kem = v["shape"].startswith("kem_")
            rows.append({"family": v["family"], "variant": name, "kind": "kem" if kem else "sig",
                         "pk_bytes": sz[0] if sz else "", "sk_bytes": sz[1] if sz else "",
                         "ct_or_sig_bytes": sz[2] if sz else "", "note": err})
    return rows


def md_variants(rows):
    L = ["| Variant | pk (B) | sk (B) | ct / sig (B) |", "|---|---:|---:|---:|"]
    for r in rows:
        if r["pk_bytes"] == "":
            L.append("| %s | n/a | n/a | n/a |" % r["variant"])
        else:
            L.append("| %s | %d | %d | %d |" % (r["variant"], r["pk_bytes"], r["sk_bytes"], r["ct_or_sig_bytes"]))
    return "\n".join(L)


def md_families(rows):
    fam = {}
    for r in rows:
        fam.setdefault(r["family"], []).append(r)
    L = ["| Family | Variants | pk (B) | sk (B) | sig (B, `CRYPTO_BYTES`) |", "|---|---:|---|---|---|"]
    def rng(rs, k):
        vals = [x[k] for x in rs if x[k] != ""]
        if not vals:
            return "n/a"
        return str(min(vals)) if min(vals) == max(vals) else "%d – %d" % (min(vals), max(vals))
    for f, rs in fam.items():
        ok = [x for x in rs if x["pk_bytes"] != ""]
        L.append("| %s | %d (%d measured) | %s | %s | %s |" % (f, len(rs), len(ok), rng(rs, "pk_bytes"),
                                                             rng(rs, "sk_bytes"), rng(rs, "ct_or_sig_bytes")))
    return "\n".join(L)


def update(path, mid, table):
    p = ROOT / path
    s = p.read_text()
    b, e = "<!-- BEGIN sizes:%s -->" % mid, "<!-- END sizes:%s -->" % mid
    if b not in s or e not in s:
        print("  (no marker sizes:%s in %s, skipped)" % (mid, path))
        return
    i, j = s.index(b) + len(b), s.index(e)
    p.write_text(s[:i] + "\n" + table + "\n" + s[j:])
    print("  updated sizes:%s in %s" % (mid, path))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--update-readmes", action="store_true")
    a = ap.parse_args()
    rows = collect()
    out = ROOT / "results"
    out.mkdir(exist_ok=True)
    with open(out / "sizes.csv", "w", newline="") as fh:
        w = csv.DictWriter(fh, fieldnames=list(rows[0]))
        w.writeheader()
        w.writerows(rows)
    bad = [r for r in rows if r["pk_bytes"] == ""]
    print("%d variants, %d with sizes, %d without (see 'note' column)" % (len(rows), len(rows) - len(bad), len(bad)))
    if a.update_readmes:
        for mid, (path, fams, mode) in MARKERS.items():
            sel = rows if fams is None else [r for r in rows if r["family"] in fams]
            if fams is None:
                sel = [r for r in rows if r["family"] not in ("mlkem", "hqc2025", "mldsa", "slhdsa", "falcon", "fndsa-exp",
                                                              "slhdsa-haraka", "slhdsa-haraka-aes", "xmss", "lms", "hss")]
            update(path, mid, md_variants(sel) if mode == "variants" else md_families(sel))


if __name__ == "__main__":
    main()
