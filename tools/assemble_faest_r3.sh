#!/usr/bin/env bash
#######################################################################################
##                                                                                   ##
## Auth: Alessandra Dolmeta                                                          ##
## @ EDGE group, at VLSI-LAB, Politecnico di Torino                                  ##
## Date: October 2026                                                                ##
## Desc: Assembles the FAEST v3 (NIST round 3) lambda=128 source tree               ##
##                                                                                   ##
#######################################################################################

# Assembles FAEST-R3 under original/Additional-DS/FAEST-R3/ from the official
# FAEST v3.0 NIST round-3 reference packages (sw/original/faest-v3/<variant>/,
# verbatim copies of the submission packages, see sw/original/PROVENANCE.md).
# Only the four lambda=128 variants are assembled (faest_128f, faest_128s,
# faest_em_128f, faest_em_128s).
#
# Everything except the per-variant paramset engine (faest_<v>.c/h), its
# crypto_sign.c and api.h is byte-identical across the four packages and lives
# once in FAEST-R3/common/{src,include} (flat, no subdirectories: the vendored
# XKCP sha3/ opt64 subtree and tables/ are flattened).
#
# Patches (all mechanical, see original/Additional-DS/README.md, "FAEST-R3"):
#   1. `#include "sha3/KeccakHash.h"` -> "KeccakHash.h" (hash_shake.h),
#      `#include "tables/X.h"` -> "X.h" (instances.c),
#      `#include "../endian_compat.h"` -> "endian_compat.h" (brg_endian.h).
#   2. `#include <config.h>` -> `#include "config.h"` everywhere (plain
#      `gcc *.c` has no -I.; angle includes would not see the local header).
#   3. sha3/config.h (4 XKCP #defines) and the package's portable config
#      (HAVE_ALIGNED_ALLOC / HAVE_EXPLICIT_BZERO / HAVE_MEMPCPY, taken from
#      sw/common/portable/config.h) share the name config.h once flattened:
#      merged into ONE config.h.
#   4. Every .c file gets `#include "faest_build_config.h"` as its first line.
#      That header hardcodes what upstream's Makefile passes via -D (-DNDEBUG,
#      -DHAVE_RANDOMBYTES) plus HAVE_CONFIG_H (switches off compat.h's x86
#      AES-NI/SSE2/AVX2 auto-detection: portable C only) and _GNU_SOURCE
#      (declares mempcpy/explicit_bzero that config.h announces).
#   5. rng.c/rng.h: the repo's software AES-256 CTR-DRBG (kat/gen/rng.c, no
#      OpenSSL); randomness.c forwards rand_bytes() to its randombytes().
#   6. crypto_sign.h (SUPERCOP only, identical to api.h) is not copied.
#
# Usage:  tools/assemble_faest_r3.sh [/path/to/sw/original/faest-v3]
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
ROOT=$(cd "$SCRIPT_DIR/.." && pwd)
SRC=${1:-"$ROOT/../sw/original/faest-v3"}
DEST="$ROOT/original/Additional-DS/FAEST-R3"
COMMON_DIR="$DEST/common"
GENDIR="$ROOT/kat/gen"
REF="$SRC/faest_128f"   # any variant: the common files are identical

[ -d "$REF" ] || { echo "error: $REF not found" >&2; exit 1; }

declare -A VARIANTS=(
  [FAEST_128F]=faest_128f     [FAEST_128S]=faest_128s
  [FAEST_EM_128F]=faest_em_128f [FAEST_EM_128S]=faest_em_128s
)

COMMON_C=(aes.c bavc.c compat.c cpu.c faest_impl.c fields.c instances.c owf.c
          random_oracle.c randomness.c universal_hashing.c utils.c vole.c
          faest_aes_128.c faest_aes_192.c faest_aes_256.c)
COMMON_H=(aes.h aesni.h bavc.h compat.h cpu.h endian_compat.h faest.h
          faest_defines.h faest_aes.h faest_impl.h fields.h hash_shake.h
          instances.h macros.h owf.h random_oracle.h randomness.h
          universal_hashing.h utils.h vole.h parameters.h)
SHA3_C=(KeccakHash.c KeccakSponge.c KeccakP-1600-opt64.c)
SHA3_H=(KeccakHash.h KeccakSponge.h KeccakSponge.inc SnP-Relaned.h align.h
        brg_endian.h KeccakP-1600-SnP.h KeccakP-1600-opt64-config.h
        KeccakP-1600-64.macros KeccakP-1600-unrolling.macros PlSnP-Fallback.inc)
TABLES=(tables_128f.h tables_128s.h tables_192f.h tables_192s.h tables_256f.h
        tables_256s.h tables_em_192f.h tables_em_192s.h)

rm -rf "$DEST"
mkdir -p "$COMMON_DIR/src" "$COMMON_DIR/include"

for f in "${COMMON_C[@]}"; do cp "$REF/$f" "$COMMON_DIR/src/"; done
for f in "${COMMON_H[@]}"; do cp "$REF/$f" "$COMMON_DIR/include/"; done
for f in "${SHA3_C[@]}"; do cp "$REF/sha3/$f" "$COMMON_DIR/src/"; done
for f in "${SHA3_H[@]}"; do cp "$REF/sha3/$f" "$COMMON_DIR/include/"; done
for f in "${TABLES[@]}"; do cp "$REF/tables/$f" "$COMMON_DIR/include/"; done
cp "$GENDIR/rng.c" "$COMMON_DIR/src/"
cp "$GENDIR/rng.h" "$COMMON_DIR/include/"

# Patch 1: flatten sub-directory includes.
sed -i 's#include "sha3/KeccakHash.h"#include "KeccakHash.h"#' "$COMMON_DIR/include/hash_shake.h"
sed -i 's#include "tables/#include "#' "$COMMON_DIR/src/instances.c"
sed -i 's#include "\.\./endian_compat\.h"#include "endian_compat.h"#' "$COMMON_DIR/include/brg_endian.h"

# Patch 3: merged config.h (sha3/config.h content + portable config).
{
  echo '/* pqrv: merged config.h. First block = XKCP sha3/config.h (verbatim); second block ='
  echo ' * sw/common/portable/config.h of the FAEST software tree, which upstream forces via'
  echo ' * -DHAVE_CONFIG_H when building the pure C code (no AES-NI/SSE2/AVX2 auto-detection). */'
  cat "$REF/sha3/config.h"
  echo
  echo '#define HAVE_ALIGNED_ALLOC'
  echo '#define HAVE_EXPLICIT_BZERO'
  echo '#define HAVE_MEMPCPY'
} > "$COMMON_DIR/include/config.h"

# Patch 4: hardcoded build configuration.
cat > "$COMMON_DIR/include/faest_build_config.h" <<'EOT'
/*
 * faest_build_config.h -- pqrv: hardcodes what the upstream Makefile passes on
 * the compile line, so that a plain `gcc *.c` (zero -D) builds the portable
 * reference code. Included as the first line of every .c file.
 *
 *   _GNU_SOURCE        mempcpy()/explicit_bzero() (announced by config.h) are
 *                      declared; without it they are implicit int functions
 *                      that truncate pointers on 64-bit.
 *   HAVE_CONFIG_H      makes compat.h/cpu.h use config.h instead of probing the
 *                      toolchain: AES-NI, SSE2 and AVX2 code paths stay disabled.
 *   HAVE_RANDOMBYTES   rand_bytes() forwards to the NIST DRBG randombytes()
 *                      (rng.c), so KAT runs are replayable.
 *   NDEBUG             as upstream's CPPFLAGS (-DNDEBUG).
 */
#ifndef FAEST_BUILD_CONFIG_H
#define FAEST_BUILD_CONFIG_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#ifndef HAVE_CONFIG_H
#define HAVE_CONFIG_H 1
#endif
#ifndef HAVE_RANDOMBYTES
#define HAVE_RANDOMBYTES 1
#endif
#ifndef NDEBUG
#define NDEBUG 1
#endif

#endif /* FAEST_BUILD_CONFIG_H */
EOT

# per-variant files
for NAME in "${!VARIANTS[@]}"; do
  low="${VARIANTS[$NAME]}"
  vdir="$DEST/$NAME"
  mkdir -p "$vdir/src" "$vdir/include"
  cp "$SRC/$low/$low.c" "$SRC/$low/crypto_sign.c" "$vdir/src/"
  cp "$SRC/$low/$low.h" "$SRC/$low/api.h" "$vdir/include/"
done

# Patch 2: quote-style config.h include.
find "$DEST" \( -name '*.c' -o -name '*.h' \) -print0 | xargs -0 sed -i 's@#include <config\.h>@#include "config.h"@'

# Patch 4 (cont.): first line of every .c file.
while IFS= read -r -d '' f; do
  sed -i '1i #include "faest_build_config.h"' "$f"
done < <(find "$DEST" -name '*.c' ! -name rng.c -print0)

echo "assembled ${#VARIANTS[@]} variants under $DEST"
