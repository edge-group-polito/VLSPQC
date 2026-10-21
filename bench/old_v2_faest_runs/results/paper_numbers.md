# Number -> CSV cell map for paper_paragraph.md / paper_table.tex

M = `bench/results_median3/results.csv` (per-metric median of runs 1-3; runs = `bench/results/results.csv`, `bench/results_run2/results.csv`, `bench/results_run3/results.csv`).
S = `bench/results_median3/spread.csv` (per metric: median, min, max over the runs, relative spread, identical-in-all-runs flag).
Cell notation: M[variant].column. Derived numbers are ratios of two M cells (rounded as shown).
Table cells (paper_table.tex, paper_table_full.tex) are copied programmatically from M by `tools/bench/make_paper_table.py`; their rows are listed in `paper_table_cells.md`. Peak mem = max over ops of (heap_<op>_KiB + stack_<op>_KiB), same M row.

## Which source for which metric
- pk_B, sk_B, sig_B: identical in all three runs (median_runs.py stops otherwise); S column identical_in_all_runs = yes. Values equal run 1 (`bench/results/results.csv`).
- heap_*_KiB, stack_*_KiB, text_B, data_B, bss_B: identical in all three runs (checked over all 39 variants: 0 non-identical); median = run 1 value.
- keygen/sign/verify ms and Mcycles: differ between runs, median of three. vmhwm_* (also differ slightly, <= 9 %) are in M but not used in the text.

## Paragraph (a) and discussion (c)
| Claim in text | Value | Source |
|---|---|---|
| 39 variants | 39 | row count of M |
| FAEST pk 32 B = smallest pk of all variants | 32 | M[FAEST-*].pk_B; next smallest M[MQOM-gf2-shorter-ct].pk_B = 52 |
| pk+sk 64 B smallest | 64 | M[FAEST-*].pk_B + sk_B; next smallest MQOM-gf2-shorter-* 52+72 = 124 |
| only MAYO sk smaller (24 B) | 24 | M[MAYO-1].sk_B, M[MAYO-2].sk_B; all FAEST sk_B = 32 (QR-UOV and UOV-pkc-skc also 32) |
| FAEST sig 3906 to 5924 B | 3906 / 5924 | M[FAEST-EM-128s].sig_B / M[FAEST-128f].sig_B |
| MQOM sig 2492 to 3316 | | M[MQOM-gf2-shorter-*].sig_B / M[MQOM-gf16-fast-*].sig_B |
| SDitH sig 3721 to 4914 | | M[SDitH-short*].sig_B / M[SDitH-fast].sig_B |
| at most 541 B for MAYO, QR-UOV, SNOVA, SQIsign, UOV | 541 | max sig_B of those families = M[QR-UOV-1q127L10-*].sig_B (MAYO max 464, SNOVA 528, SQIsign 200, UOV 135) |
| FAEST-128f largest signature | 5924 | max of M.sig_B |
| keygen about 0.002 ms | 0.0018 to 0.0019 | M[FAEST-*].keygen_ms (128f 0.0019, others 0.0018) |
| "more than ten times less than any other scheme; next SDitH 0.0269" | 0.0269 | M[SDitH-fast-cipherpow].keygen_ms (smallest non-FAEST); 0.0269/0.0019 = 14.2. Worst single run: S[FAEST-128f,keygen_ms].max 0.0024 vs S[SDitH-fast-cipherpow,keygen_ms].min 0.0256 = 10.7 |
| FAEST-EM-128f sign 10.8 / verify 7.56 ms | 10.7784 / 7.5628 | M[FAEST-EM-128f].sign_ms / verify_ms (fastest FAEST variant in both) |
| sign 2.1 / 2.6(SNOVA) / 3.6 (QR-UOV) / 10.6 (MAYO) / 46 (UOV) | | 10.7784 / M[MQOM-gf16-fast-ct].sign_ms 5.0579 = 2.13; / M[SNOVA-I-S-AES] 4.2075 = 2.56; / M[QR-UOV-1q127L3-shake] 2.9882 = 3.61; / M[MAYO-2] 1.0207 = 10.56; / M[UOV-Is-pkc] 0.233 = 46.3 |
| verify 2.1 to 9.3 | | 7.5628 / M[MQOM-gf16-fast-ot].verify_ms 3.6394 = 2.08; / M[QR-UOV-1q127L3-shake] 3.0695 = 2.46; / M[SNOVA-I-S-AES] 1.4981 = 5.05; / M[MAYO-2] 0.8116 = 9.32 |
| best SDitH 2.1x (sign), 2.8x (verify) slower | | M[SDitH-fast].sign_ms 22.9716 / 10.7784 = 2.13; M[SDitH-fast-cipherpow].verify_ms 21.4948 / 7.5628 = 2.84 |
| SQIsign sign 3.6 times slower | | M[SQIsign-p324-3].sign_ms 38.4466 / 10.7784 = 3.57 |
| SQIsign verify ratio 1.47 (no claim) | | M[FAEST-EM-128f].verify_ms 7.5628 / M[SQIsign-p324-3].verify_ms 5.1443 = 1.47 |
| FAEST sign peak heap+stack 354.1 to 2985.7 KiB | | table column Peak mem of FAEST-EM-128f (350.1 heap + 4 stack, sign) and FAEST-128s (2981.7 + 4, sign) |
| "comparable to MQOM and SDitH" | | Peak mem: MQOM-gf16-fast-ct 414.6, MQOM-gf2-shorter-ct 3099.8, SDitH-fast 338.8, SDitH-short 963.0 (table); qualitative statement |
| EM faster/smaller: sign 10.8 vs 14.6 ms, sig 5060 vs 5924 B | | M[FAEST-EM-128f].sign_ms 10.7784, M[FAEST-128f].sign_ms 14.6193, sig_B 5060 / 5924 |
| 2464.3 / 2981.7 KiB heap at sign | | M[FAEST-EM-128s].heap_sign_KiB, M[FAEST-128s].heap_sign_KiB |
| UOV-Is-pkc: sig 96 B, pk 66576 B | | M[UOV-Is-pkc].sig_B, pk_B |

## Methodology paragraph (d)
| Claim | Value | Source |
|---|---|---|
| CPU, gcc, flags, pinned CPU, 3 s budget | Core Ultra 7 155H, gcc 13.3.0, -O2, cpu 2, 3 s | `bench/results*/meta.json` (cpu_model, gcc, cflags, cpu, time_budget_s) |
| median over up to 1000 calls, min 5 | | `tools/bench/run_bench.py` / `bench/README.md`; per-variant `iters` in `bench/results/raw/*.json` |
| 1000-byte message | | bench harness MSG_LEN (bench/README.md) |
| run-to-run median pair difference 9.0 % / 26.7 % / 18.1 %, max 111.6 / 82.3 / 80.6 | | computed from the three results.csv over 117 (variant, op) time medians as |a-b|/min(a,b): run2 vs run3 9.0 (max 111.6), run1 vs run2 26.7 (82.3), run1 vs run3 18.1 (80.6). Reproduce: same formula as `tools/bench/report.py --compare` |
| per-metric spread median 24.7 % / 26.7 % / 28.2 % (sign / verify / keygen), max 69.6 / 76.8 / 85.6 | | S rel_spread_pct over the 39 variants for sign_ms / verify_ms / keygen_ms (median, max). Caption of paper_table.tex rounds these to 25--28 % and 86 % |
| load notes: run 1 about one busy core of editor load | | `bench/results/meta.json` load_notes. Run 2 load_notes: mean user 7.5 %. Run 3 meta.json has NO load_notes field: "near idle" for run 3 is the operator's statement, not recorded by vmstat in that file |
| KAT / provenance statements | | not CSV values: .claude/status/T2-T10.md (T9 'size check', T10 M1; MQOM provenance from T1) |
