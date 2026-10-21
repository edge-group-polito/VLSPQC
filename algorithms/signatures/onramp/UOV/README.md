# UOV — ported from [pqov](https://github.com/pqov/pqov)

Unbalanced Oil and Vinegar, a multivariate signature scheme. 12 variants
(4 security levels — Ip/Is/III/V — × 3 key formats — classic/pkc/pkc_skc),
each compile-time-fixed via its own `include/params.h`. All other `.c`/`.h`
files are shared unmodified across all 12 variants in [`common/`](common).

See [`../README.md`](../README.md#uov) for the full write-up (directory
layout, `.rsp` fields, `make` targets). This file covers just the two
questions that don't change per-conversation: what had to change to drop
the external-library dependency, and how to build/run each variant.

## What was changed to make it library-free

Upstream pqov defaults to OpenSSL for two things: SHAKE
(`utils/utils_hash.c`'s `_UTILS_OPENSSL_` path, via `EVP_shake256`) and the
KAT DRBG (`utils/nistkat/rng.c`'s `AES256_ECB`, via `EVP_aes_256_ecb`).
Two small patches in [`common/include/`](common/include) remove both:

- **`config.h`** — the default-selection block no longer defines
  `_UTILS_OPENSSL_`, so `utils_hash.c`/`.h` fall through to their own
  existing self-contained `fips202.c` (Keccak/SHAKE) branch instead
  (pqov already shipped this branch — it just wasn't the default).
  `_NIST_KAT_` is now defined unconditionally.
- **`utils_randombytes.h`** — its `#elif defined(_NIST_KAT_) && defined(_UTILS_OPENSSL_)`
  guard (which pulled in `rng.h`) is relaxed to just `defined(_NIST_KAT_)`.
  `common/src/rng.c` is a self-contained software AES-256 implementation
  (same one `harness/gen/rng.c` and HAWK's `src/rng.c` already use elsewhere
  in this repo) — it no longer needs `_UTILS_OPENSSL_` to be selected.

Nothing else in pqov touched OpenSSL. No other cryptographic primitive
(matrix/field arithmetic, the OV trapdoor itself) ever depended on it.

## Why it's bare-metal-ready

- **No external library**: `grep -rn "#include <openssl"` still finds hits
  (`utils_hash.c`/`.h`'s `#if defined(_UTILS_OPENSSL_)` branch, `rng.c`'s
  commented-out original), but none are live — `_UTILS_OPENSSL_` is never
  defined by default anymore (see above), and `rng.c`'s OpenSSL block sits
  inside a `/* removed ... */` comment. Verified by building and running
  every variant with plain `gcc` and no `-I`/`-l` path near an OpenSSL
  install (see "Build & run" below).
- **No file I/O**: each variant's checked-in `main.c` reads its KAT vector
  from `include/test_vectors_<SUFFIX>.h` — plain C arrays baked in at
  compile time, no `fopen()`/`fread()` anywhere in the signing path.
- **No dynamic parameters**: each variant's `include/params.h` hardcodes
  its own `n`/`m`/`GF(q)`/key-format macros — no `-D` flags needed to build
  a specific variant, no runtime branch on which parameter set is active.
- **Profiling hooks present but empty**: each `main.c` has `PERF_CNT_CYCLES`
  hooks around keygen/sign/verify (currently no-ops); wire them to your
  SoC's cycle counter when porting to real hardware.

## Build & run

### Quick check (whole family, via the repo's Makefile)

```sh
cd /home/aledolme/home/CODE/VLSPQC
make test-uov KATNUM=5        # dynamic .rsp pipeline, all 12 variants
```

### One variant, dynamic pipeline

```sh
make test-UOV_Ip KATNUM=10    # replace UOV_Ip with any variant name below
```

### Bare-metal style: compile the checked-in main.c directly (no Makefile, no file I/O)

```sh
cd algorithms/signatures/onramp/UOV/UOV_Ip
gcc -O2 -std=c11 -I ../common/include -I include main.c ../common/src/*.c -o test && ./test
```

### All 12 variants at once, bare-metal style

```sh
cd algorithms/signatures/onramp/UOV
for v in UOV_Ip UOV_Ip_pkc UOV_Ip_pkc_skc \
         UOV_Is UOV_Is_pkc UOV_Is_pkc_skc \
         UOV_III UOV_III_pkc UOV_III_pkc_skc \
         UOV_V UOV_V_pkc UOV_V_pkc_skc; do
  gcc -O2 -std=c11 -I common/include -I "$v/include" \
      "$v/main.c" common/src/*.c -o /tmp/test_"$v" \
    && echo "$v: $(/tmp/test_"$v")"
done
```
