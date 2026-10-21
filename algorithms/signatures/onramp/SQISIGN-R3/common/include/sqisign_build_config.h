/*
 * sqisign_build_config.h -- hardcoded replacement for the global -D flags
 * that upstream's CMake build (SQISIGN_BUILD_TYPE=ref) passes, so that a
 * variant builds with plain `gcc` and zero -D flags. Included as the first
 * line of every .c file in this port.
 *
 * Selected configuration: "ref" build type, portable generic field backend
 * (no AVX2/broadwell/arm64 assembly), NIST AES-256-CTR DRBG (no /dev/urandom),
 * signing enabled, asserts disabled (upstream release build uses -DNDEBUG).
 * The field radix is 64 when the compiler provides unsigned __int128, else 32
 * (same decision as upstream's CMake check, .cmake/target.cmake).
 */
#ifndef SQISIGN_BUILD_CONFIG_H
#define SQISIGN_BUILD_CONFIG_H

#include "sqisign_variant.h"

#ifndef ENABLE_SIGN
#define ENABLE_SIGN
#endif
#ifndef SQISIGN_BUILD_TYPE_REF
#define SQISIGN_BUILD_TYPE_REF
#endif
#ifndef SQISIGN_GF_IMPL_REF
#define SQISIGN_GF_IMPL_REF
#endif
#ifndef RANDOMBYTES_C
#define RANDOMBYTES_C
#endif
#ifndef NDEBUG
#define NDEBUG
#endif

#if !defined(RADIX_32) && !defined(RADIX_64)
#if defined(__SIZEOF_INT128__)
#define RADIX_64
#else
#define RADIX_32
#endif
#endif
#if defined(RADIX_64) && !defined(HAVE_UINT128)
#define HAVE_UINT128
#endif

#endif /* SQISIGN_BUILD_CONFIG_H */
