# SDitH — ported from the [SDitH reference implementation](https://github.com/sdith/sdith)

SDitH (Syndrome Decoding in the Head), an MPC-in-the-Head code-based
signature. 12 variants: 2 sub-architectures (**Hypercube_Variant**,
**Threshold_Variant** — the two MPCitH sharing strategies from SDitH's NIST
submission) × 3 NIST security categories (cat1/cat3/cat5) × 2 underlying
fields (gf256/p251), each a full self-contained copy under its own
`sdith_<arch>_cat<N>_<field>/` directory — same convention as HAWK/LESS/
CROSS, not the shared-`common/` convention used by UOV/MAYO/SNOVA. The two
sub-architectures are structurally distinct codebases (Hypercube: ~15 files
led by `sdith.c`/`sign.c`/`treeprg.c`; Threshold: ~25 files led by
`mpc.c`/`keygen.c`/`commit.c`/`merkle-tree.c`/`views.c`/`witness.c`) that
happen to share the same vendored SHA3 (XKCP) backend and the same
`crypto_sign_keypair`/`crypto_sign`/`crypto_sign_open` API shape.

See [`../README.md`](../README.md#sdith) for the full write-up (directory
layout, `.rsp` fields, `make` targets). This file covers just the questions
that don't change per-conversation: what had to change to drop the
external-library dependency, how to build/run each variant, and the two
real patches (not just build-system integration) this family needed.

## What was changed to make it library-free

Unlike MAYO-C/SNOVA (already OpenSSL-free by default), SDitH's reference
code **does** call into OpenSSL, in exactly one place per variant:
`generator/rng.c`'s `AES256_ECB()`, used by the NIST KAT DRBG
(`randombytes`/`randombytes_init`). It was replaced with the same
self-contained software AES-256 block cipher (BearSSL-derived) already used
by [`harness/gen/rng.c`](../../../../harness/gen/rng.c) and reused verbatim across
UOV/QRUOV — its `#include <openssl/...>` lines are commented out, never
live. No other file in SDitH touches OpenSSL.

Two more patches were needed to make each variant **compile-time-fixed**
(buildable with a plain `gcc *.c`, no `-D` flags) and to flatten the
vendored SHA3 subtree (`sha3/`, `sha3/opt64/`) into `src/`+`include/` with
no subdirectories, same pattern FAEST needed for its own vendored XKCP copy:

- **Hypercube's `param.h`** originally selects CAT_1/CAT_3/CAT_5 via an
  `#ifndef` fallback chain, meant to be overridden by upstream's own
  `-DCAT_1`/`-DCAT_3`/`-DCAT_5` Makefile flag — patched to a single
  hardcoded `#define CAT_N`, same technique as UOV's `params.h`. One file,
  `precomputed.c`, used `CAT_N` without including `param.h` at all (relying
  on the compiler flag being global to the translation unit); it gained an
  explicit `#include "param.h"` so it now picks up the same hardcoded value
  instead of silently compiling as if no `CAT_N` were defined at all (every
  `#ifdef CAT_1`/`CAT_3`/`CAT_5` block in that file would otherwise be
  dropped, leaving `f_poly` etc. undefined — this was originally caught as a
  linker error during bring-up). Threshold's `parameters.h` is already
  fully per-directory-instantiated upstream (literal `#define PARAM_SECURITY
  128` etc., no `CAT_N` macro anywhere) — no equivalent patch needed there.
- Both sub-architectures hardcode `#define NDEBUG` (Hypercube in `param.h`,
  Threshold in `parameters.h`), since upstream's own Makefile always passes
  `-DNDEBUG`. Without it, Hypercube's `sdith.c` prints verbose per-call
  debug hex dumps (`sk seed:`, `Salt:`, `H1:`, `H2:`, ...) to stdout on
  every keygen/sign/verify — harmless to correctness but noisy, and not
  something a bare-metal target's UART budget wants by default.

One more structural fix, specific to how this repo flattens vendored
subtrees: Hypercube has a genuine filename collision once flattened. It
ships **two** distinct `rng.h`/`rng.c` pairs upstream — a core XOF
abstraction (used by `sdith.c`/`sign.c`/`treeprg.c`/`hash-sha3.c`) and the
KAT generator's NIST-DRBG `rng.h`/`rng.c` — that upstream keeps apart via
nested directories and C's "search the including file's own directory
first" quote-include resolution. Flattened into one `src/`+`include/` pair
they'd silently overwrite each other, so the core one is renamed to
`sdith_rng.h`/`sdith_rng.c` here, keeping canonical `rng.h`/`rng.c` as the
NIST DRBG — matching the convention every other family in this repo already
uses. Threshold has no such collision (its core randomness lives in
`rnd.c`/`rnd.h`, a distinct name) — but its `rnd.h` hand-declares
`extern int randombytes(...)` instead of `#include`-ing `rng.h`, which
caused its own build headache (see the Makefile's own `SDITH` comment
block for the full story: a same-directory quote-include priority quirk
that only bites when `rnd.h`'s hand-declared prototype and
[`harness/gen/rng.h`](../../../../harness/gen/rng.c)'s prototype land in the same
translation unit); `rnd.h` was rewritten to `#include "rng.h"` instead,
which resolves this the same way every other family in this repo already
relies on.

## Why it's bare-metal-ready

- **No external library**: `grep -rln openssl` finds only `rng.c` in each
  variant (see above), and that include is never live — confirmed by
  building and running all 12 variants with plain `gcc` and no `-I`/`-l`
  path near an OpenSSL install (see "Build & run" below).
- **No file I/O**: each variant's checked-in `main.c` reads its KAT vector
  from `include/test_vectors_<SUFFIX>.h` — plain C arrays baked in at
  compile time, no `fopen()`/`fread()` anywhere in the signing path.
- **No dynamic parameters**: each variant is a self-contained directory
  with its own `param.h` (Hypercube) or `parameters.h` (Threshold) hardcoded
  to one CAT/field combination — no `-D` flags needed to build a specific
  variant.
- **Profiling hooks present but empty**: each `main.c` has
  `PERF_CNT_CYCLES` hooks around keygen/sign/verify (currently no-ops);
  wire them to your SoC's cycle counter when porting to real hardware.

## A note on signature size

SDitH is an MPCitH scheme: like FALCON, its signature length is bounded
above by `CRYPTO_BYTES` but the actual signed-message length varies per
message/seed (visible in the KAT `smlen` field). The checked-in `main.c`
already handles this correctly — it compares against the per-vector
`TVEC_IN_SM_SIGN_LEN[i]` recorded in each variant's
`test_vectors_<SUFFIX>.h`, the same pattern already used by
UOV/MAYO/QRUOV/SNOVA's generated `main.c` — no special handling needed when
running these variants.

## Key/signature sizes (bytes)

| Variant                       | pk  | sk  | max sig (`CRYPTO_BYTES`) |
|--------------------------------|-----|-----|---------------------------|
| sdith_hypercube_cat1_gf256    | 132 | 432 | 8496                      |
| sdith_hypercube_cat1_p251     | 132 | 432 | 8496                      |
| sdith_hypercube_cat3_gf256    | 180 | 628 | 19544                     |
| sdith_hypercube_cat3_p251     | 180 | 628 | 19544                     |
| sdith_hypercube_cat5_gf256    | 244 | 838 | 33924                     |
| sdith_hypercube_cat5_p251     | 244 | 838 | 33924                     |
| sdith_threshold_cat1_gf256    | 132 | 432 | 10684                     |
| sdith_threshold_cat1_p251     | 132 | 432 | 10684                     |
| sdith_threshold_cat3_gf256    | 180 | 628 | 25964                     |
| sdith_threshold_cat3_p251     | 180 | 628 | 25964                     |
| sdith_threshold_cat5_gf256    | 244 | 838 | 45676                     |
| sdith_threshold_cat5_p251     | 244 | 838 | 45676                     |

## Build & run

### Quick check (whole family, via the repo's Makefile)

```sh
cd /home/aledolme/home/CODE/VLSPQC
make test-sdith KATNUM=5      # dynamic .rsp pipeline, all 12 variants
```

### One variant, dynamic pipeline

```sh
make test-sdith_hypercube_cat1_gf256 KATNUM=10
make test-sdith_threshold_cat5_p251 KATNUM=10
```

### Bare-metal style: compile the checked-in main.c directly (no Makefile, no file I/O)

```sh
cd algorithms/signatures/onramp/SDITH/sdith_hypercube_cat1_gf256
gcc -O2 -std=c11 -I include main.c src/*.c -o test && ./test
```

### All 12 variants at once, bare-metal style

```sh
cd algorithms/signatures/onramp/SDITH
for v in sdith_hypercube_cat1_gf256 sdith_hypercube_cat1_p251 \
         sdith_hypercube_cat3_gf256 sdith_hypercube_cat3_p251 \
         sdith_hypercube_cat5_gf256 sdith_hypercube_cat5_p251 \
         sdith_threshold_cat1_gf256 sdith_threshold_cat1_p251 \
         sdith_threshold_cat3_gf256 sdith_threshold_cat3_p251 \
         sdith_threshold_cat5_gf256 sdith_threshold_cat5_p251; do
  gcc -O2 -std=c11 -I "$v/include" "$v/main.c" "$v"/src/*.c -o /tmp/test_"$v" \
    && echo "$v: $(/tmp/test_"$v")"
done
```
