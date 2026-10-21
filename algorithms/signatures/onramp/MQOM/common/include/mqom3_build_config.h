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
