#!/usr/bin/env bash
#######################################################################################
##                                                                                   ##
## Auth: Alessandra Dolmeta, Valeria Piscopo                                         ##
## @ EDGE group, at VLSI-LAB, Politecnico di Torino                                  ##
## Date: July 2026                                                                   ##
## Desc: Assembles FAEST source tree from a faest-ref checkout                       ##
##                                                                                   ##
#######################################################################################

# Assembles FAEST under algorithms/signatures/onramp/FAEST/ from a faest-ref
# checkout (https://github.com/faest-sign/faest-ref), using the concrete
# meson-configured sources under its build-test/ dir -- so no template
# engine (meson) is needed at pqrv build time; every variant here is a
# plain, compile-time-fixed C source tree, same convention as CROSS/LESS/HAWK.
#
# Unlike those families, the ~30 files that are byte-identical across all 12
# variants (the generic FAEST engine, sha3, the 3 lambda-specific AES/witness
# files, parameters.h, the NIST DRBG) live once in common/{src,include}
# instead of being duplicated x12 -- see "FAEST" under
# algorithms/signatures/onramp/README.md. Each variant dir then holds only its own
# paramset engine (faest_<variant>.c/h), its crypto_sign.c/api.h wrapper, and
# a hand-driveable main.c (see build_faest_main.py, run after this script).
#
# Usage:
#   1. In a faest-ref checkout: `meson setup build-test` (just the configure
#      step -- this instantiates every parameter set's .c.in/.h.in templates
#      into concrete build-test/*.c, build-test/<variant>/{api.h,crypto_sign.c}
#      files; `ninja` is not required, nothing needs to actually build).
#   2. From pqrv/: `tools/assemble_faest.sh /path/to/faest-ref`
#   3. `tools/build_faest_main.py` to (re)generate each variant's main.c +
#      include/test_vectors_<suffix>.h from fresh KAT data.
set -euo pipefail

REF=${1:?"usage: $0 /path/to/faest-ref (checkout with 'meson setup build-test' already run)"}
SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
ROOT=$(cd "$SCRIPT_DIR/.." && pwd)
DEST="$ROOT/algorithms/signatures/onramp/FAEST"
COMMON_DIR="$DEST/common"
GENDIR="$ROOT/harness/gen"

if [ ! -d "$REF/build-test" ]; then
  echo "error: $REF/build-test not found -- run 'meson setup build-test' in the faest-ref checkout first" >&2
  exit 1
fi

# pqrv variant name -> faest-ref lowercase name
declare -A VARIANTS=(
  [FAEST_128F]=faest_128f     [FAEST_128S]=faest_128s
  [FAEST_192F]=faest_192f     [FAEST_192S]=faest_192s
  [FAEST_256F]=faest_256f     [FAEST_256S]=faest_256s
  [FAEST_EM_128F]=faest_em_128f [FAEST_EM_128S]=faest_em_128s
  [FAEST_EM_192F]=faest_em_192f [FAEST_EM_192S]=faest_em_192s
  [FAEST_EM_256F]=faest_em_256f [FAEST_EM_256S]=faest_em_256s
)

# Common .c files (identical across all 12 variants)
COMMON_C=(aes.c bavc.c compat.c cpu.c faest_impl.c fields.c instances.c owf.c
          random_oracle.c randomness.c universal_hashing.c utils.c vole.c)

# Common headers (identical across all 12 variants)
COMMON_H=(aes.h aesni.h bavc.h compat.h cpu.h endian_compat.h faest.h
          faest_defines.h faest_aes.h faest_impl.h fields.h hash_shake.h
          instances.h macros.h owf.h random_oracle.h randomness.h
          universal_hashing.h utils.h vole.h)

SHA3_C=(sha3/KeccakHash.c sha3/KeccakSponge.c sha3/opt64/KeccakP-1600-opt64.c)
SHA3_H=(sha3/KeccakHash.h sha3/KeccakSponge.h sha3/KeccakSponge.inc
        sha3/SnP-Relaned.h sha3/align.h sha3/brg_endian.h sha3/config.h
        sha3/opt64/KeccakP-1600-SnP.h sha3/opt64/KeccakP-1600-opt64-config.h
        sha3/opt64/KeccakP-1600-64.macros sha3/opt64/KeccakP-1600-unrolling.macros
        sha3/opt64/PlSnP-Fallback.inc)

# All 3 lambda-specific AES/witness-extension files are needed by every
# variant: faest_impl.c's generic prover/verifier dispatches to
# aes_{128,192,256}_{prover,verifier} regardless of which single variant
# is being built (verified by a standalone compile+link+run smoke test) --
# and their content doesn't depend on the variant either (only on the
# lambda), so they're common too.
AES_LAMBDA_C=(faest_aes_128.c faest_aes_192.c faest_aes_256.c)

rm -rf "$DEST"
mkdir -p "$COMMON_DIR/src" "$COMMON_DIR/include"

# ---- common/ : everything byte-identical across all 12 variants ----------

for f in "${COMMON_C[@]}"; do cp "$REF/$f" "$COMMON_DIR/src/"; done
for f in "${COMMON_H[@]}"; do cp "$REF/$f" "$COMMON_DIR/include/"; done
# sha3/ is flattened into src/+include/ directly (no subdirs) to match every
# other family's flat layout, and because the static-test generator
# (tools/gen_static_test.py) only copies top-level *.c/*.h, not subdirs.
for f in "${SHA3_C[@]}"; do cp "$REF/$f" "$COMMON_DIR/src/$(basename "$f")"; done
for f in "${SHA3_H[@]}"; do cp "$REF/$f" "$COMMON_DIR/include/$(basename "$f")"; done
for f in "${AES_LAMBDA_C[@]}"; do cp "$REF/build-test/$f" "$COMMON_DIR/src/"; done
cp "$REF/build-test/parameters.h" "$COMMON_DIR/include/"
# Own copy of the NIST DRBG (like HAWK's src/rng.c).
cp "$GENDIR/rng.c" "$COMMON_DIR/src/"
cp "$GENDIR/rng.h" "$COMMON_DIR/include/"

# Fix up the two subdir-relative includes that only made sense in upstream's
# nested sha3/ tree; now that everything is flat, both targets are plain
# siblings.
sed -i 's#include "sha3/KeccakHash.h"#include "KeccakHash.h"#' "$COMMON_DIR/include/hash_shake.h"
sed -i 's#include "\.\./endian_compat.h"#include "endian_compat.h"#' "$COMMON_DIR/include/brg_endian.h"

# Always forward rand_bytes() to the injected NIST-framework randombytes()
# (from the rng.c copied above), unconditionally -- this repo only ever runs
# FAEST against a replayable DRBG for KAT testing, never "real" randomness,
# and the generated static-test folder (tools/gen_static_test.py) is
# compiled standalone with a plain `gcc *.c`, with no -D flags of its own,
# so upstream's `#if defined(HAVE_RANDOMBYTES) ...` gate would otherwise
# silently fall back to OS randomness there and break KAT replay.
sed -i 's@^#if defined(HAVE_RANDOMBYTES) || defined(SUPERCOP)$@#if 1 /* pqrv: always use the injected NIST-framework randombytes() */@' "$COMMON_DIR/src/randomness.c"

# ---- per-variant: just the paramset engine + crypto_sign wrapper ---------

for NAME in "${!VARIANTS[@]}"; do
  low="${VARIANTS[$NAME]}"
  vdir="$DEST/$NAME"
  mkdir -p "$vdir/src" "$vdir/include"

  cp "$REF/build-test/$low.c" "$vdir/src/"
  cp "$REF/build-test/$low.h" "$vdir/include/"
  cp "$REF/build-test/$low/crypto_sign.c" "$vdir/src/"
  cp "$REF/build-test/$low/api.h" "$vdir/include/"

  echo "assembled $NAME  ($(find "$vdir" -type f | wc -l) files)"
done

# compat.h (in common/) auto-detects mempcpy()/explicit_bzero() as available
# on glibc (true) and #defines faest_mempcpy/faest_explicit_bzero to call
# them, but without _GNU_SOURCE glibc's <string.h> doesn't actually declare
# their prototypes -- the resulting implicit int-returning declaration
# truncates the real pointer return value on 64-bit and corrupts memory
# (segfault). Both bavc.c (common/) and every faest_<variant>.c
# (per-variant) call these macros, so every .c file everywhere in the FAEST
# tree needs this, not just common/. -D_GNU_SOURCE on the compile line would
# fix it too, but the static-test folder is meant to compile standalone with
# a plain `gcc *.c`, no special flags -- so define it in-source instead,
# before any header pulls in <string.h>.
while IFS= read -r -d '' f; do
  sed -i '1i #define _GNU_SOURCE' "$f"
done < <(find "$DEST" -name '*.c' -print0)

echo "Done. Total variants: ${#VARIANTS[@]}"
echo "Now run tools/build_faest_main.py to generate each variant's main.c + test_vectors_<suffix>.h"
