# HAWK-512 keygen profiler

`hawk_prof.h` / `hawk_prof.c` add an optional call-count / dimension /
wall-clock profiler for the **keygen path only**, to answer a question the
RISC-V SoC cycle-counter mechanism (`PROFILING=cva6/xheep`, see
[`../../../../tools/README.md`](../../../../tools/README.md)) can't: that
mechanism wraps the *whole* keygen call in one `mcycle` read, with no
breakdown of where the time inside it actually goes. This profiler instead
instruments ~38 individual functions along the keygen call graph, on a
normal host build, and reports per function: how many times it was called,
the total vector/matrix (or big-integer limb) size it processed, and its
cumulative wall-clock time.

## Why keygen only

Scope is deliberately limited to the 8 files that HAWK-512 keygen actually
touches — `hawk_kgen.c`, `ng_hawk.c`, `ng_ntru.c`, `ng_zint31.c`,
`ng_mp31.c`, `ng_poly.c`, `ng_fxp.c`, `sha3.c`. `hawk_sign.c`/`hawk_vrfy.c`
(sign/verify-specific logic) are not instrumented. Since sign/verify reuse
some of the same low-level modules (NTT, FFT, Keccak), you'll still see
partial numbers from them show up if you profile a run that also signs and
verifies — just not their own top-level logic (Klein sampler, encoding,
etc.).

`hawk-1024` carries an inert copy of `hawk_prof.h`/`hawk_prof.c` purely so
its build doesn't break — `kat/gen/main_hawk.c` is shared between both
variants and unconditionally includes `hawk_prof.h`. None of hawk-1024's
own functions are instrumented, so `make test-hawk-1024 HAWK_PROFILE=1`
builds and runs fine but always prints an empty report.

## Only "Level 1" functions are instrumented

Whole-vector / whole-bignum operations (NTT, FFT, Keccak squeeze, big-int
CRT rebuild/Bezout, NTRU-solve recursion steps, ...) — not the scalar
helpers used inside their loops (`mp_add`, `mp_montymul`, `fxr_add`,
`tbmask`, ...), which run millions of times per keygen call. Wrapping those
individually would dominate the measurement with its own overhead and adds
nothing not already implied by (calls × dimension) of their caller.

## How it works

Disabled by default (`HAWK_PROFILE=0` — same convention as the existing
dead-by-default `NTRUGEN_STATS`/`HAWK_DEBUG` scaffolding already in
`ng_inner.h`/`hawk_inner.h`): every macro compiles away to nothing, so
normal builds and timings are completely unaffected. Verified identical
KAT pass/fail behavior with `HAWK_PROFILE=0` vs. unmodified sources.

When enabled (`HAWK_PROFILE=1`), each instrumented function is split in
two: its body compiles under a private `<name>_impl` name, and its normal
public name becomes a thin wrapper that times the call (`clock_gettime`)
and records it into a global slot. No caller anywhere has to change — the
wrapper keeps the exact original signature — and no function's internal
control flow (including early `return`s) is touched.

**Recorded times are *inclusive* (wall-clock), not exclusive/self time** —
e.g. `MP_NTT`'s time is also counted inside `SOLVE_NTRU_*`'s time, since
`solve_NTRU_*` calls it. Compare sibling slots at the same nesting depth,
or a leaf slot against its immediate parent, rather than expecting the
whole table to sum to keygen's total wall time.

## Running it

```sh
# from the repo root
make test-hawk-512 HAWK_PROFILE=1 KATNUM=100
```

The report prints once (to stderr), after the whole KAT loop — so it's
aggregated over all `KATNUM` keygen calls in the run, not just one.
Rebuilding is required to pick up the flag: Make won't detect that only
`HAWK_PROFILE` changed if `build/gen_hawk_512`/`build/test_hawk_512`
already exist from a prior run, so `rm -f build/*hawk_512*` first if a
stale binary gets reused.

## What it found (20 keygen calls, host build)

| Rank | Slot | Share |
|---|---|---|
| 1 | `SOLVE_NTRU` (whole NTRU-solve) | ~93% of `HAWK_KEYGEN` |
| 2 | `SOLVE_NTRU_INTERMEDIATE` | ~65% of `SOLVE_NTRU` |
| 3 | `ZINT_REBUILD_CRT` (big-integer CRT rebuild) | largest single leaf, ~40% of `SOLVE_NTRU_INTERMEDIATE` |
| 4 | `POLY_SUB_SCALED_NTT` (Babai-reduction subtract) | second-largest leaf |
| — | all Keccak (`SHAKE_INJECT`/`SHAKE_EXTRACT`/`SHAKE_FLIP`/`PROCESS_BLOCK`) combined | **~5%** of keygen |
| — | all NTT/iNTT (`MP_NTT`+`MP_INTT`) combined | **~14%** of keygen |

The bottleneck is the **non-NTT-accelerated big-integer arithmetic** in
`ng_zint31.c` (CRT rebuild, add/sub/negate on multi-word integers) plus the
NTT-domain Babai-reduction subtraction in `ng_poly.c`, both exercised
repeatedly across the NTRU-solve recursion's depth levels — not Keccak,
and not the NTT/iNTT primitives themselves. A RISC-V accelerator that only
speeds up Keccak/NTT would leave most of keygen's cost untouched.

## Instrumented functions, by file

| File | Functions |
|---|---|
| `hawk_kgen.c` | `Zh(keygen)` (top-level), `encode_public`, `encode_private` |
| `ng_hawk.c` | `Hawk_keygen`, `Hawk_regen_fg`, `parity`, `make_q001` |
| `ng_ntru.c` | `solve_NTRU`, `solve_NTRU_deepest`, `solve_NTRU_intermediate`, `solve_NTRU_depth0`, `make_fg_zero`, `make_fg_step`, `make_fg_intermediate`, `make_fg_deepest` |
| `ng_zint31.c` | `zint_bezout`, `zint_rebuild_CRT`, `zint_add_mul_small`, `zint_add_scaled_mul_small`, `zint_sub_scaled`, `zint_negate`, `zint_norm_zero` |
| `ng_mp31.c` | `mp_NTT`, `mp_iNTT`, `mp_mkgm`, `mp_mkigm`, `mp_mkgmigm` |
| `ng_poly.c` | `poly_sub_scaled`, `poly_sub_scaled_ntt`, `poly_sub_kf_scaled_depth1`, `poly_big_to_small`, `poly_big_to_fixed` |
| `ng_fxp.c` | `vect_FFT`, `vect_iFFT` |
| `sha3.c` | `process_block`, `shake_inject`, `shake_extract`, `shake_flip`, `shake_x4_extract_words` |
