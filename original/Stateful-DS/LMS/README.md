# LMS (Leighton-Micali Signatures)

Vendored from Cisco's [hash-sigs](https://github.com/cisco/hash-sigs)
reference implementation, with a small pqrv adapter
(`src/hss_variant.c`, `include/hss_variant.h`) on top — the same adapter
used by [`../HSS`](../HSS), since **LMS is HSS with `levels=1`**: identical
library, this directory just always calls it with one level. See
[`../README.md`](../README.md) for the DS-wide overview and
[`../../../README.md`](../../../README.md) for the top-level `make` targets.

## Why this family looks different from the others

hash-sigs has no `crypto_sign_*`-style API and no per-variant compiled
directories — it's one flat, **runtime-parametrized** library: `levels` plus
a `(height, Winternitz-w)` pair per level select the parameter set at
keygen time, not at compile time. So (like `../XMSS`) there's a single
shared `src/`+`include/` tree here, and the KAT generator/runner
(`kat/gen/PQCgenKAT_sign_lms.c`, `main_lms.c`) pick which parameter set to
exercise from a named-variant table — see
[`../HSS/include/hss_variant.h`](../HSS/include/hss_variant.h) (shared by
both families).

The `LMS_H10_W8/` subfolder sitting next to `src/`/`include/` is different
again: it's a **dedicated, self-contained copy** pinned to that one
parameter set, used only by the *static* KAT test generator
(`tools/gen_static_test.py`), which needs compile-time-fixed sizes — see
[`../../../tools/README.md`](../../../tools/README.md).

## What's vendored, and what's added

- The full hash-sigs library (`hss*.c`, `lm*.c`, `endian.c`, `hash.c`,
  `sha256.c` + headers) — unmodified except `sha256.h`'s `USE_OPENSSL` flag
  was flipped from `1` to `0`. hash-sigs already ships its own **portable,
  self-contained SHA-256** (`sha256.c`, gated behind `#if !USE_OPENSSL`) —
  flipping the flag uses that instead of requiring a `libcrypto` link, no
  new code needed (unlike XMSS/SLH-DSA-SHA2, which needed a real shim
  because they don't ship their own fallback).
- `src/hss_variant.c` / `include/hss_variant.h` (new) — wraps
  `hss_generate_private_key`/`hss_load_private_key`/`hss_generate_signature`/
  `hss_validate_signature` (hash-sigs' native API — see its doc comment for
  why) into `hss_variant_init/keypair_seed/sign/verify`, plus the named
  parameter-set tables used by the "-check" sweeps (`lms_variant_fast[]` /
  `lms_variant_slow[]` / `lms_variant_default`).
- Only `hss_thread_single.c` is linked (not `hss_thread_pthread.c`) — no
  threading needed for KAT purposes, and it avoids a `-lpthread` link
  requirement.

## Randomness

Keygen (`hss_generate_private_key`) takes an RNG **callback**
(`bool (*)(void*, size_t)`), called **exactly once**, for
`PRIVATE_KEY_SEED_LEN` = 48 bytes (`SEED_LEN(32) + I_LEN(16)`, under
`SECRET_METHOD 2` — the ACVP-compatible seed-derivation mode; see
`include/config.h` and `include/ACVP Definition.txt`). The adapter's shim
callback just serves bytes from a caller-supplied 48-byte buffer
(`keypair_seed` in the `.rsp`) instead of a real RNG.

Signing needs **no randomness at all** — `SECRET_METHOD 2` derives the
per-signature randomizer deterministically from the private key's SEED/I via
a PRF — so, like XMSS, there's no `sig_rnd` field.

**Signing mutates the private key in place** (advances an 8-byte counter,
written back into the same buffer since the adapter passes
`update_private_key=NULL`). Same as XMSS: the generator snapshots the
private key right after keygen, before calling sign, so the `.rsp` records
the pre-sign state.

## `make` targets

| Target | Coverage | In `test-all`? |
|--------|----------|-----------------|
| `test-lms` / `gen-lms` | 1 variant: `LMS_H10_W8` | yes |
| `test-lms-check` / `gen-lms-check` | 8 "fast" variants (heights 5/10 × all 4 Winternitz values) | no |
| `test-lms-check-slow` / `gen-lms-check-slow` | 12 "slow" variants (heights 15/20/25 × all 4 Winternitz values) | no |

`KATNUM` controls the default target; the check targets use `CHECK_KATNUM`
(default `1`) instead — e.g. `make test-lms-check CHECK_KATNUM=3`.

### Why "fast" vs "slow"

Same underlying cause as XMSS: keygen costs O(2^height) leaf hashes, and
this reference library doesn't cache tree state across process runs either.
Measured on this host: height 10 (1024 leaves) is near-instant; height 15
(32768 leaves) alone didn't finish in 60 seconds. Heights 20/25 are worse
still (each +5 height is a further ~32× more leaves). So:

- **Fast (8):** heights 5 and 10, all 4 Winternitz values (`W1`/`W2`/`W4`/`W8`).
- **Slow (12):** heights 15, 20, and 25, all 4 Winternitz values.

See `lms_variant_fast[]` / `lms_variant_slow[]` in
[`../HSS/src/hss_variant.c`](../HSS/src/hss_variant.c) for the exact list.
`test-lms-check-slow` is unbounded — the height-25 entries in particular can
take a long time — run it deliberately, not as a sanity check.

## `.rsp` fields

`count`, `seed`, `keypair_seed` (48 bytes), `mlen`, `msg`, `pk`, `sk`,
`smlen`, `sm` — no `sig_rnd` (see "Randomness" above). `sm` is
signature‖message (`smlen` = signature length + `mlen`); `pk`/`sk`/signature
sizes vary per parameter set (looked up via `hss_variant_*_bytes()`, not a
compile-time constant) — see [`../../kat/README.md`](../../kat/README.md)
for the general `.rsp` format.

## Why LMS isn't just "HSS with levels=1" in the Makefile too

It is, functionally — but LMS and HSS are kept as separate named `make`
targets (mirroring how the four SLH-DSA sub-families are separate targets
despite sharing structure) because their practical parameter spaces differ
enough to warrant different default/check variant tables: LMS's is bounded
(20 total `(height, w)` pairs), HSS's is a further product of that with
1-8 levels — see [`../HSS/README.md`](../HSS/README.md).
