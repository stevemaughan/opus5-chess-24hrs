# Progress log — Opus 5 chess 24hrs

Start: 2026-08-21T12:43:43-04:00
Deadline: 2026-08-22T12:43:43-04:00

## Plan (set at hour 0)

**Language:** C++ (GCC 15.2.0 MinGW-w64), unity build, `-O3 -march=x86-64-v3 -flto -static`.
Reason: fastest path to a strong engine; I know the idioms cold, GCC produces good code,
static linking is trivial with `-static`.

**Architecture chosen up front (no exploration time spent):**
- Bitboards, PEXT (BMI2) sliding attacks — target CPU guarantees fast PEXT.
- Make/unmake with a state stack; incremental Zobrist.
- Pseudo-legal generation + cheap legality test using precomputed checkers/blockers.
- PVS + transposition table + null move + LMR + killers/history/continuation history
  + SEE + quiescence.
- Tapered hand-crafted eval (PeSTO base tables, which the rules explicitly permit as
  published documentation) plus mobility, pawn structure, passed pawns, king safety.
- No NNUE: training data + training loop is not affordable inside 24h and risks
  delivering nothing. A well-tuned HCE with a strong search is the higher-expected-value
  play.

**Time budget (re-planned every hour, backwards from the deadline):**
| Hours | Task |
|---|---|
| 0.0–0.5 | Setup, plan, time-keeping |
| 0.5–3.0 | Bitboard infra, movegen, perft correct on all 126 positions |
| 3.0–4.5 | UCI + alpha-beta + eval → **first deployable build in `final/`** |
| 4.5–8.0 | TT, move ordering, quiescence, null move, LMR, aspiration |
| 8.0–13.0 | Pruning suite + search parameter tuning, measured vs Stash ladder |
| 13.0–18.0 | Eval expansion + tuning |
| 18.0–22.0 | SPRT testing of remaining ideas, time-management hardening |
| 22.0–24.0 | Final build, compliance, sanity match, install to `final/` |

**Assumptions recorded:**
- fastchess `timemargin` assumed to be **0** (undisclosed). Time management is built to
  never exceed the remaining clock, with a conservative move overhead.
- Target has POPCNT/BMI1/BMI2/AVX2, no AVX-512. Compiling `x86-64-v3` only.
- `Hash 256` will be set by the harness; the TT is sized dynamically and defaults to 256.

## Hour 0 — 2026-08-21 12:43
- Created `docs/start_time.txt`, wrote this plan.
- Verified toolchain (GCC 15.2.0, Zig 0.16.0), 12 physical cores, resources present.
- Elo estimate: not yet measurable.
- Next hour: bitboard/attack infrastructure and move generation.

## Hour 2 — 2026-08-21 14:45
- Written and working: bitboards + PEXT sliders, full movegen, position/make-unmake,
  Zobrist, tapered HCE eval (PeSTO + mobility/pawns/passers/king safety/threats),
  TT, staged MovePicker with history/killers/counter-moves, PVS search with the usual
  pruning suite, UCI layer with a separate search thread.
- **Perft: 756/756 checks pass over all 126 positions, depths 1-6, 12.8 billion nodes.**
- Wrote an exhaustive `pseudo_legal()` verifier (all 65536 move encodings x 126
  positions). It found a real bug: non-promotion moves with the promotion bits set were
  accepted, so a TT key collision could inject an aliased move into the search. Fixed;
  now 0 false positives / 0 false negatives.
- fastchess `--compliance`: **all 40 checks passed**. Binary is standalone
  (KERNEL32.dll + msvcrt.dll only). First build deployed to `final/`.
- Calibration finding: at depth 13 from startpos I use 37.7k nodes vs Stash 30's 298k
  and Stash 13's 13M. With all pruning disabled I use 743k at depth 10 vs 8.9k with it
  on - an 83x reduction. The search structure is sound; the pruning is collectively far
  too aggressive. Added a `PRUNEMASK` env var to toggle each feature for bisection.
  No single feature dominates, so this needs to be settled by match results, not node
  counts.
- Elo estimate: not yet measured. Gauntlet vs Stash 17/20/21 starting now.
- Next hour: baseline strength measurement, then A/B the pruning aggressiveness.

## Correction to time-keeping
Earlier entries were labelled "Hour 2" by mistake — actual wall-clock elapsed at that
point was ~0.7 h. Tool round-trips are much faster than I assumed. Entries below use
measured elapsed time against `docs/start_time.txt`.

## Hour 0.8 — 2026-08-21 13:30
- **First strength measurement** (gauntlet, 10+0.1, Hash 256, UHO book, 78 games so far):
  | Opponent | CCRL | My score | Implied |
  |---|---|---|---|
  | stash-21 | ~2710 | 75.0% | ~2900 |
  | stash-20 | ~2510 | 86.5% | ~2840 |
  | stash-17 | ~2300 | 96.4% | ~2860 |
  Consistent across all three: **~2850-2900 Elo**, wide error bars (24-28 games each).
- This overturns my hour-0.7 worry that the search was over-pruned. The small tree is
  buying real depth (depth ~20 in 450 ms at 10+0.1). I am dropping the node-count line
  of investigation entirely — it was a bad proxy — and will settle all further search
  questions with SPRT.
- **Bug found from the match log and fixed:** fastchess warned "Bestmove does not match
  beginning of last PV". When an iteration was aborted on the clock I re-sorted the root
  move list while it held a mix of fresh `-INF` scores and stale ones from the previous
  iteration, so the engine could play a move it had never validated. Now the best line is
  published the moment a root move is confirmed (a point only reached when the search was
  not aborted), and both the PV output and `bestmove` read from it. Verified consistent.
- Elo estimate: **~2870 (+/- ~70)**.
- Next: finish the gauntlet, deploy the PV fix, then bracket the top end against
  stash-25 (~2940) and stash-30 (~3170).

## Hour 1.1 — 2026-08-21 13:50
- Final gauntlet result (240 games, 80 per opponent, 10+0.1, Hash 256):
  stash-21 21.9% / stash-20 13.1% / stash-17 6.2% against me.
  **Zero timeouts, zero crashes, zero illegal moves on my side** over 240 games
  (the single timeout in the log was stash-20's). Implied strength ~2850 +/- 80;
  the per-opponent estimates (2770 / 2840 / 2930) spread in the usual way when the
  rating gap is large.
- **TT bug found and fixed:** depth was encoded as `clamp(d+1, 0, 255)` with 0
  meaning "empty". Quiescence stores at depth 0/-1 and the static-eval-only store
  uses -6, so every one of those entries was written with `depth8 == 0` and then
  read back as unoccupied — the quiescence transposition table had never worked at
  all. Re-encoded with an offset of 8. Bench at fixed depth 12: 546k -> 499k nodes.
- Hardened the periodic clock check: it keyed off `(Nodes & 1023) == 0`, which
  qsearch can step straight over, so the hard time limit could be missed. Now a
  counter.
- Built the tuning pipeline: `datagen` (self-play, node-limited, writes quiet
  positions labelled with the game result) and a texel-style coordinate-descent
  tuner. All 253 eval parameters moved into `src/evalparams.h`; they are
  `constexpr` in the release build and mutable only under `-DTUNE`, so tuning
  costs the shipped engine nothing. Verified the round trip: the tuner regenerates
  `evalparams.h` and the engine rebuilt from it benches to the identical node count.
- Implemented correction history (learned per-pawn-structure static-eval
  adjustment), behind `-DNO_CORRHIST` so it can be A/B tested in isolation.
  Untested as yet.
- Data generation running: 25000 self-play games across 10 processes, ~1.9M quiet
  positions expected.
- Elo estimate: **~2850 (+/- 80)**.
- Next: finish datagen, run the tuner, then SPRT (a) tuned eval and (b) correction
  history separately.

## Hour 2.0 — 2026-08-21 14:45
- **Evaluation tuning works, and it is the biggest win so far.**
  - 21,900 self-play games -> 1.73M labelled quiet positions.
  - Texel coordinate descent on 253 parameters, 577k samples, K=1.1:
    error 0.100179 -> 0.094772 (-5.4%).
  - SPRT vs an otherwise byte-identical build (only `evalparams.h` differs), 10+0.1:
    **H1 accepted after 266 games, +198 Elo +/- 42 (75.75%).**
  - The size of the gain says my hand-guessed positional values were genuinely poor,
    not that the tuner is magic: e.g. the bishop pair endgame bonus moved 46 -> 86.
  - Applied and deployed to `final/`. Compliance re-checked (40/40), still standalone.
- Also fixed: the `tte` pointer was dereferenced inside the move loop for the singular
  extension test, but searching a child can evict that cluster slot, so it could be
  describing a different position by then. Depth/bound are now snapshotted before
  the loop.
- Elo estimate: previous baseline ~2850; +198 self-play Elo does not translate
  one-for-one against a field, so I will re-measure against the Stash ladder rather
  than claim a number.
- Next: tune phase 2 including the piece-square tables (PeSTO's tables were fitted
  without my extra terms, so they double-count), then re-measure against Stash.

## Hour 4.0 — 2026-08-21 16:45
- Phase 2 tuning (989 params, now including the piece-square tables, 866k samples):
  error 0.094482 -> 0.092618 (-2.0%).
- SPRT vs phase 1 at 10+0.1: **+29.6 Elo +/- 18.7 after 800 games**, LLR 1.81 and
  rising. I stopped the test there rather than spend ~35 more minutes reaching the
  formal bound: the result is ~3 SD positive and the direction is sound (PeSTO's
  tables were fitted without my extra terms, so the two were double-counting).
  **Judgement call recorded deliberately** - that CPU is worth more spent on a
  second tuning iteration than on confirming a result I am already confident in.
- Deployed to `final/` (v3), compliance re-verified.
- Cumulative measured evaluation gain so far: +198 then +30 in self-play terms.
- Next: regenerate data with this much stronger engine (better labels) and re-tune -
  tuning iteration 2 - then SPRT correction history and the search parameters.

## Hour 7.0 — 2026-08-21 19:45
- **Tuning iteration 2** (fresh data: 25,700 self-play games at 12k nodes/move with
  the v3 engine -> 2.07M positions; 989 params, 689k samples, K=0.91):
  error 0.087041 -> 0.085092 (-2.2%).
  SPRT vs iteration 1: **H1 accepted, +47.2 Elo +/- 18.3 over 778 games.**
  Regenerating data with the stronger engine was clearly worth it.
- Cumulative measured evaluation gains, in self-play Elo: +198, +30, +47.
- **Reliability bug found and fixed:** `go wtime 0 btime 0` searched to MAX_PLY and
  never returned - `useTimeManagement()` tested the clock *values*, so an all-zero
  clock switched time management off entirely. Under any harness that is a hang and
  therefore a lost game. Time management is now driven by whether the `go` command
  carried a clock token at all. Re-verified: wtime 0/1, bare `movestogo`, and
  `go infinite` + `stop` all return a legal move promptly.
- **PGO: tried and measured, no benefit.** 2.62M vs 2.65M nps over three runs each -
  within noise. Not surprising for a unity build, where GCC already has whole-program
  visibility. `source/build_final.ps1` does the PGO build and then gates on
  standalone-ness, perft to depth 5, fastchess compliance and a real 10+0.1 move;
  the deliverable is only replaced once all of that passes.
- `final/` now holds the iteration-2 build (perft 630/630, compliance 40/40,
  KERNEL32+msvcrt only).
- Next: SPRT correction history (running), then the search parameters.

## Hour 8.0 — 2026-08-21 20:45
- **Correction history: +53 Elo +/- 20 over 498 games** (SPRT [0,6], LLR 1.89, 2.7 SD).
  Stopped early - it was already in `final/` and the answer was not in doubt; the CPU
  is better spent on the next test. A learned per-pawn-structure adjustment to the
  static evaluation turns out to be worth a lot here, which fits: even a tuned
  hand-crafted evaluation has systematic biases that a running error estimate can
  cancel.
- Pre-built four search variants for A/B testing (time-management divisors, LMR
  cut-node reduction, quiescence width). Defaults were verified behaviour-identical
  to the deployed build before any variant was built.
- Elo estimate: not re-measured against the field since hour 0.8; the self-play gains
  since then total roughly +330 (+198, +30, +47, +53) which does not translate
  one-for-one. A fresh gauntlet against the Stash ladder is scheduled before the
  final build.
- Next: search parameter A/Bs, then a third tuning iteration, then re-measure.

## Hour 9.0 — 2026-08-21 21:45
- **Time-management A/B, both directions tested, both worse:**
  - more time per move (divisors 24/30 vs 28/34): **-23 Elo +/- 22** over 300 games
  - less time per move (divisors 34/40): **-27 Elo +/- 42** over 102 games
  The current allocation sits at a local optimum, so I am leaving it alone. Recorded
  as a deliberate null result rather than a missing test.
- Started data generation iteration 3 from the current `final/` build (which is
  ~330 self-play Elo stronger than the engine that produced the iteration-1 data),
  on the reasoning that better labels were what made iteration 2 worth +47.
- Deferred (will run if time allows after tuning iteration 3): LMR cut-node
  reduction, quiescence width. Both variants are already built.

## Hour 11.0 — 2026-08-21 23:45
- **Tuning iteration 3: rejected.** Error 0.087266 -> 0.085978 (-1.5%, vs -2.2% for
  iteration 2), and the SPRT went **-2.8 Elo +/- 21.7 over 498 games** - the gain had
  vanished. The tuning pipeline has saturated on this evaluation structure.
  The three iterations went **+198, +47, ~0**; that is the whole story of texel
  tuning here, and it is worth recording that I stopped rather than run a fourth.
  `final/` keeps the iteration-2 parameters.
- Switched the remaining budget to search parameters, which are still entirely
  untested. Added -D knobs for singular depth, null-move base reduction, RFP and
  futility margins, and the late-move-pruning base, each verified
  behaviour-identical at its default.
- Running: softer LMR cut-node reduction (2048 -> 1536).

## Hour 14.0 — 2026-08-22 02:45
- **Diagnosed and fixed an overfitting failure in the tuner, worth +55.6 Elo.**
  Inspecting the tuned parameters showed the king-safety block had gone pathological:
  `KSLinearDiv = -2` (a *divisor*, sitting behind a `max(1, x)` clamp, so the tuner
  discovered it could walk off the cliff and effectively divide by one),
  `KSShelterScale = -4` (sign inverted), and `KingAttackWeight[QUEEN] = -82` — a
  negative penalty for a queen attacking the king, which is nonsense.
  Free integers in a denominator give the optimiser a discontinuous surface to
  exploit, and it duly did.
  Fixes: every king-safety knob is now a **multiplier**, never a divisor; the two
  shifts are fixed powers of two; and `KingAttackWeight` is no longer tuned at all
  (with both the per-piece weights and the multipliers free the fit is
  under-determined).
  Re-tuned: error 0.088189 -> 0.085752, which is a *better* fit than the old
  parameterisation reached (0.085978) despite having six fewer free parameters —
  confirming the old fit was exploiting the structure rather than modelling king
  safety. The resulting values are all sane and positive.
  **SPRT: +55.6 Elo +/- 17.9 over 700 games** (LLR 2.63, 3.1 SD).
- Deployed. `final/` verified: standalone (KERNEL32 + msvcrt only), perft 630/630,
  compliance 40/40, correct id name/author, legal move on a 10+0.1 clock.
- Note: `build_final.ps1`'s last verification step hung when invoked through a nested
  background pipe; the same check passes instantly when run directly, so it is a
  harness-plumbing artefact, not an engine problem. Verified the staged binary by
  hand before copying it into `final/`.
- Next: fresh gauntlet against the Stash ladder for an honest Elo estimate.

## Hour 15.3 — 2026-08-22 04:00  — STRENGTH MEASUREMENT
Gauntlet, 450 games (150 per opponent), tc=10+0.1, Hash 256, UHO book, both colours:

| Opponent | CCRL Blitz | My score | Elo diff | Implied |
|---|---|---|---|---|
| stash-30 | ~3170 | 61.7% | +83  | ~3253 |
| stash-25 | ~2940 | 86.3% | +320 | ~3260 |
| stash-21 | ~2710 | 91.7% | +417 | ~3127 |

The two nearest opponents agree closely (~3255). The stash-21 figure is lower because
Elo differences compress when scores approach 100%, so the closest opponent is the
informative one.

**Best estimate: ~3200-3250**, and I want to be explicit about why that is not a
precise claim: the Stash anchors are CCRL ratings at 2'+1", not at 10+0.1, and there
is no reason a rating gap measured at one time control transfers exactly to another.
A small, fast-evaluating engine like this one plausibly does relatively better at very
fast controls. Treat ~3200 as the defensible number and ~3255 as the optimistic end.

**Reliability: zero timeouts, zero crashes, zero illegal moves in 450 games.** All 24
"bestmove does not match PV" warnings in the log came from stash-25 and stash-30 -
none from my engine, so the hour-0.8 fix is holding.

- Next: one more data/tune cycle using the current (much stronger) engine, then final
  verification with a wide time margin.

## Note on the hour labels
The headings above were written from my own running estimate and drifted ahead of the
clock; measured against `docs/start_time.txt` the gauntlet entry labelled "Hour 15.3"
actually happened at about hour 12.5. The measured figure at the time of writing this
note is **13.5 h elapsed, 10.5 h remaining**. Entries from here on quote the measured
value only.

## 13.5 h elapsed (measured) — evaluation tuning has saturated
- Tuning cycle 4 (fresh data from the post-king-safety-fix engine, 1.96M positions,
  error 0.087601 -> 0.086394): **rejected.** SPRT ran to 998 games and the estimate
  drifted +20 -> +11 -> +7 -> **+6.3 +/- 14.5** as games accumulated, which is what an
  effect near zero looks like. Shipping a change I cannot demonstrate helps, this late,
  is a bad trade.
- Final tally for the tuning pipeline: **+198, +47, rejected, +56 (king-safety
  restructure), rejected.** Four accepted changes out of six attempts, and the two
  rejections both came after the error metric said "better". Worth stating plainly:
  a lower tuning error is not the same thing as more Elo, and the SPRT is the only
  thing that settled it.
- Remaining time is going to search-parameter tests and final verification.

## 15.2 h elapsed (measured) — search parameters are at a local optimum
Every search A/B came back neutral or worse, so nothing was changed:
| Change | Result |
|---|---|
| more time per move (div 24/30) | -23 Elo +/- 22 (300 games) |
| less time per move (div 34/40) | -27 Elo +/- 42 (102 games) |
| LMR cut-node reduction 2048 -> 1536 | -6.9 Elo +/- 26 (302 games) |
| quiescence width 2 -> 4 | +7.6 Elo +/- 18 (504 games), 0.4 SD - not shipped |
| singular extension depth 6 -> 5 | -8.1 Elo +/- 23 (300 games) |

I adopted an explicit rule for the endgame of the run: **ship a change only if its
point estimate is at least 1.5 SD above zero.** Two changes (tuning cycle 4 at 0.43 SD,
quiescence width at 0.42 SD) were positive but failed that bar and were dropped. Late
in a fixed-time run the asymmetry matters - an unverified change that turns out
negative costs more than a marginal one gains.

## Final verification of the deliverable
- `id name Opus 5 chess 24hrs`, `id author Opus 5` — correct.
- Dependencies: **KERNEL32.dll and msvcrt.dll only** — fully standalone.
- **perft suite on the shipped binary: 756/756 checks, 0 failures, 12.8 billion nodes**
  (all 126 positions, depths 1-6).
- fastchess `--compliance`: 40/40.
- `setoption name Hash value 256` from cold, exactly as the rating harness sets it:
  reaches depth 20 in 463 ms at 10+0.1.
- Clock edge cases all return a legal move in ~120 ms: `wtime 0`, `wtime 1`,
  `wtime 20`, `movetime 1`, `depth 1`, bare `movestogo 40`, `movestogo 1`.
- `go infinite` + `stop` returns promptly; unknown commands and unknown options are
  ignored rather than fatal.

## 16.9 h elapsed (measured) — second strength measurement
Gauntlet 3, 700 games (350 per opponent), tc=10+0.1, Hash 256:

| Opponent | CCRL Blitz | My score | Elo diff | Implied |
|---|---|---|---|---|
| stash-30 | ~3170 | 60.6% | +75  | ~3245 |
| stash-25 | ~2940 | 82.6% | +270 | ~3210 |

Pooled with gauntlet 2, that is **1150 games against the ladder**:
- vs stash-30: 500 games, ~60.9% -> ~3247
- vs stash-25: 500 games, ~83.7% -> ~3223

**Settled estimate: ~3200-3250, best single point ~3230**, with the standing caveat
that the Stash anchors are CCRL ratings at 2'+1" rather than at 10+0.1.

Now running one last experiment: the two changes that were positive but individually
below my 1.5 SD shipping bar (tuning cycle 4, +6.3; quiescence width, +7.6) combined
into a single candidate, so that a ~+14 effect can actually be resolved. If it clears
the bar it ships; if not, `final/` stays exactly as it is.

## 18.6 h elapsed (measured) — the combined-candidate experiment failed, as the bar predicted
- Combining tuning cycle 4 with the wider quiescence search - the two changes that were
  positive but below the 1.5 SD bar - gave **-2.6 Elo +/- 23.6 over 398 games**. Both
  were noise, and the shipping bar was the right call. Nothing changed in `final/`.
- Aspiration window 10 -> 20: **+4.3 Elo +/- 21.4 over 400 games** (0.2 SD). Neutral.
- That makes seven consecutive search/eval experiments with no measurable effect. The
  engine is at a local optimum for its architecture, and I am treating further
  parameter search as low-value: the remaining budget goes to one last margin test,
  a reliability soak, and documentation, keeping a wide safety margin before the
  deadline rather than risking the deliverable for a coin-flip.

## 19.7 h elapsed (measured) — experimentation stopped
- Reverse-futility margin 86 -> 70: started at +17.4 +/- 28.8 (200 games) then settled
  to **-7.8 +/- 20.7 at 402 games**. Rejected. That is eight consecutive experiments
  with no measurable effect, which is the clearest possible signal that the engine sits
  at a local optimum for its architecture.
- **Decision: stop experimenting with 4 h left.** Continuing to sample a parameter space
  with a demonstrated ~0% hit rate is a poor use of the remaining budget, and every
  change carries a small risk of breaking a deliverable that is currently verified and
  measured. The rules ask me not to stop early; they also ask me never to risk what is
  in `final/`. Spending the tail on verification, a reliability soak and documentation
  is the allocation I would defend.
- Deliverable identity confirmed: SHA-256 `EE98D3DD...A67D324`, byte-identical to the
  build that passed the +55.6 Elo SPRT and both gauntlets.
- Running a final 600-game soak against stash-30 for extra reliability evidence and a
  tighter Elo figure.

## 21.2 h elapsed (measured) — final reliability soak
600 games against stash-30 at 10+0.1: **58.75% (+61.4 Elo +/- 23.3)**, and
**zero timeouts, zero crashes, zero illegal moves**.

Pooled across all three measurement runs, **1100 games against stash-30 alone**
(150 + 350 + 600) at a combined ~59.7%, i.e. about +68 Elo over a ~3170 CCRL engine.
Total games played against the ladder across the run: **1750**, with no reliability
fault of any kind on my side.

**Final Elo estimate: ~3230**, honest range ~3150-3280. The dominant uncertainty is
not statistical - it is that the Stash anchors are CCRL ratings at 2'+1" and the match
is at 10+0.1.

## Last change: info line on a best-move change
The soak surfaced two "bestmove does not match last PV" warnings. Diagnosed: a *new*
best root move had been confirmed part-way through an iteration (it beat alpha at
greater depth) and the clock then cut that iteration short, so the move played was
newer than the last PV printed. Playing it is correct - it is the better move - but
the protocol asks for an info line whenever the best move changes, and without one the
output contradicts itself.
Fixed by reporting immediately when the best root move changes. Verified: search
behaviour byte-identical (same bench node count at fixed depth), and 0/10 PV/bestmove
mismatches on timed searches that previously produced them.
