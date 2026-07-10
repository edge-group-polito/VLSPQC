# Stateful-DS — NIST SP 800-208 stateful hash-based signatures

Reference-implementation sources for **stateful** hash-based signature
schemes: XMSS/XMSS^MT, LMS, and HSS, standardized under NIST
[SP 800-208](https://csrc.nist.gov/pubs/sp/800/208/final) — a separate
standard from the stateless PQC signature FIPS (204/205/206) covered by
[`../DS/`](../DS/README.md). "Stateful" means the signer must track and
persist how many one-time keys have already been used and never reuse one;
that operational requirement (plus their comparatively small NIST-defined
parameter space rather than fixed security levels) is why they get their own
category and their own `make` targets instead of joining `test-all`.

**Unlike every family in `DS/`/`Additional-DS/`**, XMSS/LMS/HSS's real APIs
are runtime-parametrized — one shared source tree per family covers every
parameter set, selected at keygen time — so each gets a single directory
plus its own README explaining the fixed-size adapter used for testing and
the resulting `-check`/`-check-slow` sweep targets; see
[`XMSS/README.md`](XMSS/README.md), [`LMS/README.md`](LMS/README.md),
[`HSS/README.md`](HSS/README.md).

These sources are compiled together with `kat/gen/PQCgenKAT_sign_*.c`
(generation) or `kat/gen/main_*.c` (testing) — see
[`../../kat/README.md`](../../kat/README.md) for how that build works, and
[`../../README.md`](../../README.md) for the `make` targets that drive it.

## Families

| Family | Variants | Notes |
|--------|----------|-------|
| XMSS / XMSS^MT | 1 default + 76 more via `-check`/`-check-slow` | runtime-parametrized, shared source tree — see [`XMSS/README.md`](XMSS/README.md) |
| LMS | 1 default + 19 more via `-check`/`-check-slow` | Cisco hash-sigs, `levels=1` — see [`LMS/README.md`](LMS/README.md) |
| HSS | 1 default + 39 more via `-check`/`-check-slow` | same library as LMS, `levels` 2-3 — see [`HSS/README.md`](HSS/README.md) |

**`test-stateful-ds` covers the 3 default variants above** (one per family).
Their full parameter spaces (77 + 20 + 40 = 137 more sets) are exercised via
their own `-check`/`-check-slow` sweeps instead, on demand — see below —
since a lot of that space is either huge (LMS/HSS' `(height, w, levels)`
combinations) or individually slow (tall single-tree tree heights).

## Per-family `make` targets

Prefix with `test-` or `gen-`: `xmss`, `lms`, `hss` (aggregate:
`test-stateful-ds`/`gen-stateful-ds`, **not** part of `test-all`/`gen-all`).

**XMSS, LMS, HSS** each have exactly three targets instead of one per
variant — `test-<family>` (default variant, in `test-stateful-ds`),
`test-<family>-check` (a "fast" parameter-set sweep), and
`test-<family>-check-slow` (a "slow" sweep). See their READMEs for what each
sweep covers and why; the short version is `make test-xmss-check`,
`make test-lms-check`, `make test-hss-check` (and the `-check-slow`
equivalents, which can each take a long time — not for casual use).

## `.rsp` fields specific to Stateful-DS

All `.rsp` files carry the standard `count`/`seed`/`mlen`/`msg`/`pk`/`sk`/
`smlen`/`sm` fields (see [`../../kat/README.md`](../../kat/README.md) for the
full format), plus `keypair_seed` (3×n bytes for XMSS, 48 B for LMS/HSS);
**no `sig_rnd`** — signing is fully deterministic for all three.

## Algorithm-specific notes

Covered in their own READMEs — [`XMSS/README.md`](XMSS/README.md),
[`LMS/README.md`](LMS/README.md), [`HSS/README.md`](HSS/README.md) — since
each needed a dedicated adapter and generator/runner pair, not just a new
variant directory.

## Adding a new Stateful-DS parameter set

Adding a new XMSS parameter set is covered in "Adding a new XMSS parameter
set" in [`XMSS/README.md`](XMSS/README.md). LMS/HSS's `-check` tables can be
extended similarly (see `lms_variant_fast[]`/`_slow[]` in
[`HSS/src/hss_variant.c`](HSS/src/hss_variant.c)); the underlying library
already supports every `(height, w)` pair NIST defines, so this is only ever
about which ones the check sweeps exercise, not adding new library code.

To add a new stateful hash-based signature family entirely, see "Adding a
new algorithm family" in [`../../README.md`](../../README.md).
