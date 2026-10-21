# Static KAT test generator

Turns the dynamic KAT tests in this repo (which `fopen()` a `.rsp` file at
runtime — fine on a host, impossible on bare-metal / FPGA / RISC-V) into a
**static** test: one `main.c` with the KAT input/output vectors baked in as
C arrays, no file I/O, no dependency on anything but the algorithm's own
source files. The output layout matches what a target project (e.g.
`cva6/tests/<variant>/`) expects, so you can copy the generated folder
straight in.

Covers every variant in `tools/algo_registry.json` (279 entries): the 23 standardized /
standard-pending variants (ML-KEM, ML-DSA, the 12 FIPS 205 SLH-DSA sets, Falcon,
HQC — this is the set `static-test-all` builds), plus the experimental FN-DSA variants,
the legacy Haraka families, the on-ramp candidate signatures, one default variant each
from XMSS/LMS/HSS, and 7 ASCON variants (not PQC at all) — none of which are part of
`static-test-all`. See [Supported families](#supported-families) below.

**XMSS/LMS/HSS are a special case.** Their real APIs are
runtime-parametrized (one shared source tree covers every parameter set,
selected at keygen time — see [`../algorithms/signatures/stateful/README.md`](../algorithms/signatures/stateful/README.md)),
which doesn't fit `algo_registry.json`'s normal per-variant
`src_dir`/`include_dir` model (built for compile-time-fixed sizes). Each of
the three gets a **dedicated, self-contained folder** pinned to exactly one
parameter set instead —
[`algorithms/signatures/stateful/XMSS/XMSS-SHA2_10_256/`](../algorithms/signatures/stateful/XMSS/XMSS-SHA2_10_256),
[`algorithms/signatures/stateful/LMS/LMS_H10_W8/`](../algorithms/signatures/stateful/LMS/LMS_H10_W8),
[`algorithms/signatures/stateful/HSS/HSS_L2_H5_W8/`](../algorithms/signatures/stateful/HSS/HSS_L2_H5_W8) — each
holding a copy of the shared library plus a small `crypto_bridge.c` and
`api.h` that pin one variant and expose the usual fixed-size
`crypto_sign_keypair`/`crypto_sign`/`crypto_sign_open` convention (see the
`ds_derand_seed` shape below). Only that one variant per family is
registered — the much larger `-check`/`-check-slow` sweeps the dynamic flow
supports for these three (see `algorithms/signatures/stateful/README.md`) have no
static-test equivalent yet; extending this would mean picking a small
representative subset and adding a dedicated folder + registry entry per
variant the same way, since gen_static_test.py always needs one
self-contained folder per variant regardless of family.

**ASCON has no keygen at all** — its two shapes (`aead_direct`,
`hash_direct`) skip the derand/coins/DRBG-reseed indirection every other
shape needs, since `crypto_aead_encrypt`/`crypto_aead_decrypt`/`crypto_hash`
are already deterministic given their inputs directly. See
[`../algorithms/symmetric/ASCON/README.md`](../algorithms/symmetric/ASCON/README.md).

Two files drive everything:

- **`algo_registry.json`** — the single source of truth. For each
  supported variant it records which family it belongs to, which API
  "shape" it uses (see below), where its `src/`/`include/` live, and where
  its `.rsp` file lives.
- **`gen_static_test.py`** — the generator. Reads the registry, (re)runs
  the existing `make gen-<variant>` to get fresh KAT data, parses the
  `.rsp` in pure Python, and renders `test_vectors_<suffix>.h` + `main.c`.

## Quick start

From the repository root (the directory containing the top-level `Makefile`), via Makefile (recommended — this is what wires in
`KATNUM`/`RUN_*` consistently with the rest of the repo's targets):

```sh
# one specific variant, 1 KAT vector (the default)
make static-test-ml-kem-512

# whole family, 10 KAT vectors
make static-test-mlkem STATIC_KATNUM=10

# every standardized family (23 variants), 5 KAT vectors each -- same scope
# as `make test-all`; does NOT include the on-ramp / stateful / ASCON / legacy families
make static-test-all STATIC_KATNUM=5

# non-standardized / separate-category families, each with their own aggregate
make static-test-onramp-ds STATIC_KATNUM=5   # CROSS, HAWK, LESS, FAEST
make static-test-stateful-ds STATIC_KATNUM=5      # XMSS, LMS, HSS
make static-test-ascon STATIC_KATNUM=5            # ASCON AEAD + hash/XOF

# only test keygen + sign, skip verify (baked into main.c as #define RUN_VERIFY 0)
make static-test-ML-DSA-5 RUN_VERIFY=0

# HQC as one combined family (like the existing `test-hqc` dynamic target)
make static-test-hqc STATIC_KATNUM=3

# bake in cva6 cycle-counter hooks around keygen/sign/verify (see "Cycle-counter
# profiling" below); "none" (default) leaves the hooks present but empty
make static-test-ml-kem-512 PROFILING=cva6

# wipe vectors/static/ to force everything to regenerate from scratch
make clean-static
```

Output lands in `vectors/static/<variant>/` (override with `STATICDIR=...`).
`make clean-static` (or `make distclean`, which also does `clean`/`clean-rsp`)
removes it if you want a clean re-do.

Or call the script directly (same thing, more explicit flags):

```sh
python3 tools/gen_static_test.py \
    --target ml-kem-512 ML-DSA-5 \
    --kat-num 10 \
    --run-keygen 1 --run-encaps 1 --run-decaps 0 \
    --run-sign 1 --run-verify 1 \
    --outdir vectors/static
```

`--target` accepts family names (`mlkem`, `mldsa`, `falcon`, ...) and/or
specific variant names (`ml-kem-512`, `ML-DSA-5`, `CROSS_1_RSDP_b`, ...) in
any mix — repeat it or list several after one `--target`.

## What gets generated

```
vectors/static/ml-kem-512/
    main.c                  # for(i=0;i<N_KAT;i++) { keygen/encaps/decaps + memcmp }
    test_vectors_512.h      # TVEC_* arrays, one row per KAT vector
    kem.h, kem.c, ...       # variant's src/*.c + include/*.h, flattened (no subdirs)
```

- `main.c` always contains a single `for (i = 0; i < N_KAT; i++)` loop, even
  when `N_KAT == 1` — same code path regardless of how many vectors you
  asked for.
- `RUN_KEYGEN` / `RUN_SIGN` / `RUN_VERIFY` (DS) or `RUN_KEYGEN` /
  `RUN_ENCAPS` / `RUN_DECAPS` (KEM) are baked in as literal `#define X 0/1`
  at the top of `main.c` — not compile-time `-D` flags — so the generated
  file is self-contained and works with a toolchain that just compiles
  `main.c` + sources, no special flags needed.
- Disabling a stage doesn't remove its data: e.g. with `RUN_KEYGEN=0` the
  keypair is `memcpy`'d in from the vectors instead of computed, so later
  stages (sign/encaps) still have something to work with — **except for
  HQC-2025, see the caveat below.**
- Signature fields that aren't fixed-length across a KAT batch (Falcon's
  compressed `sm`, in practice) get a ragged-safe representation: per-entry
  named arrays + a pointer table, plus a `TVEC_X_LEN[N_KAT]` companion array
  of per-entry lengths, instead of a flat 2D array. Fields that emit this way
  are always indexed the same as fixed ones (`TVEC_X[i]`), so `main.c` never
  needs to know which representation got used.
- The generator always deletes the target's `.rsp` file *and* its compiled
  `gen_<variant>` binary before invoking `make gen-<variant>`. Necessary
  because that binary bakes `KATNUM` in at compile time and `make`'s
  dependency rule only tracks source-file mtimes — without this, a stale
  binary from an earlier run with a different `--kat-num` would silently
  get reused.

Compile/run the result like any other C program on the host, e.g.:

```sh
cd vectors/static/ml-kem-512
gcc -O2 -std=c11 *.c -o test && ./test
```

(`randombytes.c` is included in the flattened output for link-completeness
— it's dead code on the derand path this generator exercises, but the
non-derand wrapper functions in `kem.c`/`sign.c` still reference the
`randombytes()` symbol, so it has to be present. On a bare-metal target
without `/dev/urandom` you'd swap this file for a stub.)

## The RUN_KEYGEN=0 desync caveat (HQC-2025 only)

Most families expose an explicit-randomness ("derand") API — the exact
random bytes consumed during keygen/sign are captured in the `.rsp` and
replayed directly, so any stage can be disabled independently and the rest
still reproduces bit-for-bit.

CROSS, LESS, HAWK, and FAEST don't have that entry point; instead
determinism comes from reseeding an internal CSPRNG/DRBG with a captured
`seed` and then calling the *ordinary* (non-derand) API, exactly like the
existing dynamic tests in `harness/gen/main_{cross,less,hawk,faest}.c` already
do. `crypto_sign_keypair()` is always called for all four, even with
`RUN_KEYGEN=0`, purely to advance the shared CSPRNG/DRBG stream to the
position `crypto_sign()` expects — so disabling keygen still gets you a
correct signature check. This holds even for HAWK/FAEST, whose keygen
consumes a *variable* amount of randomness (an internal rejection-sampling
retry loop): only calling the real function reproduces its exact
consumption; a hand-replicated "throwaway read of N bytes" (viable for
CROSS/LESS, whose keygen draws a single fixed-size seed) can't be done
safely there.

**HQC-2025** is the one remaining family without that compensation — its
dynamic test (`harness/gen/main_hqc2025.c`) does *not* call `crypto_sign_keypair`
when `RUN_KEYGEN=0`. With `RUN_KEYGEN=0`, the subsequent encaps call starts
from the wrong PRNG offset and **will report a mismatch even though the
algorithm itself is correct.** This isn't a bug in the generator — the same
combination already behaves this way in `make test-hqc2025 RUN_KEYGEN=0`
today. If you need to test encaps in isolation, leave `RUN_KEYGEN=1`.

## Supported families

Standardized (part of `static-test-all`):

| Family key | Shape | # variants | Notes |
|---|---|---|---|
| `mlkem` | `kem_derand` | 3 | ML-KEM-512/768/1024 |
| `mldsa` | `ds_derand_ctx` | 3 | ML-DSA-44/65/87 |
| `falcon` | `ds_derand_rnd_nonce` | 2 | Falcon round 3 (not FN-DSA); ragged `sm` (variable-length sigs) |
| `slhdsa` | `ds_derand_rnd` | 12 | the 12 FIPS 205 sets; sub-families `slhdsa-sha2` / `slhdsa-shake` select 6 each |
| `hqc2025` | `kem_prng_reseed` | 3 | `HQC2025_1/3/5`; RUN_KEYGEN=0 desync caveat |

ML-KEM, ML-DSA and the experimental FN-DSA variants are built from single-compilation-unit trees with nested
`#include "src/…"` paths, so their registry entries set `"copy_subdirs": true` (sub-directories of `src_dir` are copied
into the static folder) and `static_gcc_flags` (e.g. `-I.`; FN-DSA also needs `-D_GNU_SOURCE` and the four
`-DFNDSA_*=0` options for its integer-only build).

Legacy and experimental (own targets, **not** part of `static-test-all`):

| Family key | Shape | # variants | Notes |
|---|---|---|---|
| `slhdsa-haraka` | `ds_derand_rnd` | 6 | pre-standard SPHINCS+ with Haraka, see [`../algorithms/legacy/README.md`](../algorithms/legacy/README.md) |
| `slhdsa-haraka-aes` | `ds_derand_rnd` | 6 | RISC-V only (`aes64esm`) — `make gen-*` no-ops on other hosts, generator raises a clear error rather than a traceback |
| `fndsa-exp` | `ds_derand_rnd_nonce` | 2 | experimental pre-standard FN-DSA (c-fn-dsa), see [`../algorithms/signatures/standard/FNDSA-EXP/UPSTREAM.md`](../algorithms/signatures/standard/FNDSA-EXP/UPSTREAM.md) |

Non-standardized / separate-category (each with its own aggregate target —
`static-test-onramp-ds`, `static-test-stateful-ds`, `static-test-ascon`
— **not** part of `static-test-all`):

| Family key | Shape | # variants | Notes |
|---|---|---|---|
| `hawk` | `ds_drbg_hawk` | 3 | |
| `less` | `ds_drbg_less` | 7 | |
| `cross` | `ds_drbg_cross` | 18 | |
| `faest` | `ds_drbg_hawk` | 12 | shape shared verbatim with HAWK |
| `xmss` | `ds_derand_seed` | 1 | `XMSS-SHA2_10_256` only — see "XMSS/LMS/HSS are a special case" above |
| `lms` | `ds_derand_seed` | 1 | `LMS_H10_W8` only |
| `hss` | `ds_derand_seed` | 1 | `HSS_L2_H5_W8` only |
| `mqom` | `ds_drbg_nist` | 18 | MQOM3 (constant `mlen`); official NIST DRBG flow, nothing skipped after reseed |
| `mayo-r3` | `ds_drbg_official` | 4 | Round-3 MAYO; registry flag `size_t_api` (smlen/mlen1 are `size_t`) |
| `uov-r3` | `ds_drbg_official` | 12 | Round-3 UOV |
| `snova-r3` | `ds_drbg_official` | 18 | Round-3 SNOVA |
| `qruov-r3` | `ds_drbg_official` | 30 | Round-3 QR-UOV |
| `sdith-r3` | `ds_drbg_official` | 12 | Round-3 SDitH; registry `static_gcc_flags: "-lm"` |
| `sqisign-r3` | `ds_drbg_official` | 3 | Round-3 SQIsign; registry `static_gcc_flags: "-I."` (project headers use `<x.h>` includes) |
| `faest-r3` | `ds_drbg_official` | 4 | FAEST v3 (round 3, `FAEST_<V>-R3`); plain `gcc *.c`, no extra flags |
| `ascon-aead` | `aead_direct` | 3 | ascon128v12, ascon128av12, ascon80pqv12; no keygen, no RUN_KEYGEN |
| `ascon-hash` | `hash_direct` | 4 | asconhashv12, asconhashav12, asconxofv12, asconxofav12; no RUN_* flags at all |

`make static-test-hqc` covers all 3 HQC-2025 variants at once, matching the
existing `test-hqc` dynamic target's scope (`hqc2025` is currently the only
HQC family registered). Likewise `make static-test-ascon` covers all 7 ASCON
variants (`static-test-ascon-aead` + `static-test-ascon-hash`).

`ds_drbg_official` (Round-3 families) is `ds_drbg_nist` with a per-record `mlen`/message (the official Round-3 KATs use
`mlen = 33*(count+1)`), so `STATIC_KATNUM` > 1 works for them. The optional registry key `static_gcc_flags` is
informational: the generator prints it, and the static folder must be compiled as `gcc *.c <flags>` (libraries after
the sources). Example: `make static-test-onramp-ds` now also covers MQOM and the seven `*-R3` families (family keys
`mayo-r3`, `uov-r3`, `snova-r3`, `qruov-r3`, `sdith-r3`, `sqisign-r3`, `faest-r3`; per-variant targets `static-test-<V>-R3`).

Verified on an x86_64 host during the FIPS migration (`tools/run_all_static_kat.sh`, 1 KAT vector, log summaries in
[`../results/validation.md`](../results/validation.md)): the 23 standard variants, the 2 FN-DSA variants, and a sample of the
moved families (CROSS, LESS, HAWK, the Haraka family, HQC); afterwards also all 232 on-ramp variants (228 with the default stack; the 4 older SNOVA level-5 variants need `ulimit -s 65536`, which this runner does not set). `slhdsa-haraka-aes` needs RISC-V hardware to even produce KAT data (see the table above).

## Adding a new family

1. Look at how its `main_<family>.c` under `harness/gen/` calls the algorithm
   (keypair/sign/verify or keypair/enc/dec signature, explicit-randomness or
   DRBG-reseed) and what field names its `PQCgenKAT_*.c` writes to the
   `.rsp`.
2. If that calling convention doesn't already match one of the 10 shapes in
   `SHAPE_RENDERERS` (see the table above and each `render_*` function's
   docstring in `gen_static_test.py`), add a new shape id + a
   `render_<shape>(...)` function. Reuse `c_2d_array()` for fixed-length
   fields and `c_var_field()` for anything whose length might vary across
   KAT entries (like a compressed signature) — it auto-detects ragged vs.
   uniform and always gives you a `NAME[i]` + `NAME_LEN[i]` pair to use in
   `main.c` regardless.
3. Add the variant(s) to `algo_registry.json` under `variants` (and a
   `families` entry if it's a new family), pointing at their `src_dir` /
   `include_dir` / `rsp_path` / `gen_target` / `vec_suffix`. If the family
   has a flat source layout (no `src/`/`include/` split), point both at the
   same directory and add `"exclude_files": ["main.c"]` so the variant's own
   reference `main.c` doesn't get copied over yours. If the family splits a
   shared-across-variants `common/` directory out of `src_dir`/`include_dir`
   (like FAEST), add an `"extra_dirs"` field pointing at it.
4. Add it to the `STATIC_ALL_VARIANTS` list and the family-target loop in
   the top-level `Makefile`'s static-test section.

## Cycle-counter profiling

Every generated `main.c` always contains a `#define PERF_CNT_CYCLES 0/1` and
`#if PERF_CNT_CYCLES ... #endif` guards at four fixed points: right after the
includes (platform-specific `#include`s), at the top of `main()` (counter
setup/instantiation), and immediately before/after each of the
keygen/sign-or-encaps/verify-or-decaps calls (reset-then-read). With
`PROFILING=none` (the default) these guards are always present but empty, so
`PERF_CNT_CYCLES` stays `0` and they compile away to nothing. Passing
`--profiling <name>` / `PROFILING=<name>` fills them in from
`tools/profiling_commands/<name>.txt` and sets `PERF_CNT_CYCLES 1`.

The actual counter code is never invented by the generator — it's always
copied verbatim from that file, which you maintain. The format is four
sections, in order:

```
# includes
<code inserted once, near the top of the file, guarded by #if PERF_CNT_CYCLES>

# init
<code inserted once at the top of main(), before the KAT loop>

# before
<code inserted immediately before each keygen/sign-or-encaps/verify-or-decaps call>

# after
<code inserted immediately after each of those calls>
```

See `tools/profiling_commands/cva6.txt` (uses `encoding.h`'s
`clear_csr`/`write_csr`/`read_csr` macros) and `xheep.txt` (uses `csr.h`'s
`CSR_CLEAR_BITS`/`CSR_WRITE`/`CSR_READ`) for two working examples — both
just reset a cycle counter before each call and read+print it after. Since
performance counters are inherently platform/core/SoC-specific, add a new
`<name>.txt` file here for any other target rather than hand-editing a
generated `main.c`.

The generator additionally prints a one-line stage label (`-- keygen --`,
`-- sign --` / `-- encaps --`, `-- verify --` / `-- decaps --`) right before
each "before" block when profiling is on, so the "Cycles: N" line that
follows is identifiable — that label is generated code, not part of your
profiling file.

## Platform flavor (prints)

Aside from profiling, the generated `main.c` otherwise only targets a host
(`printf`, no UART) — matching the existing hand-written static tests under
`algorithms/kem/ML-KEM/ml-kem-512/main.c` and `algorithms/signatures/standard/ML-DSA/ML-DSA-5/main.c`. There's no
flag yet to swap `printf`/`print_uart` wholesale the way
`cva6/tests/ml-kem-512/main.c` does; for now that still means hand-porting
the prints the same way that file was hand-ported.

## `run_all_static_kat.sh` — golden-model batch runner

> The batch runner now covers every registered family (including the on-ramp ones), adds the per-family compile flags of `tools/algo_registry.json` (`sdith-r3`: `-lm`; `fndsa-exp`: the integer-only options) and runs each test with a stack limit of 64 MiB (`--stack-kb`, default 65536). Some SNOVA level-5 ports overflow the usual 8 MiB stack; if the limit cannot be raised the test still runs with the default.

A wrapper around the whole static-test pipeline above, for running many
variants unattended and getting one consolidated report instead of eyeballing
each `make static-test-*` invocation by hand. By default, `--targets all`
only covers the standardized set (same scope as `make static-test-all`);
pass `--benchmark-all` to also include one representative variant from each
non-standardized/separate-category family (Additional-DS, Stateful-DS,
ASCON) — explicit `--targets cross` (or any specific name) always works
regardless of `--benchmark-all`. For each resulting variant it:

1. Runs `make static-test-<variant>` (always `PROFILING=none`, i.e. no cycle
   counters — this is a correctness run, not a profiling run).
2. Compiles the generated `vectors/static/<variant>/` folder on the host:
   `gcc -O2 -std=c11 *.c -o test`.
3. Runs `./test`.
4. Records `PASS` / `FAIL` / `SKIP` / `GEN_FAIL` / `COMPILE_FAIL` for that
   variant, then prints + writes a final summary to a timestamped log file
   (`kat_golden_model_<timestamp>.log` at the repo root by default).

```sh
# standardized families only, 1 KAT vector each (the default)
tools/run_all_static_kat.sh

# also include one representative Additional-DS/Stateful-DS/ASCON variant each
tools/run_all_static_kat.sh --benchmark-all

# just ML-KEM, 10 vectors
tools/run_all_static_kat.sh --kat-num 10 --targets mlkem

# sign/encaps only (keygen inputs come from the KAT vectors), specific variants
tools/run_all_static_kat.sh --run-mode sign --targets "ml-kem-512 ML-DSA-5 hawk"

# verify/decaps only, standardized families
tools/run_all_static_kat.sh --run-mode verify --targets all

# list every known family/variant name and exit
tools/run_all_static_kat.sh --list
```

| Flag | Default | Description |
|------|---------|--------------|
| `-k`, `--kat-num N` | `1` | `STATIC_KATNUM` value |
| `-m`, `--run-mode MODE` | `all` | `all` \| `keygen` \| `sign` \| `verify` — toggles `RUN_*` combos (see script header) |
| `-t`, `--targets LIST` | `all` | space-separated variant and/or family names, or `all` |
| `-b`, `--benchmark-all` | off | when `--targets` is `all`, also include one representative variant from each non-standardized family |
| `-l`, `--log FILE` | `kat_golden_model_<timestamp>.log` | log file path |
| `-c`, `--cc CC` | `gcc` | compiler |
| `--cflags FLAGS` | `-O2 -std=c11` | compiler flags |
| `--list` | — | print all known variant/family names and exit |

Each flag has an environment-variable equivalent (`KAT_NUM`, `RUN_MODE`,
`TARGETS`, `BENCHMARK_ALL`, `LOGFILE`, `CC`, `CFLAGS`); a command-line flag
always wins.

**Note:** with `--run-mode sign` or `--run-mode verify` (i.e. `RUN_KEYGEN=0`),
`hqc2025` is expected to report `FAIL` even when the algorithm itself is
correct — see "The RUN_KEYGEN=0 desync caveat" above. The script detects
this and calls those failures out separately in the summary rather than
hiding them.

`tools/assemble_faest_r3.sh` rebuilds `algorithms/signatures/onramp/FAEST-R3/` (flattened common/ + the four lambda=128 variants)
from the official FAEST v3 reference packages (`sw/original/faest-v3`); see the FAEST-R3 section of
[`algorithms/signatures/onramp/README.md`](../algorithms/signatures/onramp/README.md).

## Level-1 benchmark (`tools/bench/`)

`make bench-l1` builds the static-test folders listed in `tools/bench/variants_l1.json`
with one common `CFLAGS` set, runs keygen / sign / verify of every variant (one
operation per process, pinned to one CPU) with `bench.c` + `memtrack.c`, then
`report.py` writes `bench/results/{results.md,results.csv,*.png}`. `make bench-l1`
overwrites `bench/results`. `make bench-clean` removes `build/bench`. Methodology
and regeneration: [`bench/README.md`](../bench/README.md).
