# MAYO — ported from [MAYO-C](https://github.com/PQCMayo/MAYO-C)

MAYO, a multivariate signature scheme built as a variant of UOV. 4
parameter sets (MAYO_1/2/3/5, NIST levels 1/1/3/5), each compile-time-fixed
via its own tiny `include/mayo_variant.h`. All other `.c`/`.h` files are
shared unmodified across all 4 variants in [`common/`](common).

See [`../README.md`](../README.md#mayo) for the full write-up (directory
layout, `.rsp` fields, `make` targets). This file covers just the two
questions that don't change per-conversation: what had to change to drop
the external-library dependency, and how to build/run each variant.

## What was changed to make it library-free

**Nothing crypto-related** — MAYO-C already ships fully OpenSSL-free
upstream:

- `common/randombytes_ctrdrbg.c` (the NIST KAT DRBG) uses MAYO's own
  portable AES (`common/aes_c.c`), not OpenSSL.
- `common/aes_c.c` is a constant-time, BearSSL-derived AES-128/192/256
  implementation (portable C, no intrinsics unless `ENABLE_AESNI`/
  `ENABLE_AESNEON` are explicitly defined — neither is here).
- `common/fips202.c` (SHAKE) is TweetFips202-derived, also self-contained.

The only change needed was **build-system integration**, not dependency
removal: upstream selects a parameter set purely via a
`-DMAYO_VARIANT=MAYO_n` compile flag (see MAYO-C's `src/CMakeLists.txt`).
To make each variant compile-time-fixed with no external `-D` flags (matching
every other family in this repo), `common/include/mayo.h` gained one small
guarded include right after its header guard:

```c
#ifndef MAYO_VARIANT
#include "mayo_variant.h"
#endif
```

Each `MAYO_<N>/include/mayo_variant.h` supplies `#define MAYO_VARIANT MAYO_<N>`
and `#define MAYO_BUILD_TYPE_OPT` before `mayo.h`'s own
`#if defined(MAYO_VARIANT)` checks run. Everything else in `MAYO_<N>/` —
`src/api.c`, `include/api.h` — is upstream's own unmodified per-variant code.

## Why it's bare-metal-ready

- **No external library**: `grep -rln openssl` over this tree returns
  nothing at all — there was never anything to remove.
- **No file I/O**: each variant's checked-in `main.c` reads its KAT vector
  from `include/test_vectors_<N>.h` — plain C arrays baked in at compile
  time, no `fopen()`/`fread()` anywhere in the signing path.
- **No dynamic parameters**: `ENABLE_PARAMS_DYNAMIC` is off (the default
  non-dynamic upstream build mode) — `mayo_variant.h` fixes the parameter
  set at compile time, no runtime branch on which one is active.
- **Profiling hooks present but empty**: each `main.c` has `PERF_CNT_CYCLES`
  hooks around keygen/sign/verify (currently no-ops); wire them to your
  SoC's cycle counter when porting to real hardware.

## Build & run

### Quick check (whole family, via the repo's Makefile)

```sh
cd /home/aledolme/home/CODE/VLSPQC
make test-mayo KATNUM=5       # dynamic .rsp pipeline, all 4 variants
```

### One variant, dynamic pipeline

```sh
make test-MAYO_1 KATNUM=10    # replace MAYO_1 with any variant name below
```

### Bare-metal style: compile the checked-in main.c directly (no Makefile, no file I/O)

```sh
cd original/Additional-DS/MAYO/MAYO_1
gcc -O2 -std=c11 -I ../common/include -I include main.c ../common/src/*.c src/api.c -o test && ./test
```

### All 4 variants at once, bare-metal style

```sh
cd original/Additional-DS/MAYO
for v in MAYO_1 MAYO_2 MAYO_3 MAYO_5; do
  gcc -O2 -std=c11 -I common/include -I "$v/include" \
      "$v/main.c" common/src/*.c "$v/src/api.c" -o /tmp/test_"$v" \
    && echo "$v: $(/tmp/test_"$v")"
done
```
