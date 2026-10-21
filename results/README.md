# `results/` — measured data that the READMEs and figures are built from

| File | Content | Produced by |
|---|---|---|
| [`sizes.csv`](sizes.csv) | pk / sk / ciphertext-or-signature bytes of every registry variant (272), from each variant's compiled `api.h` | `tools/gen_size_table.py` (`make results-update`) |
| [`benchmark.csv`](benchmark.csv) | host benchmark, one row per variant and operation (time, heap, stack, resident-set growth, sizes, code size) for ML-KEM, ML-DSA, SLH-DSA, Falcon, HQC and experimental FN-DSA | `tools/collect_results.py` from `make bench-fips` |
| [`benchmark_meta.json`](benchmark_meta.json) | host, compiler, flags and date of that run | copied from the run's `meta.json` |
| [`benchmark_onramp_l1.csv`](benchmark_onramp_l1.csv), [`benchmark_onramp_l1_spread.csv`](benchmark_onramp_l1_spread.csv) | the earlier level-1 benchmark of the Round-3 on-ramp families (39 variants, per-metric median of 3 runs, 2026-10-14, before the migration, not rerun) and its per-metric spread | `tools/collect_results.py --onramp-l1` |
| [`static_kat_status_onramp.csv`](static_kat_status_onramp.csv) | static-test outcome of all 232 on-ramp variants after the restructure (default stack and the rerun of the 4 SNOVA level-5 variants with a bigger stack) | scratch script, see `validation.md` |
| [`static_kat_status.csv`](static_kat_status.csv) | PASS / FAIL of the static (no-file-I/O) KAT tests of the validated variants | `tools/collect_results.py` from `tools/run_all_static_kat.sh` logs |
| [`validation.md`](validation.md) | every validation command run during the FIPS migration and its outcome | written by hand from the logs |

Figures built from these files are in [`../docs/figures/`](../docs/figures). The raw benchmark JSON, the run logs and the older level-1 on-ramp benchmark live in `bench/`
(git-ignored); regenerate with `make bench-fips` / `make bench-l1` on an otherwise idle machine.

What is **not** here: a *new* benchmark of the on-ramp families (the earlier level-1 one, from before the migration and not rerun, is `benchmark_onramp_l1.csv`).
