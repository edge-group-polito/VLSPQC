# Key encapsulation mechanisms (KEM)

Scope: the KEM families covered by the KAT suite, both part of `make test-all`:

| Family | Standard status (NIST, checked 2026-10-20) | Variants |
|---|---|---|
| [ML-KEM](ML-KEM/UPSTREAM.md) | **FIPS 203, final** (released August 2024) | 3 (ML-KEM-512, -768, -1024) |
| [HQC](HQC/UPSTREAM.md) | selected in March 2025 as a backup to ML-KEM; NIST lists it as "selected for ongoing standardization". A draft standard was expected in 2026 (final 2027); no draft number could be confirmed from NIST's pages, so the code is labelled **"HQC (NIST-selected, standard pending — implements spec 2025-08-22)"**, never as a FIPS | 3 (HQC-1, -3, -5) |

The sources are compiled together with `harness/gen/PQCgenKAT_kem_*.c` (generation) or `harness/gen/main_*.c` (testing); see
[`../../docs/kat-format.md`](../../docs/kat-format.md) for the two-phase model and the `.rsp` format, [`../../harness/README.md`](../../harness/README.md)
for the harness, and the [root README](../../README.md) for the `make` targets that drive it. Provenance of every family:
[`../README.md`](../README.md).

## Sizes (generated)

Taken from the `CRYPTO_*BYTES` constants of each variant's compiled `api.h` (`tools/gen_size_table.py`); the third column is the
ciphertext.

<!-- BEGIN sizes:kem -->
| Variant | pk (B) | sk (B) | ct / sig (B) |
|---|---:|---:|---:|
| ml-kem-512 | 800 | 1632 | 768 |
| ml-kem-768 | 1184 | 2400 | 1088 |
| ml-kem-1024 | 1568 | 3168 | 1568 |
| HQC2025_1 | 2241 | 2321 | 4433 |
| HQC2025_3 | 4514 | 4602 | 8978 |
| HQC2025_5 | 7237 | 7333 | 14421 |
<!-- END sizes:kem -->

## ML-KEM (FIPS 203)

- **Security assumption:** Module-LWE. **Parameter sets:** ML-KEM-512 / 768 / 1024 (NIST security categories 1 / 3 / 5); shared
  secret 32 B for all three.
- **Upstream:** [mlkem-native](https://github.com/pq-code-package/mlkem-native), tag `v2.0.0` (commit
  `d1b2fe782888bdb761a50336012923180be7f502`), imported 2026-10-20. License: Apache-2.0 OR ISC OR MIT ([`ML-KEM/LICENSE`](ML-KEM/LICENSE)).
- **Layout:** each `ml-kem-<n>/` holds a copy of upstream's `mlkem/` single-compilation-unit tree (`mlkem/mlkem_native.c` includes
  `src/*.c`) and `include/{kem.h,api.h,params.h}`, a thin compatibility layer that maps the harness names (`crypto_kem_*_derand`,
  `KYBER_*`) onto the `PQCP_MLKEM_NATIVE_MLKEM<n>_*` API.
- **Local patches:** two lines in `mlkem/mlkem_native_config.h` (parameter set; `MLK_CONFIG_NO_RANDOMIZED_API`, so no `randombytes()`
  provider is needed). Native backends (`src/native/`, `src/fips202/native/`) and assembly were removed; only portable C remains.
- **Verified against:** upstream's `META.yml` KAT hashes (built with its own `gen_KAT`), and every record of the harness `.rsp`
  replayed through the pristine upstream build (100/100 per variant). Details in [`ML-KEM/UPSTREAM.md`](ML-KEM/UPSTREAM.md).
- **Pre-FIPS code:** the Kyber round-3 reference that used to be labelled "ML-KEM" is kept in
  [`../legacy/Kyber-round3`](../legacy/README.md); its KATs are not FIPS 203 KATs.

## HQC

- **Security assumption:** Quasi-Cyclic Syndrome Decoding (code-based). **Parameter sets:** HQC-1 / 3 / 5 (NIST categories 1 / 3 / 5).
- **Upstream:** official reference [gitlab.com/pqc-hqc/hqc](https://gitlab.com/pqc-hqc/hqc), tag `v5.0.0` (commit `f46e542`, 2025-08-22),
  `ref` implementation only. License: public domain dedication ([`HQC/LICENSE`](HQC/LICENSE)). The unreleased `next-release` branch (new
  parameters, XOF domain separator, key format) is **not** imported.
- **Local patches:** portability and debug cleanups only (`<immintrin.h>` includes removed, `BITMASK` made 32-bit safe, debug prints
  removed); see [`HQC/UPSTREAM.md`](HQC/UPSTREAM.md).
- **Verified against:** the seeds of the official `kats/ref/*.rsp` replayed through these sources (300/300 records equal). The harness'
  own `.rsp` files use a different seed derivation and are therefore not byte-identical to the official ones.
- **Expect change:** when the final standard is published, seed, salt and shared-secret sizes may change and these KATs will not match.

## `make` targets

Prefix with `test-` or `gen-`: `mlkem` (all three ML-KEM variants), `hqc` (all three HQC variants); aggregates `test-kem` / `gen-kem`
(also part of `test-all` / `gen-all`). Per-variant targets use the directory name for ML-KEM and a sanitized name for HQC:

| Target name | Algorithm |
|---|---|
| `ml-kem-512`, `ml-kem-768`, `ml-kem-1024` | ML-KEM-512 / 768 / 1024 |
| `HQC2025_1`, `HQC2025_3`, `HQC2025_5` | HQC-1 / 3 / 5 |

Example: `make test-ml-kem-512`, `make test-HQC2025_1`, `make static-test-mlkem`.

## `.rsp` fields

All KEM `.rsp` files carry `count`, `seed`, `pk`, `sk`, `ct`, `ss` (see [`../../docs/kat-format.md`](../../docs/kat-format.md)), plus:

| Algorithm | Extra fields |
|---|---|
| ML-KEM | `keypair_coins` (64 B), `enc_coins` (32 B) |
| HQC | none: `seed` is the only randomness field |

## Algorithm-specific notes

**ML-KEM** uses the deterministic API: `crypto_kem_keypair_derand(pk, sk, coins)` with 64 bytes and `crypto_kem_enc_derand(ct, ss, pk, coins)`
with 32 bytes, so the explicit coins in the `.rsp` reproduce a run without a live RNG.

**HQC** has no explicit-randomness entry point. The harness seeds HQC's internal SHAKE-256 PRNG with `prng_init(seed, NULL, 48, 0)` and then calls the
ordinary `crypto_kem_keypair` / `crypto_kem_enc`. **`RUN_KEYGEN=0` desync caveat:** skipping keygen does not advance the PRNG as generation did, so encaps
starts at the wrong PRNG offset and reports a mismatch although the algorithm is correct; see "The RUN_KEYGEN=0 desync caveat" in
[`../../docs/static-tests.md`](../../docs/static-tests.md#the-run_keygen0-desync-caveat-hqc-2025-only).

## Known limitations

- The ML-KEM trees contain no architecture-specific code, so no accelerated path is available or measured.
- HQC status is taken from NIST's selection announcement and the upstream repository, not from a published draft FIPS.
- HQC tests cannot skip key generation (`RUN_KEYGEN=0` gives false mismatches, see above), in the dynamic as well as the static flow.

## Adding a new KEM variant or family

See [`../../docs/adding-an-algorithm.md`](../../docs/adding-an-algorithm.md).
