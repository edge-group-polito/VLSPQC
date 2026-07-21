# Additional-DS — non-standardized signature schemes

Reference-implementation sources for digital-signature schemes that are
**not (yet) standardized** by NIST: CROSS, HAWK, LESS, FAEST, UOV, MAYO,
QRUOV, SNOVA, SDitH, and SQIsign were submitted to NIST's additional-signatures
on-ramp but haven't been finalized into a FIPS the way ML-DSA (FIPS 204),
SLH-DSA (FIPS 205), and FALCON/FN-DSA have. They live in a separate category from
[`../DS/`](../DS/README.md) so that `make test-all`/`gen-all`/`static-test-all`
— which cover only standardized algorithms — don't pull them in by default.
Use this category's own aggregate targets (`test-additional-ds`,
`gen-additional-ds`, `static-test-additional-ds`) or the per-family/per-variant
targets below.

Each family directory holds one subdirectory per parameter-set variant
(`src/`/`include/` split for all ten families) and exposes a compile-time-fixed
`crypto_sign_keypair`/`crypto_sign`/`crypto_sign_open` API. These sources are
compiled together with `kat/gen/PQCgenKAT_sign_*.c` (generation) or
`kat/gen/main_*.c` (testing) — see [`../../kat/README.md`](../../kat/README.md)
for how that build works, and [`../../README.md`](../../README.md) for the
`make` targets that drive it.

## Families

| Family | Variants | Security levels | Notes |
|--------|----------|------------------|-------|
| CROSS | 18 (RSDP/RSDPG × 1/3/5 × b/f/s) | 128, 192, 256 bit | shared SHAKE CSPRNG |
| HAWK | 3 (256, 512, 1024) | 128, 192, 256 bit | own NIST DRBG (`src/rng.c`) |
| LESS | 7 | 128, 192, 256 bit | shared SHAKE CSPRNG, same model as CROSS |
| FAEST | 12 (plain/EM × 128/192/256 × f/s) | 128, 192, 256 bit | own NIST DRBG; ~30 files shared across all 12 variants live once in `FAEST/common/` |
| UOV | 12 (Ip/Is/III/V × classic/pkc/pkc_skc) | 128, 192, 256 bit | own NIST DRBG; all `.c`/`.h` shared across all 12 variants live once in `UOV/common/`, each variant dir holds only its own `include/params.h` |
| MAYO | 4 (MAYO_1/2/3/5) | 128, 192, 256 bit | own NIST DRBG; already OpenSSL-free upstream; all `.c`/`.h` shared across all 4 variants live once in `MAYO/common/`, each variant dir holds only its own `src/api.c` + `include/{api.h,mayo_variant.h}` |
| QRUOV | 24 (4 params/level × 3 levels × AES-CTR/SHAKE PRG) | 128, 192, 256 bit | own NIST DRBG; own self-contained MGF/PRG (see "QRUOV" below); all `.c`/`.h` shared across all 24 variants live once in `QRUOV/common/`, each variant dir holds only its own generated `include/{qruov_config.h,api.h}` |
| SNOVA | 36 (6 params/level × 3 levels × AES-CTR/SHAKE PRG) | 128, 192, 256 bit | own NIST DRBG; already OpenSSL-free upstream; all `.c`/`.h` shared across all 36 variants live once in `SNOVA/common/`, each variant dir holds only its own `include/snova_params.h`; the 8 level-5 variants need a bumped stack (see "SNOVA" below) |
| SDitH | 12 (Hypercube/Threshold × 1/3/5 × gf256/p251) | 128, 192, 256 bit | own NIST DRBG (patched off OpenSSL, see "SDitH" below); full self-contained copy per variant (like HAWK/LESS), no shared `common/` — the two sub-architectures are structurally distinct codebases |
| SQIsign | 3 (lvl1/lvl3/lvl5) | 128, 192, 256 bit | own NIST DRBG; already OpenSSL-free and (via mini-gmp) real-GMP-free upstream; 48 `.c`+33 `.h` shared across all 3 variants live once in `SQISIGN/common/`, each variant dir holds only its own 8 `.c` + 11 `.h` (api, field arithmetic, precomputed constants) — see "SQIsign" below |

**`test-additional-ds` covers all 131 variants above.**

## Per-family `make` targets

Prefix with `test-` or `gen-`: `cross`, `hawk`, `less`, `faest`, `uov`, `mayo`,
`qruov`, `snova`, `sdith`, `sqisign` (aggregate: `test-additional-ds`/`gen-additional-ds`,
**not** part of `test-all`/`gen-all`).

Per-variant targets are `test-<NAME>`/`gen-<NAME>` using each variant's
directory name directly, e.g. `make test-CROSS_1_RSDP_b`,
`make test-hawk-256`, `make test-LESS_252_45`, `make test-FAEST_128F`,
`make test-UOV_Ip`, `make test-MAYO_1`, `make test-QRUOV_1_q127L3v156m54_a`,
`make test-SNOVA_28_5_19_4`, `make test-sdith_hypercube_cat1_gf256`,
`make test-sqisign_lvl1`.

## `.rsp` fields specific to Additional-DS

All `.rsp` files carry the standard `count`/`seed`/`mlen`/`msg`/`pk`/`sk`/
`smlen`/`sm` fields (see [`../../kat/README.md`](../../kat/README.md) for the
full format):

| Algorithm | Extra fields |
|-----------|--------------|
| CROSS, LESS | `seed` is the CSPRNG seed (48 B); no extra fields |
| HAWK, FAEST, UOV, MAYO, QRUOV, SNOVA, SDitH, SQIsign | `seed` is the NIST DRBG seed (48 B) |

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

### UOV

Ported from [pqov](https://github.com/pqov/pqov) ("OV signature", the
reference implementation of Unbalanced Oil and Vinegar). Unlike FAEST's
common/ split — forced by genuinely per-variant-instantiated template
output — UOV's 12 variants (4 security levels: Ip/Is/III/V, NIST levels 1/1/3/5,
× 3 key formats: classic/pkc/pkc_skc) differ by exactly **one file**:
`include/params.h`, which just `#define`s which of the 4 parameter sets and
which of the 3 key formats is active (pqov's own `.c`/`.h` sources are
already macro-driven off that one header, unlike FAEST's per-parameter-set
templated engine files). So every `.c`/`.h` pqov ships lives once in
[`UOV/common/`](UOV/common), and each `UOV_<VARIANT>/` directory holds
nothing but its own compile-time-fixed `include/params.h` (generated the
same way pqov's own `create_nist_project.py` instantiates per-variant NIST
submission directories: copy `params.h`, then replace the `_OV256_112_44`/
`_OV_CLASSIC` lines with the variant's actual macros) plus the checked-in
`main.c`/`include/test_vectors_<SUFFIX>.h` pair:

```sh
cd original/Additional-DS/UOV/UOV_Ip
gcc -O2 -std=c11 -I ../common/include -I include main.c ../common/src/*.c -o test && ./test
```

pqov defaults to OpenSSL for both SHAKE (`utils_hash.c`'s `_UTILS_OPENSSL_`
path, via `EVP_shake256`) and the KAT DRBG (`utils/nistkat/rng.c`'s
`AES256_ECB`, via `EVP_aes_256_ecb`). Two small patches in
[`UOV/common/include/`](UOV/common/include) remove that dependency entirely:

- `config.h`: the default-selection block no longer defines
  `_UTILS_OPENSSL_`, so `utils_hash.c`/`.h` fall through to their existing
  self-contained `fips202.c` (Keccak/SHAKE) branch instead; `_NIST_KAT_` is
  defined unconditionally.
- `utils_randombytes.h`: its `#elif defined(_NIST_KAT_) && defined(_UTILS_OPENSSL_)`
  guard (which pulled in `rng.h`) is relaxed to just `defined(_NIST_KAT_)`,
  since `rng.c`/`rng.h` here are the same self-contained software AES-256
  implementation `kat/gen/rng.c` and HAWK's `src/rng.c` already use — no
  longer tied to OpenSSL's `AES256_ECB`.

UOV has no derand entry point, so `crypto_sign_keypair()` is always called
(even with `RUN_KEYGEN=0`) purely to advance the DRBG — same shape and same
caveat as HAWK/FAEST. `common/src/rng.c` IS the NIST AES-256-CTR DRBG (same
as HAWK's own copy) — do not additionally link `kat/gen/rng.c`.

Classic-mode public/secret keys are large (up to ~2.87 MB / ~2.44 MB for
`UOV_V`): [`kat/gen/PQCgenKAT_sign_uov.c`](../../kat/gen/PQCgenKAT_sign_uov.c)
and [`kat/gen/main_uov.c`](../../kat/gen/main_uov.c) heap-allocate `pk`/`sk`
(and, in `main_uov.c`, `pk_rsp`/`sk_rsp`) instead of using stack arrays like
`main_hawk.c` does, to avoid a stack overflow when all of them are live at
once. `common/kat_parser.h`'s `KAT_DATA_MAX` is sized at 4 MB (up from 256 KB)
to fit these fields' hex encoding.

### MAYO

Ported from [MAYO-C](https://github.com/PQCMayo/MAYO-C), the reference C
implementation of MAYO, a multivariate-quadratic signature scheme. Unlike
pqov/UOV, MAYO-C ships **already free of any external library**: its NIST
CTR-DRBG (`common/randombytes_ctrdrbg.c`) and AES (`common/aes_c.c`,
BearSSL-derived) are self-contained portable C, and SHAKE
(`common/fips202.c`) is TweetFips202-derived — no patch was needed there.

Like UOV, MAYO's 4 variants (MAYO_1/2/3/5, NIST levels 1/1/3/5) differ by
essentially nothing at the source level: every parameter set's constants are
already `#define`d unconditionally in `include/mayo.h`, selected via
token-pasting on a `MAYO_VARIANT` macro (upstream sets this with
`-DMAYO_VARIANT=MAYO_1` at the CMake level — see MAYO-C's `src/CMakeLists.txt`).
So `mayo.c`/`params.c`/`arithmetic.c` and all of `common/` live once in
[`MAYO/common/`](MAYO/common), and each `MAYO_<N>/` directory holds only
upstream's own tiny per-variant `src/api.c` (the `crypto_sign_*` wrapper) +
`include/api.h`, plus a generated `include/mayo_variant.h`:

```c
#define MAYO_VARIANT MAYO_1
#define MAYO_BUILD_TYPE_OPT
```

`common/include/mayo.h` gained one small patch — a guarded
`#include "mayo_variant.h"` right after its include guard — so that header
is picked up automatically before `mayo.h`'s own `#if defined(MAYO_VARIANT)`
checks run, making each variant compile-time-fixed with no `-D` flags needed
(matching every other family here; upstream's own build always passes
`-DMAYO_VARIANT` externally). Builds against the portable `src/generic`
backend (no AVX2/NEON/AES-NI):

```sh
cd original/Additional-DS/MAYO/MAYO_1
gcc -O2 -std=c11 -I ../common/include -I include main.c ../common/src/*.c src/api.c -o test && ./test
```

Two loose ends, both handled in `tools/build_mayo_main.sh` rather than by
changing the shared `tools/gen_static_test.py` renderer (which HAWK/FAEST/UOV
also use, correctly, for their own APIs):

- The `ds_drbg_hawk` shape's static-test renderer hardcodes
  `#include "rng.h"` in the generated `main.c` (matching HAWK/FAEST/UOV's own
  header name), but MAYO-C's DRBG header is `randombytes.h`. Rather than
  rename anything upstream, [`MAYO/common/include/rng.h`](MAYO/common/include/rng.h)
  is a two-line shim that just `#include`s `randombytes.h`.
- The same renderer also hardcodes `unsigned long long smlen, mlen1;`
  (matching HAWK/FAEST/UOV's API), but MAYO's `crypto_sign`/`crypto_sign_open`
  take `size_t *` (see `src/mayo_*/api.h`). Passing the wrong pointer type
  happens to work on any LP64 host (both are 8 bytes there) but is
  undefined behavior and would break on a 32-bit target where
  `sizeof(size_t) != sizeof(unsigned long long)` — `build_mayo_main.sh`
  rewrites that one declaration to `size_t` as a post-processing step.

MAYO has no derand entry point, so `crypto_sign_keypair()` is always called
(even with `RUN_KEYGEN=0`) purely to advance the DRBG — same shape and same
caveat as HAWK/FAEST/UOV. `common/src/randombytes_ctrdrbg.c` IS the NIST
AES-256-CTR DRBG — do not additionally link `kat/gen/rng.c`.

### QRUOV

Ported from the QR-UOV round-2 reference implementation
([github.com/qruov/round2](https://github.com/qruov/round2)), a
multivariate signature scheme. Upstream builds 12 official parameter sets
(4 per NIST level 1/3/5: `q7L10`, `q31L3`, `q31L10`, `q127L3` — see
`src/qruov_config.src`) × 2 public-key-expansion PRG modes (`a` = AES-CTR,
`s` = SHAKE) = 24 combinations, via its own `src/Makefile`, using the
`ref` backend (not `portable64`/`avx2`/`avx512`). Like UOV, every variant
here differs by exactly one generated header pair —
`include/qruov_config.h` + a derived `include/api.h` — both produced by
literally compiling and running upstream's own `qruov_config_h_gen.c`/
`api_h_gen.c` generator programs once per variant (same idea as UOV's
`create_nist_project.py`-style `params.h` instantiation, but using
upstream's actual generator instead of text substitution). Every other
`.c`/`.h` lives once in [`QRUOV/common/`](QRUOV/common):

```sh
cd original/Additional-DS/QRUOV/QRUOV_1_q127L3v156m54_a
gcc -O2 -std=c11 -I ../common/include -I include main.c ../common/src/*.c -o test && ./test
```

**OpenSSL dependency here is much deeper than pqov/MAYO-C**, and needed a
real rewrite rather than a one- or two-line patch:

- Upstream's default MGF (`mgf.c`) reaches into **private** OpenSSL
  `EVP_MD_CTX`/`EVP_MD` struct internals (vendored inline from
  `openssl-1.1.1t`/`openssl-3.1.0` source, both versions) and calls
  non-public `SHA3_absorb`/`SHA3_squeeze` symbols to get a streaming SHAKE
  XOF — OpenSSL's public API doesn't expose "absorb repeatedly, then
  squeeze repeatedly" for XOFs in older releases, so upstream reached
  around it instead.
- The PRG (public-key/secret-key matrix expansion from a seed, used
  `QRUOV_m` times per keygen/sign/verify) additionally uses
  `EVP_aes_{128,192,256}_ctr` (AES-CTR keystream, key size keyed to the
  NIST security level) in the default build, or `EVP_shake{128,256}`
  (one-shot XOF) in `-DQRUOV_PRG_SHAKE` mode.

Both replaced with self-contained code in `QRUOV/common/`:

- **`mgf.c`/`mgf.h`** — rewritten on the portable `fips202.c` already used
  by the pqov/UOV and MAYO-C ports (copied from pqov's `utils/fips202.c`):
  its `keccak_state` naturally supports "absorb, finalize once, then
  squeeze repeatedly", exactly the `MGF_init`/`MGF_update`/`MGF_yield`
  contract the rest of the codebase calls through — no OpenSSL internals
  needed.
- **`qruov_prg.c`/`qruov_prg.h`** (new files — upstream has no equivalent;
  these were `static` functions inline in `qruov.c`) — the AES-CTR PRG is
  built on MAYO-C's portable AES-128/192/256 (`qruov_aes.h`/`aes_c.c`,
  copied from MAYO-C's `src/common/aes_c.c`: constant-time,
  BearSSL-derived, ECB-only) wrapped as CTR-mode-of-zeros == raw
  keystream, matching upstream's own
  `memset(dst,0,len); EVP_EncryptUpdate(...)` trick; `qruov_aes.h` is a
  new header extracting `aes_c.c`'s `aes{128,192,256}ctx` types +
  ECB-only prototypes (upstream MAYO-C keeps them file-local) so
  `qruov_prg.c` can share them without duplicating the struct layout by
  hand. The SHAKE-mode PRG is a one-shot fips202 absorb+squeeze.
- **`qruov.c`** — trimmed of the ~150 lines of OpenSSL PRG/MGF/hash glue
  (`RejSamp`, `PRG_init_*`, `RejSampPRG_*`, `Expand_mu`, `Hash`); now just
  `#include`s `qruov_prg.h` for those declarations.
- **`qruov.h`** — OpenSSL includes and the `EVP_AES_CTR`/`EVP_SHAKE`
  selection macros removed; the `PRG_init`/`PRG_yield`/... name-switching
  table (AES-CTR vs SHAKE, on `QRUOV_PRG_SHAKE`) kept as upstream had it,
  just repointed at the `qruov_prg.h`/`mgf.h` functions.
- **`rng.c`** — same `AES256_ECB` OpenSSL-EVP-to-software-AES swap as
  pqov/UOV (see "UOV" above), but kept QRUOV's own `rng.h` contract
  (`int`-returning `randombytes()`, unlike HAWK/FAEST/UOV/MAYO's
  `void`/`int`-returning `kat/gen/rng.h` convention) since QRUOV's own
  `sign.c`/`api.c` already expect that exact signature.
- `refcount.h` (a small OpenSSL-internal-struct-refcounting helper only
  the old `mgf.c` needed) was dropped entirely — nothing includes it once
  `mgf.c` no longer needs it.

QRUOV has no derand entry point, so `crypto_sign_keypair()` is always
called (even with `RUN_KEYGEN=0`) purely to advance the DRBG — same shape
and same caveat as HAWK/FAEST/UOV/MAYO. `common/src/rng.c` IS the NIST
AES-256-CTR DRBG — do not additionally link `kat/gen/rng.c`.

### SNOVA

Ported from the SNOVA reference implementation, a multivariate signature
scheme. 36 variants: 18 official parameter sets (grouped in 3s per NIST
level 1/3/5, see `dist/makesnova.py` for the authoritative list) × 2
public-key-expansion PRG modes (plain = SHAKE, `_AES` = AES-CTR) — using
the `ref` backend (`snova_ref.c`/`symmetric_ref.c`), not upstream's
`opt`/`avx2` SIMD backends.

**No OpenSSL patch was needed at all** — unlike UOV/QRUOV, and like
MAYO-C, SNOVA already ships fully self-contained by default: its AES
(`common/src/aes.c`, an OQS-derived portable/AES-NI implementation) and
its NIST KAT DRBG (`common/src/rng.c`) both gate their OpenSSL `EVP_*`
calls behind `#ifdef USE_OPENSSL` / `#if 0`, neither ever defined by
upstream's own build system. The only integration work was per-variant:
every variant differs by exactly one tiny generated header,
`include/snova_params.h` (`SNOVA_v`/`SNOVA_o`/`SNOVA_q`/`SNOVA_l`/
`SNOVA_r`/`AESCTR` — `snova.h`/`api.h`/`sign.c`/etc. compute every key/
signature size from it via preprocessor arithmetic, no other file
differs), checked in directly per variant instead of being written by
upstream's `dist/makesnova.py` at build time — mirroring that script's own
template exactly:

```sh
cd original/Additional-DS/SNOVA/SNOVA_28_5_19_4
gcc -O2 -std=c11 -I ../common/include -I include main.c ../common/src/*.c -o test && ./test
```

**One real caveat, not an OpenSSL issue**: the 8 level-5 variants
(`SNOVA_97_33_16_2`, `SNOVA_96_32_19_2`, `SNOVA_52_6_16_4x6`,
`SNOVA_52_6_19_4x6`, ×2 for `_AES`) overflow the default 8 MB Linux
thread stack **inside upstream's own `snova_ref.c`** keygen/sign/verify
(large fixed-size local arrays that scale with `v`/`o`) — reproducible
with upstream's own unmodified code, not a bug introduced by this port,
and fixed simply by giving the process more stack. `make gen-*`/`test-*`
bump the stack to 64 MB via `ulimit -s` before invoking the generator/
tester binaries; do the same before running these 8 variants' checked-in
`main.c` standalone.

SNOVA has no derand entry point, so `crypto_sign_keypair()` is always
called (even with `RUN_KEYGEN=0`) purely to advance the DRBG — same shape
and same caveat as HAWK/FAEST/UOV/MAYO/QRUOV. `common/src/rng.c` is
SNOVA's own copy of the NIST AES-256-CTR DRBG — do not additionally link
`kat/gen/rng.c`.

### SDitH

Ported from the SDitH reference implementation, an MPC-in-the-Head
code-based signature. 12 variants: 2 sub-architectures
(**Hypercube_Variant**, **Threshold_Variant**) × 3 NIST security categories
(cat1/cat3/cat5) × 2 underlying fields (gf256/p251). Unlike FAEST/UOV/MAYO/
QRUOV/SNOVA, no shared `common/` was used — full self-contained copy per
variant instead (like HAWK/LESS), because the two sub-architectures are
structurally distinct codebases (Hypercube: ~15 files led by
`sdith.c`/`sign.c`/`treeprg.c`; Threshold: ~25 files led by
`mpc.c`/`keygen.c`/`commit.c`/`merkle-tree.c`).

Unlike MAYO-C/SNOVA, SDitH's `generator/rng.c` (the NIST KAT DRBG) **does**
call OpenSSL for `AES256_ECB()` — patched to use the same self-contained
software AES-256 block cipher already used by `kat/gen/rng.c` and reused
across UOV/QRUOV. Two more per-variant patches make each variant
compile-time-fixed: Hypercube's `param.h` hardcodes `CAT_1`/`CAT_3`/`CAT_5`
(same technique as UOV's `params.h`; Threshold's `parameters.h` is already
fully per-directory-instantiated upstream, no equivalent patch needed), and
both hardcode `#define NDEBUG` (upstream's own Makefile always passes
`-DNDEBUG`; without it Hypercube's `sdith.c` prints verbose debug hex dumps
on every keygen/sign/verify). See
[`SDITH/README.md`](SDITH/README.md) for the full patch list, including a
filename collision unique to Hypercube's flattened `sha3/` + core-XOF
`rng.h`/`rng.c` pair, and a same-directory quote-include quirk in
Threshold's `rnd.h` that only bites inside `kat/gen/`.

```sh
cd original/Additional-DS/SDITH/sdith_hypercube_cat1_gf256
gcc -O2 -std=c11 -I include main.c src/*.c -o test && ./test
```

Like FALCON, SDitH's signature length is bounded above by `CRYPTO_BYTES`
but the actual signed-message length varies per message/seed — the
checked-in `main.c` already handles this via the per-vector
`TVEC_IN_SM_SIGN_LEN[i]` array, same pattern as UOV/MAYO/QRUOV/SNOVA.

SDitH has no derand entry point, so `crypto_sign_keypair()` is always
called (even with `RUN_KEYGEN=0`) purely to advance the DRBG — same shape
and same caveat as HAWK/FAEST/UOV/MAYO/QRUOV/SNOVA. Each variant's own
`src/rng.c` is its own patched copy of the NIST AES-256-CTR DRBG — do not
additionally link `kat/gen/rng.c`.

### SQIsign

Ported from the-sqisign reference implementation, an isogeny-based
signature over quaternion algebras — a different mathematical structure
from every other family here (all multivariate or code-based). 3 variants
(lvl1/lvl3/lvl5, NIST levels 1/3/5), "ref" build type (no ASM/SIMD).

**No OpenSSL, no real GMP** — upstream already ships its own AES
(`common/src/aes_c.c`, backing the NIST KAT DRBG) and its own bundled
mini-gmp (`GMP_LIBRARY=MINI` in upstream's own CMake) instead of libgmp.
Every `#include <gmp.h>` in the source is behind `#if defined(MINI_GMP)
... #else #include <gmp.h> #endif`, dead code here since `MINI_GMP` is
always defined (see below). Of the ~72 files needed per variant, 48 `.c` +
33 `.h` are byte-identical text across all 3 levels and live once in
`SQISIGN/common/`; only 8 `.c` + 11 `.h` per level actually differ
(`api.c`/`api.h`, one field-arithmetic file, and 6 SageMath-precomputed
constant files/headers), matching MAYO's `common/` + per-variant
`src/api.c` split.

The only real integration work was making the build compile-time-fixed
with zero `-D` flags: upstream's CMake passes five global flags (`MINI_GMP`,
`RANDOMBYTES_C`, `HAVE_UINT128`, `RADIX_64`, `GMP_LIMB_BITS=64`) that aren't
tied to any single naturally-first-included header across this codebase's
~40 files, so they're hardcoded (`#ifndef`-guarded) into a small
`common/include/sqisign_build_config.h`, `#include`d as the literal first
line of every `.c` file. Two more (`SQISIGN_VARIANT`, the one value that's
genuinely per-level, plus `SQISIGN_BUILD_TYPE_REF`/`ENABLE_SIGN`) are
hardcoded the same way inside each variant's own copy of
`sqisign_namespace.h` — see
[`SQISIGN/README.md`](SQISIGN/README.md) for the full explanation and how
this was cross-checked against upstream's own official `KAT/*.rsp` before
any flattening began.

```sh
cd original/Additional-DS/SQISIGN/sqisign_lvl1
gcc -O2 -std=c11 -I ../common/include -I include main.c ../common/src/*.c src/*.c -o test -lm && ./test
```

SQIsign has no derand entry point, so `crypto_sign_keypair()` is always
called (even with `RUN_KEYGEN=0`) purely to advance the DRBG — same shape
and same caveat as HAWK/FAEST/UOV/MAYO/QRUOV/SNOVA/SDitH.
`common/src/randombytes_ctrdrbg.c` is the NIST AES-256-CTR DRBG — do not
additionally link `kat/gen/rng.c`. Needs `-lm` at link time — the only
family here that does (quaternion arithmetic uses libm).

## Adding a new Additional-DS variant

1. Copy the algorithm sources into `original/Additional-DS/<FAMILY>/<VARIANT>/`
   (`src/`+`include/` split for CROSS/HAWK/LESS/SDitH; `src/`+`include/` plus a
   shared `common/` for FAEST/UOV/MAYO/QRUOV/SNOVA/SQIsign — see
   "FAEST"/"UOV"/"MAYO"/"QRUOV"/"SNOVA"/"SDitH"/"SQIsign" above).
2. Open `../../Makefile`, locate the `<FAMILY>_VARIANTS` list, and add the
   variant name.
3. Run `make gen-<VARIANT>` then `make test-<VARIANT>` to verify.

To add a new non-standardized signature family entirely, see "Adding a new
algorithm family" in [`../../README.md`](../../README.md).
