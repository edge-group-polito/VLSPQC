# SNOVA — ported from the [SNOVA reference implementation](https://github.com/SNOVA-project/SNOVA)

SNOVA, a multivariate signature scheme. 36 variants: 18 official parameter
sets (grouped in 3s per NIST level 1/3/5) × 2 public-key-expansion PRG
modes (plain = SHAKE, `_AES` = AES-CTR), each compile-time-fixed via its
own `include/snova_params.h`. All other `.c`/`.h` files are shared
unmodified across all 36 variants in [`common/`](common) — uses the `ref`
backend (`snova_ref.c`/`symmetric_ref.c`), not upstream's `opt`/`avx2` SIMD
backends, same convention as every other family here.

See [`../README.md`](../README.md#snova) for the full write-up (directory
layout, `.rsp` fields, `make` targets). This file covers just the
questions that don't change per-conversation: what had to change to drop
the external-library dependency, how to build/run each variant, and one
real caveat (stack size) the largest variants need.

## What was changed to make it library-free

**Nothing crypto-related** — like MAYO-C, SNOVA already ships fully
OpenSSL-free by default:

- `common/src/aes.c` (an OQS-derived AES implementation covering both a
  portable constant-time path and an AES-NI path, auto-selected by
  `#if __AVX2__ && __AES__`) gates its OpenSSL `EVP_*` calls behind
  `#ifdef USE_OPENSSL`, never defined anywhere in upstream's own build
  system (`src/Makefile`, `dist/makesnova.py`).
- `common/src/rng.c` (the NIST KAT DRBG) gates its OpenSSL include behind
  `#if 0` (permanently disabled) in favor of calling SNOVA's own
  `AES256_ECB` from `aes.c`.

No patch was needed to either file. The only change was **build-system
integration**, not dependency removal: upstream's `dist/makesnova.py`
writes each variant's tiny `snova_params.h` (`SNOVA_v`/`SNOVA_o`/`SNOVA_q`/
`SNOVA_l`/`SNOVA_r`/`AESCTR` — literally the only variant-specific file;
`snova.h`/`api.h`/`sign.c`/etc. compute every key/signature size from it
via preprocessor arithmetic) at *build* time, once per instance directory
it generates. Here, that same tiny header is checked in directly per
variant instead, mirroring `makesnova.py`'s own template exactly — see
that script for the authoritative 18-parameter-set list and naming
(`SNOVA_<v>_<o>_<q>_<l>[x<r>][_AES]`).

## Why it's bare-metal-ready

- **No external library**: `grep -rln openssl` finds `aes.c`/`rng.c`
  (see above), but neither include is ever live — confirmed by building
  and running all 36 variants with plain `gcc` and no `-I`/`-l` path near
  an OpenSSL install (see "Build & run" below).
- **No file I/O**: each variant's checked-in `main.c` reads its KAT vector
  from `include/test_vectors_<SUFFIX>.h` — plain C arrays baked in at
  compile time, no `fopen()`/`fread()` anywhere in the signing path.
- **No dynamic parameters**: each variant's `include/snova_params.h`
  hardcodes its own `v`/`o`/`q`/`l`/`r`/AES-mode macros — no `-D` flags
  needed to build a specific variant.
- **Profiling hooks present but empty**: each `main.c` has
  `PERF_CNT_CYCLES` hooks around keygen/sign/verify (currently no-ops);
  wire them to your SoC's cycle counter when porting to real hardware.

### One real caveat: stack size on the largest variants

The 8 level-5 variants — `SNOVA_97_33_16_2`, `SNOVA_96_32_19_2`,
`SNOVA_52_6_16_4x6`, `SNOVA_52_6_19_4x6` (×2 for `_AES`) — overflow the
default 8 MB Linux thread stack **inside upstream's own `snova_ref.c`**
keygen/sign/verify (large fixed-size local arrays that scale with `v`/`o`;
not a bug introduced by this port — verified reproducible with upstream's
own unmodified code, and fixed by simply giving the process more stack).
This repo's `make gen-*`/`test-*` recipes already bump the stack to 64 MB
via `ulimit -s` before invoking the generator/tester binaries. Do the same
when running these 8 variants' checked-in `main.c` standalone (see below)
or when porting to a bare-metal target with a small default stack —
64 MB was sufficient in testing, but if your target's actual constraint is
tighter, this is upstream algorithm behavior to budget for, not something
this port can shrink away.

## Build & run

### Quick check (whole family, via the repo's Makefile)

```sh
cd /home/aledolme/home/CODE/VLSPQC
make test-snova KATNUM=5      # dynamic .rsp pipeline, all 36 variants (~2 min)
```

### One variant, dynamic pipeline

```sh
make test-SNOVA_28_5_19_4 KATNUM=10       # SHAKE PRG mode
make test-SNOVA_28_5_19_4_AES KATNUM=10   # AES-CTR PRG mode
```

### Bare-metal style: compile the checked-in main.c directly (no Makefile, no file I/O)

```sh
cd algorithms/signatures/onramp/SNOVA/SNOVA_28_5_19_4
gcc -O2 -std=c11 -I ../common/include -I include main.c ../common/src/*.c -o test && ./test
```

For the 8 large variants listed above, bump the stack first:

```sh
cd algorithms/signatures/onramp/SNOVA/SNOVA_97_33_16_2
gcc -O2 -std=c11 -I ../common/include -I include main.c ../common/src/*.c -o test
(ulimit -s 65536 && ./test)
```

### All 36 variants at once, bare-metal style

```sh
cd algorithms/signatures/onramp/SNOVA
ulimit -s 65536   # covers the 8 large variants; harmless for the rest
for v in SNOVA_28_5_19_4_AES SNOVA_28_5_19_4 SNOVA_24_5_16_4_AES SNOVA_24_5_16_4 \
         SNOVA_48_17_16_2_AES SNOVA_48_17_16_2 SNOVA_48_16_19_2_AES SNOVA_48_16_19_2 \
         SNOVA_28_4_16_4x5_AES SNOVA_28_4_16_4x5 SNOVA_28_4_19_4x5_AES SNOVA_28_4_19_4x5 \
         SNOVA_40_7_19_4_AES SNOVA_40_7_19_4 SNOVA_37_8_16_4_AES SNOVA_37_8_16_4 \
         SNOVA_72_25_16_2_AES SNOVA_72_25_16_2 SNOVA_72_24_19_2_AES SNOVA_72_24_19_2 \
         SNOVA_38_5_16_4x5_AES SNOVA_38_5_16_4x5 SNOVA_38_5_19_4x5_AES SNOVA_38_5_19_4x5 \
         SNOVA_50_9_19_4_AES SNOVA_50_9_19_4 SNOVA_60_10_16_4_AES SNOVA_60_10_16_4 \
         SNOVA_97_33_16_2_AES SNOVA_97_33_16_2 SNOVA_96_32_19_2_AES SNOVA_96_32_19_2 \
         SNOVA_52_6_16_4x6_AES SNOVA_52_6_16_4x6 SNOVA_52_6_19_4x6_AES SNOVA_52_6_19_4x6; do
  gcc -O2 -std=c11 -I common/include -I "$v/include" \
      "$v/main.c" common/src/*.c -o /tmp/test_"$v" \
    && echo "$v: $(/tmp/test_"$v")"
done
```
