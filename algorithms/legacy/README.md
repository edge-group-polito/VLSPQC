# Legacy (pre-standard) code, kept for comparison

Everything here is **not part of any FIPS standard**, is **not in `test-all`**, and (except the Haraka
families) is **not wired into the Makefile**. It is kept so the pre-FIPS reference code stays available
next to its FIPS replacement (for example to compare KATs or to reproduce older results). Git history
before the FIPS migration is also reachable through the tag `pre-fips-migration`.

| Folder | What it is | Replaced by | Build targets |
|---|---|---|---|
| `Kyber-round3/ml-kem-{512,768,1024}` | CRYSTALS-Kyber round-3 reference (`pqcrystals_kyber*_ref`), previously labelled "ML-KEM" | `../kem/ML-KEM` (mlkem-native, FIPS 203) | none |
| `Dilithium-round3/ML-DSA-{2,3,5}` | CRYSTALS-Dilithium round-3 reference, previously labelled "ML-DSA" (`ML-DSA-2/3/5` are the Dilithium 2/3/5 parameter sets, the round-3 counterparts of ML-DSA-44/65/87 in security level; the sizes and KATs differ from FIPS 204) | `../signatures/standard/ML-DSA` (mldsa-native, FIPS 204) | none |
| `SPHINCSplus-round3-SHAKE/` | SPHINCS+ SHAKE reference, previously labelled "SLH-DSA" (`simple` and `robust`; the `s` sizes used an early Saarinen FIPS 205 adapter without a context string) | `../signatures/standard/SLH-DSA` (slhdsa-c, FIPS 205) | none |
| `SPHINCSplus-round3-SHA2/` | same, SHA-2 instantiation | same | none |
| `SLH-DSA-Haraka/` | SPHINCS+ with Haraka (`simple`/`robust`, `f` sizes). Haraka is **not** in FIPS 205 | none (no standard) | `make test-slhdsa-haraka`, `gen-slhdsa-haraka` |
| `SLH-DSA-Haraka-AES/` | SPHINCS+ with Haraka-AES and RISC-V `aes64esm` accelerated paths. **Not** in FIPS 205 | none | `make test-slhdsa-haraka-aes` (RISC-V host only, skipped elsewhere) |

## Sizes of the Haraka variants (generated)

<!-- BEGIN sizes:legacy -->
| Variant | pk (B) | sk (B) | ct / sig (B) |
|---|---:|---:|---:|
| HARAKAp128fsimple | 32 | 64 | 17088 |
| HARAKAp128frobust | 32 | 64 | 17088 |
| HARAKAp192fsimple | 48 | 96 | 35664 |
| HARAKAp192frobust | 48 | 96 | 35664 |
| HARAKAp256fsimple | 64 | 128 | 49856 |
| HARAKAp256frobust | 64 | 128 | 49856 |
| HARAKAAESp128fsimple | 32 | 64 | 17088 |
| HARAKAAESp128frobust | 32 | 64 | 17088 |
| HARAKAAESp192fsimple | 48 | 96 | 35664 |
| HARAKAAESp192frobust | 48 | 96 | 35664 |
| HARAKAAESp256fsimple | 64 | 128 | 49856 |
| HARAKAAESp256frobust | 64 | 128 | 49856 |
<!-- END sizes:legacy -->

## Notes
- The Kyber, Dilithium and SPHINCS+ round-3 trees are verbatim copies of the files at the tag
  `pre-fips-migration` (`original/KEM/ML-KEM`, `original/DS/ML-DSA`, `original/DS/SLH-DSA`,
  `original/DS/SLH-DSA-SHA2`); only their location changed. Their KATs are **not compatible** with FIPS
  203/204/205 (different hashing and domain separation, no ML-DSA/SLH-DSA context string).
- To build one of the unwired trees, use the Makefile rules of that tag (`git show
  pre-fips-migration:Makefile`); the `main.c` and `test_vectors_*.h` files in each folder belong to the old
  hand-written bare-metal tests.
- Haraka and Haraka-AES results and their static tests are legacy material: they are no longer part of the
  "standardized" set (`test-all`, `static-test-ds`, `tools/run_all_static_kat.sh` default expansion); they
  run only under their own targets and as extra families in `run_all_static_kat.sh`.
