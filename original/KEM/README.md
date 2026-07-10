# Key Encapsulation Mechanism (KEM) algorithms

Reference-implementation sources for all KEM families covered by the KAT
suite. Each family directory holds one subdirectory per parameter-set/version
variant (`src/`/`include/` split). These sources are compiled together with
`kat/gen/PQCgenKAT_kem_*.c` (generation) or `kat/gen/main_*.c` (testing) — see
[`../../kat/README.md`](../../kat/README.md) for how that build works, and
[`../../README.md`](../../README.md) for the `make` targets that drive it.

## Families

| Family | Variants | Security levels | Notes |
|--------|----------|------------------|-------|
| ML-KEM | 3 (512, 768, 1024) | 128, 192, 256 bit | derand API |
| HQC | 3 (HQC-2025 only: levels 1, 3, 5) | 128, 192, 256 bit | internal-PRNG-reseed API |

**Total: 6 KEM variants.**

## Per-family `make` targets

Prefix with `test-` or `gen-`: `mlkem` (ml-kem-512/768/1024), `hqc` (all 3 HQC
variants at once).

Per-variant targets are `test-<NAME>` / `gen-<NAME>`. ML-KEM uses its
directory name directly (`ml-kem-512`, `ml-kem-768`, `ml-kem-1024`). HQC uses
sanitized names encoding the security level:

| Safe target name | Algorithm |
|-----------------|-----------|
| `HQC2025_1` / `HQC2025_3` / `HQC2025_5` | HQC-2025 / HQC-1, 3, 5 |

Example: `make test-ml-kem-512`, `make test-HQC2025_1`.

## `.rsp` fields specific to KEM

All KEM `.rsp` files carry the standard `count`/`seed`/`pk`/`sk`/`ct`/`ss`
fields (see [`../../kat/README.md`](../../kat/README.md) for the full format), plus:

| Algorithm | Extra fields |
|-----------|--------------|
| ML-KEM | `keypair_coins` (64 B), `enc_coins` (32 B) |
| HQC | none — `seed` is the only randomness field (see below) |

## Algorithm-specific notes

### ML-KEM

Derand API: `crypto_kem_keypair_derand(pk, sk, coins)` with 64 bytes, and
`crypto_kem_enc_derand(ct, ss, pk, coins)` with 32 bytes. `src/randombytes.c`
is excluded; `kat/gen/rng.c` is included instead to satisfy the linker symbol
used by the non-derand wrappers in `kem.c`.

### HQC

No explicit-seed/derand entry point. HQC-2025 seeds its own internal
SHAKE-256 PRNG via `prng_init(seed, NULL, 48, 0)` and then calls the
*ordinary* `crypto_kem_keypair(pk, sk)`/`crypto_kem_enc(ct, ss, pk)` API — the
48-byte `seed` field is all the `.rsp` needs to reproduce a run
deterministically. **`RUN_KEYGEN=0` desync caveat:** since there's no
explicit-randomness API, skipping keygen doesn't advance the PRNG the way
generation did, so `crypto_kem_enc` starts from the wrong PRNG offset and
reports a mismatch even though the algorithm itself is correct — see
[`../../tools/README.md`](../../tools/README.md), "The RUN_KEYGEN=0 desync
caveat".

## Adding a new KEM variant

1. Copy the algorithm sources into `original/KEM/<FAMILY>/<VARIANT>/src/` and
   `original/KEM/<FAMILY>/<VARIANT>/include/`.
2. Open `../../Makefile`, locate the `<FAMILY>_VARIANTS` list, and add the
   variant name.
3. Run `make gen-<VARIANT>` then `make test-<VARIANT>` to verify.

To add a new KEM family entirely, see "Adding a new algorithm family" in
[`../../README.md`](../../README.md).
