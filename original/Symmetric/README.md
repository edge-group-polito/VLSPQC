# Symmetric — non-PQC symmetric-key primitives

A category for symmetric-key primitives, parallel to
[`../KEM/`](../KEM/README.md) (key encapsulation) and [`../DS/`](../DS/README.md)
(digital signatures). Nothing here is post-quantum cryptography — these
algorithms are included because the reference sources were already
available, not because they belong in the PQC KAT suite's standardized
scope. Accordingly, **nothing under `Symmetric/` is part of
`test-all`/`gen-all`/`static-test-all`**; each family gets its own aggregate
`make` targets instead.

## Families

| Family | Notes |
|--------|-------|
| [ASCON](ASCON/README.md) | NIST-standardized (SP 800-232) lightweight AEAD + hash/XOF, formerly the NIST Lightweight Cryptography competition winner |

See [`ASCON/README.md`](ASCON/README.md) for variants, `.rsp` field formats,
and `make` targets.
