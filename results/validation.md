# Validation record — FIPS migration and repository restructure

Every line below is the outcome of a command run on 2026-10-20 on the benchmark host (Intel Core Ultra 7 155H, WSL2, gcc 13.3, plain `gcc`, no `-march`),
with the log kept under `bench/results_fips_20261020/logs/` (git-ignored; copy them out if you need them). "0 failures" means every
vector of every variant of the target passed (keygen, sign/encaps, verify/decaps). Anything not listed was **not run**.

## Baseline before the migration (tag `pre-fips-migration`)

| Command | Result |
|---|---|
| `make distclean && make test-all` | 35 variants, 100 vectors each, 0 failures (included Kyber, Dilithium, SPHINCS+ SHA2/SHAKE/Haraka) |
| `make test-stateful-ds` | 3 of 3, 0 failures |
| `make test-ascon` | 7 of 7, 0 failures |
| `make test-onramp-ds` (then called `test-additional-ds`) | **incomplete**: stopped by the 5400 s timeout inside LESS; CROSS (18), HAWK (3) and part of LESS had passed, every later family did not run |
| static baseline | not run |

## After the FIPS imports (end of Phase 1)

| Command | Result |
|---|---|
| `make distclean && make test-all` | **23 variants**, 100 vectors each, 0 failures (ML-KEM 3, ML-DSA 3, SLH-DSA 12, Falcon 2, HQC 3) |
| `make test-fndsa-exp` | 2 of 2, 100 vectors, 0 failures |
| `make test-slhdsa-haraka` (legacy) | 6 of 6, 100 vectors, 0 failures |
| `tools/run_all_static_kat.sh` (standard families, 1 vector) | 23 of 23 pass |
| static FN-DSA | first attempt failed (the family was not registered in the batch runner — my mistake); after registering it: 2 of 2 pass |

Independent cross-checks (scripts in the session scratch area, not committed; their method and results are in each family's `UPSTREAM.md`): upstream KAT hashes for
ML-KEM and ML-DSA, NIST ACVP vectors for SLH-DSA (120 keyGen + 36 sigGen cases), the official v5.0.0 KATs for HQC (300 records), upstream's own tests for FN-DSA.

## After the restructure (Phase 2)

| Command | Result |
|---|---|
| `make distclean && make -j4 gen-all gen-fndsa-exp`, compared with a snapshot taken before the move | **25 of 25 `.rsp` files byte-identical** |
| `make KATNUM=10 gen-cross gen-less gen-hawk gen-MAYO_1-R3 gen-sqisign_p324_3-R3 gen-mqom3_cat1_gf16_fast_ct gen-xmss gen-lms gen-hss gen-slhdsa-haraka gen-ascon128v12`, same comparison | **66 of 66 `.rsp` files byte-identical** (including the 25 above) |
| `make test-all` | 23 variants, 100 vectors, 0 failures (same pass set as before the move) |
| `make test-fndsa-exp` | 2 of 2, 100 vectors, 0 failures |
| `make KATNUM=10 test-cross test-less test-hawk test-slhdsa-haraka test-stateful-ds test-ascon` | 44 test blocks, 10 vectors each, 0 failures |
| `tools/run_all_static_kat.sh` (standard families) | 23 of 23 pass |
| static FN-DSA | 2 of 2 pass |
| static sample: CROSS_1_RSDP_b, LESS_252_45, hawk-256, the 6 Haraka variants, HQC2025_1 | 10 of 10 pass (first attempt failed on my command quoting, rerun passed) |
| registry paths | all 279 `src_dir` / `include_dir` entries and all literal Makefile paths exist |

### On-ramp families after the restructure (run on 2026-10-21)

| Command | Result |
|---|---|
| `make -k -j4 KATNUM=10 test-onramp-ds` (all 232 on-ramp variants, **10 vectors each**, not the default 100) | exit 0, 232 test blocks, 0 failures |
| `make -k -j4 static-test-onramp-ds STATIC_KATNUM=1`, then each of the 232 static folders compiled with plain `gcc -O2 -std=c11 -I.` plus the registry's `static_gcc_flags` (+ `-lm`) and run (1 vector) | 228 pass, **4 fail with a segmentation fault** (`SNOVA_96_32_19_2`, `SNOVA_96_32_19_2_AES`, `SNOVA_97_33_16_2`, `SNOVA_97_33_16_2_AES`: the older SNOVA level-5 ports, default 8 MB stack) |
| the same 4 folders rerun with `ulimit -s 65536` | 4 of 4 pass (the documented stack requirement of the level-5 SNOVA ports, not a regression; the batch runner did not raise the stack limit at that time, it does now, see below) |

Per-variant results: [`static_kat_status_onramp.csv`](static_kat_status_onramp.csv). The default-100-vector `test-onramp-ds` was not run (the pre-migration run was stopped by a 90 minute timeout), and `make test-xmss-check` / `-check-slow` and the other sweeps were not run.
This closes the gap left in the Phase 2 validation: the on-ramp families pass after the move, at 10 vectors for the dynamic tests.

## Documentation phase (Phase 3)

| Command | Result |
|---|---|
| `src/fips202/native/` (ARM / x86 FIPS 202 backends) removed from the six ML-KEM / ML-DSA trees; upstream `gen_KAT` hashes recomputed | equal to upstream `META.yml` for all six variants |
| `make test-mlkem test-mldsa` after that removal | 6 of 6, 100 vectors, 0 failures |
| `tools/run_all_static_kat.sh --targets "mlkem mldsa"` | 6 of 6 pass |
| `python3 -I tools/check_links.py` | see the end of the Phase 3 report; rerun it after editing any Markdown file |

## Follow-up changes (2026-10-21): official Falcon KAT, batch-runner stack limit, licenses

| Command | Result |
|---|---|
| Falcon round-3 package KAT files replayed through the explicit-randomness API (scratch program first, then `make test-falcon-512-official test-falcon-1024-official`) | 100/100 records for Falcon-512 and 100/100 for Falcon-1024, 0 mismatches (keygen, sign, verify) |
| the same runner on one deliberately corrupted official record (scratch copy) | 1 sign failure reported, exit 1 (the test is not vacuous) |
| `make distclean && make -j4 test-all` | exit 0, **25 test blocks** (23 variants + the 2 official Falcon replays), 100 vectors each, 0 failures |
| `make test-fndsa-exp` | exit 0 |
| `tools/run_all_static_kat.sh` (standard families, new default stack limit 64 MiB) | 23 of 23 pass |
| `tools/run_all_static_kat.sh --targets "fndsa-exp sdith-r3 sqisign-r3 snova"` (per-family compile flags applied automatically, no manual `--cflags`) | 53 of 53 pass, including the 4 older SNOVA level-5 variants that segfaulted with the old 8 MiB stack |
| the same SNOVA level-5 variant with `--stack-kb 8192` | fails (segfault), showing that the stack limit was the cause |
| upstream license lookup (git clones and file-hash comparison, see the `UPSTREAM.md` of CROSS, LESS, HAWK, the stateful families, ASCON and `legacy/`) | licenses copied from upstream into the family folders; the closest upstream versions are recorded, not verified import points |

## Host benchmark

`bench/results_fips_20261020/` (`make bench-fips`): 25 variants, 3 operations each, 3 s budget per operation, one CPU pinned, machine about 94 % idle before and 98 % idle after; **a single run**,
so run-to-run variation is not quantified for these families (the earlier level-1 benchmark of the on-ramp schemes measured a median 26.8 % difference, maximum 82 %, between two back-to-back runs; see `bench/README.md`). The CSV is [`benchmark.csv`](benchmark.csv).
