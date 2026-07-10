# `kat/` — KAT generation, `.rsp` storage, and static tests

This directory holds everything related to producing and consuming Known
Answer Test vectors. See [`../README.md`](../README.md) for the `make`
targets that drive these, and [`../original/DS/README.md`](../original/DS/README.md),
[`../original/Additional-DS/README.md`](../original/Additional-DS/README.md),
[`../original/Stateful-DS/README.md`](../original/Stateful-DS/README.md),
[`../original/KEM/README.md`](../original/KEM/README.md), and
[`../original/Symmetric/ASCON/README.md`](../original/Symmetric/ASCON/README.md)
for per-algorithm specifics.

```
kat/
├── gen/      ← KAT generator sources (PQCgenKAT_*.c) and test runners (main_*.c)
├── rsp/      ← generated .rsp files (created at runtime by `make gen-*`)
│   ├── DS/<FAMILY>/<VARIANT>/              (standardized)
│   ├── Additional-DS/<FAMILY>/<VARIANT>/   (not standardized)
│   ├── Stateful-DS/<FAMILY>/<VARIANT>/     (NIST SP 800-208)
│   ├── KEM/<FAMILY>/<VARIANT>/
│   └── Symmetric/ASCON/<VARIANT>/          (not PQC)
└── static/   ← generated static (no file-I/O) test folders (created by `make static-test-*`)
    └── <VARIANT>/  main.c, test_vectors_*.h, flattened algorithm sources
```

## Two-phase model

### Phase 1 — KAT generation (`gen/PQCgenKAT_*.c`)

A dedicated generator binary is compiled from the algorithm's source files
plus `gen/rng.c` (NIST AES-256-CTR DRBG). It:

1. Seeds the DRBG deterministically from `{0, 1, ..., 47}`.
2. For each KAT vector, derives fresh randomness (seed, message, etc.) from
   the DRBG.
3. Runs the full algorithm (keygen → sign/encaps → verify/decaps).
4. Writes one entry per vector to a `.rsp` file under `kat/rsp/`.

### Phase 2 — KAT testing (`gen/main_*.c`)

A test binary is compiled from the same algorithm sources plus
`../common/kat_parser.c`. It:

1. Opens the `.rsp` file written in Phase 1.
2. For each entry, reads the stored randomness fields and re-runs the
   selected operations.
3. Compares the output byte-by-byte against the stored values.
4. Reports pass/fail per operation and a total count.

The two phases are independent: `.rsp` files can be regenerated at any time,
and tests re-read them automatically. `KATNUM` must match between generation
and testing — the test binary stops after reading `KATNUM` entries.

### Static tests (`kat/static/`)

`tools/gen_static_test.py` (see [`../tools/README.md`](../tools/README.md))
converts a variant's `.rsp` into a self-contained `kat/static/<variant>/`
folder: KAT vectors baked in as C arrays, no file I/O, no dependency beyond
the algorithm's own sources. Intended for bare-metal/FPGA/RISC-V targets
where `fopen()` isn't available.

## `.rsp` file format

Standard NIST PQC KAT format, extended with algorithm-specific randomness
fields so each vector is fully self-contained and reproducible without a live
RNG. Entries are separated by a blank line; lines starting with `#` are
comments.

### DS — standard fields

```
# <ALGO_NAME>

count = 0
seed = <48-byte hex>
mlen = 33
msg = <hex>
pk = <hex>
sk = <hex>
smlen = <N>
sm = <hex>

count = 1
...
```

### KEM — standard fields

```
# <ALGO_NAME>

count = 0
seed = <48-byte hex>
keypair_coins = <hex>
enc_coins = <hex>
pk = <hex>
sk = <hex>
ct = <hex>
ss = <hex>

count = 1
...
```

Per-family extra fields (e.g. FALCON's `nonce`, ML-KEM's `keypair_coins`/
`enc_coins`) are documented in [`../original/DS/README.md`](../original/DS/README.md),
[`../original/Additional-DS/README.md`](../original/Additional-DS/README.md),
[`../original/Stateful-DS/README.md`](../original/Stateful-DS/README.md), and
[`../original/KEM/README.md`](../original/KEM/README.md).

### ASCON — no keygen, so a different shape entirely

ASCON (ascon-aead/ascon-hash families under
[`../original/Symmetric/ASCON/`](../original/Symmetric/ASCON/README.md)) has
no keygen, so its `.rsp` fields don't follow the DS/KEM pattern above:

```
# AEAD variants (ascon128v12, ascon128av12, ascon80pqv12)
count = 0
seed = <48-byte hex>
key = <hex>
nonce = <hex>
ad = <hex>
pt = <hex>
ct = <hex>

# Hash/XOF variants (asconhashv12, asconhashav12, asconxofv12, asconxofav12)
count = 0
seed = <48-byte hex>
msg = <hex>
md = <hex>
```

See [`../original/Symmetric/ASCON/README.md`](../original/Symmetric/ASCON/README.md)
for field sizes and per-variant notes.

## `common/kat_parser`

The `.rsp` parser used by every `main_*.c` test runner lives in
[`../common/README.md`](../common/README.md).
