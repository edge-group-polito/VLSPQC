# XMSS / XMSS^MT

Vendored from the [xmss-reference](https://github.com/XMSS/xmss-reference)
implementation (non-"fast"/non-BDS core — see below), with a small pqrv
adapter (`src/sign.c`, `include/xmss_variant.h`) on top since this family
doesn't fit the rest of the repo's per-variant-directory convention. See
[`../README.md`](../README.md) for the DS-wide overview and
[`../../../README.md`](../../../README.md) for the top-level `make` targets.

## Why this family looks different from the others

Every other DS family in this repo is *compiled* per parameter set (a
directory per variant, different `#define`s baked in). XMSS's reference API
is **runtime-parametrized** instead — one binary, and `xmss_keypair(pk, sk,
oid)` picks the parameter set via a 4-byte OID argument. So instead of one
directory per variant, there's a single shared `src/`+`include/` tree, and
the KAT generator/runner (`kat/gen/PQCgenKAT_sign_xmss.c`, `main_xmss.c`)
select which of the **77 parameter sets** (21 plain XMSS + 56 XMSS^MT) to
exercise via a build-time string or an internal table — see
[`include/xmss_variant.h`](include/xmss_variant.h).

The `XMSS-SHA2_10_256/` subfolder sitting next to `src/`/`include/` is
different again: it's a **dedicated, self-contained copy** pinned to that
one parameter set, used only by the *static* KAT test generator
(`tools/gen_static_test.py`), which needs compile-time-fixed sizes — see
[`../../../tools/README.md`](../../../tools/README.md).

## What's vendored, and what's added

- `src/{params,hash,hash_address,wots,utils,fips202,xmss_commons,xmss_core}.c`
  and matching headers — unmodified except `hash.c`'s `#include
  <openssl/sha.h>` was swapped for a local `openssl_sha_shim.h` (see below).
  Uses the plain **`xmss_core.c`**, not `xmss_core_fast.c` (BDS-cached) — the
  BDS variant only speeds up *later* signatures on an already-loaded key, not
  the one-shot keygen→sign→verify this KAT harness does per vector, so it
  wouldn't help here (see "slow" discussion below) and would add state-size
  complexity for no benefit.
- `src/sign.c` (new) — `xmss_variant_init/keypair_seed/sign/open` wrapping
  `xmssmt_core_seed_keypair`/`xmssmt_core_sign`/`xmssmt_core_sign_open`
  directly (bypassing `xmss.c`'s OID-prefixing wrapper, which only exposes an
  *internal-RNG* keypair call, no derand entry point). `xmss_core_keypair`/
  `xmss_core_sign`/`xmss_core_sign_open` (the plain-XMSS, non-MT names) are
  themselves just forwarders to the `xmssmt_core_*` equivalents with `d=1`,
  so one code path covers both XMSS and XMSS^MT.
- `src/{sha2_256,sha2_512}.c` + `include/{sha2_api,plat_local}.h` (new) — the
  reference code calls OpenSSL's one-shot `SHA256()`/`SHA512()`; rather than
  add a `libcrypto` dependency (this repo otherwise depends on nothing but
  each algorithm's own source), `src/openssl_sha_shim.c` implements those two
  entry points on top of a self-contained SHA-256/512 (the same one used by
  `DS/SLH-DSA-SHA2`'s "s" variants).

## Randomness

`xmssmt_core_seed_keypair(params, pk, sk, seed)` takes an explicit
**3×n-byte** seed (`keypair_seed` in the `.rsp`). Signing
(`xmssmt_core_sign`) is **fully deterministic given `sk`** — it never calls
an RNG at all — so unlike most other DS families here, there's no `sig_rnd`
field.

**Signing mutates `sk` in place** (advances XMSS's leaf index by one per
signature — this is a stateful scheme). The generator snapshots `sk`
immediately after keygen, before calling sign, so the `.rsp` records the
pre-sign state; the test runner's own fresh `keypair_seed` call reproduces
that same state for comparison.

## `make` targets

| Target | Coverage | In `test-all`? |
|--------|----------|-----------------|
| `test-xmss` / `gen-xmss` | 1 variant: `XMSS-SHA2_10_256` | yes |
| `test-xmss-check` / `gen-xmss-check` | 49 "fast" variants | no |
| `test-xmss-check-slow` / `gen-xmss-check-slow` | 28 "slow" variants | no |

`KATNUM` controls the default target as usual; the two check targets use
`CHECK_KATNUM` (default `1`) instead, since it's multiplied across dozens of
variants — e.g. `make test-xmss-check CHECK_KATNUM=3`.

### Why "fast" vs "slow"

Keygen costs **O(2^height) leaf hashes** with this non-BDS reference core.
For plain XMSS, `height` is the parameter set's full height. Measured on
this host: height 10 (1024 leaves) is near-instant; height 16 (65536 leaves)
didn't finish in 3 minutes; height 20 (1,048,576 leaves) is worse still.

For XMSS^MT, keygen only computes **one subtree**, of height
`full_height / d` (number of layers) — *not* the full height. Most XMSS^MT
sets keep that per-layer height ≤10 (e.g. `20/4`, `40/8`, `60/12`), but two
combinations — **`40/2`** and **`60/3`** — both reduce to a per-layer height
of **20**, exactly as expensive as a single-tree height-20 set. (An earlier
version of the fast/slow split missed this and put all 56 XMSS^MT sets in
"fast," which left `gen-xmss-check` stuck for hours on those 14 sets.)

- **Fast (49):** the 7 height-10 plain-XMSS sets, plus the 42 XMSS^MT sets
  with per-layer height ≤10.
- **Slow (28):** the 14 height-16/20 plain-XMSS sets, plus the 14 XMSS^MT
  `40/2`/`60/3` sets.

`test-xmss-check` (49 variants, `KATNUM=1`, generate+test both redo keygen)
takes on the order of ten-plus minutes on a modern host. `test-xmss-check-slow`
is unbounded — some individual variants alone can take minutes to well over
an hour — run it deliberately, not as a sanity check.

See `xmss_variant_fast_names[]` / `xmss_variant_slow_names[]` in
[`src/sign.c`](src/sign.c) for the exact lists.

## `.rsp` fields

`count`, `seed`, `keypair_seed` (3×n bytes), `mlen`, `msg`, `pk`, `sk`,
`smlen`, `sm` — no `sig_rnd` (see "Randomness" above). `pk`/`sk`/`sm` sizes
vary per parameter set (looked up via `xmss_variant_pk_bytes()` etc., not a
compile-time constant like other families) — see
[`../../kat/README.md`](../../kat/README.md) for the general `.rsp` format.

## Adding a new XMSS parameter set

If `xmss-reference`'s `params.c` is ever extended with new OIDs, add the
corresponding name string(s) to `xmss_variant_fast_names[]` or
`xmss_variant_slow_names[]` in [`src/sign.c`](src/sign.c) (classify by
per-layer height, per the rule above), and update the counts in this file,
`include/xmss_variant.h`, `kat/gen/PQCgenKAT_sign_xmss.c`, and
`kat/gen/main_xmss.c`'s header comments.
