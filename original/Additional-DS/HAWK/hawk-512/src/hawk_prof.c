#include "hawk_prof.h"

#if HAWK_PROFILE

#include <stdio.h>

hawk_prof_slot hawk_prof_slots[HAWK_PROF_COUNT] = {
#define X(name)   { #name, 0, 0, 0 },
	HAWK_PROF_SLOTS
#undef X
};

/* see hawk_prof.h */
void
hawk_prof_report(void)
{
	/*
	 * Sort a small index array by descending cumulative time. N is a
	 * few dozen slots, so an insertion sort is plenty.
	 */
	unsigned order[HAWK_PROF_COUNT];
	for (unsigned i = 0; i < HAWK_PROF_COUNT; i ++) {
		order[i] = i;
	}
	for (unsigned i = 1; i < HAWK_PROF_COUNT; i ++) {
		unsigned j = i;
		while (j > 0 && hawk_prof_slots[order[j - 1]].nsec
			< hawk_prof_slots[order[j]].nsec)
		{
			unsigned t = order[j - 1];
			order[j - 1] = order[j];
			order[j] = t;
			j --;
		}
	}

	fprintf(stderr, "\n=== HAWK_PROFILE report ===\n");
	fprintf(stderr,
		"(inclusive wall-clock time -- a slot's time is also counted\n"
		" inside any slot that calls it; see hawk_prof.h)\n");
	fprintf(stderr, "%-28s %10s %14s %12s %10s\n",
		"function", "calls", "elems", "time(ms)", "ns/call");
	for (unsigned i = 0; i < HAWK_PROF_COUNT; i ++) {
		hawk_prof_slot *s = &hawk_prof_slots[order[i]];
		if (s->calls == 0) {
			continue;
		}
		fprintf(stderr, "%-28s %10llu %14llu %12.3f %10.1f\n",
			s->name,
			(unsigned long long)s->calls,
			(unsigned long long)s->elems,
			(double)s->nsec / 1.0e6,
			(double)s->nsec / (double)s->calls);
	}
	fprintf(stderr, "===========================\n");
}

#endif
