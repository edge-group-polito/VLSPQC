# PQC KAT Test Suite

A structured, configurable Known Answer Test (KAT) framework for post-quantum
cryptography algorithms. Covers digital signature (DS) and key encapsulation
mechanism (KEM) families, with `.rsp` file-based testing, a static
(no-file-I/O) test generator for bare-metal targets, and a single top-level
Makefile driving all of it.

## Supported algorithms

This repository currently supports:

- **ML-KEM**    [Kyber](https://github.com/pq-crystals/kyber)  
This repository contains the official reference implementation of the [Kyber](https://www.pq-crystals.org/kyber/) key encapsulation mechanism.
Kyber has been selected for standardization in [round 3](https://csrc.nist.gov/Projects/post-quantum-cryptography/round-3-submissions)
of the [NIST PQC](https://csrc.nist.gov/projects/post-quantum-cryptography) standardization project. It is standardized as [FIPS 203](https://csrc.nist.gov/pubs/fips/203/final).

- **ML-DSA**    [Dilithium](https://github.com/pq-crystals/dilithium)  
This repository contains the official reference implementation of the [Dilithium](https://www.pq-crystals.org/dilithium/) signature scheme.
Dilithium is standardized as [FIPS 204](https://csrc.nist.gov/pubs/fips/204/final).

- **HQC**       [HQC](https://gitlab.com/pqc-hqc/hqc/)  
This repository provides the official implementation of HQC-2025 (levels 1, 3, 5), a code-based Key Encapsulation Mechanism (KEM) whose security is based on the hardness of solving the Quasi-Cylic Syndrome Decoding (QCSD) problem. HQC is one of the selected algorithms from the [NIST's Post-Quantum Cryptography Standardization Project](https://csrc.nist.gov/projects/post-quantum-cryptography).

- **Falcon**    [Falcon](https://falcon-sign.info/)  
This repository contains the software that accompanies the [FN-DSA submission](https://falcon-sign.info/) to [NIST's Post-Quantum Cryptography](https://csrc.nist.gov/Projects/Post-Quantum-Cryptography) project.

- **SPHINCS+**  
This repository contains the software that accompanies the [SPHINCS+ submission](https://sphincs.org/) to [NIST's Post-Quantum Cryptography](https://csrc.nist.gov/Projects/Post-Quantum-Cryptography) project.
It includes SLH-DSA/SPHINCS+ SHAKE, SHA2, Haraka, and Haraka-AES variants. Some SHA2 and Haraka-AES variants include RISC-V instruction accelerated paths and require a compatible RISC-V toolchain to build those accelerated versions. SLH-DSA is standardized as [FIPS 205](https://csrc.nist.gov/pubs/fips/205/final).

- **CROSS**    [CROSS](https://www.cross-crypto.com/)  
This repository contains the reference implementation of [CROSS](https://www.cross-crypto.com/) (Codes and Restricted Objects Signature Scheme), a code-based signature scheme whose security relies on the hardness of the Restricted Syndrome Decoding Problem. CROSS advanced to [round 2](https://csrc.nist.gov/projects/pqc-dig-sig/round-2-additional-signatures) of [NIST's Additional Digital Signature Schemes](https://csrc.nist.gov/projects/pqc-dig-sig) standardization process.

- **HAWK**    [Hawk](https://hawk-sign.info/)  
This repository contains the reference implementation of [HAWK](https://hawk-sign.info/), a lattice-based, hash-then-sign signature scheme based on the Lattice Isomorphism Problem. HAWK was selected by NIST to move forward to the [third round](https://csrc.nist.gov/projects/pqc-dig-sig) of the Additional Digital Signature Schemes standardization process.

- **LESS**    [LESS](https://www.less-project.com/)  
This repository contains the reference implementation of [LESS](https://www.less-project.com/) (Linear Equivalence Signature Scheme), a code-based signature scheme whose security relies on the hardness of the Linear Code Equivalence problem. LESS was submitted to [NIST's Additional Digital Signature Schemes](https://csrc.nist.gov/projects/pqc-dig-sig) standardization process.

- **FAEST**    [FAEST](https://faest.info/)  
This repository contains a port of the [FAEST](https://faest.info/) reference implementation, a signature scheme built on the VOLE-in-the-head zero-knowledge proof paradigm with security based on AES and SHA-3. FAEST was selected by NIST to move forward to the [third round](https://csrc.nist.gov/projects/pqc-dig-sig) of the Additional Digital Signature Schemes standardization process.

- **XMSS / LMS / HSS**  
This repository contains implementations of the stateful hash-based signature schemes [XMSS](https://datatracker.ietf.org/doc/html/rfc8391) (RFC 8391) and [LMS/HSS](https://datatracker.ietf.org/doc/html/rfc8554) (RFC 8554), both approved by NIST in [SP 800-208](https://csrc.nist.gov/projects/stateful-hash-based-signatures) for stateful hash-based signatures.

- **ASCON**    [Ascon](https://ascon.isec.tugraz.at/)  
This repository contains an implementation of [Ascon](https://ascon.isec.tugraz.at/), NIST's selected algorithm family for lightweight cryptography, covering authenticated encryption, hashing, and extendable-output functions. Ascon is standardized as [NIST SP 800-232](https://csrc.nist.gov/pubs/sp/800/232/final). It is not a post-quantum algorithm, but is included here alongside the PQC families.

Algorithms are organized by standardization status:

| Category | What's there | Part of `test-all`? |
|----------|--------------|----------------------|
| `original/DS/` | Standardized digital signatures: ML-DSA, SLH-DSA ×4, FALCON | Yes |
| `original/KEM/` | Standardized KEMs: ML-KEM, HQC (HQC-2025 only) | Yes |
| `original/Additional-DS/` | Non-standardized candidate signatures: CROSS, HAWK, LESS, FAEST | No — `test-additional-ds` |
| `original/Stateful-DS/` | NIST SP 800-208 stateful hash-based signatures: XMSS, LMS, HSS | No — `test-stateful-ds` |
| `original/Symmetric/` | ASCON (lightweight AEAD + hash/XOF) — not PQC at all | No — `test-ascon` |

**`test-all`/`gen-all`/`static-test-all` cover only the standardized set: 41
variants** across ML-KEM, ML-DSA, SLH-DSA ×4 sub-families (including "s"
sizes for 2 of those), FALCON, and HQC-2025. Everything else has its
own aggregate `make` targets (`test-additional-ds`, `test-stateful-ds`,
`test-ascon`) and is excluded from `test-all` by design — see the category
READMEs linked below. XMSS, LMS, and HSS are additionally
runtime-parametrized — their full parameter spaces (77 + 20 + 40 = 137 more
sets) are exercised via dedicated `-check`/`-check-slow` sweep targets
instead, since a lot of that space is either huge (LMS/HSS'
`(height, w, levels)` combinations) or individually slow (tall single-tree
tree heights) — see [`original/Stateful-DS/README.md`](original/Stateful-DS/README.md).

---

## Documentation map

This README covers the top-level build (Quick Start, Makefile flags, adding a
variant/family). Everything algorithm- or tool-specific lives in the relevant
subfolder's own README:

| Where | What's there |
|-------|--------------|
| [`original/DS/README.md`](original/DS/README.md) | Standardized DS families/variants, per-family `make` targets, `.rsp` fields, algorithm notes (FALCON/ML-DSA/SLH-DSA) |
| [`original/Additional-DS/README.md`](original/Additional-DS/README.md) | Non-standardized DS families (CROSS/HAWK/LESS/FAEST) |
| [`original/Stateful-DS/README.md`](original/Stateful-DS/README.md) | Stateful hash-based signatures (XMSS/LMS/HSS, NIST SP 800-208) |
| [`original/KEM/README.md`](original/KEM/README.md) | KEM families/variants, per-family `make` targets, KEM-specific `.rsp` fields, algorithm notes (ML-KEM/HQC) |
| [`original/Symmetric/ASCON/README.md`](original/Symmetric/ASCON/README.md) | ASCON AEAD + hash/XOF variants, `.rsp` fields, `make` targets |
| [`kat/README.md`](kat/README.md) | The generate/test two-phase model, `.rsp` file format, `kat/gen`/`kat/rsp`/`kat/static` layout |
| [`common/README.md`](common/README.md) | The shared `.rsp` parser (`kat_parser.h/.c`) used by every test runner |
| [`tools/README.md`](tools/README.md) | The static-test generator (`gen_static_test.py`) for bare-metal/FPGA/RISC-V targets, cycle-counter profiling, and the batch runner (`run_all_static_kat.sh`, with its `--benchmark-all` flag) |

## Repository layout

```
pqrv/
├── Makefile              ← top-level build system
├── common/                see common/README.md
├── kat/                    see kat/README.md
│   ├── gen/                generators + test runners
│   ├── rsp/                generated .rsp files
│   └── static/              generated static-test folders
├── original/               reference-implementation sources
│   ├── DS/                 standardized -- see original/DS/README.md
│   │   └── <FAMILY>/<VARIANT>/
│   ├── Additional-DS/      not standardized -- see original/Additional-DS/README.md
│   │   └── <FAMILY>/<VARIANT>/
│   ├── Stateful-DS/        NIST SP 800-208 -- see original/Stateful-DS/README.md
│   │   └── XMSS, LMS, HSS/  (one shared source tree each — no
│   │                          per-variant split, see their READMEs)
│   ├── KEM/                see original/KEM/README.md
│   │   └── <FAMILY>/<VARIANT>/
│   └── Symmetric/          not PQC -- see original/Symmetric/README.md
│       └── ASCON/<VARIANT>/
└── tools/                  see tools/README.md
    ├── gen_static_test.py + algo_registry.json
    ├── run_all_static_kat.sh
    └── profiling_commands/
```

---

## Quick start

```bash
# Build and run the standardized set (41 variants, 100 vectors each)
make test-all

# Just generate all .rsp files
make gen-all

# Test a single family or variant (see the category READMEs above for names)
make test-mlkem
make test-ml-kem-512

# Non-standardized / separate-category algorithms -- NOT part of test-all
make test-additional-ds   # CROSS, HAWK, LESS, FAEST
make test-stateful-ds     # XMSS, LMS, HSS (default variant each)
make test-ascon           # ASCON AEAD + hash/XOF (not PQC)

# XMSS/LMS/HSS: sweep their full (runtime-selected) parameter spaces —
# not part of test-all/test-stateful-ds, since these can take minutes to
# hours; see original/Stateful-DS/README.md and each family's own README.
make test-xmss-check
make test-lms-check-slow

# Static (no file-I/O) test for bare-metal targets — see tools/README.md
make static-test-all              # standardized set only
make static-test-additional-ds
make static-test-stateful-ds
make static-test-ascon

# Batch-run + log every static test's pass/fail — see tools/README.md
tools/run_all_static_kat.sh                  # standardized set only
tools/run_all_static_kat.sh --benchmark-all  # + one representative of each extra family

# Clean up
make clean          # build/ only
make clean-rsp       # kat/rsp/ only
make clean-static    # kat/static/ only
make distclean       # all of the above
```

## Makefile flags

All flags can be set on the command line and override the defaults.

| Flag | Default | Description |
|------|---------|--------------|
| `KATNUM` | `100` | Number of KAT vectors to generate/test (dynamic tests) |
| `CHECK_KATNUM` | `1` | Vectors per variant for XMSS/LMS/HSS's `-check`/`-check-slow` sweeps |
| `RUN_KEYGEN` | `1` | Test key generation |
| `RUN_SIGN` / `RUN_VERIFY` | `1` | Test signing/verification — DS only |
| `RUN_ENCAPS` / `RUN_DECAPS` | `1` | Test encapsulation/decapsulation — KEM only, and ASCON's encrypt/decrypt |
| `DEBUG` | `0` | Print hex of inputs/outputs on failure |
| `MLEN` | `33` | Message length (bytes) used during generation |
| `CC` / `CFLAGS` | `gcc` / `-O2 -std=c11 ...` | Compiler / flags |
| `STATIC_KATNUM` | `1` | Vectors baked into a static test (see `tools/README.md`) |
| `STATICDIR` | `kat/static` | Output dir for static tests |
| `PROFILING` | `none` | Cycle-counter profile to bake in (see `tools/README.md`) |

```bash
# Run only 10 vectors, verbose output on failure
make test-CROSS_1_RSDP_b KATNUM=10 DEBUG=1

# Test sign+verify only, skip keygen (uses stored keys from .rsp)
make test-hawk-512 RUN_KEYGEN=0 RUN_SIGN=1 RUN_VERIFY=1

# Cross-compile
make test-all CC=arm-linux-gnueabihf-gcc CFLAGS="-O2 -std=c11"
```

> **Note:** `KATNUM` must match between generation and testing — the test
> binary stops after reading `KATNUM` entries from the `.rsp` file.

`make help` prints a target summary; the category READMEs linked above list
every per-family and per-variant target name.

---

## Adding a new algorithm

- **New variant of an existing family:** see "Adding a new DS variant" in
  [`original/DS/README.md`](original/DS/README.md) (or the
  `Additional-DS`/`Stateful-DS` equivalents), "Adding a new KEM variant" in
  [`original/KEM/README.md`](original/KEM/README.md), or "Adding a new ASCON
  variant" in [`original/Symmetric/ASCON/README.md`](original/Symmetric/ASCON/README.md).
- **New family entirely:**
  1. Write a `kat/gen/PQCgenKAT_*` generator (see existing ones as templates).
  2. Write a `kat/gen/main_*.c` test runner.
  3. Add a `define` macro and variant list in `Makefile` following the
     existing patterns. Decide which category it belongs in (standardized
     `DS`/`KEM`, `Additional-DS`, `Stateful-DS`, or `Symmetric`) and whether
     it should join `test-all`/`gen-all` (standardized only) or get its own
     aggregate target instead.
  4. To also support static tests for it, see "Adding a new family" in
     [`tools/README.md`](tools/README.md).


## 📄 License

This repository follows the licensing terms of the respective reference implementations used as the starting point. Please check individual algorithm directories for specific license details.

## 👥 Authors

- Alessandra Dolmeta    - alessandra.dolmeta@polito.it
- Valeria Piscopo       - valeria.piscopo@polito.it

In collaboration with: Behnam Farnaghinejad (behnam.farnaghinejad@polito.it, DAUIN, Politecnico di Torino)
