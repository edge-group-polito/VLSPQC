#!/usr/bin/env bash
#######################################################################################
##                                                                                   ##
## Auth: Alessandra Dolmeta, Valeria Piscopo                                         ##
## @ EDGE group, at VLSI-LAB, Politecnico di Torino                                  ##
## Date: October 2026                                                                ##
## Desc: Assembles the MQOM3 source tree from the NIST round-3 submission package    ##
##                                                                                   ##
#######################################################################################

# Assembles MQOM (MQOM3, NIST additional-signatures round 3) under
# original/Additional-DS/MQOM/ from the Reference_Implementation/ folder of the
# official submission package (submission_package_v3/Reference_Implementation).
#
# The 18 upstream variants (mqom3_cat{1,3,5}_{gf2_shorter,gf16_fast,gf16_short}_{ct,ot})
# are byte-identical except for one file each, mqom3_parameters_<variant>.h (plus
# the Makefile's -D selection of the variant, which is baked in here as a tiny
# generated include/mqom3_variant.h). Everything else lives once in MQOM/common/
# in a FLAT layout (src/ + include/, no subdirectories): upstream's blc/, piop/,
# ggm_tree/, rijndael/, fields/, fields_bitsliced/ and the vendored XKCP sha3/
# (opt64 backend only) are flattened (no file name collides once only one XKCP
# platform is kept; the two include paths that crossed directories are rewritten).
#
# Patches applied to the upstream text (all mechanical, see
# original/Additional-DS/README.md, section "MQOM"):
#   1. `#include "sha3/X.h"` -> `#include "X.h"` (xof.h), and sha3's
#      `#include "../endian_compat.h"` -> `#include "endian_compat.h"`.
#   2. Every .c file gets `#include "mqom3_build_config.h"` as its first line,
#      and api.h gets it right after its include guard. That header hardcodes
#      what upstream's Makefile passes as -D flags (portable backends forced,
#      same flags as the default non-SIMD build) and pulls in the variant's
#      mqom3_variant.h, so no -D flag is needed to build any variant.
#   3. `#include <common.h>` / `<fields.h>` (piop_cache.h, seed_expand_cache.h)
#      -> quoted includes, so a plain `gcc *.c` in a flat folder (static test)
#      builds without -I. .
#   4. The NIST KAT DRBG (rng.c/rng.h) is the self-contained software
#      AES-256-CTR one from kat/gen/ (upstream's generator/rng.c needs OpenSSL).
# Dropped as not needed: benchmark/, generator/, tests, Makefiles, mupq/liboqs
# glue (enc_mupq.h, enc_liboqs.h), the arm/avx XKCP backends, ARMv7-M assembly,
# and the x8 Keccak sources (only compiled on AVX-512 upstream).
#
# NOTE: in the copy of the package under DS-add/ the subdirectories of
# Reference_Implementation/<variant>/ (blc, piop, ggm_tree, sha3, rijndael,
# fields, fields_bitsliced) are EMPTY (0-byte files). Pass a COMPLETE copy of
# Reference_Implementation/ (see original/Additional-DS/README.md, "MQOM",
# provenance); the script aborts on the empty-file check below otherwise.
#
# Usage (from VLSPQC/):
#   tools/assemble_mqom.sh /path/to/submission_package_v3/Reference_Implementation
#   tools/build_mqom_main.sh        # (re)generate each variant's main.c + test vectors
set -euo pipefail

REF=${1:?"usage: $0 /path/to/submission_package_v3/Reference_Implementation"}
SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
ROOT=$(cd "$SCRIPT_DIR/.." && pwd)
DEST="$ROOT/original/Additional-DS/MQOM"
COMMON_DIR="$DEST/common"
GENDIR="$ROOT/kat/gen"
BASE="$REF/mqom3_cat1_gf16_fast_ct"

VARIANTS=(
  mqom3_cat1_gf2_shorter_ct mqom3_cat1_gf2_shorter_ot
  mqom3_cat1_gf16_fast_ct   mqom3_cat1_gf16_fast_ot
  mqom3_cat1_gf16_short_ct  mqom3_cat1_gf16_short_ot
  mqom3_cat3_gf2_shorter_ct mqom3_cat3_gf2_shorter_ot
  mqom3_cat3_gf16_fast_ct   mqom3_cat3_gf16_fast_ot
  mqom3_cat3_gf16_short_ct  mqom3_cat3_gf16_short_ot
  mqom3_cat5_gf2_shorter_ct mqom3_cat5_gf2_shorter_ot
  mqom3_cat5_gf16_fast_ct   mqom3_cat5_gf16_fast_ot
  mqom3_cat5_gf16_short_ct  mqom3_cat5_gf16_short_ot
)

for v in "${VARIANTS[@]}"; do
  [ -s "$REF/$v/mqom3_parameters_${v#mqom3_}.h" ] || {
    echo "error: $REF/$v/mqom3_parameters_${v#mqom3_}.h missing or empty (truncated package?)" >&2; exit 1; }
done
[ -s "$BASE/ggm_tree/ggm_tree_small.c" ] || {
  echo "error: $BASE/ggm_tree/ggm_tree_small.c missing or empty (truncated package?)" >&2; exit 1; }

# Keep any checked-in main.c / test vectors of the variants across a re-assembly
SAVE=$(mktemp -d)
for v in "${VARIANTS[@]}"; do
  [ -d "$DEST/$v" ] || continue
  mkdir -p "$SAVE/$v"
  [ -f "$DEST/$v/main.c" ] && cp "$DEST/$v/main.c" "$SAVE/$v/"
  cp "$DEST/$v"/include/test_vectors_*.h "$SAVE/$v/" 2>/dev/null || true
done
rm -rf "$DEST"
mkdir -p "$COMMON_DIR/src" "$COMMON_DIR/include"

# ---- common/include: every header (top level + flattened subdirs) ----------
for f in "$BASE"/*.h; do
  b=$(basename "$f")
  case "$b" in
    mqom3_parameters_cat*|enc_mupq.h|enc_liboqs.h) continue ;;
  esac
  cp "$f" "$COMMON_DIR/include/"
done
for d in blc piop ggm_tree fields fields_bitsliced rijndael; do
  cp "$BASE/$d"/*.h "$COMMON_DIR/include/"
done
# XKCP: shared part of sha3/ (no Makefile/.gitignore) plus the opt64 backend only
cp "$BASE"/sha3/*.h "$BASE"/sha3/*.inc "$COMMON_DIR/include/"
cp "$BASE"/sha3/opt64/*.h "$BASE"/sha3/opt64/*.inc "$BASE"/sha3/opt64/*.macros "$COMMON_DIR/include/"
cp "$BASE/LICENSE" "$COMMON_DIR/LICENSE"

# ---- common/src: every .c ---------------------------------------------------
for f in "$BASE"/*.c; do cp "$f" "$COMMON_DIR/src/"; done
for d in blc piop ggm_tree; do cp "$BASE/$d"/*.c "$COMMON_DIR/src/"; done
# rijndael: the ISA-specific units (AES-NI, VAES, ARM AES, external) are
# compiled to nothing unless their RIJNDAEL_* macro is set, which
# mqom3_build_config.h never does; only the portable ones are kept.
cp "$BASE"/rijndael/rijndael_ref.c "$BASE"/rijndael/rijndael_table.c \
   "$BASE"/rijndael/rijndael_ct64.c "$COMMON_DIR/src/"
cp "$BASE"/sha3/KeccakHash.c "$BASE"/sha3/KeccakSponge.c \
   "$BASE"/sha3/KeccakHashtimes4.c "$BASE"/sha3/KeccakSpongetimes4.c "$COMMON_DIR/src/"
cp "$BASE"/sha3/opt64/KeccakP-1600-opt64.c "$BASE"/sha3/opt64/KeccakP-1600-times4-on1.c "$COMMON_DIR/src/"
# NIST KAT DRBG (software AES-256, no OpenSSL)
cp "$GENDIR/rng.c" "$COMMON_DIR/src/rng.c"
cp "$GENDIR/rng.h" "$COMMON_DIR/include/rng.h"

# ---- patch 1: flatten the cross-directory includes --------------------------
sed -i -E 's|#include "sha3/([A-Za-z0-9_.-]+)"|#include "\1"|' "$COMMON_DIR/include/xof.h"
sed -i 's|#include "../endian_compat.h"|#include "endian_compat.h"|' "$COMMON_DIR/include/brg_endian.h"

# ---- patch 3: quote project-local includes (plain gcc without -I.) ---------
sed -i -e 's|#include <common.h>|#include "common.h"|' \
       -e 's|#include <fields.h>|#include "fields.h"|' \
       "$COMMON_DIR/include/piop_cache.h" "$COMMON_DIR/include/seed_expand_cache.h"

# ---- patch 2: build config as first include everywhere ----------------------
cat > "$COMMON_DIR/include/mqom3_build_config.h" <<'EOF'
#ifndef __MQOM3_BUILD_CONFIG_H__
#define __MQOM3_BUILD_CONFIG_H__

/* Added by the VLSPQC port (not part of upstream MQOM3).
 *
 * Upstream selects the variant and its build options with -D flags passed by
 * its Makefile. They are hardcoded here (each define is #ifndef-guarded, so an
 * explicit -D on the command line still wins) so that every translation unit
 * of every variant builds with a plain `gcc` and zero -D flags. This header is
 * the first include of every .c file and of api.h.
 *
 * Only portable code paths are selected: no AES-NI/VAES/ARM-AES Rijndael, no
 * AVX2/AVX-512/NEON fields, and the opt64 (plain C) Keccak backend, whatever
 * -m flags the compiler may have been given. */

/* Variant selection: MQOM3_PARAM_{SECURITY,BASE_FIELD,TRADEOFF,OT_VARIANT} */
#include "mqom3_variant.h"

/* Portable backends */
#ifndef FIELDS_REF
#define FIELDS_REF 1
#endif
#if !defined(RIJNDAEL_BITSLICE) && !defined(RIJNDAEL_TABLE) && !defined(RIJNDAEL_CONSTANT_TIME_REF)
#define RIJNDAEL_BITSLICE 1
#endif
#ifndef KECCAK_PLATFORM
#define KECCAK_PLATFORM opt64
#endif
#ifndef NO_MISALIGNED_ACCESSES
#define NO_MISALIGNED_ACCESSES 1
#endif

/* Defaults of upstream's Makefile on a non-SIMD target */
#ifndef USE_PIOP_CACHE
#define USE_PIOP_CACHE 1
#endif
#ifndef USE_XOF_X4
#define USE_XOF_X4 1
#endif
#ifndef AES_GRINDING_X4
#define AES_GRINDING_X4 1
#endif
#ifndef USE_ENC_X8
#define USE_ENC_X8 1
#endif
#ifndef LARGE_TREE_BFS
#define LARGE_TREE_BFS 1
#endif
#ifndef LARGE_TREE_BFS_X8
#define LARGE_TREE_BFS_X8 1
#endif
#ifndef NDEBUG
#define NDEBUG 1
#endif

#endif /* __MQOM3_BUILD_CONFIG_H__ */
EOF
for f in "$COMMON_DIR"/src/*.c; do
  [ "$(basename "$f")" = "rng.c" ] && continue
  sed -i '1i #include "mqom3_build_config.h"' "$f"
done
# api.h: right after the include guard
sed -i '0,/^#define __MQOM_API_H__/s//#define __MQOM_API_H__\n\n#include "mqom3_build_config.h"/' "$COMMON_DIR/include/api.h"

# ---- per-variant directories -------------------------------------------------
for v in "${VARIANTS[@]}"; do
  s=${v#mqom3_}                     # cat1_gf16_fast_ct
  cat=${s%%_*}; rest=${s#*_}        # cat1 | gf16_fast_ct
  case "$cat" in cat1) sec=128 ;; cat3) sec=192 ;; cat5) sec=256 ;; esac
  case "$rest" in gf2_*) bf=1 ;; *) bf=4 ;; esac
  case "$rest" in *_fast_*) to=0 ;; *_short_*) to=1 ;; *_shorter_*) to=2 ;; esac
  case "$rest" in *_ot) ot=1 ;; *) ot=0 ;; esac
  mkdir -p "$DEST/$v/include"
  cp "$REF/$v/mqom3_parameters_$s.h" "$DEST/$v/include/"
  cat > "$DEST/$v/include/mqom3_variant.h" <<EOF
#ifndef __MQOM3_VARIANT_H__
#define __MQOM3_VARIANT_H__

/* $v: what upstream's Makefile passes as -DMQOM3_PARAM_*=... for
 * MQOM3_VARIANT=$s */
#define MQOM3_PARAM_SECURITY   $sec
#define MQOM3_PARAM_BASE_FIELD $bf
#define MQOM3_PARAM_TRADEOFF   $to
#define MQOM3_PARAM_OT_VARIANT $ot

#endif /* __MQOM3_VARIANT_H__ */
EOF
  if [ -d "$SAVE/$v" ]; then
    [ -f "$SAVE/$v/main.c" ] && cp "$SAVE/$v/main.c" "$DEST/$v/"
    cp "$SAVE/$v"/test_vectors_*.h "$DEST/$v/include/" 2>/dev/null || true
  fi
done
rm -rf "$SAVE"
echo "assembled MQOM: common/ + ${#VARIANTS[@]} variants in $DEST"
