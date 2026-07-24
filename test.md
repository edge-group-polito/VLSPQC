# Test commands — all algorithms

Run from `/home/aledolme/home/CODE/pqrv`.

## Dynamic (gcc, `.rsp`-based)

```bash
make test-all KATNUM=5              # standardized: ML-KEM, ML-DSA, SLH-DSA x4, FALCON, HQC (41 variants)
make test-additional-ds KATNUM=5    # CROSS, HAWK, LESS, FAEST (40 variants)
make test-stateful-ds KATNUM=5      # XMSS, LMS, HSS (default variant each)
make test-ascon KATNUM=5            # ASCON AEAD + hash/XOF

# per family
make test-falcon test-mldsa test-slhdsa test-slhdsa-haraka test-slhdsa-haraka-aes test-slhdsa-sha2 test-mlkem test-hqc
make test-cross test-hawk test-less test-faest
make test-xmss test-lms test-hss
make test-ascon-aead test-ascon-hash

# per variant, e.g.
make test-ml-kem-512 test-ML-DSA-2 test-falcon-512 test-hawk-512 test-FAEST_128F
```

## Static (no file I/O, plain `gcc`)

```bash
make static-test-all STATIC_KATNUM=5              # standardized set
make static-test-additional-ds STATIC_KATNUM=5    # CROSS, HAWK, LESS, FAEST
make static-test-stateful-ds STATIC_KATNUM=5
make static-test-ascon STATIC_KATNUM=5

# per family / per variant, same naming as above with static-test- prefix
make static-test-mlkem static-test-hawk static-test-FAEST_128F ...

# or the batch runner -- compiles + runs + gives one PASS/FAIL summary for everything
tools/run_all_static_kat.sh                    # standardized only (default)
tools/run_all_static_kat.sh --benchmark-all     # + one representative from each non-standardized family
tools/run_all_static_kat.sh --targets all --benchmark-all --kat-num 3   # everything, thorough
```
