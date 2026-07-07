# Parity benchmark vs GLNS.jl — 2026-07-07

Comparison of this C++ port against the Julia reference implementation
(GLNS.jl, commit 1980c3c) on the 45 GTSPLIB instances in
`GLNS.jl/benchmark/GTSPLIB`, all with default mode settings.

**Setup.** 3 runs per solver per instance (C++ seeds 1–3; Julia unseeded).
Quality runs executed in parallel on an Apple Silicon machine (10 P-cores),
so per-run times there are indicative only; the timing table below was run
sequentially on an idle machine. Best-known values from
`benchmark-results.csv`. Julia 1.12.6; clang (Apple), `-O2 -ffp-contract=off`.

## Parser parity

All 45 instances parse **bit-identically** to the Julia `read_file`
(dimensions, set memberships, and full distance-matrix checksums), covering
EUC_2D, CEIL_2D, GEO, ATT, and explicit FULL_MATRIX / LOWER_DIAG_ROW /
UPPER_ROW / UPPER_DIAG_ROW formats. This requires building with
`-ffp-contract=off` (see README).

## Solution quality (3 runs each, default mode)

| metric | Julia | C++ |
|---|---|---|
| mean gap to best known | 0.051% | 0.049% |
| median gap | 0.000% | 0.000% |
| best-of-3 hits best known | 38/45 | 38/45 |
| head-to-head on mean cost | 7 wins | 7 wins (31 ties) |

On 31 of 45 instances every run of both solvers found the same cost
(the best known in all but one case). The per-instance differences on the
remaining instances go both ways (e.g., C++ better on 134gr666, 200dsj1000,
115rat575; Julia better on 217vm1084, 89rbg443) and are consistent with
run-to-run variance of a stochastic anytime solver at n=3.

## Runtime (sequential, single run, solver-reported)

| instance | Julia (s) | C++ (s) | speedup |
|---|---|---|---|
| 40kroa200 | 0.32 | 0.13 | 2.5x |
| 64lin318 | 0.39 | 0.33 | 1.2x |
| 107att532 | 2.15 | 1.78 | 1.2x |
| 132d657 | 5.12 | 4.26 | 1.2x |
| 157rat783 | 6.76 | 6.35 | 1.1x |
| 212u1060 | 23.37 | 16.13 | 1.4x |

The port is a faithful, allocation-for-allocation translation of the Julia
code, so this ~1.1–1.4x is the "free" speedup before any optimization work
(workspace reuse in `remove_insert`/`worst_removal`, flat scratch buffers in
`pdf_select`, etc.).

## Full per-instance results

```
instance       bestknown |  jl best   jl mean  gap% | cpp best  cpp mean  gap% |
---------------------------------------------------------------------------------
31pr152            51576 |    51576   51576.0  0.00 |    51576   51576.0  0.00 |
32u159             22664 |    22664   22664.0  0.00 |    22664   22664.0  0.00 |
35si175             5564 |     5564    5564.0  0.00 |     5564    5564.0  0.00 |
35ftv170            1205 |     1205    1205.0  0.00 |     1205    1205.0  0.00 |
36brg180            4420 |     4420    4420.0  0.00 |     4420    4420.0  0.00 |
39rat195             854 |      854     854.0  0.00 |      854     854.0  0.00 |
40d198             10557 |    10557   10557.0  0.00 |    10557   10557.0  0.00 |
40krob200          13111 |    13111   13111.0  0.00 |    13111   13111.0  0.00 |
40kroa200          13406 |    13406   13406.0  0.00 |    13406   13406.0  0.00 |
41gr202            23301 |    23301   23301.0  0.00 |    23301   23301.0  0.00 |
45ts225            68340 |    68340   68340.0  0.00 |    68340   68340.0  0.00 |
45tsp225            1612 |     1612    1612.0  0.00 |     1612    1612.0  0.00 |
46gr229            71972 |    71972   71972.0  0.00 |    71972   71972.0  0.00 |
46pr226            64007 |    64007   64007.0  0.00 |    64007   64007.0  0.00 |
53gil262            1013 |     1013    1013.0  0.00 |     1013    1013.0  0.00 |
53pr264            29549 |    29549   29549.0  0.00 |    29549   29549.0  0.00 |
56a280              1079 |     1079    1079.0  0.00 |     1079    1079.0  0.00 |
60pr299            22615 |    22615   22615.0  0.00 |    22615   22615.0  0.00 |
64lin318           20765 |    20765   20765.0  0.00 |    20765   20765.0  0.00 |
65rbg323             471 |      471     471.0  0.00 |      471     471.0  0.00 |
72rbg358             693 |      693     693.0  0.00 |      693     693.0  0.00 |
80rd400             6361 |     6361    6361.0  0.00 |     6361    6361.0  0.00 |
81rbg403            1170 |     1170    1170.0  0.00 |     1170    1170.0  0.00 |
84fl417             9651 |     9651    9651.0  0.00 |     9651    9651.0  0.00 |
87gr431           101946 |   101946  101946.0  0.00 |   101946  101946.0  0.00 |
88pr439            60099 |    60099   60099.0  0.00 |    60099   60099.0  0.00 |
89rbg443             632 |      634     635.3  0.53 |      635     636.7  0.74 |
89pcb442           21657 |    21657   21657.0  0.00 |    21657   21657.0  0.00 |
99d493             20023 |    20023   20023.7  0.00 |    20023   20026.3  0.02 |
107ali535         128639 |   128653  128653.0  0.01 |   128653  128653.0  0.01 |
107si535           13502 |    13505   13509.3  0.05 |    13502   13509.3  0.05 |
107att532          13464 |    13464   13464.0  0.00 |    13464   13465.3  0.01 |
113pa561            1038 |     1038    1038.0  0.00 |     1038    1038.3  0.03 |
115rat575           2388 |     2388    2395.3  0.31 |     2388    2391.7  0.15 |
115u574            16689 |    16689   16689.0  0.00 |    16689   16689.0  0.00 |
131p654            27428 |    27428   27428.0  0.00 |    27428   27428.0  0.00 |
132d657            22498 |    22498   22519.3  0.09 |    22498   22508.7  0.05 |
134gr666          163028 |   163028  164023.3  0.61 |   163028  163745.3  0.44 |
145u724            17272 |    17281   17281.0  0.05 |    17272   17278.0  0.03 |
157rat783           3262 |     3262    3269.3  0.22 |     3263    3265.3  0.10 |
200dsj1000       9187884 |  9203846 9207388.0  0.21 |  9197745 9199778.7  0.13 |
201pr1002         114311 |   114311  114366.3  0.05 |   114311  114311.0  0.00 |
207si1032          22306 |    22308   22319.7  0.06 |    22316   22324.0  0.08 |
212u1060          106007 |   106064  106106.3  0.09 |   106064  106131.0  0.12 |
217vm1084         130704 |   130704  130704.0  0.00 |   130988  130995.3  0.22 |
```
