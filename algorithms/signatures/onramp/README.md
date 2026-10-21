# On-ramp — NIST additional digital signature candidates

Reference-implementation sources for the signature schemes of NIST's
[Additional Digital Signature Schemes](https://csrc.nist.gov/projects/pqc-dig-sig) ("on-ramp") process. None of them is a
standard: they are candidates, and they live apart from the standardized families
([`../standard/`](../standard/README.md)) so that `make test-all` / `gen-all` / `static-test-all` never pull them in.
Use this category's own aggregates (`test-onramp-ds`, `gen-onramp-ds`, `static-test-onramp-ds`; the old `*-additional-ds`
names still work and print a deprecation note) or the per-family and per-variant targets below.

Each family folder holds one subdirectory per parameter-set variant (most share a `common/` directory) and exposes a
compile-time-fixed `crypto_sign_keypair` / `crypto_sign` / `crypto_sign_open` API. The sources are compiled together with
`harness/gen/PQCgenKAT_sign_*.c` (generation) or `harness/gen/main_*.c` (testing); see
[`../../../docs/kat-format.md`](../../../docs/kat-format.md) and [`../../../harness/README.md`](../../../harness/README.md), and the
[root README](../../../README.md) for the `make` targets.

## NIST status of the schemes

Checked on NIST's CSRC pages on 2026-10-20 ([round 3](https://csrc.nist.gov/projects/pqc-dig-sig/round-3-additional-signatures),
[round 2](https://csrc.nist.gov/projects/pqc-dig-sig/round-2-additional-signatures); the rationale is in NIST IR 8610). The round-3
page lists nine candidates (updated 2026-10-13); the round-3 announcement date is not stated on those pages (a secondary source gives
2026-05-14, unverified).

| Scheme | Type | Furthest NIST round | Folders here |
|---|---|---|---|
| FAEST | symmetric-based (VOLE-in-the-head) | **round 3** | `FAEST`, `FAEST-decoupled`, `FAEST-R3` |
| HAWK | lattice | round 3 candidate; the NIST page states that **the submission team has withdrawn HAWK** (no date given) | `HAWK` |
| MAYO | multivariate | **round 3** | `MAYO`, `MAYO-R3` |
| MQOM | MPC-in-the-head | **round 3** | `MQOM` |
| QR-UOV | multivariate | **round 3** | `QRUOV`, `QRUOV-R3` |
| SDitH | MPC-in-the-head | **round 3** | `SDITH`, `SDITH-R3` |
| SNOVA | multivariate | **round 3** | `SNOVA`, `SNOVA-R3` |
| SQIsign | isogeny | **round 3** | `SQISIGN`, `SQISIGN-R3` |
| UOV | multivariate | **round 3** | `UOV`, `UOV-R3` |
| CROSS | code-based | round 2 only (not among the nine round-3 candidates) | [`round2-only/CROSS`](round2-only/CROSS) |
| LESS | code-based | round 2 only (not among the nine round-3 candidates) | [`round2-only/LESS`](round2-only/LESS) |

The other round-2 candidates (Mirath, PERK, RYDE) are not in this repository. CROSS and LESS are out of the competition and kept in
the `round2-only/` subfolder; their `make` targets are unchanged.

## Families

Two generations of code coexist: the folders without a suffix are the older (round-2 era) ports, `*-R3` are round-3 ports of the
same schemes (parameters and sizes differ, e.g. MAYO_1 pk 1420 → 1456 B; they are separate families by design), and `MQOM` is round 3
only.

| Folder | Variants | Security levels | Notes |
|--------|----------|------------------|-------|
| `round2-only/CROSS` | [CROSS-implementation](https://github.com/CROSS-signature/CROSS-implementation), version 2.0 (Feb 2025) | closest match: tag `v2.0` (`a412eacb…`, 17 of 83 files identical); not a verified import point | CC0 1.0 (`LICENSE`); headers say public domain |
| `HAWK` | [hawk-sign/dev](https://github.com/hawk-sign/dev) | closest match: `main` `1b9fef52…` (19 of 31 files identical; no tags) | MIT (`LICENSE`) |
| `round2-only/LESS` | [LESS](https://github.com/less-sig/LESS) | closest match: tag `v2.0.0` (`c5759390…`, 26 of 47 files identical) | headers say public domain; upstream added a CC0 1.0 `LICENSE` only in July 2026 (none at `v2.0.0`) |
| `FAEST` | 12 (plain/EM × 128/192/256 × f/s) | 128, 192, 256 bit | older v2-sized port (does not match the official v3 KAT); own NIST DRBG; ~30 files shared across all 12 variants live once in `FAEST/common/` |
| `FAEST-decoupled` | 12 | 128, 192, 256 bit | same variants as `FAEST`, but signing is decoupled from keygen and the DRBG (`crypto_sign_with_randomness`); targets are suffixed `-decoupled` (`make test-faest-decoupled`); not part of `test-onramp-ds` |
| `UOV` | 12 (Ip/Is/III/V × classic/pkc/pkc_skc) | 128, 192, 256 bit | own NIST DRBG; all `.c`/`.h` shared across all 12 variants live once in `UOV/common/`, each variant dir holds only its own `include/params.h` |
| `MAYO` | 4 (MAYO_1/2/3/5) | 128, 192, 256 bit | own NIST DRBG; already OpenSSL-free upstream; shared code in `MAYO/common/`, per-variant `src/api.c` + `include/{api.h,mayo_variant.h}` |
| `QRUOV` | 24 (4 params/level × 3 levels × AES-CTR/SHAKE PRG) | 128, 192, 256 bit | own NIST DRBG; own self-contained MGF/PRG (see "QRUOV" below); shared code in `QRUOV/common/`, generated per-variant `include/{qruov_config.h,api.h}` |
| `SNOVA` | 36 (6 params/level × 3 levels × AES-CTR/SHAKE PRG) | 128, 192, 256 bit | own NIST DRBG; already OpenSSL-free upstream; shared code in `SNOVA/common/`, per-variant `include/snova_params.h`; the 8 level-5 variants need a bumped stack (see "SNOVA" below) |
| `SDITH` | 12 (Hypercube/Threshold × 1/3/5 × gf256/p251) | 128, 192, 256 bit | own NIST DRBG (patched off OpenSSL, see "SDitH" below); full self-contained copy per variant, no shared `common/` |
| `SQISIGN` | 3 (lvl1/lvl3/lvl5) | 128, 192, 256 bit | own NIST DRBG; OpenSSL-free and (via mini-gmp) real-GMP-free upstream; shared code in `SQISIGN/common/` — see "SQIsign" below |
| `MQOM` | 18 (cat1/3/5 × gf2_shorter/gf16_fast/gf16_short × ct/ot) | 128, 192, 256 bit | round-3 package; own NIST DRBG (`common/src/rng.c`); shared code in `MQOM/common/`, per-variant `include/{mqom3_parameters_<v>.h,mqom3_variant.h}` |
| `MAYO-R3` | 4 (MAYO_1/2/3/5) | 128, 192, 256 bit | round-3 MAYO; `MAYO-R3/common/` + per-variant `src/api.c`, `include/{api.h,mayo_variant.h}` |
| `UOV-R3` | 12 (Ip/Is/III/V × classic/pkc/pkc_skc) | 128, 192, 256 bit | round-3 UOV (Ip/III/V parameters changed); `UOV-R3/common/` + per-variant `include/params.h`; no offline official KAT |
| `SNOVA-R3` | 18 (I/III/V × B/K/S × plain/AES) | 128, 192, 256 bit | round-3 SNOVA; `SNOVA-R3/common/` + per-variant `include/snova_params.h`; level-V variants need a bigger stack |
| `QRUOV-R3` | 30 (5 sets/level × 3 levels × aes/shake) | 128, 192, 256 bit | round-3 QR-UOV (rewrite of the round-2 code); `QRUOV-R3/common/` + per-variant `include/qruov_build_config.h` |
| `SDITH-R3` | 12 (cat1/3/5 × fast/short × -/cipherpow) | 128, 192, 256 bit | SDitH v3.0.0 (sources from the upstream tag, the package ships none); needs `-lm` |
| `SQISIGN-R3` | 3 (p324_3/p500_27/p664_17) | 128, 192, 256 bit | round-3 SQIsign (sources from upstream tag `nist-v3`, the package ships almost none); static folder needs `-I.` |
| `FAEST-R3` | 4 (FAEST_128F/128S/EM_128F/EM_128S) | 128 bit | FAEST v3 (round 3); `FAEST-R3/common/` + per-variant `src/`+`include/`; assembled by `tools/assemble_faest_r3.sh` |

**`test-onramp-ds` covers 232 variants** (the 131 variants of the round-2-era families CROSS, HAWK, LESS, FAEST, UOV, MAYO, QR-UOV, SNOVA,
SDitH and SQIsign, plus MQOM (18) and the seven `*-R3` families (83)). `FAEST-decoupled` has its own target.

## Sizes (generated)

Per-family ranges of the `CRYPTO_*BYTES` constants of the compiled `api.h` (from `tools/gen_size_table.py`; per-variant values are in
[`../../../results/sizes.csv`](../../../results/sizes.csv)). `CRYPTO_BYTES` is the signature size reported by the variant's API (for several
schemes the maximum, i.e. the overhead of the `sm = sig || m` format).

<!-- BEGIN sizes:onramp -->
| Family | Variants | pk (B) | sk (B) | sig (B, `CRYPTO_BYTES`) |
|---|---:|---|---|---|
| hawk | 3 (3 measured) | 450 – 2440 | 96 – 360 | 249 – 1221 |
| less | 7 (7 measured) | 13940 – 197315 | 32 – 64 | 1329 – 10712 |
| cross | 18 (18 measured) | 54 – 153 | 32 – 64 | 8960 – 74590 |
| faest | 12 (12 measured) | 32 – 64 | 32 – 64 | 3906 – 26548 |
| uov | 12 (12 measured) | 43576 – 2869440 | 32 – 2436704 | 96 – 260 |
| mayo | 4 (4 measured) | 1420 – 5554 | 24 – 40 | 186 – 964 |
| qruov | 24 (24 measured) | 12266 – 173676 | 32 – 64 | 157 – 807 |
| snova | 36 (36 measured) | 379 – 71890 | 48 | 146 – 759 |
| sdith | 12 (12 measured) | 132 – 244 | 432 – 838 | 8496 – 45676 |
| sqisign | 3 (3 measured) | 65 – 129 | 353 – 701 | 148 – 292 |
| mqom | 18 (18 measured) | 52 – 128 | 72 – 192 | 2492 – 13540 |
| mayo-r3 | 4 (4 measured) | 1456 – 5554 | 24 – 40 | 239 – 964 |
| uov-r3 | 12 (12 measured) | 46591 – 3232320 | 32 – 2801024 | 96 – 275 |
| snova-r3 | 18 (18 measured) | 376 – 2716 | 96 | 272 – 896 |
| qruov-r3 | 30 (30 measured) | 11041 – 173660 | 32 | 157 – 1039 |
| sdith-r3 | 12 (12 measured) | 70 – 132 | 147 – 275 | 3721 – 19144 |
| sqisign-r3 | 3 (3 measured) | 83 – 169 | 270 – 549 | 200 – 406 |
| faest-r3 | 4 (4 measured) | 32 | 32 | 3466 – 5170 |
<!-- END sizes:onramp -->

## Benchmark of the Round-3 families (level 1)

The Round-3 (`*-R3`) ports and MQOM **were already benchmarked** at NIST level 1 before the FIPS migration; these are the numbers to use for the FAEST comparison. They were **not
rerun** after the restructure (the migration only moved these families and updated path comments, it did not change their code).

- **What:** 39 variants (FAEST-R3 4, MAYO-R3 2, MQOM 6, QR-UOV-R3 10, SDitH-R3 4, SNOVA-R3 6, SQIsign-R3 1, UOV-R3 6), built from the flattened static-test folders with plain `gcc -O2`, identical flags for
  every variant, portable reference C (no SIMD, no AES-NI), one operation per process, one CPU pinned, 1000-byte message, median wall-clock time over up to 1000 calls within 3 s.
  Memory: peak heap (`malloc` wrapper), touched stack pages, resident-set growth, code size. Method and caveats: [`bench/README.md`](../../../bench/README.md) (git-ignored directory, kept locally).
- **When / where:** 2026-10-14, Intel Core Ultra 7 155H under WSL2, gcc 13.3, `make bench-l1`. The table is the **per-metric median of three runs**
  ([`../../../results/benchmark_onramp_l1.csv`](../../../results/benchmark_onramp_l1.csv); per-metric spread in [`benchmark_onramp_l1_spread.csv`](../../../results/benchmark_onramp_l1_spread.csv)).
- **Caveats:** the machine was not fully idle (mean 10–11 % user CPU from a user-owned editor process during the runs). The timing differs between runs by a median **31 %** (maximum 80 %), so
  differences between schemes below roughly 1.5× should not be claimed. The FAEST v3 code is the unoptimized reference (signing takes about 0.5–6 s). The UOV-R3 and QR-UOV-R3 ports have no offline official KAT. Keygen times of a few microseconds are near timer resolution.
  `heap` and `stack` columns are the larger of the three operations; a stack of 4 KiB means "at most one page".

<!-- BEGIN bench:onramp-l1 -->
| Scheme | Variant | pk (B) | sk (B) | sig (B) | keygen (ms) | sign (ms) | verify (ms) | heap, max of ops (KiB) | stack, max of ops (KiB) |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|
| FAEST | FAEST-128f | 32 | 32 | 5170 | 0.026 | 763.0 | 872.4 | 340.8 | 4.0 |
| FAEST | FAEST-128s | 32 | 32 | 4066 | 0.032 | 6305.6 | 5706.0 | 2708.0 | 4.0 |
| FAEST | FAEST-EM-128f | 32 | 32 | 4170 | 0.023 | 505.0 | 580.4 | 266.6 | 4.0 |
| FAEST | FAEST-EM-128s | 32 | 32 | 3466 | 0.029 | 4154.3 | 3943.2 | 2178.1 | 4.0 |
| MAYO | MAYO-1 | 1456 | 24 | 464 | 1.346 | 2.154 | 1.484 | 0.9 | 808.0 |
| MAYO | MAYO-2 | 2928 | 24 | 239 | 1.310 | 1.199 | 0.990 | 0.9 | 360.0 |
| MQOM | MQOM-gf2-shorter-ct | 52 | 72 | 2492 | 1.257 | 67.4 | 30.6 | 3091.8 | 8.0 |
| MQOM | MQOM-gf2-shorter-ot | 52 | 72 | 2492 | 1.208 | 111.6 | 50.1 | 6530.4 | 12.0 |
| MQOM | MQOM-gf16-fast-ct | 64 | 96 | 3316 | 0.486 | 6.293 | 4.397 | 406.6 | 8.0 |
| MQOM | MQOM-gf16-fast-ot | 64 | 96 | 3316 | 0.508 | 7.382 | 4.174 | 406.9 | 12.0 |
| MQOM | MQOM-gf16-short-ct | 64 | 96 | 2932 | 0.510 | 26.4 | 13.6 | 1672.5 | 8.0 |
| MQOM | MQOM-gf16-short-ot | 64 | 96 | 2932 | 0.526 | 32.5 | 16.1 | 1954.5 | 8.0 |
| QR-UOV | QR-UOV-1q7L10-aes | 20641 | 32 | 331 | 667.8 | 109.9 | 91.3 | 0.7 | 120.0 |
| QR-UOV | QR-UOV-1q7L10-shake | 20641 | 32 | 331 | 591.8 | 90.8 | 88.1 | 0.0 | 120.0 |
| QR-UOV | QR-UOV-1q31L3-aes | 23641 | 32 | 157 | 56.5 | 7.519 | 7.358 | 0.7 | 32.0 |
| QR-UOV | QR-UOV-1q31L3-shake | 23641 | 32 | 157 | 52.2 | 5.175 | 4.762 | 0.0 | 32.0 |
| QR-UOV | QR-UOV-1q31L10-aes | 12266 | 32 | 435 | 237.7 | 40.8 | 39.0 | 0.7 | 72.0 |
| QR-UOV | QR-UOV-1q31L10-shake | 12266 | 32 | 435 | 161.4 | 35.9 | 36.9 | 0.0 | 72.0 |
| QR-UOV | QR-UOV-1q127L3-aes | 24256 | 32 | 200 | 36.7 | 6.140 | 7.831 | 0.7 | 24.0 |
| QR-UOV | QR-UOV-1q127L3-shake | 24256 | 32 | 200 | 39.8 | 4.421 | 3.794 | 0.0 | 24.0 |
| QR-UOV | QR-UOV-1q127L10-aes | 11041 | 32 | 541 | 112.3 | 30.0 | 27.8 | 0.7 | 60.0 |
| QR-UOV | QR-UOV-1q127L10-shake | 11041 | 32 | 541 | 117.6 | 25.2 | 16.7 | 0.0 | 56.0 |
| SDitH | SDitH-fast | 70 | 147 | 4914 | 0.032 | 31.6 | 31.8 | 334.8 | 4.0 |
| SDitH | SDitH-fast-cipherpow | 70 | 147 | 4643 | 0.036 | 34.0 | 27.9 | 330.1 | 4.0 |
| SDitH | SDitH-short | 70 | 147 | 3721 | 0.038 | 50.5 | 47.5 | 959.0 | 4.0 |
| SDitH | SDitH-short-cipherpow | 70 | 147 | 3721 | 0.035 | 46.8 | 45.2 | 959.4 | 4.0 |
| SNOVA | SNOVA-I-B | 656 | 96 | 388 | 1.300 | 5.942 | 2.349 | 1.0 | 364.0 |
| SNOVA | SNOVA-I-B-AES | 656 | 96 | 388 | 1.468 | 5.615 | 2.493 | 1.0 | 364.0 |
| SNOVA | SNOVA-I-K | 376 | 96 | 528 | 1.097 | 8.813 | 4.209 | 1.0 | 408.0 |
| SNOVA | SNOVA-I-K-AES | 376 | 96 | 528 | 1.470 | 8.549 | 4.236 | 1.0 | 408.0 |
| SNOVA | SNOVA-I-S | 1016 | 96 | 272 | 2.409 | 6.293 | 2.106 | 1.0 | 352.0 |
| SNOVA | SNOVA-I-S-AES | 1016 | 96 | 272 | 2.393 | 6.085 | 2.260 | 1.0 | 352.0 |
| SQIsign | SQIsign-p324-3 | 83 | 270 | 200 | 20.0 | 59.5 | 7.423 | 0.0 | 104.0 |
| UOV | UOV-Ip | 321300 | 278087 | 135 | 49.6 | 0.707 | 0.510 | 0.0 | 20.0 |
| UOV | UOV-Ip-pkc | 46591 | 278087 | 135 | 42.0 | 0.649 | 3.569 | 313.8 | 20.0 |
| UOV | UOV-Ip-pkc-skc | 46591 | 32 | 135 | 42.1 | 5.003 | 3.783 | 313.8 | 20.0 |
| UOV | UOV-Is | 412160 | 348704 | 96 | 26.1 | 0.275 | 0.033 | 0.0 | 4.0 |
| UOV | UOV-Is-pkc | 66576 | 348704 | 96 | 25.4 | 0.252 | 3.854 | 402.5 | 4.0 |
| UOV | UOV-Is-pkc-skc | 66576 | 32 | 96 | 25.3 | 4.509 | 3.823 | 402.5 | 8.0 |
<!-- END bench:onramp-l1 -->

To rerun: `make bench-l1` on an otherwise idle machine, then `python3 -I tools/collect_results.py --onramp-l1 <results dir>`.

## Provenance and licenses

What this repository records about each family's source. "Package" means a copy of the NIST submission package kept outside this
repository (`DS-add/`, not versioned here). Licenses are taken from the `LICENSE` file placed in the folder (copied from the upstream at the matched version), the license statements in the source headers, or SPDX identifiers. CROSS, LESS and HAWK were
looked up on 2026-10-21 (details in their `UPSTREAM.md`); for the other families no license file is in the tree and the SPDX identifiers are the only evidence.

| Folder | Source | Pin recorded here | License evidence in the tree |
|---|---|---|---|
| `round2-only/CROSS` | [CROSS-implementation](https://github.com/CROSS-signature/CROSS-implementation), version 2.0 (Feb 2025) | closest match: tag `v2.0` (`a412eacb…`, 17 of 83 files identical); not a verified import point | CC0 1.0 (`LICENSE`); headers say public domain |
| `round2-only/LESS` | [LESS](https://github.com/less-sig/LESS) | closest match: tag `v2.0.0` (`c5759390…`, 26 of 47 files identical) | headers say public domain; upstream added a CC0 1.0 `LICENSE` only in July 2026 (none at `v2.0.0`) |
| `HAWK` | [hawk-sign/dev](https://github.com/hawk-sign/dev) | closest match: `main` `1b9fef52…` (19 of 31 files identical; no tags) | MIT (`LICENSE`) |
| `FAEST`, `FAEST-decoupled` | [faest-ref](https://github.com/faest-sign/faest-ref), instantiated from its meson templates | none | MIT (SPDX) |
| `FAEST-R3` | official FAEST v3.0 NIST round-3 reference packages (`sw/original/faest-v3`), `tools/assemble_faest_r3.sh` | package | MIT (SPDX) |
| `MAYO` | [MAYO-C](https://github.com/PQCMayo/MAYO-C) | none | Apache-2.0 (SPDX) |
| `MAYO-R3` | MAYO round-3 package = MAYO-C commit `fef05b5` ("NIST Round 3") | commit `fef05b5` | Apache-2.0 (SPDX) |
| `MQOM` | MQOM3 round-3 submission package, `Reference_Implementation/`, `tools/assemble_mqom.sh` | package | MIT (`MQOM/common/LICENSE`, SPDX) |
| `QRUOV` | [qruov/round2](https://github.com/qruov/round2) | none | Apache-2.0 / MIT (SPDX) |
| `QRUOV-R3` | QR-UOV round-3 package | package | MIT and Apache-2.0 (SPDX) |
| `SDITH` | SDitH round-2 reference (Hypercube / Threshold), [sdith](https://github.com/sdith/sdith) | none | MIT (SPDX) |
| `SDITH-R3` | [sdith](https://github.com/sdith/sdith) tag `v3.0.0` | commit `ce7adf06476674d4ff990f378f83f9981cd6745b` | MIT (SPDX) |
| `SNOVA` | [SNOVA reference](https://github.com/SNOVA-project/SNOVA) | none | MIT / public domain (SPDX) |
| `SNOVA-R3` | SNOVA round-3 package, `Reference_Implementation` | package | MIT (`SNOVA-R3/common/LICENSE`, SPDX) |
| `SQISIGN` | [the-sqisign](https://github.com/SQIsign/the-sqisign), round-2 era | none | Apache-2.0 (SPDX) |
| `SQISIGN-R3` | [the-sqisign](https://github.com/SQIsign/the-sqisign) tag `nist-v3` | commit `6d017708db403bf83977fa70770fc4f7f9e9ff21` | Apache-2.0 (SPDX) |
| `UOV` | [pqov](https://github.com/pqov/pqov) | none | CC0 OR Apache-2.0 (SPDX) |
| `UOV-R3` | UOV round-3 package, `Reference_Implementation` | package | CC0 OR Apache-2.0 (SPDX) |

The pins marked "none" were not recorded when these ports were made and are not reconstructed here (CROSS, LESS and HAWK show the closest upstream state found, not a verified import point). The "Algorithm-specific notes"
below document the local patches of each port.

## Per-family `make` targets

Prefix with `test-` or `gen-`: `cross`, `hawk`, `less`, `faest`, `uov`, `mayo`, `qruov`, `snova`, `sdith`, `sqisign`, `mqom`, and the round-3
aggregates `mayo-r3`, `uov-r3`, `snova-r3`, `qruov-r3`, `sdith-r3`, `sqisign-r3`, `faest-r3` (aggregate: `test-onramp-ds` /
`gen-onramp-ds`, **not** part of `test-all` / `gen-all`).

Per-variant targets are `test-<NAME>` / `gen-<NAME>` using each variant's directory name, e.g. `make test-CROSS_1_RSDP_b`,
`make test-hawk-256`, `make test-LESS_252_45`, `make test-FAEST_128F`, `make test-UOV_Ip`, `make test-MAYO_1`,
`make test-QRUOV_1_q127L3v156m54_a`, `make test-SNOVA_28_5_19_4`, `make test-sdith_hypercube_cat1_gf256`, `make test-sqisign_lvl1`;
round-3 variants carry a `-R3` suffix: `make test-MAYO_1-R3`, `make test-UOV_Ip-R3`, `make test-SNOVA_I_B-R3`,
`make test-QRUOV_1q127L3_shake-R3`, `make test-sdith_cat1_fast-R3`, `make test-sqisign_p324_3-R3`, `make test-FAEST_128F-R3`,
`make test-mqom3_cat1_gf16_fast_ct`. Static tests: `make static-test-<variant>` or `static-test-onramp-ds`.

## `.rsp` fields specific to the on-ramp families

All `.rsp` files carry the standard `count` / `seed` / `mlen` / `msg` / `pk` / `sk` / `smlen` / `sm` fields (see
[`../../../docs/kat-format.md`](../../../docs/kat-format.md)):

| Algorithm | Extra fields |
|-----------|--------------|
| CROSS, LESS | `seed` is the CSPRNG seed (48 B); no extra fields |
| HAWK, FAEST, UOV, MAYO, QR-UOV, SNOVA, SDitH, SQIsign, MQOM (and their `-R3` ports) | `seed` is the NIST DRBG seed (48 B) |
| FAEST-decoupled | also the captured signing randomness `rho` |

## Known limitations

- Record #0 equals the official round-3 KAT for MAYO-R3, SNOVA-R3 (level I), SDitH-R3 (category 1), SQIsign-R3 and MQOM, and the 100
  official FAEST v3 records equal for `FAEST-R3` (these checks were made when the ports were assembled; they were not repeated in the
  FIPS migration). UOV-R3 has no offline official KAT (validated against the package's own generator on records 0–1). The round-2-era
  ports (no suffix) are not claimed to match any current official KAT.
- After the repository restructure all 232 on-ramp variants were re-run (2026-10-21, details in [`../../../results/validation.md`](../../../results/validation.md)): `make KATNUM=10 test-onramp-ds`
  passes with 0 failures (10 vectors per variant, **not** the default 100, which the pre-migration run could not finish in 90 minutes), and the static tests pass for 228 of 232 variants with the
  default stack. The other four (older SNOVA level 5) segfaulted with the default 8 MB stack; `tools/run_all_static_kat.sh` now raises the stack to 64 MiB by default and they pass. There is still no recorded pre-migration baseline for most of these families
  (that run was stopped inside LESS), so "unchanged by the move" rests on the code not having been modified, not on a before/after comparison of the full set.
- The level-5 SNOVA variants and the `-R3` SNOVA level-V variants need a larger stack; the SQIsign and SDitH ports need `-I.` and
  `-lm` respectively in the static folders.
- HAWK is listed by NIST as withdrawn by its team; the code is kept for completeness.

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
`harness/gen/rng.c`. The generator uses HAWK's own RNG directly;
`harness/gen/rng.c` is not linked separately for HAWK. HAWK has no derand entry
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

**Layout differs from every other on-ramp family:** ~30 of FAEST's
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
[`algorithms/signatures/standard/FALCON/*/main.c`](../standard/FALCON):

```sh
cd algorithms/signatures/onramp/FAEST/FAEST_128F
gcc -O2 -std=c11 -I ../common/include -I include main.c src/*.c ../common/src/*.c -o test && ./test
```

Two portability fixes were applied uniformly across every `.c` file in both
`common/` and each variant dir (see
[`../../tools/assemble_faest.sh`](../../../tools/assemble_faest.sh), which
regenerates the whole tree — `common/` plus all 12 variants' 4 files each —
from a fresh faest-ref checkout; `main.c`+`test_vectors_*.h` are then
(re)placed by [`../../tools/build_faest_main.sh`](../../../tools/build_faest_main.sh)):

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
cd algorithms/signatures/onramp/UOV/UOV_Ip
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
  implementation `harness/gen/rng.c` and HAWK's `src/rng.c` already use — no
  longer tied to OpenSSL's `AES256_ECB`.

UOV has no derand entry point, so `crypto_sign_keypair()` is always called
(even with `RUN_KEYGEN=0`) purely to advance the DRBG — same shape and same
caveat as HAWK/FAEST. `common/src/rng.c` IS the NIST AES-256-CTR DRBG (same
as HAWK's own copy) — do not additionally link `harness/gen/rng.c`.

Classic-mode public/secret keys are large (up to ~2.87 MB / ~2.44 MB for
`UOV_V`): [`harness/gen/PQCgenKAT_sign_uov.c`](../../../harness/gen/PQCgenKAT_sign_uov.c)
and [`harness/gen/main_uov.c`](../../../harness/gen/main_uov.c) heap-allocate `pk`/`sk`
(and, in `main_uov.c`, `pk_rsp`/`sk_rsp`) instead of using stack arrays like
`main_hawk.c` does, to avoid a stack overflow when all of them are live at
once. `harness/kat_parser.h`'s `KAT_DATA_MAX` is sized at 4 MB (up from 256 KB)
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
cd algorithms/signatures/onramp/MAYO/MAYO_1
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
AES-256-CTR DRBG — do not additionally link `harness/gen/rng.c`.

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
cd algorithms/signatures/onramp/QRUOV/QRUOV_1_q127L3v156m54_a
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
  `void`/`int`-returning `harness/gen/rng.h` convention) since QRUOV's own
  `sign.c`/`api.c` already expect that exact signature.
- `refcount.h` (a small OpenSSL-internal-struct-refcounting helper only
  the old `mgf.c` needed) was dropped entirely — nothing includes it once
  `mgf.c` no longer needs it.

QRUOV has no derand entry point, so `crypto_sign_keypair()` is always
called (even with `RUN_KEYGEN=0`) purely to advance the DRBG — same shape
and same caveat as HAWK/FAEST/UOV/MAYO. `common/src/rng.c` IS the NIST
AES-256-CTR DRBG — do not additionally link `harness/gen/rng.c`.

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
cd algorithms/signatures/onramp/SNOVA/SNOVA_28_5_19_4
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
`harness/gen/rng.c`.

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
software AES-256 block cipher already used by `harness/gen/rng.c` and reused
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
Threshold's `rnd.h` that only bites inside `harness/gen/`.

```sh
cd algorithms/signatures/onramp/SDITH/sdith_hypercube_cat1_gf256
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
additionally link `harness/gen/rng.c`.

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
cd algorithms/signatures/onramp/SQISIGN/sqisign_lvl1
gcc -O2 -std=c11 -I ../common/include -I include main.c ../common/src/*.c src/*.c -o test -lm && ./test
```

SQIsign has no derand entry point, so `crypto_sign_keypair()` is always
called (even with `RUN_KEYGEN=0`) purely to advance the DRBG — same shape
and same caveat as HAWK/FAEST/UOV/MAYO/QRUOV/SNOVA/SDitH.
`common/src/randombytes_ctrdrbg.c` is the NIST AES-256-CTR DRBG — do not
additionally link `harness/gen/rng.c`. Needs `-lm` at link time — the only
family here that does (quaternion arithmetic uses libm).

### MQOM

Ported from the MQOM3 round-3 submission package (`Reference_Implementation/`), a multivariate-quadratic
MPC-in-the-Head signature. 18 variants `mqom3_cat{1,3,5}_{gf2_shorter,gf16_fast,gf16_short}_{ct,ot}`
(NIST levels 1/3/5; names verbatim from upstream, no `-R3` suffix: there is no older MQOM in the tree).

Layout: `MQOM/common/{src,include}` holds everything shared, FLATTENED (upstream's `blc/ piop/ ggm_tree/
rijndael/ fields/ fields_bitsliced/ sha3/` subdirectories become plain files; only the portable Rijndael units
and the XKCP `opt64` Keccak backend are kept). Each variant dir holds only `include/mqom3_parameters_<v>.h`
(the one file in which upstream's 18 dirs differ) and `include/mqom3_variant.h` (the four `MQOM3_PARAM_*`
defines upstream passes with `-D`). `tools/assemble_mqom.sh <complete Reference_Implementation>` regenerates the tree
(verified: re-running it reproduces the checked-in tree byte for byte).

Patches vs upstream (all mechanical):
1. `#include "sha3/X.h"` -> `"X.h"` in `xof.h`; `"../endian_compat.h"` -> `"endian_compat.h"` in `brg_endian.h`.
2. New `common/include/mqom3_build_config.h` (hardcodes upstream's Makefile `-D` defaults, portable backends only,
   every define `#ifndef`-guarded); included first by every `.c` and by `api.h`. No `-D` flag needed.
3. `piop_cache.h` / `seed_expand_cache.h`: `#include <common.h>` / `<fields.h>` -> quoted includes, so that
   `gcc *.c` in the flat static-test folder works without `-I.`.
4. `rng.c`/`rng.h`: upstream's OpenSSL-based generator DRBG is replaced by the software AES-256 CTR-DRBG of `harness/gen/`
   (shipped as `common/src/rng.c`; do not add `harness/gen/rng.c` to the build).
Dropped: benchmark/, generator/, tests, Makefiles, mupq/liboqs glue, AVX/NEON/ARM/AES-NI units, x8 Keccak.

KAT: `ds_drbg_nist` shape (official procedure: message drawn from the entropy-seeded DRBG before reseeding with
`seed`); record #0 of every variant equals the package's `KAT/<variant>/PQCsignKAT_*.rsp` byte for byte.

Provenance: in the copy of the package under `DS-add/` the subdirectories of `Reference_Implementation/<v>/`
(blc, piop, ggm_tree, sha3, rijndael, fields, fields_bitsliced, ...) contain only 0-byte files (so do those of
`Optimized_Implementation/`). The sources of those directories come from a complete copy of
`Reference_Implementation/` whose top-level files and `mqom3_parameters_*.h` are byte-identical to the DS-add ones
(41/41 top-level files of cat1_gf16_fast_ct; the 18 parameter headers). The original archive of that complete copy
was not found on this machine. Independent evidence that it is the
official code: all 18 record #0 equal the official KAT.

### MAYO-R3

Round-3 MAYO (NIST additional signatures), ported from the Round-3 package
`DS-add/MAYO/MAYO/Reference_Implementation` (= MAYO-C commit `fef05b5` "NIST Round 3",
github.com/PQCMayo/MAYO-C). Not the same as the older `MAYO/` family (Round 2): pk/sig sizes
and KATs differ (MAYO_1 pk 1420 -> 1456 B, sig 454 -> 464 B). The old family is untouched.

Layout: `MAYO-R3/common/{src,include}` (mayo.c, params.c, arithmetic.c, aes_c.c, fips202.c, mem.c,
randombytes_ctrdrbg.c = NIST AES-256 CTR-DRBG; src/generic arithmetic headers) + per variant
`MAYO_{1,2,3,5}/{src/api.c, include/{api.h,mayo_variant.h}}`. Variants: MAYO_1 (cat 1), MAYO_2 (cat 1),
MAYO_3 (cat 3), MAYO_5 (cat 5). Make targets `test-MAYO_<N>-R3`, `gen-MAYO_<N>-R3`, `test-mayo-r3`,
`gen-mayo-r3`; rsp in `vectors/rsp/signatures/onramp/MAYO-R3/`.

Patches vs upstream (portable-build only, no algorithmic change):
- The package ships several files as 0-byte placeholders (src/common/{aes_c.c,fips202.c/.h,mem.c,
  randombytes_ctrdrbg.c,aes_ctr.h}, src/generic/*.h, src/mayo_N/api.{c,h}). They were taken from MAYO-C
  `fef05b5`; every non-empty package file in Reference_Implementation/{src,include} is byte-identical to that
  commit except `arithmetic.h` (a GFNI feature check, inactive here; the package version is kept).
- Build type fixed at compile time: `include/mayo_variant.h` defines `MAYO_VARIANT=MAYO_<N>` and
  `MAYO_BUILD_TYPE_OPT` (fixed parameters, portable `src/generic` backend; the upstream CMake default for the
  Reference package is `ref`, i.e. dynamic parameters, which yields identical outputs). `mayo.h` got a guarded
  `#include "mayo_variant.h"` before `mayo_constants.h` (same fix as in MAYO/).
- Local headers included with `<...>` were changed to `"..."` (needed for flat `gcc *.c` static builds).
- Omitted (dead code without -DENABLE_AESNI/-DENABLE_AESNEON): aes128ctr.c, aes_neon.c, randombytes_system.c,
  debug_bench_tools.h, src/AVX2, src/neon.
- `common/include/rng.h`: shim aliasing randombytes.h (needed by the generic static-test renderer).

KAT: `harness/gen/PQCgenKAT_sign_mayo_r3.c` + `main_mayo_r3.c` follow the official NIST flow (seed/msg drawn from the
entropy-seeded DRBG, keygen right after `randombytes_init(seed)`), with `size_t` smlen/mlen. Records are
byte-identical to the official `PQCsignKAT_{24_MAYO_1,24_MAYO_2,32_MAYO_3,40_MAYO_5}.rsp`.

    make test-MAYO_1-R3 KATNUM=10

### UOV-R3 (Round-3 UOV, 12 variants)

Source: `DS-add/UOV-R3/Reference_Implementation` (UOV Round-3 submission, generic reference BLAS only).
This is a NEW family next to the older `UOV` port: Round 3 changed the OV(256) parameter sets
(Ip 112/44 -> 119/45, III 184/72 -> 193/72, V 244/96 -> 259/96; Is = OV(16,160,64) keeps its parameters), so
sizes and KATs differ and the old family cannot represent Round 3.

Layout: `UOV-R3/common/{src,include}` (shared code) + `UOV-R3/<VARIANT>/include/params.h` per variant
(UOV_{Ip,Is,III,V} x {"", _pkc, _pkc_skc}). Make targets `test-<VARIANT>-R3` / `gen-<VARIANT>-R3`,
aggregates `test-uov-r3` / `gen-uov-r3`, rsp in `vectors/rsp/signatures/onramp/UOV-R3/`.
KAT driver: `harness/gen/PQCgenKAT_sign_uov_r3.c` + `harness/gen/main_uov_r3.c` (official NIST flow: entropy 0..47,
msg of length 33*(count+1) drawn before the per-record reseed; the older UOV drivers use a different flow).

Patches vs the package (only these 4 files differ; all other .c/.h and every params.h are byte-identical):
- `rng.c`, `rng.h`: OpenSSL EVP AES-256-ECB replaced by a self-contained software AES-256; `randombytes()` returns void.
- `config.h`: `_UTILS_OPENSSL_` no longer defined (SHAKE from bundled `fips202.c`), `_NIST_KAT_` defined.
- `utils_randombytes.h`: `_NIST_KAT_` branch no longer requires `_UTILS_OPENSSL_`.

Caveats: no official R3 KAT is available offline (package ships only a download link); validation is by
self-consistency and by agreement with the package's own generator (built with OpenSSL in a scratch copy).
Static tests use the registry shape `ds_drbg_official` (variable `mlen`, see "Round-3 static-test shape" below).

### SNOVA-R3 (Round-3 reference implementation, 18 variants)

Source: `DS-add/SNOVA_Round3/SNOVA_Round3/Reference_Implementation` (levels I/III/V x B/K/S x plain(SHAKE)/`_AES`).
Layout: `SNOVA-R3/common/{src,include}` holds the 5 `.c` + 5 `.h` files that are byte-identical in all 18 upstream
dirs (verified with `cmp` against SNOVA_I_B); each variant dir `SNOVA-R3/<VARIANT>/include/snova_params.h` holds only
the parameters. Variant dir names are the upstream names verbatim. No subdirectories below `src/`/`include/`.

Patches vs upstream (only one): every `snova_params.h` gets `#define SNOVA_REF 1` appended (upstream passes
`-DSNOVA_REF=1` in its Makefile), so plain `gcc` with zero `-D` flags works. No OpenSSL (the upstream `rng.c` use is
behind `#if 0`, AES comes from `aes.c`), no AVX2/NEON backend is compiled. No empty files in the package or the port.

KAT: official Round-3 KATs ship in the package (`KAT/<variant>/PQCsignKAT_<variant>.rsp`). The official generator
draws seed and message from one entropy-seeded DRBG with `mlen = 33*(count+1)` and calls keygen right after the
per-record reseed, which differs from the Round-2 SNOVA generator, hence the dedicated
`harness/gen/PQCgenKAT_sign_snova_r3.c` and `harness/gen/main_snova_r3.c`. Static tests use the registry shape
`ds_drbg_official` (variable `mlen`, see "Round-3 static-test shape" below).

Targets: `make test-<VARIANT>-R3 [KATNUM=n]`, `make gen-<VARIANT>-R3`, aggregates `test-snova-r3` / `gen-snova-r3`.
Level-V variants may need a bigger stack; the recipes run `ulimit -s 65536`.

Validated (level I / NIST cat 1): all 6 variants with KATNUM=10, keygen/sign/verify 10/10 each; the first 10
generated records equal the official KAT. Levels III/V were not run.

### QRUOV-R3

Ported from the **NIST Round-3** QR-UOV submission
(`Reference_Implementation/ref`). The Round-3 code is a rewrite of the Round-2
code base ported as [QRUOV](#qruov) above (different file set, new parameter
mechanism, new PRG layout, new `q127L10` level-1/3/5 sets), so it lives in its
own family `QRUOV-R3/`; the older `QRUOV/` family is untouched.

30 variants = 15 parameter sets (`q7L10`, `q31L3`, `q31L10`, `q127L3`,
`q127L10` x NIST level 1/3/5) x 2 public-seed-expansion PRG modes (`aes` =
AES-128-CTR, `shake` = SHAKE128). Variant dirs are named exactly like upstream's
`make PARAM=<..> PRG=<..>` options: `QRUOV_<cat>q<q>L<L>_<aes|shake>`, e.g.
`QRUOV_1q127L3_aes`. Level-1 (cat 1) variants: `QRUOV_1q7L10_*`,
`QRUOV_1q31L3_*`, `QRUOV_1q31L10_*`, `QRUOV_1q127L3_*`, `QRUOV_1q127L10_*`.

Layout: all `.c`/`.h` shared by the 30 variants live once in
[`QRUOV-R3/common/{src,include}`](QRUOV-R3/common) (flat, no subdirectories);
each variant dir holds only `include/qruov_build_config.h`, which hardcodes what
upstream's GNUmakefile passes as `-DQRUOV_PARAM_<..> -DPRG_IS_AES=<0|1>` (it is
included at the top of `qruov_param.h`). Plain `gcc`, no `-D` flags:

```sh
gcc -O2 -std=c11 -I QRUOV-R3/common/include -I QRUOV-R3/QRUOV_1q127L3_aes/include \
    <static test main.c> QRUOV-R3/common/src/*.c -o test
```

`make test-QRUOV_1q127L3_aes-R3`, `make gen-QRUOV_1q127L3_aes-R3`; aggregates
`test-qruov-r3` / `gen-qruov-r3`. `.rsp` files: `vectors/rsp/signatures/onramp/QRUOV-R3/<variant>.rsp`.

**Patches vs upstream Round-3 `ref/`** (everything else is byte-identical):

- `prim.c`/`prim.h` rewritten OpenSSL-free: SHAKE128/SHAKE256 (incremental
  absorb/squeeze, `reset`) on the portable `fips202.c` and AES-128-CTR on the
  portable constant-time `aes_c.c` + `qruov_aes.h` (both reused from the older
  QRUOV port). Same function contract as upstream, so `prg.c`, `qrop.c`,
  `qruov_common.c` are untouched. AES-CTR: upstream's 8-byte nonce || 64-bit BE
  counter is realised through `aes128_ctr()` with a 12-byte IV + 32-bit counter
  (identical keystream below 2^32 blocks per call).
- `fips202.c/.h`: the copy from the older port renames its `shake128*/shake256*`
  symbols to `f202_shake128*/f202_shake256*` (the old `ov_fips202_ref_` macro
  aliasing would collide with upstream's `shake256` type/function names in
  `prim.h`).
- `rng.c`: OpenSSL `EVP` AES-256-ECB replaced by the software AES-256 used in the
  older port (upstream Round-3 `rng.h` contract kept: `void randombytes(.., unsigned long long)`).
- `qruov_param.h`: `#include "qruov_build_config.h"` added (see above).
- `util.h`: `secure_zero()` always uses the volatile-memset fallback (the
  `explicit_bzero` path needs `_GNU_SOURCE`, unavailable under plain `-std=c11`).
- Not ported: `GNUmakefile`, upstream `PQCgenKAT_sign.c` (replaced by
  `harness/gen/PQCgenKAT_sign_qruov_r3.c`), the optimized (`opt`/`avx2`) code.

**KAT flow (differs from the older QRUOV family).** `harness/gen/PQCgenKAT_sign_qruov_r3.c`
/ `main_qruov_r3.c` follow the official `PQCgenKAT_sign.c` exactly: seeds *and*
messages are drawn from one continuous DRBG stream (entropy 0..47), message
length is `33*(count+1)`, and after `randombytes_init(seed)` **nothing is
skipped** before keygen. (The older generator draws the message after the
reseed, so its records do not equal upstream's.) For static tests this needs the
registry shape `ds_drbg_official` (variable `mlen`, no skip), see
`integration/qruov-r3/gen_static_test.patch`.

Official Round-3 KATs are **not** shipped in the local package (only a
`kat_aes.zip`/`kat_shake.zip` mention in the top-level README); record #0 of
every variant was therefore checked against the output of upstream's own
`PQCgenKAT_sign` built with OpenSSL (see REPORT.md).

### SDITH-R3

Ported from the **NIST Round-3** SDitH submission (VOLE-in-the-Head SDitH,
`v3.0.0`). This is a different scheme code base from the Round-2 Hypercube /
Threshold variants ported as [SDITH](#sdith) above, so it lives in its own
family `SDITH-R3/`; the older `SDITH/` family is untouched.

**Provenance.** The local Round-3 package (`sdith3_submission_package`) ships
only CMake files, docs and (truncated) KATs, no sources. The sources come from
upstream `github.com/sdith/sdith`, tag `v3.0.0` (commit `ce7adf06476674d4ff990f378f83f9981cd6745b`),
which is what the package's CMakeLists (generated by upstream
`utils/make_nist_package.py`) lists, and the package `.rsp` files are byte-prefixes
of upstream `kat_r3/`. Portable C only (upstream `ONLY_REF_IMPLEMENTATION` file
set: no `*_avx2.c`, `lib/aes` reference code, `lib/sha3` + opt64 Keccak, NIST `sign.c`).

12 variants, named exactly like upstream: `sdith_cat{1,3,5}_{fast,short}[_cipherpow]`.
Category-1: `sdith_cat1_fast`, `sdith_cat1_short`, `sdith_cat1_fast_cipherpow`,
`sdith_cat1_short_cipherpow`.

Layout: everything shared lives once in [`SDITH-R3/common/{src,include}`](SDITH-R3/common)
(flat, no subdirectories); each variant dir holds only `include/api.h`
(`CRYPTO_*BYTES`, `CRYPTO_ALGNAME`, `SIGNATURE_PARAMS`; the 12 parameter sets are
run-time parameter structs in the library). Plain `gcc`, no `-D` flags; the one
extra requirement is `-lm` (`log2()` in `sdith_signature.c`):

```sh
gcc -O2 -std=c11 -I SDITH-R3/common/include -I SDITH-R3/sdith_cat1_fast/include \
    <main.c> SDITH-R3/common/src/*.c -o test -lm
```

`make test-sdith_cat1_fast-R3`, `make gen-sdith_cat1_fast-R3`; aggregates
`test-sdith-r3` / `gen-sdith-r3`. `.rsp` files: `vectors/rsp/signatures/onramp/SDITH-R3/<variant>.rsp`.

**Patches vs upstream v3.0.0** (everything else is byte-identical):

- Flattened `src/`, `lib/aes/`, `lib/sha3/` (+ `opt64`) into `common/{src,include}`; relative
  includes (`../aes/..`, `../sha3/..`, `../endian_compat.h`) rewritten to plain
  `"..."` includes (`sdith_prng.c`, `brg_endian.h`). No filename collisions.
- New `common/include/sdith_build_config.h` (hardcodes `ONLY_REF_IMPLEMENTATION`,
  `NDEBUG`), included first by every `.c` and by `sdith_prng_private.h`.
- `brg_endian.h`: x86 intrinsics block now also guarded by `!ONLY_REF_IMPLEMENTATION`.
- `sdith_prng_private.h`: `typeof` -> `__typeof__` (needed for `-std=c11`).
- `rng.c`/`rng.h`: the repo's self-contained NIST CTR-DRBG (`harness/gen/rng.c`), no OpenSSL.
- `sign.c`: NIST wrapper as upstream plus the build-config include.
- Not ported: AVX2 files, `CMakeLists.txt`, upstream KAT generator, Optimized_Implementation.

**KAT flow.** `harness/gen/PQCgenKAT_sign_sdith_r3.c` / `main_sdith_r3.c` follow the official
`PQCgenKAT_sign.c`: seeds and messages come from one entropy-seeded DRBG stream
(entropy 0..47), `mlen = 33*(count+1)`, then `randombytes_init(seed)` with nothing
skipped before keygen. The first 10 records of the four cat-1 variants are equal
(seed, mlen, msg, pk, sk, smlen, sm) to the official package KAT (which is
truncated at 256 KiB, ~20-25 complete records per variant).

**Static tests.** Registry shape `ds_drbg_official` (variable `mlen`, see "Round-3 static-test shape"
below); folders are built with `gcc -O2 -std=c11 *.c -o test -lm` (registry key `static_gcc_flags: "-lm"`,
the library must come after the sources).

### SQISIGN-R3

Ported from the **NIST Round-3** SQIsign submission (package `sqisign/SQIsign`,
`Reference_Implementation`, CMake VERSION 2.0). The package ships only
`src/sqisign.c`, headers, apps and KATs (every `src/<module>/` dir is empty), so
the library sources come from the official upstream repository
`github.com/SQIsign/the-sqisign`, **tag `nist-v3`** (commit
`6d017708db403bf83977fa70770fc4f7f9e9ff21`). Every file that exists in the
package is byte-identical to upstream (except README.md) and the package `.rsp`
files are byte-prefixes of upstream `KAT/*.rsp`. This is a different code base
than the older [SQISIGN](#sqisign) family (Round-2 era, GMP/mini-gmp based,
other sizes); that family is untouched.

3 variants = upstream parameter sets `sqisign_p324_3` (NIST level 1, pk 83 B,
sk 270 B, sig 200 B), `sqisign_p500_27` (level 3, 129/417/306 B) and
`sqisign_p664_17` (level 5, 169/549/406 B).
Level 1 is `sqisign_p324_3`.

Layout: all `.c`/`.h` shared by the variants live once in
[`SQISIGN-R3/common/{src,include}`](SQISIGN-R3/common) (flat, no subdirectories,
the upstream `src/<module>/` tree flattened without filename collisions); each
variant dir holds `src/` (`api.c`, `fp_<p>.c` and the five precomputed-constant
`.c` files) and `include/` (`api.h`, precomp headers, `fp_<p>_{32,64}_inc.h`,
`sqisign_variant.h`). Only the portable "ref" build type is ported (no
broadwell/arm64 assembly).

Portability: no GMP, no mini-gmp, no OpenSSL. Round-3 SQIsign replaced GMP by its
own fixed-size big-integer module (`mp.c`, `integers.c`), the field arithmetic
is the generic C backend (64-bit limbs when `unsigned __int128` exists, else
32-bit), randomness is the bundled NIST AES-256-CTR DRBG (`randombytes_ctrdrbg.c`,
`aes_c.c`) and SHA-3 is the bundled `fips202.c`. The Makefile links `-lm` (not actually required by the static folder).

`make test-sqisign_p324_3-R3`, `make gen-sqisign_p324_3-R3`; aggregates
`test-sqisign-r3` / `gen-sqisign-r3`. `.rsp` files:
`vectors/rsp/signatures/onramp/SQISIGN-R3/<variant>.rsp`. Generator:
`harness/gen/PQCgenKAT_sign_sqisign_r3.c` + `main_sqisign_r3.c` (official two-phase
NIST flow, message length `33*(count+1)`), so records equal the official KAT.

**Patches vs upstream `nist-v3`** (everything else byte-identical):

- `common/include/sqisign_build_config.h` (new, included first by every `.c`
  file) and per-variant `include/sqisign_variant.h` (new) hardcode what upstream
  CMake passes as `-D` flags (`SQISIGN_VARIANT`, `SQISIGN_BUILD_TYPE_REF`,
  `SQISIGN_GF_IMPL_REF`, `ENABLE_SIGN`, `RANDOMBYTES_C`, `NDEBUG`, `RADIX_64`/`RADIX_32`).
- Nested upstream directories flattened into `common/{src,include}` and per-variant `src/`/`include/`.

**Caveats**

- Signing takes about 10-15 s per vector for p324_3 (reference code).
- The KAT message length varies (33, 66, ...); static tests use the `ds_drbg_official` shape (see
  "Round-3 static-test shape" below).
- Project headers are included with angle brackets (`#include <ec.h>`, as in
  upstream), so the static folder builds with `gcc -I. *.c` (not with bare
  `gcc *.c`) unless those includes are changed to quotes (see REPORT.md).

### FAEST-R3 (FAEST v3, NIST round 3, lambda = 128: FAEST_128F, FAEST_128S, FAEST_EM_128F, FAEST_EM_128S)

Official FAEST v3.0 reference code (NIST round-3 submission), assembled by `tools/assemble_faest_r3.sh` from
`sw/original/faest-v3/<variant>/` (verbatim copies of the submission packages, hashes in `sw/original/PROVENANCE.md`).
NOT the same as the `FAEST/` family (v2-sized signatures 5924/4506/5060/3906 B): v3 sizes are
sig = 5170 / 4066 / 4170 / 3466 B (smlen = sig + mlen), pk = sk = 32 B. Only the four lambda = 128 sets are
ported (192/256-bit sets are not in the imported tree, but their tables and AES files are kept because the generic
`instances.c`/`faest_impl.c` reference them). The existing `FAEST/` family is untouched.

Layout: `FAEST-R3/common/{src,include}` (everything byte-identical across the four packages, flat, XKCP sha3 opt64
and `tables/` flattened) + `FAEST-R3/<V>/{src/{crypto_sign.c,faest_<v>.c}, include/{api.h,faest_<v>.h}}`.
Targets: `test-<V>-R3` / `gen-<V>-R3`, aggregates `test-faest-r3` / `gen-faest-r3`, rsp in
`vectors/rsp/signatures/onramp/FAEST-R3/<V>.rsp`. Generator/runner: `harness/gen/PQCgenKAT_sign_faest_r3.c`, `main_faest_r3.c`
(official NIST flow: seed and msg drawn from the entropy-seeded DRBG, mlen = 33*(count+1), keygen right after
`randombytes_init(seed)`; `unsigned long long` API).

Portable C only: no AES-NI / SSE2 / AVX2 code (compat.h's x86 auto-detection is switched off), no OpenSSL, no
`-march`, zero `-D`. Builds with plain `gcc *.c` (default flags, and `-O2 -std=c11`); the only diagnostics are
`-Wpsabi` notes (GCC vector-extension types in fields.h, not AVX code). `-Wno-psabi` is passed by the Makefile only
to keep the log quiet.

Patches vs the official package (all mechanical, algorithm code untouched; applied by the assembly script):
1. Sub-directory includes flattened: `sha3/KeccakHash.h` -> `KeccakHash.h` (hash_shake.h), `tables/X.h` -> `X.h`
   (instances.c), `../endian_compat.h` -> `endian_compat.h` (sha3 brg_endian.h).
2. `#include <config.h>` -> `#include "config.h"` (plain `gcc *.c` has no `-I.`).
3. `config.h` name collision resolved by merging: the 4 XKCP `#define`s of `sha3/config.h` + the three
   `HAVE_ALIGNED_ALLOC/HAVE_EXPLICIT_BZERO/HAVE_MEMPCPY` of the package's portable config (`sw/common/portable/config.h`).
4. New header `common/include/faest_build_config.h` (first line of every .c): hardcodes `_GNU_SOURCE`,
   `HAVE_CONFIG_H` (forces config.h, i.e. portable paths), `HAVE_RANDOMBYTES` (rand_bytes -> NIST DRBG randombytes),
   `NDEBUG` (upstream's own CPPFLAGS). Official Makefile also passes `-DFAEST_EXPORT=` which is already the default.
5. `rng.c`/`rng.h`: the repo's software AES-256 CTR-DRBG (harness/gen/rng.c). The official KAT tool links OpenSSL AES
   for the same DRBG; the streams are identical (record equality below).
6. `crypto_sign.h` (SUPERCOP-only duplicate of api.h) and the package's own tests/NIST-KATs/cpu-feature tests are not copied.
   `cpu.c` is kept (compiled, unused paths).

Validation: official 100-record KAT (`sw/original/faest-v3/<v>/kat/PQCsignKAT_faest_<v>.rsp`) vs
`make gen-<V>-R3 KATNUM=100`: 100/100 records byte-identical for all four variants (seed, mlen, msg, pk, sk, smlen, sm).
Caveat: the portable code is slow (generation of 100 records took 128..1365 s with four generators in parallel;
the s sets are the slowest); use a small `KATNUM` for routine runs.

### Round-3 static-test shape (`ds_drbg_official`) and consistency across the `*-R3` families

All seven Round-3 families (MAYO-R3, UOV-R3, SNOVA-R3, QRUOV-R3, SDITH-R3, SQISIGN-R3, FAEST-R3) use the registry shape
`ds_drbg_official` in `tools/algo_registry.json`. It is `ds_drbg_nist` (official NIST `PQCgenKAT_sign` flow, nothing
skipped after `randombytes_init(seed)`) plus per-record `mlen`/messages, because the official Round-3 KATs use
`mlen = 33*(count+1)`; with it `--kat-num`/`STATIC_KATNUM` > 1 works. MQOM (constant `mlen`) keeps `ds_drbg_nist`.
Optional per-variant registry keys: `"size_t_api": true` (MAYO-R3: `smlen`/`mlen1` are `size_t`, an error on gcc >= 14
with `unsigned long long`) and `"static_gcc_flags"` (extra flags the static folder needs with plain `gcc *.c`,
printed by the generator: `-lm` for SDITH-R3, `-I.` for SQISIGN-R3).
Binary names are `build/gen_<VARIANT>_R3` / `build/test_<VARIANT>_R3` for all six families (this is the name
`gen_static_test.py` deletes before regenerating).

**Open item (SQISIGN-R3):** the static folder needs `-I.` because the upstream project headers use `#include <x.h>`
(about 255 includes in ~130 files). They were deliberately not bulk-edited to quotes (as was done for MQOM);
that is a decision for the maintainer.

## Adding a new on-ramp variant

1. Copy the algorithm sources into `algorithms/signatures/onramp/<FAMILY>/<VARIANT>/`
   (`src/`+`include/` split for CROSS/HAWK/LESS/SDitH; `src/`+`include/` plus a
   shared `common/` for FAEST/UOV/MAYO/QRUOV/SNOVA/SQIsign — see
   "FAEST"/"UOV"/"MAYO"/"QRUOV"/"SNOVA"/"SDitH"/"SQIsign" above).
2. Open `../../Makefile`, locate the `<FAMILY>_VARIANTS` list, and add the
   variant name.
3. Run `make gen-<VARIANT>` then `make test-<VARIANT>` to verify.

To add a new non-standardized signature family entirely, see "Adding a new
algorithm family" in [`../../README.md`](../../../README.md).
