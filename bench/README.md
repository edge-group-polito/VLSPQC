# bench/ — level-1 benchmark of the additional signatures

Results of `make bench-l1` (tools: [`tools/bench/`](../tools/bench), see also [`tools/README.md`](../tools/README.md)).

| Path | Content |
|---|---|
| `results/` | Run 1: `results.md` (all tables + charts + caveats), `results.csv`, `time.png`, `memory.png`, `tradeoff.png`, `meta.json` (machine, compiler, load notes), `raw/<variant>.json` (one file per variant: timings, heap, stack, VmHWM, sizes) |
| `results_run2/` | Second run of the same command, back to back with the first (idle rerun 2026-10-14; see `meta.json` `load_notes` for the measured load). Medians differ from run 1 by a median 26.8 % (max 82 %), see the run-to-run section of `results.md`; the previous runs measured under foreign load are kept in `old_loaded_run/` |

## Methodology

- Scope: NIST level 1 only; variants are listed in `tools/bench/variants_l1.json` (FAEST 4, MAYO 2, MQOM 6, QR-UOV 10, SDitH 4, SNOVA 6, SQIsign 1, UOV 6 = 39).
  Round-3 code (`*-R3` families, `MQOM`) and the FAEST port, built from the flattened static-test folders (`tools/gen_static_test.py`, 1 KAT vector), i.e. the code the static KAT tests exercise.
- Build: plain `gcc`, identical `-O2 -std=gnu11 -w -Wno-psabi` for every variant (warnings suppressed; the static KAT tests were built with `-std=c11`), portable reference C (no SIMD/AES-NI: no `-march`, so the x86-64 SSE2 baseline and gcc `-O2` auto-vectorisation apply equally to all; SDitH selects its reference code through a `#define ONLY_REF_IMPLEMENTATION` in its header `sdith_build_config.h` instead of a `-D` flag), link with `-lm -lpthread` for every variant
  (needed by SDitH; applied to all for uniformity). SQIsign needs its folder on the include path (`-I.` in `cflags_extra`; the runner already passes `-I<folder>`).
- Run: one process per operation (keygen / sign / verify), pinned to one CPU (`--cpu`, default 2), 1000-byte message, median wall-clock time
  (`CLOCK_MONOTONIC`) over up to 1000 calls within a 3 s budget (at least 5). TSC cycles are raw ticks, relative only under WSL2.
- Memory: heap = peak requested bytes in `malloc/calloc/realloc` (link-time wrapping, `memtrack.c`; harness buffers excluded);
  stack = touched pages of a dedicated 1 GiB mmap'ed thread stack, 4 KiB granularity, minus an empty thread; VmHWM = peak resident set of the process. The report shows `vmhwm_kb - vmhwm_base_kb` (growth during the operation); the raw VmHWM, in `results.csv` next to the baseline columns, includes a ~2.0-2.4 MiB baseline (libc, harness, pages left by the preparation thread) that depends on the operation, so raw VmHWM is not comparable across operations or schemes and the net value under-reports sign/verify where freed pages are reused. Stack values of 0/4/8 KiB are page-granularity floors ("<= 4 KiB");
  code/data/bss = `size` of the algorithm objects only.
- KAT status: MAYO, SNOVA, MQOM, SDitH and SQIsign match the official KAT record #0; UOV and QR-UOV (16 of the 39 variants) matched only the package's own generator on records 0-1 (no official KAT offline); FAEST is the v3 (round 3) port (`FAEST_<V>-R3`, 100 official KAT records equal), not the older v2-sized `FAEST/` tree.
- FAEST sign/verify medians moved by 0-22 % between the two idle reruns; the earlier sibling anomalies (EM-128f verify > sign, EM-128s verify << sign) did not reproduce. Keygen (about 2 us) is near timer resolution.
- Sizes are the `CRYPTO_*BYTES` of each variant plus the measured signature length.

## Regenerate

```
make bench-l1                          # builds, runs, writes bench/results (~10 min)
make bench-l1 BENCH_OUT=bench/results_run2     # a second run
python3 -I tools/bench/report.py --results bench/results --compare bench/results_run2   # adds the load-sensitivity section (a spread estimate only if run 2 was measured on an idle machine)
make bench-clean                       # removes build/bench (not the results)
```

Run on an otherwise idle machine, with nothing else compiling or simulating. The numbers are for the reference
(non-optimized) code and are not comparable with papers that benchmark optimized/AVX2 packages.
