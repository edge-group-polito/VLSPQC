# Digital signatures — standard and standard-pending

Scope: the signature families of the "standardized" suite. They are compiled with `harness/gen/PQCgenKAT_sign_*.c` (generation) or
`harness/gen/main_*.c` (testing); see [`../../../docs/kat-format.md`](../../../docs/kat-format.md), [`../../../harness/README.md`](../../../harness/README.md)
and the [root README](../../../README.md) for the `make` targets. Provenance of every family: [`../../README.md`](../../README.md).

| Family | Status (NIST, checked 2026-10-20) | Variants | In `test-all` |
|---|---|---|---|
| [ML-DSA](ML-DSA/UPSTREAM.md) | **FIPS 204, final** (August 2024) | 3 (ML-DSA-44, -65, -87) | yes |
| [SLH-DSA](SLH-DSA/UPSTREAM.md) | **FIPS 205, final** (August 2024) | 12 (SHA2 / SHAKE × 128 / 192 / 256 × s / f) | yes |
| [Falcon](FALCON/UPSTREAM.md) | round-3 submission, **basis of the upcoming FN-DSA (FIPS 206), which is not a final standard** (NIST: "selected for ongoing standardization"; no FIPS 206 draft was found) — Falcon and FN-DSA are not wire-compatible | 2 (falcon-512, -1024) | yes, labelled as pending |
| [FN-DSA experimental](FNDSA-EXP/UPSTREAM.md) | **pre-standard, experimental**: Thomas Pornin's best-guess implementation of the FN-DSA draft, not FIPS 206 | 2 (fndsa-exp-512, -1024) | **no** — `make test-fndsa-exp` |

Related categories that are *not* part of `test-all`: the on-ramp candidates ([`../onramp/`](../onramp/README.md)), the stateful hash-based
signatures ([`../stateful/`](../stateful/README.md)) and the pre-standard code in [`../../legacy/`](../../legacy/README.md).

## Sizes (generated)

From the `CRYPTO_*BYTES` constants of each variant's compiled `api.h` (`tools/gen_size_table.py`). The last column is `CRYPTO_BYTES`, i.e. the
signature size reported by the API: the fixed size for ML-DSA, SLH-DSA and FN-DSA, and the **maximum** for Falcon (the actual compressed
signatures are shorter: 656 B for the Falcon-512 signature measured in the benchmark run).

<!-- BEGIN sizes:standard -->
| Variant | pk (B) | sk (B) | ct / sig (B) |
|---|---:|---:|---:|
| ML-DSA-44 | 1312 | 2560 | 2420 |
| ML-DSA-65 | 1952 | 4032 | 3309 |
| ML-DSA-87 | 2592 | 4896 | 4627 |
| falcon-512 | 897 | 1281 | 690 |
| falcon-1024 | 1793 | 2305 | 1330 |
| SLH-DSA-SHA2-128s | 32 | 64 | 7856 |
| SLH-DSA-SHA2-128f | 32 | 64 | 17088 |
| SLH-DSA-SHA2-192s | 48 | 96 | 16224 |
| SLH-DSA-SHA2-192f | 48 | 96 | 35664 |
| SLH-DSA-SHA2-256s | 64 | 128 | 29792 |
| SLH-DSA-SHA2-256f | 64 | 128 | 49856 |
| SLH-DSA-SHAKE-128s | 32 | 64 | 7856 |
| SLH-DSA-SHAKE-128f | 32 | 64 | 17088 |
| SLH-DSA-SHAKE-192s | 48 | 96 | 16224 |
| SLH-DSA-SHAKE-192f | 48 | 96 | 35664 |
| SLH-DSA-SHAKE-256s | 64 | 128 | 29792 |
| SLH-DSA-SHAKE-256f | 64 | 128 | 49856 |
| fndsa-exp-512 | 897 | 1345 | 666 |
| fndsa-exp-1024 | 1793 | 2369 | 1280 |
<!-- END sizes:standard -->

## ML-DSA (FIPS 204)

- **Security assumption:** Module-LWE / Module-SIS (Fiat–Shamir with aborts). **Parameter sets:** ML-DSA-44 / 65 / 87 (NIST categories 2 / 3 / 5).
- **Upstream:** [mldsa-native](https://github.com/pq-code-package/mldsa-native), tag `v2.0.0` (commit `834a90d5e846ffa1e1611bd24e160bb2e9b86d35`), imported
  2026-10-20; Apache-2.0 OR ISC OR MIT ([`ML-DSA/LICENSE`](ML-DSA/LICENSE)). Each `ML-DSA-<n>/` holds upstream's `mldsa/` tree plus `include/{api.h,params.h,sign.h}`,
  a compatibility layer for the explicit-seed API.
- **Local patches:** two config lines (parameter set, `MLD_CONFIG_NO_RANDOMIZED_API`); native backends and assembly removed. The KATs are **pure ML-DSA with an
  empty context string**; HashML-DSA is not provided. Old names `ML-DSA-2/3/5` remain as deprecated `make` aliases.
- **Verified against:** upstream's `META.yml` KAT hashes and the harness `.rsp` replayed through the pristine build (100/100 per variant) — see [`ML-DSA/UPSTREAM.md`](ML-DSA/UPSTREAM.md).

## SLH-DSA (FIPS 205)

- **Security assumption:** hash-function security (stateless hash-based, SPHINCS+). **Parameter sets:** the 12 FIPS 205 sets `SLH-DSA-{SHA2,SHAKE}-{128,192,256}{s,f}`
  (s = small signatures, f = fast signing). Haraka and `robust` modes are **not** in FIPS 205 and are no longer here (Haraka is in [`../../legacy/`](../../legacy/README.md)).
- **Upstream:** [slhdsa-c](https://github.com/pq-code-package/slhdsa-c) commit `174c02e42257f95c210963272877c49dbb50070f` (no release tags), imported 2026-10-20;
  Apache-2.0 OR ISC OR MIT ([`SLH-DSA/LICENSE`](SLH-DSA/LICENSE)). One shared source tree `slhdsa-c/` (the parameter set is selected at run time) plus a per-variant `include/` compatibility layer.
  The sources are byte-identical to the `slhdsa-c` copy used by the SLIDER SLH-DSA co-design project.
- **Local patches:** none to upstream files. KATs use **pure SLH-DSA with an empty context string**; HashSLH-DSA is compiled but not exercised.
- **Verified against:** NIST ACVP-Server vectors (keyGen and deterministic external/pure sigGen) for all 12 sets, 0 mismatches, plus the harness `.rsp` replayed through the pristine API.
  Details in [`SLH-DSA/UPSTREAM.md`](SLH-DSA/UPSTREAM.md).
- **Cost note:** the `s` sets sign slowly on the reference code (hundreds of milliseconds to seconds per signature on the benchmark host), so `make test-slhdsa` with the default 100 vectors takes minutes.

## Falcon (round 3, basis of FN-DSA)

- **Security assumption:** NTRU lattices, GPV hash-and-sign (floating-point Gaussian sampling). **Parameter sets:** Falcon-512 / 1024 (categories 1 / 5).
- **Upstream:** the round-3 submission package ([falcon-round3.zip](https://falcon-sign.info/falcon-round3.zip), SHA-256 `d625407dbda9e5835f610aaeba1147e029988a6610e0107dfd292033138e1d47`),
  `falcon512int` / `falcon1024int` reference code; MIT. **Not FN-DSA**: FIPS 206 was not final when this was written and the two are not compatible.
- **Local patches:** `nist.c` takes the keygen seed, nonce and signing seed explicitly (the harness' explicit-randomness API); debug leftovers in `shake.c` / `vrfy.c`. See [`FALCON/UPSTREAM.md`](FALCON/UPSTREAM.md).
- **Verified against:** the package's **official KAT files** (100 records each for Falcon-512 and Falcon-1024), kept in [`FALCON/official-kat/`](FALCON/official-kat/README.md) and replayed byte for byte by `make test-falcon-512-official` / `make test-falcon-1024-official` (part of `make test-falcon` and `test-all`): 100/100 records, 0 mismatches.
- **Limitations:** this is the floating-point *reference* code (not an optimized or integer-only build).

## FN-DSA experimental (c-fn-dsa)

- **Status:** experimental and pre-standard; upstream says it is a best guess of the FIPS 206 draft and that backward compatibility is not guaranteed. **Not in `test-all`.**
- **Upstream:** [c-fn-dsa](https://github.com/pornin/c-fn-dsa), commit `1b858880dcac091b5e5f84f0e4d8617c35ba1392` (2026-09-21); public domain ([`FNDSA-EXP/LICENSE`](FNDSA-EXP/LICENSE)).
- **Integer-only build:** yes — with `-DFNDSA_AVX2=0 -DFNDSA_SSE2=0 -DFNDSA_NEON=0 -DFNDSA_RV64D=0` the floating-point values are emulated with integer operations (`sign_fpr.c`),
  so no FPU is needed. Checked on x86-64 (no floating-point arithmetic instruction in the compiled objects; upstream's own test suite passes). Details and the `-D_GNU_SOURCE` note in [`FNDSA-EXP/UPSTREAM.md`](FNDSA-EXP/UPSTREAM.md).
- **KATs:** the seed-to-key mapping of the harness is a local choice, so these are not official FN-DSA vectors.

## `make` targets

Prefix with `test-` or `gen-`: `mldsa`, `slhdsa` (all 12) with the subsets `slhdsa-sha2` and `slhdsa-shake`, `falcon` (the harness `.rsp` tests plus the two `*-official` replays of the official KAT), and `fndsa-exp`; the aggregates `test-ds` / `gen-ds` (= `mldsa`, `slhdsa`,
`falcon`, part of `test-all` / `gen-all`). Per-variant targets use the variant name: `make test-ML-DSA-44`, `make test-SLH-DSA-SHA2-128s`, `make test-falcon-512`, `make test-fndsa-exp-512`.
Static tests: `make static-test-<variant>` or `static-test-ds`.

## `.rsp` fields

All signature `.rsp` files carry `count`, `seed`, `mlen`, `msg`, `pk`, `sk`, `smlen`, `sm` (see [`../../../docs/kat-format.md`](../../../docs/kat-format.md)), plus the explicit randomness:

| Family | Extra fields |
|---|---|
| ML-DSA | `keypair_seed` (32 B), `sign_rnd` (32 B) |
| SLH-DSA | `keypair_rnd` (3·n B), `sig_rnd` (n B) |
| Falcon, FN-DSA experimental | `keypair_rnd` (48 B), `nonce` (40 B), `sign_rnd` (48 B; unused by FN-DSA) |

## Known limitations

- ML-DSA and SLH-DSA KATs cover the pure interface with an empty context; the pre-hash variants (HashML-DSA, HashSLH-DSA) are not tested.
- Reference code only: no accelerated backends.
- Falcon and FN-DSA experimental follow standards that are still moving; their KATs are not expected to match the final FN-DSA.

## Adding a variant or family

See [`../../../docs/adding-an-algorithm.md`](../../../docs/adding-an-algorithm.md).
