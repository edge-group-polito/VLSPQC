# QRUOV — ported from [QR-UOV round 2](https://github.com/qruov/round2)

QR-UOV, a multivariate signature scheme built as a structured
(quotient-ring) variant of UOV. 24 variants: 12 official parameter sets
(4 per NIST level 1/3/5 — `q7L10`, `q31L3`, `q31L10`, `q127L3`) × 2
public-key-expansion PRG modes (`_a` = AES-CTR, `_s` = SHAKE), each
compile-time-fixed via its own generated `include/qruov_config.h` +
`include/api.h`. All other `.c`/`.h` files are shared unmodified (or, for
the crypto glue below, self-contained-replaced) across all 24 variants in
[`common/`](common).

See [`../README.md`](../README.md#qruov) for the full write-up (directory
layout, `.rsp` fields, `make` targets). This file covers just the two
questions that don't change per-conversation: what had to change to drop
the external-library dependency, and how to build/run each variant.

## What was changed to make it library-free

This is the deepest OpenSSL dependency of any Additional-DS family here —
not a 1-2 line patch like pqov/UOV, but a real rewrite:

- Upstream's default MGF (`mgf.c`, the scheme's core hash-based seed
  expander) reaches into **private** OpenSSL `EVP_MD_CTX`/`EVP_MD` struct
  layouts (vendored inline from `openssl-1.1.1t`/`openssl-3.1.0` source)
  and calls non-public `SHA3_absorb`/`SHA3_squeeze` symbols to get a
  streaming SHAKE XOF — OpenSSL's public API didn't expose "absorb
  repeatedly, then squeeze repeatedly" for XOFs in older releases.
- The PRG (public-key/secret-key matrix expansion from a seed, used
  `QRUOV_m` times per keygen/sign/verify) uses `EVP_aes_{128,192,256}_ctr`
  (AES-CTR keystream, key size keyed to the NIST security level) by
  default, or `EVP_shake{128,256}` (one-shot XOF) in `-DQRUOV_PRG_SHAKE`
  mode — both ported, giving the `_a`/`_s` variant split.

Replaced with self-contained code in [`common/`](common):

- **`mgf.c`/`mgf.h`** — rewritten on the portable `fips202.c` (copied from
  pqov's `utils/fips202.c`, same file already used by the pqov/UOV and
  MAYO-C ports in this repo): its `keccak_state` naturally supports
  "absorb, finalize once, then squeeze repeatedly" — exactly the
  `MGF_init`/`MGF_update`/`MGF_yield` contract the rest of the codebase
  calls through. No OpenSSL internals needed.
- **`qruov_prg.c`/`qruov_prg.h`** (new files — upstream has no equivalent;
  these were `static` functions inline in `qruov.c`) — the AES-CTR PRG is
  built on MAYO-C's portable AES-128/192/256 (`qruov_aes.h`/`aes_c.c`,
  copied from MAYO-C's `src/common/aes_c.c`: constant-time, BearSSL-derived,
  ECB-only), wrapped as CTR-mode-of-zeros == raw keystream, matching
  upstream's own `memset(dst,0,len); EVP_EncryptUpdate(...)` trick.
  `qruov_aes.h` is a new header extracting `aes_c.c`'s
  `aes{128,192,256}ctx` types + ECB-only prototypes (upstream MAYO-C keeps
  them file-local) so `qruov_prg.c` can share them without duplicating the
  struct layout by hand. The SHAKE-mode PRG is a one-shot fips202
  absorb+squeeze.
- **`qruov.c`** — trimmed of the ~150 lines of OpenSSL PRG/MGF/hash glue
  (`RejSamp`, `PRG_init_*`, `RejSampPRG_*`, `Expand_mu`, `Hash`); now just
  `#include`s `qruov_prg.h` for those declarations.
- **`qruov.h`** — OpenSSL includes and the `EVP_AES_CTR`/`EVP_SHAKE`
  selection macros removed; the `PRG_init`/`PRG_yield`/... name-switching
  table (AES-CTR vs SHAKE, on `QRUOV_PRG_SHAKE`) kept as upstream had it,
  just repointed at the `qruov_prg.h`/`mgf.h` functions.
- **`rng.c`** (the NIST KAT DRBG) — same `AES256_ECB` OpenSSL-EVP-to-
  software-AES swap as pqov/UOV, but kept QRUOV's own `rng.h` contract
  (`int`-returning `randombytes()`) since QRUOV's own `sign.c`/`api.c`
  already expect that exact signature.
- `refcount.h` (a small OpenSSL-internal-struct-refcounting helper only
  the old `mgf.c` needed) was dropped entirely.

Both `qruov_config.h` and `api.h` per variant were produced by literally
compiling and running upstream's own `qruov_config_h_gen.c`/`api_h_gen.c`
generator programs once per variant — those generators are pure macro
printfs with zero OpenSSL dependency, untouched.

## Why it's bare-metal-ready

- **No external library**: `grep -rn "#include <openssl"` over this tree
  returns nothing live — the only remaining hits are inside a
  commented-out block in `rng.c` (kept only as provenance) and prose
  comments in `mgf.h` explaining what was replaced.
- **No file I/O**: each variant's checked-in `main.c` reads its KAT vector
  from `include/test_vectors_<SUFFIX>.h` — plain C arrays baked in at
  compile time, no `fopen()`/`fread()` anywhere in the signing path.
- **No dynamic parameters**: each variant's `include/qruov_config.h`
  hardcodes its own `q`/`v`/`m`/`L`/PRG-mode macros — no `-D` flags needed
  to build a specific variant.
- **Profiling hooks present but empty**: each `main.c` has
  `PERF_CNT_CYCLES` hooks around keygen/sign/verify (currently no-ops);
  wire them to your SoC's cycle counter when porting to real hardware.
- **Verified memory-safe**: the new `qruov_prg.c` PRG-context allocation/
  copy/free paths were checked with AddressSanitizer (no leaks, no
  double-frees, no use-after-free) across parameter sets spanning all
  three field sizes (q=7,31,127), both L values (3,10), and all three
  security levels.

## Build & run

### Quick check (whole family, via the repo's Makefile)

```sh
cd /home/aledolme/home/CODE/VLSPQC
make test-qruov KATNUM=2      # dynamic .rsp pipeline, all 24 variants (~2-3 min)
```

### One variant, dynamic pipeline

```sh
make test-QRUOV_1_q127L3v156m54_a KATNUM=10   # AES-CTR PRG mode
make test-QRUOV_1_q127L3v156m54_s KATNUM=10   # SHAKE PRG mode
```

### Bare-metal style: compile the checked-in main.c directly (no Makefile, no file I/O)

```sh
cd original/Additional-DS/QRUOV/QRUOV_1_q127L3v156m54_a
gcc -O2 -std=c11 -I ../common/include -I include main.c ../common/src/*.c -o test && ./test
```

### All 24 variants at once, bare-metal style

```sh
cd original/Additional-DS/QRUOV
for v in QRUOV_1_q7L10v740m100_a QRUOV_1_q7L10v740m100_s \
         QRUOV_1_q31L3v165m60_a QRUOV_1_q31L3v165m60_s \
         QRUOV_1_q31L10v600m70_a QRUOV_1_q31L10v600m70_s \
         QRUOV_1_q127L3v156m54_a QRUOV_1_q127L3v156m54_s \
         QRUOV_3_q7L10v1100m140_a QRUOV_3_q7L10v1100m140_s \
         QRUOV_3_q31L3v246m87_a QRUOV_3_q31L3v246m87_s \
         QRUOV_3_q31L10v890m100_a QRUOV_3_q31L10v890m100_s \
         QRUOV_3_q127L3v228m78_a QRUOV_3_q127L3v228m78_s \
         QRUOV_5_q7L10v1490m190_a QRUOV_5_q7L10v1490m190_s \
         QRUOV_5_q31L3v324m114_a QRUOV_5_q31L3v324m114_s \
         QRUOV_5_q31L10v1120m120_a QRUOV_5_q31L10v1120m120_s \
         QRUOV_5_q127L3v306m105_a QRUOV_5_q127L3v306m105_s; do
  gcc -O2 -std=c11 -I common/include -I "$v/include" \
      "$v/main.c" common/src/*.c -o /tmp/test_"$v" \
    && echo "$v: $(/tmp/test_"$v")"
done
```
