# Opus 5 chess 24hrs

A UCI chess engine written from scratch in a single 24-hour session by one language
model (Opus 5), as an entry in the **24-Hour Chess Engine Benchmark**.

Estimated strength: **~3250 Elo** at the benchmark's time control (range ~3180–3320,
see [Estimated strength](#estimated-strength) for what that number does and does not
mean).

---

## What this is

The benchmark gives a model 24 hours of wall-clock time and one instruction: produce
the strongest possible UCI chess engine. The rules in [`CLAUDE.md`](CLAUDE.md) in full,
but the essentials:

- **Everything is written from scratch in the session.** Published documentation is
  fair game — chessprogramming.org, papers, forum posts, and published tables such as
  PeSTO's piece-square tables. Copying source code from an existing engine is not, nor
  is using any pre-existing network, opening book or tablebase.
- **Standard library only**, no third-party dependencies. C, C++ or Zig.
- **The only thing scored is playing strength** of the executable left in `final/`.
  Not code quality, not documentation, not feature count.
- **Rated at `tc=10+0.1`** (10 seconds sudden death plus 0.1 s increment),
  single-threaded, `Hash` forced to 256 MB, using fastchess with an unbalanced opening
  book, against a field including the supplied Stash versions.
- A loss on time, a crash, an illegal move or a protocol hang all count as a **loss**.
  Reliability is part of strength.
- **Managing the 24 hours is itself part of what is measured.** What to build, what to
  measure, what to accept on judgement, and what to leave out are all the model's
  decisions.

The complete hour-by-hour record of the run — including the wrong turns — is in
[`docs/progress.md`](docs/progress.md).

---

## The engine

| | |
|---|---|
| **Name** | `Opus 5 chess 24hrs` |
| **Author** | `Opus 5` |
| **Language** | C++20 |
| **Compiler** | GCC 15.2.0 (MinGW-w64) |
| **Binary** | `final/Opus5chess24hrs.exe`, statically linked, x86-64-v3 |
| **Threads** | Single-threaded search (plus the main thread for UCI I/O) |

### Why C++

Not an interesting decision, and it was made in about a minute. C++ has the idioms this
problem wants (bitboard intrinsics, templates over colour and move-generation type,
zero-cost abstraction), GCC produces good code for it, and `-static` makes a standalone
Windows binary trivially. Zig would have been fine too; nothing about the engine depends
on the choice. Spending time deliberating would have been the mistake.

### Building

The whole engine is a **unity build**: one translation unit that `#include`s the others,
so the optimiser sees everything without needing LTO.

```sh
g++ -O3 -march=x86-64-v3 -static -std=c++20 -DNDEBUG -fno-math-errno \
    -Wl,--stack,16777216 -o final/Opus5chess24hrs.exe source/src/main.cpp
```

`source/build_final.ps1` does this with profile-guided optimisation and then gates the
result on the full verification suite (standalone, perft, compliance, a real move on a
clock), only replacing the deliverable once everything passes. PGO was measured at
**no benefit** on this build (2.62M vs 2.65M nps, within run-to-run noise) — unsurprising
for a unity build where GCC already has whole-program visibility — so the shipped binary
is the plain `-O3` one above.

The large linker stack (`-Wl,--stack,16777216`) is deliberate: search recursion to
`MAX_PLY` with a ~2 KB `MovePicker` per frame needs more than the default.

### Running

A standard UCI engine — point any GUI at the executable.

```
> uci
id name Opus 5 chess 24hrs
id author Opus 5
option name Hash type spin default 256 min 1 max 32768
option name Threads type spin default 1 min 1 max 1
option name Move Overhead type spin default 25 min 0 max 5000
option name Clear Hash type button
uciok
```

| Option | Default | Notes |
|---|---|---|
| `Hash` | 256 | Transposition table size in MB |
| `Threads` | 1 | Accepted and ignored; the search is single-threaded |
| `Move Overhead` | 25 | Milliseconds held back per move for scheduling and I/O latency |
| `Clear Hash` | — | Button |

Non-UCI commands useful for development: `bench [depth]`, `perft <depth>`,
`perftsuite <file> <depth>`, `eval`, `d`, and `datagen <games> <nodes> <out> <seed>`.

---

## Architecture

### Board and move generation

Bitboards throughout. Sliding attacks use **BMI2 `PEXT`** (the target CPU is specified to
have fast PEXT), with a plain magic-multiply path compiled in when `__BMI2__` is absent so
the code still builds and runs elsewhere. Make/unmake with a `StateInfo` chain, incremental
Zobrist keys, and a separate pawn key.

Moves are generated pseudo-legally and filtered by a cheap legality test against
precomputed checkers and pinned-piece (`blockersForKing`) bitboards; check evasions get
their own generator. Moves are 16 bits (from, to, promotion type, move type).

**The move generator is exhaustively verified.** The perft suite — all 126 positions in
`resources/perft/perft.epd`, depths 1 through 6 — passes completely:

```
perft suite: 756 checks, 0 failures, 12,814,553,817 nodes
```

Perft does not exercise `pseudo_legal()`, the function that validates a possibly-corrupt
transposition-table move, so I wrote a separate exhaustive verifier
(`source/src/selfcheck_main.cpp`) that checks all 65,536 possible 16-bit move encodings
against the real generator, in every one of the 126 positions. It immediately found a real
bug — non-promotion moves with the promotion bits set were being accepted, so a 16-bit key
collision could smuggle an aliased move into the search — and after the fix reports
**0 false positives and 0 false negatives**.

### Search

Principal variation search in an iterative-deepening loop with aspiration windows.

- **Transposition table** — 10-byte entries, three per 32-byte cluster, replacement by
  depth minus age, 16-bit key verification.
- **Selectivity** — null move pruning (with a verification search at high depth),
  ProbCut, razoring, reverse futility, futility pruning, late move pruning, SEE-based
  pruning of both captures and quiets, late move reductions, internal iterative
  reduction.
- **Extensions** — singular extensions (with multi-cut and negative extensions),
  check extensions.
- **Move ordering** — TT move, then a staged move picker: good captures by MVV plus
  capture history, killers and the counter-move, quiets scored by butterfly history,
  pawn-structure history and six plies of continuation history (with threat-awareness
  bonuses), then losing captures.
- **Quiescence** — captures and promotions, SEE-filtered, with delta pruning and full
  evasion generation when in check.
- **Correction history** — a learned per-pawn-structure adjustment to the static
  evaluation. The table stores a running estimate of how wrong the evaluation tends to
  be for a given pawn structure; the transposition table always stores the *raw* value
  and the correction is applied on read, so a stale correction can never be baked in.
  Worth **+53 Elo** here, which is a lot, and consistent with a hand-crafted evaluation
  having systematic biases that a running error estimate can cancel.

### Evaluation

A tapered hand-crafted evaluation, interpolated between middlegame and endgame by a
24-point material phase.

Material and piece-square tables started as **PeSTO's** (published on the Chess
Programming Wiki, which the rules explicitly permit). On top of that: mobility per piece
type, pawn structure (isolated, doubled, backward, connected/phalanx, behind a pawn
hash), passed pawns with king proximity and path-safety terms, king safety from a
shelter/storm model plus a weighted attacker/safe-check danger score, threats, outposts,
rooks on open and semi-open files and the seventh, the bishop pair, a trapped-rook term,
and scale-down factors for drawish endgames (opposite-coloured bishops, few or no pawns).

**All 983 evaluation parameters were then fitted to self-play results** (see below).
They live in one file, `source/src/evalparams.h`, which is `constexpr` in the release
build and mutable only under `-DTUNE`, so tuning costs the shipped engine nothing.

### Time management

Allocation is proportional rather than budgeted: a fixed fraction of the remaining clock
plus most of the increment, capped at 30% of what is left, with a hard ceiling of half
the remaining clock and never less than `Move Overhead` held back. Because it is
proportional it decays gracefully and cannot run the clock to zero. On top of that the
target is scaled by best-move instability, best-move stability across iterations, and a
falling evaluation.

The clock is checked on a node counter, not on a bitmask of the node count — an exact
`(nodes & 1023) == 0` test can be stepped over entirely by quiescence and let the search
blow past the hard limit.

### Deliberately left out

- **NNUE.** The single biggest possible gain, and the one thing I decided against
  immediately. It needs a training pipeline, a large amount of data, and a training run,
  and getting it wrong means delivering nothing. A well-tuned hand-crafted evaluation
  behind a strong search was the higher expected value inside 24 hours. I stand by the
  call, but it is the obvious ceiling on this engine.
- **Pondering, MultiPV, Chess960, tablebases, an opening book** — not required, no Elo
  in this format.
- **Multi-threaded search** — the benchmark is single-threaded.

---

## How the 24 hours were spent

| Hours | What |
|---|---|
| 0.0–0.7 | Plan, bitboards, move generation, **perft passing to depth 6**, evaluation, search, UCI |
| 0.7–0.8 | First build in `final/`, compliance passed, **first measurement: ~2850 Elo** |
| 0.8–2.0 | Transposition-table bug fix, self-play data generation, tuner |
| 2.0–4.0 | Evaluation tuning **+198**, piece-square tuning **+30** |
| 4.0–8.0 | Tuning iteration 2 **+47**, correction history **+53** |
| 8.0–13.5 | Tuning iteration 3 **rejected**; time management, LMR, quiescence width, singular depth all **null** |
| 13.5–15.5 | King-safety overfitting diagnosed and restructured **+56** |
| 15.5–20.0 | 1750-game measurement; tuning iteration 4 **rejected**; more null search tests |
| 20.0–22.5 | Fine-granularity tuning **+40**, start-up stall fix, final verification |

### What was measured versus taken on judgement

Everything that shipped was measured. Six changes were accepted:

| Change | Result | Games |
|---|---|---|
| Evaluation tuning, iteration 1 | **+198 ± 42** | 266 |
| Piece-square table tuning | **+29.6 ± 18.7** | 800 |
| Correction history | **+53 ± 20** | 498 |
| Evaluation tuning, iteration 2 | **+47.2 ± 18.3** | 778 |
| King-safety restructure + re-tune | **+55.6 ± 17.9** | 700 |
| Fine-granularity tuning (step 2) | **+39.7 ± 18.6** | 598 |

And eight were rejected, every one of them after measurement:

| Change | Result |
|---|---|
| Tuning iteration 3 | −2.8 ± 21.7 (498 games) |
| Tuning iteration 4 | +6.3 ± 14.5 (998 games) — 0.4 SD, below bar |
| Quiescence width 2 → 4 | +7.6 ± 17.9 (504 games) — 0.4 SD, below bar |
| Those last two **combined** | −2.6 ± 23.6 (398 games) |
| More time per move | −23 ± 22 (300 games) |
| Less time per move | −27 ± 42 (102 games) |
| Softer LMR at cut nodes | −6.9 ± 26 (302 games) |
| Singular extension depth 6 → 5 | −8.1 ± 23 (300 games) |
| Wider aspiration window | +4.3 ± 21.4 (400 games) |
| Lower reverse-futility margin | −7.8 ± 20.7 (402 games) |

Two things are worth drawing out of that table.

**A lower tuning error is not the same thing as more Elo.** Iterations 3 and 4 both
reduced the tuner's objective and both failed in play. Only the match settled it.

**Late in a fixed-time run I adopted an explicit shipping bar: a point estimate at least
1.5 SD above zero.** Tuning iteration 4 (+6.3) and the wider quiescence search (+7.6)
were both positive and both failed that bar. Combining them into one candidate — so that
a ~+14 effect could actually be resolved — measured −2.6 over 398 games. Both were noise,
and the bar was right.

### The most interesting bug

After four tuning cycles I read the tuned parameters rather than just the error number,
and the king-safety block had gone pathological: `KSLinearDiv = -2` (a *divisor*, sitting
behind a `max(1, x)` clamp — the optimiser had found the cliff and walked off it, making
the division a no-op), the shelter term had inverted its sign, and
`KingAttackWeight[QUEEN]` had gone to **−82**, i.e. a queen attacking the king made the
king *safer*.

Free integers in a denominator give coordinate descent a discontinuous surface to exploit,
and it duly did. The fix was to make every king-safety knob a **multiplier**, fix the two
shifts at powers of two, and stop tuning the per-piece attack weights (with both those and
the multipliers free, the fit is under-determined). The restructured version reached a
*better* fit — 0.085752 against 0.085978 — with six fewer free parameters, which is what
you would expect if the old fit had been exploiting the structure rather than modelling
king safety. **+55.6 Elo.**

### What went wrong

- I spent about 45 minutes early on convinced the search was catastrophically
  over-pruned, because at depth 13 it used 37k nodes where Stash 30 used 298k. I built a
  runtime prune-mask and bisected every pruning feature. The conclusion was that node
  count at fixed depth is a bad proxy, and the first real match — 78 games — showed the
  engine at ~2850. That time was wasted; I should have measured Elo first and diagnosed
  second.
- My running estimate of elapsed time repeatedly drifted ahead of the clock, twice by
  more than two hours. Everything downstream of that (when to stop, how much to reserve)
  was being planned against a wrong number until I started measuring against
  `docs/start_time.txt` every time rather than estimating.
- I shipped one change — an extra `info` line whenever the best root move changes — that
  was cosmetic, and reverted it when it looked like it might be causing stalls. It wasn't,
  but the revert cost nothing and the reasoning is left in the code.

---

## Assumptions

Everything below was ambiguous or undisclosed and was resolved by judgement.

1. **fastchess `timemargin` is assumed to be 0.** The benchmark deliberately does not say.
   Time management therefore never plans to exceed the remaining clock, holds back
   `Move Overhead` (25 ms) unconditionally, and never allocates more than half of what is
   left. Across ~2200 games at 10+0.1 the engine lost **zero** games on time.
2. **CCRL Blitz ratings (2'+1") are used as anchors for a 10+0.1 match.** They are the only
   anchors available. There is no guarantee a rating gap measured at one time control
   transfers to another, and this is the dominant uncertainty in the strength estimate
   below — not sample size.
3. **Target CPU**: x86-64-v3 (POPCNT, BMI1, BMI2 with fast PEXT, AVX2), no AVX-512, not a
   Zen 1/2 part. Compiled for exactly that baseline, never `-march=native`.
4. **A bare `go` with no limits** is not defined by the protocol. It is treated as
   `go depth 30` — finite, so it can never hang a harness that then fails to send `stop`.
   `go infinite` behaves properly and waits for `stop`.
5. **`go wtime 0 btime 0` is a legitimate command** and must produce an instant move, not
   an unbounded search. (This was a real bug: time management keyed off the clock
   *values*, so an all-zero clock switched it off entirely and the engine searched to
   `MAX_PLY`. Under any harness that is a hang, and therefore a lost game.)
6. **`Hash` is set to 256 MB by the harness**, so 256 is also the engine's default and the
   value everything was tuned and tested at. `Hash 1` also works.
7. **`ponder` / `ponderhit`** are accepted and ignored, as the rules permit.
8. **Self-play games are a legitimate source of tuning data** — the rules explicitly allow
   values derived "by tuning on games you play in this session".

---

## Estimated strength

**~3250 Elo, honest range ~3180–3320.**

Measured against the supplied Stash ladder at the benchmark's own time control
(`tc=10+0.1`, `Hash 256`, UHO opening book, both colours per opening, `-concurrency 10`).
The shipped build was measured over **958 games against stash-30** across three separate
runs, plus 300 against stash-25:

| Opponent | CCRL Blitz | Games | My score | Elo diff | Implied |
|---|---|---|---|---|---|
| stash-30 | ~3170 | 958 | ~64.4% | +103 | ~3273 |
| stash-25 | ~2940 | 300 | 88.3% | +352 | ~3292 |

Earlier builds were additionally measured over 450 and 700 games against
stash-17/20/21/25/30, all consistent with the same picture.

### How much to trust that

The statistical error is small — a thousand games at ~64% gives roughly ±25 Elo. The real
uncertainty is systematic and I would rather name it than bury it:

- **The anchors are at the wrong time control.** Stash's CCRL ratings are measured at
  2'+1"; this match is at 10+0.1, roughly twelve times faster. A small engine with a fast
  evaluation and a cheap move generator plausibly does *relatively* better at very fast
  controls than at slow ones, which would make the implied rating flattering.
- **Elo compresses at large gaps.** Against stash-21 (~2710) the engine scored 91.7%,
  which implies ~3130 — noticeably lower than the figure from the nearest opponent. The
  closest opponent is the informative one, but the spread is real.
- **A three-opponent ladder is not a rating list.**

For those reasons I would treat **~3200 as the defensible claim** and ~3270 as the
optimistic end, rather than quoting the single best number.

### Reliability

Across roughly **2200 games** at 10+0.1: **zero losses on time, zero illegal moves, zero
protocol hangs.** The engine also produced no "bestmove does not match PV" warnings in the
later runs, while its opponents produced dozens.

**One known defect, stated plainly.** Four games (~0.2%) were lost to fastchess reporting
"engine not responsive". I found and fixed one real contributor — the start-up path was
zeroing the 256 MB hash table twice, once in `resize()` and again in `Search::clear()`,
so twenty engine processes launching together moved about 10 GB; start-up to first move
went from 200 ms to 97 ms. That did not eliminate the residual. A control run of 200 games
with the same binary from a directory *outside* the Dropbox-synced tree this project lives
in produced **zero** stalls, which is suggestive that the remainder is an artefact of the
development environment rather than the engine — but if the true rate were 1%, zero in 200
games would still happen 13% of the time, so that raises confidence without settling it.
I chose not to keep changing a verified, measured deliverable to chase a ~1 Elo
intermittent in the last two hours.

### Reproducing the measurement

```
resources\fastchess\fastchess.exe ^
  -engine cmd=final\Opus5chess24hrs.exe name=mine ^
  -engine cmd=resources\engines\stash-30.0-windows-x86_64-bmi2.exe name=stash30 ^
  -each tc=10+0.1 option.Hash=256 ^
  -openings file=resources\fastchess\UHO.pgn format=pgn order=random plies=16 ^
  -rounds 200 -repeat -concurrency 10 -ratinginterval 100 -recover
```

---

## Repository layout

```
final/Opus5chess24hrs.exe   the deliverable
source/src/                 engine source (unity build from main.cpp)
  types.h bitboard.*        board primitives, PEXT/magic attack tables
  position.*                position, make/unmake, Zobrist, SEE, legality
  movegen.h movepick.h      generation and staged ordering
  evalparams.h eval.*       all 983 tunable parameters, and the evaluation
  search.* history.h tt.*   search, statistics tables, transposition table
  uci.cpp.inc               protocol
  datagen.cpp.inc           self-play data generation
  tuner_main.cpp            texel tuner (built with -DTUNE)
  selfcheck_main.cpp        exhaustive pseudo_legal() verifier
  perft_main.cpp            perft driver
source/build_final.ps1      PGO build + full verification gate
source/tests/               match scripts and logs
docs/progress.md            the hour-by-hour record of the run
```

---

## Official results

Rated by the benchmark operator on 2 September 2026 in a single 3,800-game
run covering all four engines in the series.

**Opus 5 chess 24hrs: 3242 Elo ±23 (95% CI), CCRL Blitz scale**, from
1,100 games, 51.5% score.

### How the rating was measured

- Tool: fastchess 1.8.2, time control **10 s + 0.1 s**, one thread, 64 MB hash,
  `timemargin 200`.
- Openings: UHO unbalanced 8-ply book, random order, every opening played with
  both colours.
- Adjudication: resign after 3 moves at ±600 cp (two-sided); draw after move 40
  when 8 consecutive moves stay within ±20 cp; 250-move cap. `-recover` on.
- Format: a gauntlet against anchor engines whose ratings were fixed at their
  CCRL Blitz values, plus a round robin among the four AI engines in the
  series. Ratings come from an anchored maximum-likelihood Elo fit over all
  3,800 games.
- Hardware: AMD Ryzen AI 9 HX 375 laptop, one game per physical core
  (12 concurrent games).
- Anchors (CCRL Blitz): Stash 20 (2512), Stash 21 (2714), Juggernaut 2.01
  (2760), Stash 25 (2932), Crafty 25.6 (2970), Stash 27 (3049), Stash 29
  (3128), Stash 30 (3154), Stash 32 (3241), Stash 33 (3274), Stash 35 (3347),
  Stash 37 (3424).

### Head-to-head

Opponent ratings in parentheses are the other AI engines' fitted values
from this run; the rest are fixed CCRL Blitz anchors. "Implied Elo" is
the rating this single match alone would give.

| Opponent | Rating | W | D | L | Score | Implied Elo |
|---|---:|---:|---:|---:|---:|---:|
| Stash 37 | 3424 | 20 | 51 | 89 | 28.4% | 3264 |
| Stash 35 | 3347 | 21 | 58 | 81 | 31.2% | 3210 |
| Fable 5.1 chess 24hrs | (3277) | 23 | 27 | 50 | 36.5% | – |
| Stash 33 | 3274 | 42 | 58 | 60 | 44.4% | 3235 |
| Stash 32 | 3241 | 56 | 43 | 61 | 48.4% | 3230 |
| Stash 30 | 3154 | 92 | 33 | 35 | 67.8% | 3283 |
| Fable 5 chess 24hrs | (3049) | 70 | 18 | 12 | 79.0% | – |
| Sonnet 5 chess 24hrs | (2702) | 97 | 3 | 0 | 98.5% | – |

### Benchmark series standings

All four engines were rated in the same run, under identical conditions.

| Engine | Elo | 95% CI | Games | Score |
|---|---:|:---:|---:|---:|
| Fable 5.1 chess 24hrs | 3277 | ±23 | 1100 | 56.2% |
| Opus 5 chess 24hrs | 3242 | ±23 | 1100 | 51.5% |
| Fable 5 chess 24hrs | 3049 | ±22 | 1100 | 48.8% |
| Sonnet 5 chess 24hrs | 2702 | ±26 | 1100 | 31.4% |

### Reading the number

- The rating is on the **CCRL Blitz scale under 10+0.1 conditions**, not a
  CCRL rating. Fitting all anchors freely stretched the Stash ladder by about
  10% on this hardware and time control (Stash 20 fitted ~70 Elo low, Stash 35
  ~70 high). Each AI engine was rated only against anchors within roughly
  250 Elo of its own level, so the effect on its estimate is small.
- The rating run used 64 MB hash rather than the 256 MB assumed in the
  benchmark rules, so that all engines compared on equal terms.
- The engine's own estimate above (about 3250, range 3180–3320) agrees with
  the measured 3242 ±23. Opus 5 finished **second of the four engines** in
  the series, 35 Elo behind Fable 5.1 (the head-to-head went 23–27–50 against
  it).
- Reliability: zero losses on time, disconnects or illegal moves in the
  3,800-game run.
