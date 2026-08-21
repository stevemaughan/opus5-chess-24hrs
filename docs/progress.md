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
