# Falcon (round 3) — upstream provenance

Label: **Falcon (round 3, basis of the upcoming FN-DSA / FIPS 206)**. This code is *not* FN-DSA:
FIPS 206 was not a final standard when this was written (status to be re-checked on
https://csrc.nist.gov before relying on it), and Falcon and FN-DSA are not wire-compatible, so the KATs
produced here will not match FN-DSA once it is published.

| Item | Value |
|---|---|
| Upstream | Falcon project, round-3 submission package https://falcon-sign.info/falcon-round3.zip |
| Package version | `falcon-round3` (files dated 2020-10, NIST PQC round 3) |
| Package SHA-256 | `d625407dbda9e5835f610aaeba1147e029988a6610e0107dfd292033138e1d47` (downloaded 2026-10-20) |
| Directory in package | `Reference_Implementation/falcon512/falcon512int/` and `Reference_Implementation/falcon1024/falcon1024int/` |
| Version control | none (zip package, no git commit) |
| License | MIT (headers in the sources, "Copyright (c) 2017-2019 Falcon Project") |
| Variants | `falcon-512`, `falcon-1024` |

## Comparison with the package (line-level `diff`)
Identical to the package: `codec.c`, `common.c`, `fft.c`, `fpr.c`, `keygen.c`, `rng.c`, `sign.c`, `inner.h`,
`fpr.h` (both sizes); `vrfy.c` for 1024.

Local changes:
- `src/nist.c`: `crypto_sign_keypair` takes the 48-byte seed as `keypair_rnd`, and `crypto_sign` takes
  the 40-byte `nonce` and the 48-byte `sign_seed` as arguments instead of calling `randombytes()`
  (explicit-randomness API used by the KAT harness); `NONCELEN` replaces `sizeof nonce`.
- `src/shake.c`: `#include <stdio.h>`, an unused `call_count` global and commented-out debug counters.
- `src/vrfy.c` (512): `#include <stdio.h>` / `<inttypes.h>` and commented-out debug prints.
- `include/api.h`: adapted to the explicit-randomness signatures.
- `include/test_vectors_*.h`, `main.c`: repository test code, not upstream.

## Notes
- The optional integer-only implementation by Thomas Pornin (https://github.com/pornin/c-fn-dsa, a
  pre-standard FN-DSA implementation) is NOT imported; it would be a separate experimental variant
  outside `test-all` (decision pending).

## Verification against the official KAT
The package's own KAT files (`KAT/falcon512-KAT.rsp`, `KAT/falcon1024-KAT.rsp`, 100 records each) are kept in
[`official-kat/`](official-kat/README.md) and replayed by `make test-falcon-512-official` / `make test-falcon-1024-official`
(part of `make test-falcon` and `make test-all`; runner `harness/gen/main_falcon_official.c`). Each record's `seed` initializes the
NIST DRBG, which supplies the key-generation seed, the nonce and the signing seed in the order the original generator drew them; `pk`, `sk`,
`smlen` and `sm` are compared byte for byte and the signature is verified. Result (2026-10-21): **100/100 records for both sizes, 0 mismatches**
(keygen, sign and verify). A scratch run with one corrupted byte in one official record fails as it must, so the test is not vacuous.
