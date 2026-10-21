# ASCON

Reference-implementation sources of **Ascon v1.2** (the Lightweight Cryptography competition submission; variant names end in `v12`).
Ascon was selected by NIST for lightweight cryptography and standardized in
[SP 800-232](https://csrc.nist.gov/pubs/sp/800/232/final) (final, August 2025), which defines Ascon-AEAD128, Ascon-Hash256, Ascon-XOF128 and
Ascon-CXOF128. **These v1.2 variants are not claimed to match SP 800-232** (their KATs were not compared with it). Not post-quantum —
see [`../README.md`](../README.md) for why it lives in this repo anyway and
why it's excluded from `test-all`/`gen-all`/`static-test-all`.

ASCON has no keygen: AEAD variants are a single encrypt/decrypt pair per
message, hash/XOF variants are a single `crypto_hash()` call. There is no
derand/coins indirection either — `crypto_aead_encrypt`/`crypto_aead_decrypt`
take key/nonce/associated-data/plaintext directly and are deterministic by
construction, so KAT generation needs no reseed/replay tricks (unlike CROSS,
LESS, HAWK, or HQC-2025).

## Variants

| Variant | Shape | Key | Nonce | Tag | Notes |
|---------|-------|-----|-------|-----|-------|
| `ascon128v12` | AEAD | 16 B | 16 B | 16 B | Ascon-128 v1.2 |
| `ascon128av12` | AEAD | 16 B | 16 B | 16 B | wider rate (`ASCON_AEAD_RATE=16`) |
| `ascon80pqv12` | AEAD | 20 B | 16 B | 16 B | 160-bit key variant |
| `asconhashv12` | Hash | — | — | — | 32-byte fixed-length digest |
| `asconhashav12` | Hash | — | — | — | 32-byte fixed-length digest, faster permutation rounds |
| `asconxofv12` | XOF | — | — | — | 32-byte output (fixed here; `crypto_hash_BYTES`) |
| `asconxofav12` | XOF | — | — | — | 32-byte output, faster permutation rounds |

Each variant is a `src/`+`include/` directory under
`algorithms/symmetric/ASCON/<variant>/`, same layout as every other family in
this repo.

**`test-ascon` covers all 7 variants above** (`test-ascon-aead` for the 3
AEAD variants, `test-ascon-hash` for the 4 hash/XOF variants).

## Per-family `make` targets

Prefix with `test-` or `gen-`: `ascon-aead`, `ascon-hash` (aggregate:
`test-ascon`/`gen-ascon`, **not** part of `test-all`/`gen-all`). Static
equivalents: `static-test-ascon-aead`, `static-test-ascon-hash`, aggregate
`static-test-ascon` (**not** part of `static-test-all`).

Per-variant targets use each variant's directory name directly, e.g.
`make test-ascon128v12`, `make test-asconhashv12`,
`make static-test-ascon80pqv12`.

## `.rsp` fields

AEAD (`ascon128v12`, `ascon128av12`, `ascon80pqv12`):

```
# <ALGO_NAME>

count = 0
seed = <48-byte hex>
key = <CRYPTO_KEYBYTES-byte hex>
nonce = <CRYPTO_NPUBBYTES-byte hex>
ad = <ADLEN-byte hex>
pt = <MLEN-byte hex>
ct = <(MLEN+CRYPTO_ABYTES)-byte hex>

count = 1
...
```

`seed` is only used to derive `key`/`nonce`/`ad`/`pt` from the NIST DRBG at
generation time (see [`../../../kat/README.md`](../../../docs/kat-format.md) for
the shared two-phase generate/test model) — it plays no role in the
algorithm itself, unlike DS/KEM families where `seed` feeds a derand API.
`ADLEN` defaults to 16 bytes (`#ifndef ADLEN #define ADLEN 16 #endif` in
`harness/gen/PQCgenKAT_aead_ascon.c`/`main_ascon_aead.c` — override at compile
time if you need a different fixed AD length for a batch); `MLEN` is the
usual top-level `Makefile` flag (default 33).

Hash/XOF (`asconhashv12`, `asconhashav12`, `asconxofv12`, `asconxofav12`):

```
# <ALGO_NAME>

count = 0
seed = <48-byte hex>
msg = <MLEN-byte hex>
md = <crypto_hash_BYTES-byte hex>

count = 1
...
```

## Algorithm-specific notes

### AEAD variants

`RUN_ENCAPS`/`RUN_DECAPS` (the same flags KEM families use) gate
encrypt/decrypt respectively — there's no keygen stage, so `RUN_KEYGEN` is
unused for ASCON. `nsec` (secret message number) is always `NULL`;
`CRYPTO_NSECBYTES` is 0 for every ASCON AEAD variant.

### Hash/XOF variants

Single stage — no `RUN_*` flag exists to disable the hash call (there'd be
nothing left to test). `asconxofv12`/`asconxofav12` are registered as
fixed-32-byte-output variants here (`crypto_hash_BYTES = 32` in every
variant's `api.h`), even though the underlying construction supports
arbitrary-length XOF output — this repo's KAT harness only exercises the
fixed-length call, matching the other three variants' interface.

## Static tests and profiling

`make static-test-ascon128v12 PROFILING=xheep` (or any
`tools/profiling_commands/<name>.txt`) bakes in cycle-counter hooks around
encrypt/decrypt (AEAD) or the single hash call (hash/XOF), using the exact
same generic four-section (`includes`/`init`/`before`/`after`) mechanism
every other family's static test uses — see
[`../../../tools/README.md`](../../../tools/README.md). This replaces the
old hand-written, xheep-hardware-specific `main.c`/`Makefile` that used to
live alongside each variant's sources; those aren't carried into this repo.

## Adding a new ASCON variant

1. Copy the algorithm sources into
   `algorithms/symmetric/ASCON/<variant>/{src,include}/`.
2. Open `../../../Makefile`, add the variant name to `ASCON_AEAD_VARIANTS`
   or `ASCON_HASH_VARIANTS` (whichever API shape it uses).
3. Run `make gen-<variant>` then `make test-<variant>` to verify.
4. To also support static tests, add a `variants` entry to
   `tools/algo_registry.json` under the `ascon-aead`/`ascon-hash` family
   (or a new family, if the variant uses a different API shape) — see
   `tools/README.md`, "Adding a new family".
