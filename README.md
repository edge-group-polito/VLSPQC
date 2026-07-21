# VLSPQC

**VLSPQC** (VLSI-Lab Post-Quantum Cryptography) is a collection of clean, hardware-oriented C implementations of the main post-quantum algorithms that have either **passed** or are **still ongoing** in the NIST standardization process.  

The repository is designed to provide **runnable code on FPGA platforms** while remaining compliant with the **Known Answer Test (KAT)** format defined in the original NIST submissions.

---

## 📁 Repository Structure

| Folder | Description |
|--------|-------------|
| [`original/`](original/) | Original, unmodified reference implementations of PQC algorithms |
| [`profiling/`](profiling/) | Instrumented implementations for function call profiling and performance analysis. See [`profiling/pqc_profiling_analysis.md`](profiling/pqc_profiling_analysis.md) for comprehensive profiling results |
| [`masked/`](masked/) | Masked implementations for side-channel attack resistance (includes ASCON variants) |
| [`other/`](other/) | Auxiliary tests and experiments used during hardware exploration analysis |

---

## 📦 Included Algorithms

This repository currently supports:

- **ML-KEM**    [Kyber](https://github.com/pq-crystals/kyber)  
This repository contains the official reference implementation of the [Kyber](https://www.pq-crystals.org/kyber/) key encapsulation mechanism.
Kyber has been selected for standardization in [round 3](https://csrc.nist.gov/Projects/post-quantum-cryptography/round-3-submissions) 
of the [NIST PQC](https://csrc.nist.gov/projects/post-quantum-cryptography) standardization project.


- **ML-DSA**    [Dilithium](https://github.com/pq-crystals/dilithium)  
This repository contains the official reference implementation of the [Dilithium](https://www.pq-crystals.org/dilithium/) signature scheme.
Dilithium is standardized as [FIPS 204](https://csrc.nist.gov/pubs/fips/204/final).

- **HQC**       [HQC](https://gitlab.com/pqc-hqc/hqc/)  
This repository provides the official implementation of HQC-2025 (levels 1, 3, 5), a code-based Key Encapsulation Mechanism (KEM) whose security is based on the hardness of solving the Quasi-Cylic Syndrome Decoding (QCSD) problem. HQC is one of the selected algorithms from the [NIST's Post-Quantum Cryptography Standardization Project](https://csrc.nist.gov/projects/post-quantum-cryptography).

- **Falcon**  [Falcon](https://falcon-sign.info/)  
This repository contains the software that accompanies the [FN-DSA submission](https://falcon-sign.info/) to [NIST's Post-Quantum Cryptography](https://csrc.nist.gov/Projects/Post-Quantum-Cryptography) project.

- **SPHINCS+**  
This repository contains the software that accompanies the [SPHINCS+ submission](https://sphincs.org/) to [NIST's Post-Quantum Cryptography](https://csrc.nist.gov/Projects/Post-Quantum-Cryptography) project.
It includes SLH-DSA/SPHINCS+ SHAKE, SHA2, Haraka, and Haraka-AES variants. Some SHA2 and Haraka-AES variants include RISC-V instruction accelerated paths and require a compatible RISC-V toolchain to build those accelerated versions.

- **CROSS**

---

## 🧪 KAT-Compliant Tests

Each algorithm comes with a **self-contained KAT test**, adapted from the **official reference implementation submitted to NIST**.  

Each test provides:
- **Key generation** (`keygen`)  
- **Encapsulation / Signing** (`encaps` / `sign`)  
- **Decapsulation / Verification** (`decaps` / `verify`)  

This makes the implementations straightforward to validate and benchmark, while ensuring compliance with the standard reference behavior.

---

## 🎯 Purpose

VLSPQC has been developed within the **VLSI-Lab** as a foundation for:
- **Hardware/software co-design** of PQC algorithms.  
- **FPGA-based prototyping and benchmarking.**  
- **Integration into VLSI-oriented PQC accelerators.**  
- **Ensuring reproducibility** against NIST’s official KATs.  

---

## 🔧 Usage

### Running Tests

Use the interactive test launcher to run KAT tests or profiling:

```bash
./profiling/run_test.sh
```

This will guide you through selecting:
1. **Test category**: `common` (original implementations) or `profiled` (instrumented versions)
2. **Algorithm family**: ML-KEM, ML-DSA, HQC, FALCON, SLH-DSA, CROSS, etc.
3. **Specific variant**: parameter set selection

For command-line usage, see [`profiling/README.md`](profiling/README.md).

### Building Individual Algorithms

Each algorithm resides in its own directory with a corresponding `Makefile`:

```bash
cd original/ML-KEM/ml-kem-512
make clean && make
./ml-kem-512
```

---

## 📊 Profiling

The [`profiling/`](profiling/) folder contains instrumented implementations for performance analysis. Function call profiling results are documented in:

📄 **[`profiling/pqc_profiling_analysis.md`](profiling/pqc_profiling_analysis.md)**

This analysis covers:
- Function call frequency across KeyGen, Encaps/Sign, and Decaps/Verify operations
- Bottleneck identification for each algorithm
- Cross-algorithm performance comparisons

---
## 📄 License

This repository follows the licensing terms of the respective reference implementations used as the starting point. Please check individual algorithm directories for specific license details.

## 👥 Authors

- Alessandra Dolmeta    - alessandra.dolmeta@polito.it
- Valeria Piscopo       - valeria.piscopo@polito.it
