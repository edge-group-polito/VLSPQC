# `tools/` — generators, batch runners, benchmarks and report scripts

Everything here is driven from the repository root (`make …` or `python3 -I tools/<script>`); the long-form
documentation of the static-test flow lives in [`../docs/static-tests.md`](../docs/static-tests.md).

| Script | What it does | Typical use |
|---|---|---|
| [`algo_registry.json`](algo_registry.json) | Single source of truth for the static-test generator: per variant, its family, API "shape", source/include directories and `.rsp` path (279 variants) | edited when a family or variant is added |
| [`gen_static_test.py`](gen_static_test.py) | Builds a self-contained, no-file-I/O test folder `vectors/static/<variant>/` (KAT vectors baked into C arrays) | `make static-test-<variant>`; see [`../docs/static-tests.md`](../docs/static-tests.md) |
| [`run_all_static_kat.sh`](run_all_static_kat.sh) | Batch runner: generates, compiles with plain `gcc` and runs every static test, logs PASS/FAIL per variant | `tools/run_all_static_kat.sh [--targets "…"] [--benchmark-all]` |
| [`profiling_commands/`](profiling_commands) | Cycle-counter snippets baked into static tests for RISC-V (CVA6) targets | `make static-test-<variant> PROFILING=cva6` |
| [`bench/`](bench) | Host benchmark: time, heap, stack, RSS and code size per operation (`run_bench.py`, `bench.c`, `report*.py`, variant lists) | `make bench-l1` (additional signatures, level 1), `make bench-fips` (ML-KEM / ML-DSA / SLH-DSA / Falcon / HQC / FN-DSA) |
| [`gen_size_table.py`](gen_size_table.py) | pk / sk / ct-or-signature sizes of every registry variant from its compiled `api.h` constants; writes `results/sizes.csv` and the marked tables in the READMEs | `make results-update` |
| [`collect_results.py`](collect_results.py) | Turns benchmark JSON and static-test logs into `results/benchmark.csv` and `results/static_kat_status.csv` | `make results-update` |
| [`plot_results.py`](plot_results.py) | Draws `docs/figures/time_per_operation.png` and `docs/figures/sizes.png` from `results/benchmark.csv` | `make results-update` |
| [`check_links.py`](check_links.py) | Checks that every relative link and anchor in the Markdown files resolves | `python3 -I tools/check_links.py` |
| `assemble_*.sh`, `build_*_main.sh` | One-off assembly of a ported family from its official package (MQOM, FAEST-R3) and generation of hand-run `main.c` files for older ports | see the header of each script and the family READMEs |

## Regenerating tables and figures

```bash
make bench-fips                        # idle machine; writes bench/results_fips_<date>/ (bench/ is git-ignored)
make results-update BENCH_FIPS_OUT=bench/results_fips_<date>
python3 -I tools/check_links.py        # after editing any README
```

The README tables between `<!-- BEGIN sizes:… -->` / `<!-- END sizes:… -->` markers are rewritten by
`tools/gen_size_table.py --update-readmes`; do not edit them by hand.
