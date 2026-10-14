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
