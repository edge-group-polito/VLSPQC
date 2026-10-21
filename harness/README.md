# `harness/` — KAT parser, generators and test runners

The framework code shared by every algorithm family. It never contains algorithm code; each family under
[`../algorithms/`](../algorithms/README.md) is compiled *together with* the files here.

```
harness/
├── kat_parser.h / kat_parser.c   dependency-free reader/writer for NIST-style .rsp files
└── gen/
    ├── rng.c / rng.h             NIST AES-256-CTR DRBG (deterministic seed expansion; randombytes_init/randombytes)
    ├── PQCgenKAT_<kind>_<family>.c   generators: run keygen → sign/encaps → verify/decaps, write the .rsp
    └── main_<family>.c           test runners: read the .rsp, replay every operation, compare byte for byte
```

The two-phase generate/test model and the `.rsp` format are described in [`../docs/kat-format.md`](../docs/kat-format.md);
the Makefile rules that compile these files with each variant are in [`../Makefile`](../Makefile).

## `kat_parser`

- Reads one blank-line-separated block at a time into a `kat_entry_t` (up to `KAT_MAX_FIELDS` fields, each a hex-decoded
  byte string or an integer).
- `kat_entry_get()` / `kat_get_bytes()` / `kat_get_long()` look a field up by name (`"pk"`, `"sign_rnd"`, `"enc_coins"`, …)
  regardless of which algorithm wrote it, which is what lets one parser serve every signature and KEM family.
- `KAT_DATA_MAX` (4 MB per field) sizes the largest field seen across all variants; the header notes that it covers the
  classic UOV-V expanded public key (about 2.87 MB).
- `kat_fprint_comment/int/hex()` are the write-side helpers the generators use to produce `.rsp` files in the same format.

## Generators and runners by API shape

The shape names are the ones used in [`../tools/algo_registry.json`](../tools/algo_registry.json); a "shape" is the way a family
exposes randomness to the harness.

| Shape | Generator / runner | Families |
|---|---|---|
| `kem_derand` | `PQCgenKAT_kem_mlkem.c` / `main_mlkem.c` | ML-KEM |
| `kem_prng_reseed` | `PQCgenKAT_kem_hqc2025.c` / `main_hqc2025.c` | HQC |
| `ds_derand_ctx` | `PQCgenKAT_sign_mldsa.c` / `main_mldsa.c` | ML-DSA |
| `ds_derand_rnd` | `PQCgenKAT_sign_slhdsa.c` / `main_slhdsa.c` | SLH-DSA (FIPS 205), legacy Haraka SPHINCS+ |
| `ds_derand_rnd_nonce` | `PQCgenKAT_sign_falcon.c` / `main_falcon.c` | Falcon, experimental FN-DSA |
| `ds_derand_seed` | `PQCgenKAT_sign_{xmss,lms,hss}.c` / `main_{xmss,lms,hss}.c` | XMSS, LMS, HSS |
| `ds_drbg_*`, `ds_drbg_official` | `PQCgenKAT_sign_<family>[_r3].c` / `main_<family>[_r3].c` | the on-ramp candidates (CROSS, LESS, HAWK, FAEST, MAYO, UOV, QR-UOV, SNOVA, SDitH, SQIsign, MQOM and their Round-3 ports) |
| `aead_direct`, `hash_direct` | `PQCgenKAT_{aead,hash}_ascon.c` / `main_ascon_{aead,hash}.c` | ASCON |

For FIPS-final and FN-DSA code the algorithm's own API is wrapped by a small per-variant compatibility header
(`include/api.h`, `kem.h`, `sign.h`, …) so these generators and runners stay unchanged; each family's `UPSTREAM.md`
documents the mapping.
