#ifndef HAWK_PROF_H__
#define HAWK_PROF_H__

/*
 * Lightweight call-count / dimension / wall-clock profiler for the HAWK
 * keygen path -- answers "how many times does each function run, over
 * what size of vector/matrix, and how much of the total time does it
 * account for" on a normal host build. This is complementary to the
 * PROFILING=cva6/xheep RISC-V SoC cycle counter (see tools/README.md),
 * which only wraps the *whole* keygen call in one mcycle read.
 *
 * Disabled (HAWK_PROFILE=0, the default): every macro here compiles away
 * to nothing, so normal builds/timings are unaffected.
 * Enabled (HAWK_PROFILE=1, e.g. `make test-hawk-512 HAWK_PROFILE=1`): each
 * instrumented function's body is compiled under a private "_impl" name,
 * and its normal public name becomes a thin wrapper that times the call
 * (clock_gettime) and records it into the matching slot below. No caller
 * anywhere has to change: the wrapper keeps the exact original signature.
 *
 * IMPORTANT: recorded times are *inclusive* (wall-clock), not exclusive/
 * self time -- e.g. mp_NTT's time is also included inside solve_NTRU_*'s
 * time, since solve_NTRU_* calls it. Compare sibling slots at the same
 * nesting depth, or a leaf slot against its immediate parent, rather than
 * expecting the whole table to sum to keygen's total wall time.
 *
 * Only "Level 1" functions (whole-vector / whole-bignum operations) are
 * instrumented -- not the scalar helpers used inside their loops
 * (mp_add, mp_montymul, fxr_add, tbmask, ...), which run millions of
 * times per keygen call: wrapping those individually would dominate the
 * measurement with its own overhead and add nothing not already implied
 * by (calls x dimension) of their caller.
 */

#ifndef HAWK_PROFILE
#define HAWK_PROFILE   0
#endif

#if HAWK_PROFILE

/*
 * Needed for clock_gettime()/CLOCK_MONOTONIC under -std=c11 (strict ISO
 * mode hides POSIX declarations otherwise). Only takes effect if this
 * header is the first to touch libc in the current translation unit --
 * ng_inner.h/hawk_inner.h already define it earlier for that reason, this
 * covers translation units that include hawk_prof.h directly instead.
 */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE   200809L
#endif

#include <stddef.h>
#include <stdint.h>
#include <time.h>

typedef struct {
	const char *name;
	uint64_t calls;
	uint64_t elems;
	uint64_t nsec;
} hawk_prof_slot;

/*
 * One X-macro entry per instrumented function. Keep this list and the
 * per-file wrappers (search for HAWK_PROF_ADD) in sync.
 */
#define HAWK_PROF_SLOTS \
	X(KGEN_TOP) \
	X(ENCODE_PUBLIC) \
	X(ENCODE_PRIVATE) \
	X(HAWK_KEYGEN) \
	X(REGEN_FG) \
	X(PARITY) \
	X(MAKE_Q001) \
	X(SOLVE_NTRU) \
	X(SOLVE_NTRU_DEEPEST) \
	X(SOLVE_NTRU_INTERMEDIATE) \
	X(SOLVE_NTRU_DEPTH0) \
	X(MAKE_FG_ZERO) \
	X(MAKE_FG_STEP) \
	X(MAKE_FG_INTERMEDIATE) \
	X(MAKE_FG_DEEPEST) \
	X(ZINT_BEZOUT) \
	X(ZINT_REBUILD_CRT) \
	X(ZINT_ADD_MUL_SMALL) \
	X(ZINT_ADD_SCALED_MUL_SMALL) \
	X(ZINT_SUB_SCALED) \
	X(ZINT_NEGATE) \
	X(ZINT_NORM_ZERO) \
	X(MP_NTT) \
	X(MP_INTT) \
	X(MP_MKGM) \
	X(MP_MKIGM) \
	X(MP_MKGMIGM) \
	X(POLY_SUB_SCALED) \
	X(POLY_SUB_SCALED_NTT) \
	X(POLY_SUB_KF_SCALED_DEPTH1) \
	X(POLY_BIG_TO_SMALL) \
	X(POLY_BIG_TO_FIXED) \
	X(VECT_FFT) \
	X(VECT_IFFT) \
	X(PROCESS_BLOCK) \
	X(SHAKE_INJECT) \
	X(SHAKE_EXTRACT) \
	X(SHAKE_FLIP) \
	X(SHAKE_X4_EXTRACT_WORDS)

enum {
#define X(name)   HAWK_PROF_##name,
	HAWK_PROF_SLOTS
#undef X
	HAWK_PROF_COUNT
};

extern hawk_prof_slot hawk_prof_slots[HAWK_PROF_COUNT];

static inline uint64_t
hawk_prof_now(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

#define HAWK_PROF_ADD(slot, elems_, nsec_)   do { \
		hawk_prof_slots[HAWK_PROF_##slot].calls ++; \
		hawk_prof_slots[HAWK_PROF_##slot].elems += (uint64_t)(elems_); \
		hawk_prof_slots[HAWK_PROF_##slot].nsec += (uint64_t)(nsec_); \
	} while (0)

void hawk_prof_report(void);

#else

#define HAWK_PROF_ADD(slot, elems_, nsec_)   ((void)0)

static inline void
hawk_prof_report(void)
{
}

#endif

#endif
