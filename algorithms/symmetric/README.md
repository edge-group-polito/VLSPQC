# Symmetric — non-PQC symmetric-key primitives

Scope: symmetric-key primitives, parallel to [`../kem/`](../kem/README.md) (key encapsulation) and [`../signatures/`](../signatures/standard/README.md) (signatures).
Nothing here is post-quantum cryptography: the sources are kept because a reference implementation was already available for the hardware/software co-design
work, not because ASCON belongs to the PQC standardization scope. Accordingly **nothing under `symmetric/` is part of `test-all` / `gen-all` / `static-test-all`**;
the family has its own aggregate targets.

## Families

| Family | Standard status (NIST, checked 2026-10-20) | What this repository contains | Variants |
|---|---|---|---|
| [ASCON](ASCON/README.md) | SP 800-232, **final** (August 2025), defines Ascon-AEAD128, Ascon-Hash256, Ascon-XOF128, Ascon-CXOF128 | **Ascon v1.2** (competition submission, `*v12` variants); **not claimed to match SP 800-232** | 7 (3 AEAD, 4 hash/XOF) |

- **Upstream, pin, license:** [ascon-c](https://github.com/ascon/ascon-c); the closest upstream state is tag `v1.2.5` (22 of 25 `.c` / `.h` files byte-identical, which fits the `v12` names); CC0 1.0 ([`ASCON/LICENSE`](ASCON/LICENSE)). Details in [`ASCON/UPSTREAM.md`](ASCON/UPSTREAM.md); the version was not recorded when the code was added.
- **Local patches:** none documented.

## `make` targets

Prefix with `test-` or `gen-`: `ascon-aead`, `ascon-hash` (aggregate `test-ascon` / `gen-ascon`). Static tests: `static-test-ascon-aead`, `static-test-ascon-hash`, `static-test-ascon`.
Variants: `ascon128v12`, `ascon128av12`, `ascon80pqv12`, `asconhashv12`, `asconhashav12`, `asconxofv12`, `asconxofav12` (`make test-ascon128v12`).

## `.rsp` fields

ASCON has no key generation, so its `.rsp` files do not follow the signature / KEM pattern: AEAD variants carry `key`, `nonce`, `ad`, `pt`, `ct`; hash / XOF variants carry `msg`, `md`
(see [`../../docs/kat-format.md`](../../docs/kat-format.md) and [`ASCON/README.md`](ASCON/README.md)).

## Known limitations

- The code is Ascon v1.2, not the SP 800-232 functions; no comparison with SP 800-232 test vectors was made.
- The vendored version was not recorded; `v1.2.5` is the closest match found.
- Not part of the FIPS migration: it was not changed, and only rerun (`test-ascon`, `KATNUM=10`) after the repository restructure.
