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
