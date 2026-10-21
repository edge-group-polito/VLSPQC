# FN-DSA experimental (c-fn-dsa) — upstream provenance

**EXPERIMENTAL, PRE-STANDARD. Not in `test-all`.** This is Thomas Pornin's *best guess* of the upcoming
FN-DSA (FIPS 206); the upstream README states that no FN-DSA draft had been published when it was written
(last upstream note 2026-07-22) and that backward compatibility is not guaranteed. It is not Falcon
(`algorithms/signatures/standard/FALCON`), not wire-compatible with it, and not guaranteed to match the final FIPS 206. Status
of FIPS 206 must be re-checked on https://csrc.nist.gov before relying on any of this.

| Item | Value |
|---|---|
| Upstream | https://github.com/pornin/c-fn-dsa |
| Release tag | none; pinned by commit |
| Commit | `1b858880dcac091b5e5f84f0e4d8617c35ba1392` (2026-09-21, "Reenabled function (needed for FP-emulated code).") |
| Import date | 2026-10-20 |
| License | public domain / Unlicense-style dedication (see `LICENSE`, copied from upstream) |
| Variants | `fndsa-exp-512` (logn 9), `fndsa-exp-1024` (logn 10) |

`c-fn-dsa/` holds the upstream sources verbatim (`fndsa.h`, `inner.h`, `kgen_inner.h`, `sign_inner.h`,
`codec.c`, `mq.c`, `sha3.c`, `sysrng.c`, `util.c`, `kgen*.c`, `sign*.c`, `vrfy.c`). Not imported: `test_*.c`,
`speed_fndsa.c`, the Cortex-M4 assembly (`*_cm4.s`, `Makefile.cm4`, `bench_cm4/`), the Makefiles, the README.

## Integer-only (no-FPU) build
Yes. Upstream signing/key generation use floating-point values only through the `fpr` type; with the
architecture options below disabled, `sign_fpr.c` implements `fpr` entirely with integer operations
(mantissa/exponent in a `uint64_t`), so no floating-point hardware is needed:
`-DFNDSA_AVX2=0 -DFNDSA_SSE2=0 -DFNDSA_NEON=0 -DFNDSA_RV64D=0` (these are the flags the Makefile and the
static-test registry entries pass, together with `-D_GNU_SOURCE` for upstream's `sysrng.c` under `-std=c11`).
`FNDSA_DIV_EMU` / `FNDSA_SQRT_EMU` only matter on targets that otherwise use hardware FP (e.g. RV64D).
Checked on this x86-64 host: with these flags, none of the 17 compiled objects other than `sysrng.c` contains a
floating-point arithmetic instruction (only `movdqa`/`movups` data moves), and upstream's own `test_fndsa` (including its
KAT test) passes.

## Local changes
None to upstream files. New, not upstream: `fndsa-exp-<n>/include/api.h` maps the harness'
Falcon-style calls onto the seeded API: `keypair_rnd[0..32)` → `fndsa_keygen_seeded`, `nonce` (40 B) →
`fndsa_sign_seeded` seed (raw interface `FNDSA_HASH_ID_RAW`, empty context), `sign_rnd` unused,
layout `sm = sig || m`. `sysrng.c` (OS RNG, used only by the non-seeded API) is kept so the library links
unchanged; bare-metal targets must replace it.

## Verification
- Upstream `test_fndsa` built with the integer-only flags (portable gcc, no SIMD): all phases pass,
  including "Test KAT" (upstream's own test vectors).
- `.rsp` records from this repository's harness (20 per variant) replayed through the pristine upstream
  API (keygen from the first 32 bytes of `keypair_rnd`, deterministic signature from `nonce`, byte-for-byte pk/sk/sig
  comparison, then verification): 0 mismatches for both variants. This checks the compat layer and the import; the
  seed-to-key mapping itself is a local choice, so these KATs are not an official FN-DSA vector set.
