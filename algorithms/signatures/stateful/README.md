# Stateful hash-based signatures (XMSS, LMS, HSS)

Scope: reference implementations of the **stateful** hash-based signature schemes XMSS / XMSS^MT, LMS and HSS. They are approved in
NIST [SP 800-208](https://csrc.nist.gov/pubs/sp/800/208/final) (*Recommendation for Stateful Hash-Based Signature Schemes*, **final**,
October 2020 — checked on NIST's page 2026-10-20), a standard separate from the PQC signature FIPS 204 / 205 / 206. "Stateful" means the signer
must track and never reuse a one-time key; together with their NIST-defined parameter spaces (instead of fixed security levels) that is why
they are a separate category with their own targets, **not part of `test-all`**.

Unlike every other family, the real APIs are **runtime-parametrized**: one shared source tree per family covers every parameter set, selected at
key-generation time. Each family therefore has a single directory with one pinned default variant plus its own README explaining the fixed-size adapter and the
`-check` / `-check-slow` sweep targets: [`XMSS/README.md`](XMSS/README.md), [`LMS/README.md`](LMS/README.md), [`HSS/README.md`](HSS/README.md).

The sources are compiled with `harness/gen/PQCgenKAT_sign_*.c` / `harness/gen/main_*.c`; see [`../../../docs/kat-format.md`](../../../docs/kat-format.md),
[`../../../harness/README.md`](../../../harness/README.md) and the [root README](../../../README.md). Provenance of every family: [`../../README.md`](../../README.md).

## Families

| Family | Upstream | Closest upstream state found | License | Default variant (`test-stateful-ds`) | More via `-check` / `-check-slow` |
|---|---|---|---|---|---|
| XMSS / XMSS^MT | [xmss-reference](https://github.com/XMSS/xmss-reference) | closest match: `master` `171ccbd2…` (17 of 28 files identical) | CC0 1.0 ([`XMSS/LICENSE`](XMSS/LICENSE)) | `XMSS-SHA2_10_256` | 76 |
| LMS | Cisco [hash-sigs](https://github.com/cisco/hash-sigs) | closest match: `master` `cea4dda4…` (33 of 49 files identical) | Cisco 3-clause BSD-style ([`LMS/LICENSE`](LMS/LICENSE)) | `LMS_H10_W8` | 19 |
| HSS | Cisco hash-sigs (same library as LMS, `levels` ≥ 2) | same as LMS | Cisco 3-clause BSD-style ([`HSS/LICENSE`](HSS/LICENSE)) | `HSS_L2_H5_W8` | 39 |

- **Security assumption:** hash-function security (Merkle trees of one-time signatures); no number-theoretic assumption.
- **Local patches:** a small adapter (`crypto_bridge.c`, `api.h`) per default variant that pins one parameter set and exposes the fixed-size
  `crypto_sign_keypair` / `crypto_sign` / `crypto_sign_open` convention; details in each family README.
- **Pins:** the pins in the table are the *closest upstream states found* by comparing file hashes (details and method in [`UPSTREAM.md`](UPSTREAM.md)), not verified import points.
- **Sizes of the default variants (generated):** from the compiled `api.h` constants (`tools/gen_size_table.py`); the other parameter sets have other sizes (see the
  family READMEs).

<!-- BEGIN sizes:stateful -->
| Variant | pk (B) | sk (B) | ct / sig (B) |
|---|---:|---:|---:|
| XMSS-SHA2_10_256 | 68 | 136 | 2500 |
| LMS_H10_W8 | 60 | 64 | 1456 |
| HSS_L2_H5_W8 | 60 | 64 | 2644 |
<!-- END sizes:stateful -->

## `make` targets

Prefix with `test-` or `gen-`: `xmss`, `lms`, `hss` (aggregate `test-stateful-ds` / `gen-stateful-ds`, **not** part of `test-all` / `gen-all`). Each family has three
targets instead of one per variant: `test-<family>` (the default variant, in `test-stateful-ds`), `test-<family>-check` (a "fast" parameter-set sweep) and
`test-<family>-check-slow` (a "slow" sweep that can take minutes to hours). Static tests: `make static-test-stateful-ds` (default variants only).

## `.rsp` fields

All `.rsp` files carry `count`, `seed`, `mlen`, `msg`, `pk`, `sk`, `smlen`, `sm` (see [`../../../docs/kat-format.md`](../../../docs/kat-format.md)) plus `keypair_seed`
(3·n bytes for XMSS, 48 B for LMS and HSS). There is **no `sig_rnd`**: signing is fully deterministic for all three.

## Known limitations

- Only the three default variants are covered by `test-stateful-ds` and the static tests; the full parameter spaces are exercised only by the on-demand `-check` / `-check-slow`
  sweeps (not run in the FIPS migration; `test-stateful-ds` itself was run at `KATNUM=10` after the restructure and passed).
- The state-handling requirements of stateful schemes (never reusing a one-time key) are outside what a KAT suite checks.
- The exact upstream version of the vendored libraries was not recorded; the table gives the closest matches found.

## Adding a parameter set or family

See "Adding a new XMSS parameter set" in [`XMSS/README.md`](XMSS/README.md); the LMS / HSS `-check` tables are extended in
[`HSS/src/hss_variant.c`](HSS/src/hss_variant.c). For a new family see [`../../../docs/adding-an-algorithm.md`](../../../docs/adding-an-algorithm.md).
