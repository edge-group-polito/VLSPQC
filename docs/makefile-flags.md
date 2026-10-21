# Makefile flags and targets

All flags can be set on the command line and override the defaults. `make help` prints a target summary; the category READMEs under
[`../algorithms/`](../algorithms/README.md) list every per-family and per-variant target name.

| Flag | Default | Description |
|------|---------|-------------|
| `KATNUM` | `100` | Number of KAT vectors to generate / test (dynamic tests) |
| `CHECK_KATNUM` | `1` | Vectors per variant for XMSS / LMS / HSS `-check` / `-check-slow` sweeps |
| `RUN_KEYGEN` | `1` | Test key generation |
| `RUN_SIGN` / `RUN_VERIFY` | `1` | Test signing / verification (signatures only) |
| `RUN_ENCAPS` / `RUN_DECAPS` | `1` | Test encapsulation / decapsulation (KEM), and ASCON's encrypt / decrypt |
| `DEBUG` | `0` | Print hex of inputs / outputs on failure |
| `MLEN` | `33` | Message length (bytes) used during generation |
| `CC` / `CFLAGS` | `gcc` / `-O2 -std=c11 …` | Compiler / flags |
| `STATIC_KATNUM` | `1` | Vectors baked into a static test (see [`static-tests.md`](static-tests.md)) |
| `STATICDIR` | `vectors/static` | Output directory of static tests |
| `PROFILING` | `none` | Cycle-counter profile baked into a static test (see [`static-tests.md`](static-tests.md)) |
| `BENCH_TIME`, `BENCH_OUT`, `BENCH_FIPS_OUT` | `3`, `bench/results`, `bench/results_fips_<date>` | seconds per operation and output directories of `make bench-l1` / `make bench-fips` |

```bash
# Run only 10 vectors, verbose output on failure
make test-CROSS_1_RSDP_b KATNUM=10 DEBUG=1

# Test sign+verify only, skip keygen (uses stored keys from the .rsp)
make test-hawk-256 RUN_KEYGEN=0 RUN_SIGN=1 RUN_VERIFY=1

# Cross-compile
make test-all CC=arm-linux-gnueabihf-gcc CFLAGS="-O2 -std=c11"
```

> `KATNUM` must match between generation and testing: the test binary stops after reading `KATNUM` entries from the `.rsp` file.

## Aggregate targets

| Target | Covers | In `test-all`? |
|---|---|---|
| `test-all` / `gen-all` / `static-test-all` | the 23 standardized and standard-pending variants (ML-KEM 3, ML-DSA 3, SLH-DSA 12, Falcon 2, HQC 3); `test-all` also runs `test-falcon-512-official` and `test-falcon-1024-official` (the official Falcon round-3 KAT) | — |
| `test-ds`, `test-kem` | signatures (`mldsa`, `slhdsa`, `falcon`) / KEMs (`mlkem`, `hqc`) of the above | part of it |
| `test-fndsa-exp` | experimental FN-DSA (2 variants) | no |
| `test-onramp-ds` (alias `test-additional-ds`, deprecated) | the on-ramp candidates (232 variants) | no |
| `test-stateful-ds` | XMSS, LMS, HSS default variants | no |
| `test-ascon` | ASCON (7 variants, not PQC) | no |
| `test-slhdsa-haraka`, `test-slhdsa-haraka-aes` | legacy SPHINCS+ with Haraka | no |

Each has `gen-` and (where it exists) `static-test-` equivalents. Sweeps: `test-xmss-check`, `test-lms-check-slow`, … (see [`../algorithms/signatures/stateful/README.md`](../algorithms/signatures/stateful/README.md)).
