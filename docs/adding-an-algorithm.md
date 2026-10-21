# Adding an algorithm, a variant or a family

A checklist distilled from the FIPS migration (ML-KEM, ML-DSA, SLH-DSA, FN-DSA) and the earlier on-ramp ports. Layout and
vocabulary: [`../algorithms/README.md`](../algorithms/README.md); harness: [`../harness/README.md`](../harness/README.md).

## 1. Pick the category

| If it is… | Put it in | In `test-all`? |
|---|---|---|
| a final FIPS (203/204/205) or a selected-but-pending standard (Falcon/FN-DSA, HQC) | `algorithms/kem/` or `algorithms/signatures/standard/` | yes for the final FIPS and the pending-standard families already there; a new pending family is a decision to make explicitly |
| experimental / pre-standard code of a standard-track scheme (like `FNDSA-EXP`) | next to its family, own `make` targets | **no** |
| a NIST additional-signature candidate | `algorithms/signatures/onramp/` (`round2-only/` once it is out of the competition) | no — `test-onramp-ds` |
| NIST SP 800-208 stateful hash-based signature | `algorithms/signatures/stateful/` | no — `test-stateful-ds` |
| not PQC (e.g. ASCON) | `algorithms/symmetric/` | no |
| superseded or non-standard code kept for comparison | `algorithms/legacy/` | no |

## 2. Import with provenance

1. Take the sources from a **pinned release tag or commit**, never from a moving branch head. If the local clone is past the
   tag, export the tag (`git archive <tag>`) instead of copying the working tree.
2. Keep only portable C: no assembly, no architecture backends (AVX2, NEON, RISC-V vector), no CBMC/HOL-Light proof
   directories, CI files or build systems. Confirm that no architecture-specific code is compiled (the repository
   targets RISC-V bare metal as well as the host).
3. Keep the upstream license file next to the code.
4. Add an `UPSTREAM.md` in the family folder. Use the existing ones as templates; it must state: upstream URL, tag, full commit
   hash, import date, the specification (number and date) or the status if it is not final, what was removed, every local
   change as a diff summary, and how the import was verified.
5. Do not label pre-standard code with a standard's name. Say what it is ("Falcon (round 3, basis of the upcoming FN-DSA)").

## 3. Fit the harness without touching the algorithm

Look at the algorithm's real API, then choose the registry *shape* whose generator and runner already match it
(`kem_derand`, `kem_prng_reseed`, `ds_derand_ctx`, `ds_derand_rnd`, `ds_derand_rnd_nonce`, `ds_derand_seed`, `ds_drbg_*`,
`ds_drbg_official`, `aead_direct`, `hash_direct`; see [`../harness/README.md`](../harness/README.md)). When the API differs,
add a small per-variant compatibility header (`include/api.h`, `kem.h`, `sign.h`) that maps the harness names onto the
algorithm's functions, as the ML-KEM, ML-DSA, SLH-DSA and FN-DSA families do. Only write a new generator/runner pair when no
existing shape fits. Route all randomness through the harness NIST DRBG (or explicit inputs) so the KATs are deterministic.

## 4. Wire it into the build

1. **Makefile**: add a `define <FAMILY>_RULES` macro and a variant list following the nearest existing family; keep the
   `gen-*`/`test-*` per-variant targets and the family aggregate. Add a directory entry to `DIRS` for the new `vectors/rsp/`
   subdirectory. Add the family to the right aggregate (`test-all` only for the standardized set).
2. **`tools/algo_registry.json`**: one `families` entry and one `variants` entry per variant (`src_dir`, `include_dir`,
   `rsp_path`, `gen_target`, `vec_suffix`, `main_headers`). Optional keys: `copy_subdirs` (sources with nested includes),
   `static_gcc_flags` (extra flags the flat static folder needs), `extra_dirs`, `size_t_api`.
3. **`tools/run_all_static_kat.sh`**: add the family to `FAMILIES` and to `STANDARD_FAMILIES` or `EXTRA_FAMILIES`.
4. For static tests of a new shape, see "Adding a new family" in [`static-tests.md`](static-tests.md).

## 5. Verify, honestly

- `make gen-<variant>` then `make test-<variant>`; then `make static-test-<variant> STATIC_KATNUM=1` and compile and run the
  generated folder with plain `gcc`.
- Cross-check against an **independent** reference, not only against the harness' own round trip: the upstream's published
  KAT hashes or `.rsp` files, NIST ACVP vectors, or the pristine upstream API replaying the harness' `.rsp` records. Record the
  commands and outcome in the family's `UPSTREAM.md`. If no independent reference exists, say so there.
- Never lower `KATNUM` to make something pass.

## 6. Document and publish the numbers

- Add the family to its category README (scope, spec status, parameters, upstream pin, license, local patches, `make`
  targets, `.rsp` fields, limitations) and to the coverage table in the root README.
- Size tables are generated: put the `<!-- BEGIN sizes:… -->` / `<!-- END sizes:… -->` markers where the table belongs
  and run `make results-update` (see [`../tools/README.md`](../tools/README.md)). Do not type sizes by hand.
- Benchmark numbers: add the variants to `tools/bench/variants_fips.json` (or `variants_l1.json`), run `make bench-fips` on an
  idle machine, then `make results-update`. A number that was not measured is written as `TODO (not run)`.
- Run `python3 -I tools/check_links.py` after editing any Markdown file.
