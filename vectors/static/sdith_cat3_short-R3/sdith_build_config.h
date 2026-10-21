/*
 * sdith_build_config.h
 *
 * Hardcodes the build flags that upstream's CMake build passes with -D for the
 * "Reference_Implementation" packages, so that a plain `gcc *.c` (no -D flags)
 * builds the portable C reference code:
 *   ONLY_REF_IMPLEMENTATION  drop the AVX2/AES-NI run-time dispatch
 *   NDEBUG                   upstream's Release flags (-O3 -DNDEBUG)
 * Included as the first line of every sdith/ src .c file (assert.h and the
 * NDEBUG-guarded blocks must see these before anything else).
 */
#ifndef SDITH_BUILD_CONFIG_H
#define SDITH_BUILD_CONFIG_H

#ifndef ONLY_REF_IMPLEMENTATION
#define ONLY_REF_IMPLEMENTATION 1
#endif

#ifndef NDEBUG
#define NDEBUG 1
#endif

#endif /* SDITH_BUILD_CONFIG_H */
