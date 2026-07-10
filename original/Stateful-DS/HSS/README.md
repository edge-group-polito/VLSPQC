# HSS (Hierarchical Signature System)

Vendored from Cisco's [hash-sigs](https://github.com/cisco/hash-sigs)
reference implementation — **the same library as [`../LMS`](../LMS)**; HSS
is just LMS stacked into 2-8 levels, each level independently choosing its
own `(height, Winternitz-w)` pair. See [`../LMS/README.md`](../LMS/README.md)
for what's vendored/added, the randomness model, and the `.rsp` field list —
all of that is identical here (same `src/hss_variant.c` /
`include/hss_variant.h` adapter, copied into both directories). This file
covers only what's specific to HSS: the multi-level parameter space and the
`make` targets.

See [`../README.md`](../README.md) for the DS-wide overview and
[`../../../README.md`](../../../README.md) for the top-level `make` targets.

The `HSS_L2_H5_W8/` subfolder sitting next to `src/`/`include/` is
different again: it's a **dedicated, self-contained copy** pinned to that
one parameter set, used only by the *static* KAT test generator
(`tools/gen_static_test.py`), which needs compile-time-fixed sizes — see
[`../../../tools/README.md`](../../../tools/README.md).

## Parameter space

Each of hash-sigs' 20 `(height, Winternitz-w)` pairs (5 heights × 4 `w`
values, see `../LMS/README.md`) can be used **independently at each level**
of a 1-8 level hierarchy — so the fully general HSS parameter space is
20^levels, effectively unbounded for KAT-sweep purposes. Real-world HSS
usage overwhelmingly uses the **same** pair at every level, so that's what
the "-check" tables here do: each of the 20 pairs, repeated uniformly across
`levels=2` and `levels=3` (chosen as representative of "more than one level"
without approaching the true combinatorial space) — e.g. `HSS_L2_H10_W8`
means 2 levels, each `H10`/`W8`.

## `make` targets

| Target | Coverage | In `test-all`? |
|--------|----------|-----------------|
| `test-hss` / `gen-hss` | 1 variant: `HSS_L2_H5_W8` | yes |
| `test-hss-check` / `gen-hss-check` | 16 "fast" variants (heights 5/10 × 4 Winternitz values × levels 2/3) | no |
| `test-hss-check-slow` / `gen-hss-check-slow` | 24 "slow" variants (heights 15/20/25 × 4 Winternitz values × levels 2/3) | no |

`KATNUM` controls the default target; the check targets use `CHECK_KATNUM`
(default `1`) instead — e.g. `make test-hss-check CHECK_KATNUM=3`.

### Why the default is H5, not H10

Multi-level HSS compounds LMS's per-tree cost (see `../LMS/README.md`'s
"fast vs slow" section) across every level. Measured on this host: 2-level
`H10`/`W8` keygen+sign+verify took ~10s for a *single* vector — at the
`KATNUM=100` default `make test-all` uses, that alone would add 15+ minutes
to every full run. 2-level `H5`/`W8` measured at ~0.65s/vector instead, so
that's the default here (LMS's single-level default stayed at `H10`/`W8`,
since one `H10` tree alone is cheap).

### Why "fast" vs "slow"

Same underlying cause as LMS/XMSS (O(2^height) leaf hashes per tree, no
cross-run caching): heights 5/10 are cheap per level; 15/20/25 are not.
`test-hss-check` (16 variants, `KATNUM=1`) takes on the order of a minute.
`test-hss-check-slow` is unbounded — some height-25, 3-level combinations
can take a very long time — run it deliberately, not as a sanity check.

See `hss_variant_fast[]` / `hss_variant_slow[]` in
[`src/hss_variant.c`](src/hss_variant.c) for the exact list.
