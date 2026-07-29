# Additional-DS — non-standardized signature schemes

Reference-implementation sources for digital-signature schemes that are
**not (yet) standardized** by NIST: CROSS, HAWK, LESS, and FAEST were
submitted to NIST's additional-signatures on-ramp but haven't been finalized
into a FIPS the way ML-DSA (FIPS 204), SLH-DSA (FIPS 205), and FALCON/FN-DSA
have. They live in a separate category from [`../DS/`](../DS/README.md) so
that `make test-all`/`gen-all`/`static-test-all` — which cover only
standardized algorithms — don't pull them in by default. Use this category's
own aggregate targets (`test-additional-ds`, `gen-additional-ds`,
`static-test-additional-ds`) or the per-family/per-variant targets below.

Each family directory holds one subdirectory per parameter-set variant
(`src/`/`include/` split for all four families) and exposes a compile-time-fixed
`crypto_sign_keypair`/`crypto_sign`/`crypto_sign_open` API. These sources are
compiled together with `kat/gen/PQCgenKAT_sign_*.c` (generation) or
`kat/gen/main_*.c` (testing) — see [`../../kat/README.md`](../../kat/README.md)
for how that build works, and [`../../README.md`](../../README.md) for the
`make` targets that drive it.

## Families

| Family | Variants | Security levels | Notes |
|--------|----------|------------------|-------|
| CROSS | 18 (RSDP/RSDPG × 1/3/5 × b/f/s) | 128, 192, 256 bit | shared SHAKE CSPRNG |
| HAWK | 2 (512, 1024) | 128, 256 bit | own NIST DRBG (`src/rng.c`) |
| LESS | 7 | 128, 192, 256 bit | shared SHAKE CSPRNG, same model as CROSS |
| FAEST | 12 (plain/EM × 128/192/256 × f/s) | 128, 192, 256 bit | own NIST DRBG; ~30 files shared across all 12 variants live once in `FAEST/common/` |

**`test-additional-ds` covers all 39 variants above.**

## Per-family `make` targets

Prefix with `test-` or `gen-`: `cross`, `hawk`, `less`, `faest` (aggregate:
`test-additional-ds`/`gen-additional-ds`, **not** part of `test-all`/`gen-all`).

Per-variant targets are `test-<NAME>`/`gen-<NAME>` using each variant's
directory name directly, e.g. `make test-CROSS_1_RSDP_b`,
`make test-hawk-512`, `make test-LESS_252_45`, `make test-FAEST_128F`.

## `.rsp` fields specific to Additional-DS

All `.rsp` files carry the standard `count`/`seed`/`mlen`/`msg`/`pk`/`sk`/
`smlen`/`sm` fields (see [`../../kat/README.md`](../../kat/README.md) for the
full format):

| Algorithm | Extra fields |
|-----------|--------------|
| CROSS, LESS | `seed` is the CSPRNG seed (48 B); no extra fields |
| HAWK, FAEST | `seed` is the NIST DRBG seed (48 B) |

## Algorithm-specific notes

### CROSS

Uses a SHAKE-based CSPRNG (`platform_csprng_state`) shared continuously
between keygen and sign. The KAT generator seeds a private SHAKE state with
`{0,...,47}` to derive test seeds, then initializes the platform CSPRNG fresh
for each keygen+sign pair.

**Important for testing with `RUN_KEYGEN=0`:** the test runner still calls
`crypto_sign_keypair()` internally to advance the shared CSPRNG to the correct
state, then overwrites `pk`/`sk` from the stored `.rsp` values before calling
sign.

### LESS

Same SHAKE CSPRNG model as CROSS. The generator uses the NIST DRBG (separate
from LESS's internal CSPRNG) to derive the 48-byte seed, then calls
`initialize_csprng()`. The same CSPRNG state-continuity rule applies when
`RUN_KEYGEN=0`.

### HAWK

`src/rng.c` IS the NIST AES-256-CTR DRBG — the same implementation copied to
`kat/gen/rng.c`. The generator uses HAWK's own RNG directly;
`kat/gen/rng.c` is not linked separately for HAWK. HAWK has no derand entry
point, so `crypto_sign_keypair()` is always called (even with
`RUN_KEYGEN=0`, output discarded in favor of the `.rsp` values) purely to
advance the DRBG to the position `crypto_sign()` expects — same fix as
CROSS/LESS, needed here too since HAWK's keygen consumes a variable amount
of randomness (an internal rejection-sampling retry loop), so only calling
the real function reproduces its exact consumption.

`hawk-512` also carries an optional keygen call-count/dimension/timing
profiler (`HAWK_PROFILE=1`, disabled by default) — see
[`HAWK/hawk-512/README.md`](HAWK/hawk-512/README.md).

### FAEST

Ported from the [faest-ref](https://github.com/faest-sign/faest-ref)
reference implementation, which normally builds via meson (`.c.in`/`.h.in`
templates instantiated per parameter set). Each variant here is the
already-instantiated, compile-time-fixed output of that templating step —
no meson/build-system dependency remains. Like HAWK, FAEST's own copy of the
NIST AES-256-CTR DRBG lives in `common/src/rng.c`, and FAEST has no derand
entry point: `crypto_sign_keypair`/`crypto_sign` draw randomness via an
internal `rand_bytes()` that forwards straight to that DRBG's
`randombytes()`. As with HAWK, `crypto_sign_keypair()` is always called
even when `RUN_KEYGEN=0` (output discarded in favor of the `.rsp`/test
vector values) purely to advance the DRBG — necessary since FAEST's keygen
also has a variable-length internal rejection-sampling loop.

**Layout differs from every other Additional-DS family:** ~30 of FAEST's
files (the generic engine, the vendored `sha3`/XKCP subtree, the 3
lambda-specific AES/witness-extension files, `parameters.h`, `rng.c`/`rng.h`)
are byte-identical across all 12 variants, so instead of duplicating them
per variant (as CROSS/LESS/HAWK do) they live once in
[`FAEST/common/`](FAEST/common), with `src/`/`include/` split the same way
as each variant dir. `make gen-<VARIANT>`/`test-<VARIANT>` compile
`common/src/*.c` together with `<VARIANT>/src/*.c`. Each variant dir itself
holds only 4 files: its paramset engine (`faest_<variant>.c`/`.h`), its
`crypto_sign.c`/`api.h` wrapper — plus, checked in directly (not generated
on demand), a **`main.c`** and `include/test_vectors_<SUFFIX>.h` with one
real KAT vector baked in as C arrays, no file I/O, same
`TEST_KEY`/`TEST_SIGN`/`TEST_SIGN_OPEN` convention as
[`original/DS/FALCON/*/main.c`](../DS/FALCON):

```sh
cd original/Additional-DS/FAEST/FAEST_128F
gcc -O2 -std=c11 -I ../common/include -I include main.c src/*.c ../common/src/*.c -o test && ./test
```

Two portability fixes were applied uniformly across every `.c` file in both
`common/` and each variant dir (see
[`../../tools/assemble_faest.sh`](../../tools/assemble_faest.sh), which
regenerates the whole tree — `common/` plus all 12 variants' 4 files each —
from a fresh faest-ref checkout; `main.c`+`test_vectors_*.h` are then
(re)placed by [`../../tools/build_faest_main.sh`](../../tools/build_faest_main.sh)):

- `randomness.c`'s upstream `#if defined(HAVE_RANDOMBYTES) || ...` gate
  (needed when building via meson with `-DHAVE_RANDOMBYTES`) is forced to
  always take the "use the injected `randombytes()`" branch, since this repo
  only ever runs FAEST against a replayable DRBG for KAT testing.
- Every `.c` file gets `#define _GNU_SOURCE` prepended: `compat.h`
  auto-detects `mempcpy()`/`explicit_bzero()` as available on glibc and
  wires `faest_mempcpy`/`faest_explicit_bzero` straight to them, but without
  `_GNU_SOURCE` glibc's `<string.h>` doesn't declare their prototypes — the
  resulting implicit (int-returning) declaration truncates the real pointer
  return value on 64-bit and corrupts memory.

The vendored `sha3/` (XKCP, opt64 backend) subtree is flattened into
`common/src/`+`common/include/` (no subdirectories) rather than kept
nested, both to match every other family's flat layout and because the
static-test generator (`tools/gen_static_test.py`) only copies top-level
`*.c`/`*.h` files from each of a variant's directories, not subdirectories
— which is also why FAEST's `algo_registry.json` entries carry an
`extra_dirs: [".../FAEST/common/src", ".../FAEST/common/include"]` field
(the only family that needs it) so the static-test output stays
self-contained.

## Adding a new Additional-DS variant

1. Copy the algorithm sources into `original/Additional-DS/<FAMILY>/<VARIANT>/`
   (`src/`+`include/` split for CROSS/HAWK/LESS; `src/`+`include/` plus a
   shared `common/` for FAEST — see "FAEST" above).
2. Open `../../Makefile`, locate the `<FAMILY>_VARIANTS` list, and add the
   variant name.
3. Run `make gen-<VARIANT>` then `make test-<VARIANT>` to verify.

To add a new non-standardized signature family entirely, see "Adding a new
algorithm family" in [`../../README.md`](../../README.md).
