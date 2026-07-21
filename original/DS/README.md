# Digital Signature (DS) algorithms — standardized

Reference-implementation sources for the **standardized** digital-signature
families covered by the KAT suite: finalized/near-finalized NIST PQC
standards (FIPS 204, FIPS 205, and FN-DSA/FALCON). Each family directory
holds one subdirectory per parameter-set variant (`src/`/`include/` split)
and exposes a compile-time-fixed
`crypto_sign_keypair`/`crypto_sign`/`crypto_sign_open` API.

Two related but distinct categories live elsewhere and are **not** part of
`test-all`/`gen-all`/`static-test-all`:

- [`../Additional-DS/README.md`](../Additional-DS/README.md) — CROSS, HAWK,
  LESS: signature schemes that are not (yet) standardized.
- [`../Stateful-DS/README.md`](../Stateful-DS/README.md) — XMSS, LMS, HSS:
  stateful hash-based signatures standardized separately under NIST
  SP 800-208, with runtime-parametrized APIs unlike every family here.

These sources are compiled together with `kat/gen/PQCgenKAT_sign_*.c`
(generation) or `kat/gen/main_*.c` (testing) — see
[`../../kat/README.md`](../../kat/README.md) for how that build works, and
[`../../README.md`](../../README.md) for the `make` targets that drive it.

## Families

| Family | Variants | Security levels | Notes |
|--------|----------|------------------|-------|
| FALCON | 2 (512, 1024) | 128, 256 bit | explicit-randomness API, ragged signature length |
| ML-DSA | 3 (2, 3, 5) | 128, 192, 256 bit | explicit-seed API |
| SLH-DSA | 9 (SHAKE, simple × 128/192/256{s,f}; robust × 128/192/256f) | 128, 192, 256 bit | reference SPHINCS+; "s" from a 2nd, simple-only codebase |
| SLH-DSA-Haraka | 6 | 128, 192, 256 bit | Haraka-based SPHINCS+ |
| SLH-DSA-Haraka-AES | 6 | 128, 192, 256 bit | RISC-V only (`aes64esm`) |
| SLH-DSA-SHA2 | 9 (simple × 128/192/256{s,f}; robust × 128/192/256f) | 128, 192, 256 bit | SHA2-based SPHINCS+; "s" as above |

**`test-all`/`test-ds` covers all 33 variants above.**

## Per-family `make` targets

Prefix with `test-` or `gen-`: `falcon`, `mldsa`, `slhdsa`, `slhdsa-haraka`,
`slhdsa-haraka-aes`, `slhdsa-sha2` (aggregate: `test-ds`/`gen-ds`, part of
`test-all`/`gen-all`).

Per-variant targets are `test-<NAME>` / `gen-<NAME>`, where `<NAME>` is the
directory name for most families. SLH-DSA families use sanitized names since
`+` and `/` aren't valid in Make targets:

| Safe target name | Algorithm |
|-----------------|-----------|
| `SPHINCSp<L>f{simple,robust}` | SLH-DSA / SPHINCS+-`<L>`f-{simple,robust} |
| `SPHINCSp<L>ssimple` | SLH-DSA / SPHINCS+-`<L>`s-simple |
| `HARAKAp<L>f{simple,robust}` | SLH-DSA-Haraka / SPHINCS+-`<L>`f-{simple,robust} |
| `HARAKAAESp<L>f{simple,robust}` | SLH-DSA-Haraka-AES / SPHINCS+-`<L>`f-{simple,robust} |
| `SHA2p<L>f{simple,robust}` | SLH-DSA-SHA2 / SPHINCS+-`<L>`f-{simple,robust} |
| `SHA2p<L>ssimple` | SLH-DSA-SHA2 / SPHINCS+-`<L>`s-simple |

where `<L>` is `128`, `192`, or `256`. Example: `make test-SPHINCSp128fsimple`.

## `.rsp` fields specific to DS

All DS `.rsp` files carry the standard `count`/`seed`/`mlen`/`msg`/`pk`/`sk`/
`smlen`/`sm` fields (see [`../../kat/README.md`](../../kat/README.md) for the full
format). Some families store extra randomness so a test run can replay
operations without depending on a live RNG:

| Algorithm | Extra fields |
|-----------|--------------|
| FALCON | `keypair_rnd` (48 B), `nonce` (40 B), `sign_rnd` (48 B) |
| ML-DSA | `keypair_seed` (SEEDBYTES), `sign_rnd` (RNDBYTES) |
| SLH-DSA (all 4 sub-families, "f" and "s") | `keypair_rnd` (3×SPX_N bytes), `sig_rnd` (SPX_N bytes) |

## Algorithm-specific notes

### FALCON

Non-standard API: `crypto_sign_keypair(pk, sk, keypair_rnd)` and
`crypto_sign(sm, smlen, m, mlen, sk, nonce, sign_rnd)`. All randomness is
stored explicitly in the `.rsp` file — no RNG needed during testing.

### ML-DSA

Explicit-seed API: `crypto_sign_keypair(pk, sk, seed)` and
`crypto_sign(sm, smlen, m, mlen, NULL, 0, sk, rnd)`. `src/randombytes.c` (reads
`/dev/urandom`) is excluded from compilation; only the derand entry points are
used.

### SLH-DSA (all 4 sub-families)

Explicit randomness: `crypto_sign_keypair(pk, sk, keypair_rnd)` with
`3×SPX_N` bytes, and `crypto_sign(sm, smlen, m, mlen, sk, sig_rnd)` with
`SPX_N` bytes. `SPX_N` differs per variant (16/24/32), defined in each
variant's `params.h`. `randombytes.c` is excluded as with ML-DSA.

### SLH-DSA "s" variants (SLH-DSA and SLH-DSA-SHA2 only)

The "f" (fast) variants above come from the standard per-variant SPHINCS+
reference source. The "s" (small) variants — which that source doesn't
implement — come from a *different*, unified/runtime-parametrized SLH-DSA
codebase (Saarinen's FIPS 205 reference). That codebase only implements
"simple" mode (not "robust", which FIPS 205 final dropped) and only
SHA2/SHAKE hashing — which is why SLH-DSA-Haraka and SLH-DSA-Haraka-AES
don't get "s" variants. A small shim (`src/sign.c` in each `*-s-simple` variant
directory) adapts its `slh_keygen`/`slh_sign`/`slh_verify` (RNG-callback API)
to the same `crypto_sign_keypair`/`crypto_sign`/`crypto_sign_open` convention
the "f" variants use, so it slots into the *same* generator/runner
(`PQCgenKAT_sign_slhdsa.c`/`main_slhdsa.c`) unchanged.

## Adding a new DS variant

1. Copy the algorithm sources into `original/DS/<FAMILY>/<VARIANT>/` (`src/`
   + `include/`).
2. Open `../../Makefile`, locate the `<FAMILY>_VARIANTS` list, and add the
   variant name.
3. Run `make gen-<VARIANT>` then `make test-<VARIANT>` to verify.

To add a new DS family entirely, see "Adding a new algorithm family" in
[`../../README.md`](../../README.md). To add a non-standardized candidate
scheme or a stateful hash-based signature, see
[`../Additional-DS/README.md`](../Additional-DS/README.md) or
[`../Stateful-DS/README.md`](../Stateful-DS/README.md) instead.
