# `algorithms/` — reference implementations by standardization status

Every family compiled by the Makefile, grouped by what its standard status actually is. Status was checked on NIST's CSRC pages on
2026-10-20 unless a cell says otherwise; "not verified" means no NIST page was available to confirm it.

```
algorithms/
├── kem/                    ML-KEM (FIPS 203), HQC                           → kem/README.md
├── signatures/
│   ├── standard/           ML-DSA (FIPS 204), SLH-DSA (FIPS 205),
│   │                       Falcon (round 3, basis of FN-DSA), FN-DSA experimental → standard/README.md
│   ├── onramp/             NIST additional-signature candidates (+ round2-only/CROSS, LESS) → onramp/README.md
│   └── stateful/           XMSS, LMS, HSS (NIST SP 800-208)                 → stateful/README.md
├── symmetric/              ASCON (not PQC)                                  → symmetric/README.md
└── legacy/                 pre-standard code kept for comparison            → legacy/README.md
```

## Three groups, one rule

| Group | Meaning | Families | In `test-all`? |
|---|---|---|---|
| **Final FIPS** | published as a final Federal Information Processing Standard | ML-KEM (FIPS 203), ML-DSA (FIPS 204), SLH-DSA (FIPS 205) | yes |
| **Selected, standard pending** | NIST selected the scheme and the standard is not final; the code is pre-standard and its KATs may not match the final standard | Falcon (→ FN-DSA, FIPS 206), HQC | yes (they were in the suite before; labelled as pending) |
| **On-ramp candidates** | submissions to the additional digital signature process, not standards | the families in `signatures/onramp/` | no — `test-onramp-ds` |

Also outside `test-all`: the stateful hash-based signatures (a separate NIST standard, SP 800-208), ASCON, the experimental FN-DSA
(`FNDSA-EXP`) and everything under `legacy/`.

## Provenance of every family

"Pin" is the exact upstream tag or commit recorded in the repository (see the family's `UPSTREAM.md`); "none" means none was recorded
when the code was added. Licenses are as recorded in the tree (`LICENSE` files, SPDX tags); where neither exists it says so.

| Family | Folder | Spec and status | Variants | Upstream | Pin | License | In `test-all` |
|---|---|---|---|---|---|---|---|
| ML-KEM | `kem/ML-KEM` | FIPS 203, final (Aug 2024) | 3 | [mlkem-native](https://github.com/pq-code-package/mlkem-native) | tag `v2.0.0`, `d1b2fe78…` | Apache-2.0 OR ISC OR MIT | yes |
| HQC | `kem/HQC` | selected March 2025 as backup KEM; standard pending (NIST: "ongoing standardization"; no draft number confirmed) | 3 | [pqc-hqc/hqc](https://gitlab.com/pqc-hqc/hqc) | tag `v5.0.0`, `f46e542` | public domain | yes |
| ML-DSA | `signatures/standard/ML-DSA` | FIPS 204, final (Aug 2024) | 3 | [mldsa-native](https://github.com/pq-code-package/mldsa-native) | tag `v2.0.0`, `834a90d5…` | Apache-2.0 OR ISC OR MIT | yes |
| SLH-DSA | `signatures/standard/SLH-DSA` | FIPS 205, final (Aug 2024) | 12 | [slhdsa-c](https://github.com/pq-code-package/slhdsa-c) | commit `174c02e4…` (no tags) | Apache-2.0 OR ISC OR MIT | yes |
| Falcon | `signatures/standard/FALCON` | round-3 submission; FN-DSA (FIPS 206) not final (NIST: "ongoing standardization"); Falcon and FN-DSA are not wire-compatible | 2 | [falcon-round3.zip](https://falcon-sign.info/falcon-round3.zip) | package SHA-256 `d625407d…` | MIT | yes |
| FN-DSA (experimental) | `signatures/standard/FNDSA-EXP` | best-guess pre-standard implementation, **not** FIPS 206 | 2 | [c-fn-dsa](https://github.com/pornin/c-fn-dsa) | commit `1b858880…` | public domain | no — `test-fndsa-exp` |
| On-ramp candidates | `signatures/onramp` | NIST additional signatures: 9 candidates in round 3, CROSS and LESS round 2 only | 232 in `test-onramp-ds` (+ FAEST-decoupled 12) | see [`signatures/onramp/README.md`](signatures/onramp/README.md) | per family | per family | no |
| XMSS | `signatures/stateful/XMSS` | NIST SP 800-208, final (Oct 2020) | 1 default (+76 via `-check`) | [xmss-reference](https://github.com/XMSS/xmss-reference) | closest match: `master` `171ccbd2…` (not a verified import point) | CC0 1.0 | no — `test-stateful-ds` |
| LMS | `signatures/stateful/LMS` | NIST SP 800-208 | 1 default (+19) | Cisco [hash-sigs](https://github.com/cisco/hash-sigs) | closest match: `master` `cea4dda4…` (not a verified import point) | Cisco BSD-style (3-clause) | no |
| HSS | `signatures/stateful/HSS` | NIST SP 800-208 | 1 default (+39) | Cisco hash-sigs (same library as LMS) | same as LMS | Cisco BSD-style (3-clause) | no |
| ASCON | `symmetric/ASCON` | the code is **Ascon v1.2** (competition submission, `*v12`); NIST SP 800-232 (final, Aug 2025) defines different functions (Ascon-AEAD128, Ascon-Hash256, Ascon-XOF128, Ascon-CXOF128), so these KATs are not claimed to match SP 800-232 | 7 | [ascon-c](https://github.com/ascon/ascon-c) | closest match: tag `v1.2.5` (22 of 25 files identical) | CC0 1.0 | no — `test-ascon` |
| Kyber (round 3) | `legacy/Kyber-round3` | pre-standard, replaced by ML-KEM | 3 | [kyber](https://github.com/pq-crystals/kyber) | closest match: `main` `3edd5af5…`; tag `pre-fips-migration` here | CC0 / Apache-2.0 | no |
| Dilithium (round 3) | `legacy/Dilithium-round3` | pre-standard, replaced by ML-DSA | 3 | [dilithium](https://github.com/pq-crystals/dilithium) | closest match: `master` `d35ba3fe…`; tag `pre-fips-migration` here | CC0 / Apache-2.0 / GPL-2.0 | no |
| SPHINCS+ (round 3) | `legacy/SPHINCSplus-round3-*` | pre-standard, replaced by SLH-DSA | 9 + 9 | [sphincsplus](https://github.com/sphincs/sphincsplus) | closest match: `master` `7ec789ac…`; tag `pre-fips-migration` here | per `LICENSE`; the `*-s-simple` files (Saarinen's early FIPS 205 code) are **unconfirmed** | no |
| SPHINCS+ Haraka, Haraka-AES | `legacy/SLH-DSA-Haraka*` | not part of FIPS 205 | 6 + 6 | sphincsplus (same repository) | closest match: `master` `7ec789ac…` | per `LICENSE` | no |

Per-family details (parameter sets, sizes, local patches, `make` targets, `.rsp` fields, limitations) are in the category READMEs.
Shared framework documentation: [`../harness/README.md`](../harness/README.md), [`../docs/kat-format.md`](../docs/kat-format.md),
[`../docs/static-tests.md`](../docs/static-tests.md), [`../docs/adding-an-algorithm.md`](../docs/adding-an-algorithm.md).
