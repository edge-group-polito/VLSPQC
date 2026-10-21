# HQC — upstream provenance

Label: **HQC (NIST-selected, FIPS 207 pending — implements spec 2025-08-22)**. HQC was selected by NIST on
2025-03-11 as a backup KEM to ML-KEM. A draft standard was expected in 2026 and a final one in 2027; the
draft's FIPS number ("FIPS 207") and publication state could not be confirmed from NIST's own pages when this
was written, and this code is not "FIPS 207". The standard may change seed, salt and shared-secret sizes, so
these KATs may not match the final standard.

| Item | Value |
|---|---|
| Upstream | https://gitlab.com/pqc-hqc/hqc (official reference implementation) |
| Release tag | `v5.0.0` |
| Commit | `f46e542` ("Release v5.0.0", 2025-08-22), tip of `main` |
| Import / verification date | 2026-10-20 (the code was already in the repository; provenance first recorded here) |
| Implementation | `ref` (portable C) only; the upstream `x86_64/avx256` code is not used |
| License | public domain dedication (upstream `LICENSE`, copied here as `LICENSE`; also covers the PQClean-derived FIPS 202 code) |
| Variants | `HQC-2025/HQC-1`, `HQC-3`, `HQC-5` (security levels 1, 3, 5) |

## Newer upstream work not imported
`main` stops at v5.0.0. The `next-release` branch holds later, unreleased commits (e.g. `6fd7ec3`
"Update sampling", 2026-02-10, through 2026-09-25: GF(2^8) polynomial, RM(1,7) generator matrix, w / w_r / w_e
parameters, XOF domain separator, compressed `dk_kem`, KEM API). They change KATs and sizes and are a moving
target, so they are not used. (A commit `161cd4f` mentioned in earlier notes does not exist in this repository.)

## Local changes relative to v5.0.0 (line-level `diff` of every file)
Portability and debug cleanups only, no algorithmic change:
- `#include <immintrin.h>` removed from `hqc.h`, `parsing.h`, `vector.h`;
- `HQC-5/parameters.h`: `BITMASK` uses `(uint64_t)1 <<` instead of `1UL <<` (32-bit safe);
- debug prints commented out / removed in `kem.c`, `hqc.c`, `vector.c`, `reed_solomon.c`, and `#ifdef VERBOSE`
  guards dropped around them;
- flat `src/` + `include/` layout instead of upstream's `src/{common,ref}/` and `lib/fips202/`;
- `include/test_vectors_*.h`, `main.c`: repository test code, not upstream.

## Verification
- The seeds of the official KAT files `kats/ref/hqc-{1,3,5}/PQCkemKAT_{2321,4602,7333}.rsp` of v5.0.0 were
  replayed through these sources (`prng_init(seed)`, `crypto_kem_keypair`, `crypto_kem_enc`, `crypto_kem_dec`);
  pk / sk / ct / ss compared byte for byte: 100/100 records, 0 mismatches, for each of the three levels.
- The `.rsp` files produced by this repository's harness use a different seed derivation (NIST AES-CTR DRBG,
  see `harness/gen/PQCgenKAT_kem_hqc2025.c`) than the official files (HQC SHAKE PRNG seeded with 0..47), so they
  are valid but not byte-identical to the official `.rsp`; the replay above is the cross-check.
