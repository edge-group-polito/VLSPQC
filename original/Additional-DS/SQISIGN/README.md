# SQIsign — ported from [the-sqisign](https://github.com/SQIsign/the-sqisign)

SQIsign (Short Quaternion and Isogeny Signature), an isogeny-based signature
over quaternion algebras — a fundamentally different mathematical structure
from every other family in this repo (all multivariate or code-based). 3
variants: `sqisign_lvl1`/`sqisign_lvl3`/`sqisign_lvl5` (NIST security
categories 1/3/5). Uses upstream's "ref" build type (no ASM/SIMD backend)
and its own bundled **mini-gmp** instead of system GMP — the same
`GMP_LIBRARY=MINI` option upstream's own CMake already supports.

Of the ~72 files needed per variant, 48 `.c` + 33 `.h` are byte-identical
text across all 3 levels and live once in [`common/`](common) (shared
`src/`+`include/`, same convention as MAYO/UOV/QRUOV/SNOVA); only 8 `.c` +
11 `.h` per level actually differ — `api.c`/`api.h`, the field-arithmetic
`fp_p*.c` file, and 6 SageMath-precomputed constant files/headers — and
live in each variant's own `sqisign_lvl<N>/{src,include}`.

See [`../README.md`](../README.md#sqisign) for the full write-up (directory
layout, `.rsp` fields, `make` targets). This file covers just the questions
that don't change per-conversation: what had to change to make it
compile-time-fixed, how to build/run each variant, and how this was
validated against upstream's own official KAT vectors.

## What was changed to make it library-free

**Nothing crypto-related** — SQIsign's own upstream already ships fully
OpenSSL-free and (via its `MINI` GMP option) fully real-GMP-free by
default:

- `common/src/randombytes_ctrdrbg.c` (the NIST KAT DRBG) calls
  `common/src/aes_c.c`, a self-contained AES-256 implementation — no
  OpenSSL anywhere.
- Every file that would otherwise `#include <gmp.h>` gates it behind
  `#if defined(MINI_GMP) ... #else #include <gmp.h> #endif`, and `MINI_GMP`
  is always defined here (see below), so those branches are dead code —
  confirmed via `grep -rln '#include <gmp.h>'`, and by building/running all
  3 variants with plain `gcc` and no `-I`/`-l` path near a real GMP
  install.

**The only real integration work** was making the build compile-time-fixed
with *zero* `-D` flags, matching every other family here. Upstream's own
CMake passes a handful of global `-D` flags
(`MINI_GMP`, `RANDOMBYTES_C`, `HAVE_UINT128`, `RADIX_64`, `GMP_LIMB_BITS=64`)
that aren't tied to any per-file `#ifdef` guard living in a single
naturally-first-included header. Since ~40 different `.c` files in this
codebase reach these checks through wildly different include chains (no
single "config.h" is guaranteed to be included first everywhere), the
simplest reliable fix was a tiny
[`common/include/sqisign_build_config.h`](common/include/sqisign_build_config.h)
with all five macros (`#ifndef`-guarded), `#include`d as the literal first
line of every `.c` file in `common/src/` and each variant's own `src/`
(a mechanical prepend, not a source-logic change). Two more per-level
build flags (`SQISIGN_VARIANT`, `SQISIGN_BUILD_TYPE_REF`) plus `ENABLE_SIGN`
are hardcoded the same way, but *inside* each variant's own copy of
`sqisign_namespace.h` — the one file upstream's namespacing macros need,
and the one value that's genuinely different per level (`lvl1`/`lvl3`/`lvl5`),
so it couldn't live in the shared `common/include/` at all.

No header-name collisions exist between `common/` and any variant's own
`include/` (verified file-by-file when assembling this port) — the only
basename that would have collided, `sqisign_namespace.h`, is exactly the
one kept per-variant instead of shared.

## Why it's bare-metal-ready

- **No external library**: confirmed by building and running all 3
  variants with plain `gcc` and no `-I`/`-l` path near an OpenSSL or GMP
  install (see "Build & run" below). `libm` (`-lm`) is needed at link
  time — the only family in this repo that does, since the quaternion
  arithmetic module calls libm functions — but libm ships with essentially
  every C toolchain (including bare-metal ones), unlike OpenSSL/GMP.
- **No file I/O**: each variant's checked-in `main.c` reads its KAT vector
  from `include/test_vectors_sqisign_lvl<N>.h` — plain C arrays baked in at
  compile time, no `fopen()`/`fread()` anywhere in the signing path.
- **No dynamic parameters**: each variant's own `sqisign_lvl<N>/include/`
  hardcodes its own field/curve/quaternion constants and API sizes — no
  `-D` flags needed to build a specific variant (see above).
- **Profiling hooks present but empty**: each `main.c` has
  `PERF_CNT_CYCLES` hooks around keygen/sign/verify (currently no-ops);
  wire them to your SoC's cycle counter when porting to real hardware.

## How this was validated against upstream's own official KAT

Before porting anything, upstream's own CMake build
(`-DGMP_LIBRARY=MINI -DSQISIGN_BUILD_TYPE=ref`) was run standalone and its
generated `PQCsignKAT_353_SQIsign_lvl1.rsp` was diffed byte-for-byte
against the official `KAT/PQCsignKAT_353_SQIsign_lvl1.rsp` shipped in the
repo — an exact match, confirming the mini-gmp/ref configuration is
functionally identical to upstream's official reference before any of this
repo's own file-flattening began. The exact `.c` file list linked into each
level's `PQCgenKAT_sign_<lvl>` binary was then extracted directly from that
same CMake build (`--verbose` compile log), rather than hand-traced from
the ~15 nested `CMakeLists.txt` files, to guarantee nothing was missed or
mis-flattened.

This repo's own `.rsp` files (generated by `kat/gen/PQCgenKAT_sign_sqisign.c`)
use the simpler single-phase "ds_drbg_hawk" seeding convention shared by
every other family here, not upstream's own two-phase `.req`/`.rsp`
NIST-submission format — so they won't byte-match upstream's official
`KAT/*.rsp` directly, but they're internally self-consistent (generator and
tester agree), exactly like every other family in this repo.

## Key/signature sizes (bytes)

| Variant       | pk  | sk  | max sig (`CRYPTO_BYTES`) |
|---------------|-----|-----|---------------------------|
| sqisign_lvl1  | 65  | 353 | 148                       |
| sqisign_lvl3  | 97  | 529 | 224                       |
| sqisign_lvl5  | 129 | 701 | 292                       |

## Build & run

### Quick check (whole family, via the repo's Makefile)

```sh
cd /home/aledolme/home/CODE/VLSPQC
make test-sqisign KATNUM=5      # dynamic .rsp pipeline, all 3 variants
```

### One variant, dynamic pipeline

```sh
make test-sqisign_lvl1 KATNUM=10
make test-sqisign_lvl5 KATNUM=10
```

### Bare-metal style: compile the checked-in main.c directly (no Makefile, no file I/O)

```sh
cd original/Additional-DS/SQISIGN/sqisign_lvl1
gcc -O2 -std=c11 -I ../common/include -I include main.c ../common/src/*.c src/*.c -o test -lm && ./test
```

### All 3 variants at once, bare-metal style

```sh
cd original/Additional-DS/SQISIGN
for v in sqisign_lvl1 sqisign_lvl3 sqisign_lvl5; do
  gcc -O2 -std=c11 -I common/include -I "$v/include" \
      "$v/main.c" common/src/*.c "$v"/src/*.c -o /tmp/test_"$v" -lm \
    && echo "$v: $(/tmp/test_"$v" | tail -1)"
done
```
