# The 24-Hour Chess Engine Benchmark

You are being benchmarked. Your task is to design, build, test and deliver a
UCI-compliant chess engine, from scratch, within **24 hours of wall-clock
time**. The only thing that will be measured is the **playing strength (Elo)**
of the executable you leave in `final/`. Nothing else counts: not code quality,
not documentation, not feature count, not how elegant the architecture is.
Strength at the final time control, on the target hardware, is the whole score.

Every technical decision is yours: language, board representation, search,
evaluation, move ordering, time management, testing strategy, what to build
first and what to skip. This file deliberately gives you no technical guidance
on how to build a strong engine. It tells you the rules, the constraints, the
resources available, and what you must deliver.

**Managing your own time and effort is part of what is being benchmarked.**
The 24 hours is a fixed budget that covers everything you do — thinking,
reading, coding, compiling, debugging, testing and writing. How much of that
budget you spend on each task, when you stop polishing one thing and move on
to the next, what you measure versus accept on judgement, and what you decide
not to do at all, are all your decisions — and they will determine the final
Elo as much as any individual technical choice. Nobody will tell you when to
move on. Budget deliberately, time-box your work, and re-plan against the
deadline as you go.

## You are on your own

This run is **fully autonomous**. Nobody is watching, nobody will answer
questions, and no prompts, hints or clarifications will be given during the 24
hours. Do not ask questions, do not wait for confirmation, and do not end your
turn expecting a reply — there will be none. Wherever these instructions are
ambiguous or silent, interpret them as you see fit, note the assumption in
`docs/progress.md`, and keep going. Anything you stop to ask about simply
costs you time.

Your context may be compacted or reset during the run. Everything you need
to resume must be on disk (`docs/`, `source/`, the git history). If you find
yourself starting with `docs/start_time.txt` already present, you are
**resuming, not starting** — see "Time-keeping" below.

## Identity

- Engine name (reported by `id name`): **`Opus 5 chess 24hrs`**
- Author (reported by `id author`): **`Opus 5`**
- Executable name: `Opus5chess24hrs.exe` (no spaces, so it's easy to pass to tools)

(General rule for this benchmark series: the name is
`<model name> chess 24hrs` and the author is `<model name>`.)

## How the engine will be evaluated

- **Time control: game in 10 seconds + 0.1 second increment (`tc=10+0.1`)**,
  single-threaded, played with fastchess against a range of other engines
  (including the Stash versions in `resources/engines/` and others) using an
  opening book. Ratings are then computed on the Elo scale.
- The rating harness will set **`Hash` to 256 MB** for every engine
  (`setoption name Hash value 256`), so your engine must accept that and play
  well with it. No other options will be set: your engine's own defaults will
  be used for everything else, so make sure the defaults are the ones you
  want in the final match.
- A game lost on time, a crash, an illegal move, or a protocol hang all count as
  a **loss**. Reliability is part of strength. At 10+0.1 the engine must manage
  its clock carefully — there is very little slack.
- fastchess has a per-engine `timemargin` setting (the grace period before a
  late move is scored as a loss on time). Whether the rating match uses one,
  and how large, is **deliberately not disclosed**. Think about this when you
  design and test your time management.
- The target machine is a **modern Windows 11 laptop** (not this development
  machine, but comparable). Assume an x86-64 CPU with **POPCNT, BMI1, BMI2
  (including fast PEXT/PDEP), AVX2**. It will **not** be a Zen 1 / Zen 2 AMD
  part (which have slow microcoded PEXT). Do **not** assume AVX-512: this
  development machine (AMD Ryzen AI 9 HX PRO 375, Zen 5) has AVX-512 but the
  target laptop may not. Compile for the `x86-64-v3` baseline
  (`-march=x86-64-v3`, optionally plus `-mtune=znver4` or similar),
  **never `-march=native`**.
- The final executable must be **fully standalone**: no dependency on MinGW
  DLLs (`libstdc++-6.dll`, `libgcc_s_seh-1.dll`, `libwinpthread-1.dll`, …) or
  any other non-system DLL. Link statically (`-static` with GCC). Verify by
  running it from a plain `cmd.exe` / PowerShell with no MinGW on the PATH.
- The executable must be built with full optimisation (e.g. `-O3`, LTO,
  optionally PGO). Asserts and debug checks should be compiled out of the
  final build.

## Functional requirements

The engine must:

- Play **full legal chess**: all piece moves, castling (including all the
  legality rules), en passant, promotion to all four pieces, check/checkmate/
  stalemate detection, and correct handling of the 50-move rule and threefold
  repetition as they affect play. The move generator must pass the perft
  suite in `resources/perft/perft.epd` exactly.
- Speak **UCI** as described in `resources/protocol/uci-protocol.md`. At
  minimum it must correctly handle:
  - `uci` → `id name`, `id author`, option list, `uciok`
  - `isready` → `readyok` (at any time, including during a search)
  - `ucinewgame`
  - `setoption name <Name> value <Value>`
  - `position startpos [moves ...]` and `position fen <FEN> [moves ...]`
  - `go wtime <ms> btime <ms> [winc <ms>] [binc <ms>]` (game-in-X and
    game-in-X-plus-increment — this is what the final match uses). `movestogo`
    may be sent by some GUIs; handle it or ignore it gracefully.
  - `go movetime <ms>` (fixed time per move)
  - `go depth <n>` (fixed depth)
  - `go infinite` (search until `stop`)
  - `stop` → must promptly (within a few ms) terminate the search and print
    `bestmove`
  - `quit` → exit cleanly, including mid-search
  - Unknown commands and tokens must be ignored, not crash the engine.
- Always answer every `go` with a `bestmove <move>` line, even in lost,
  drawn, or no-time situations. Moves are in long algebraic notation
  (`e2e4`, `e7e8q`, castling as `e1g1`).
- Print **`info` lines** during the search, as every real UCI engine does. At
  least once per completed iteration (and ideally whenever the best move or
  score changes), output a line in the standard UCI format containing:
  - `depth <n>` (and `seldepth <n>` if you track it)
  - `score cp <centipawns>` from the side to move's point of view, or
    `score mate <n>` when a forced mate is found (`n` in moves, negative when
    being mated)
  - `nodes <n>`
  - `nps <n>` (nodes per second)
  - `time <ms>` (elapsed search time)
  - `pv <move1> <move2> ...` (the principal variation in long algebraic
    notation)
  - plus any other standard fields you find useful (`hashfull`, `currmove`,
    `multipv 1`, `string ...`).
    Example: `info depth 12 seldepth 18 score cp 34 nodes 1843210 nps 1520000 time 1212 pv e2e4 e7e5 g1f3 b8c6`.
    These lines are how a GUI or test harness sees what the engine is doing;
    fastchess records them in its PGN output, and you will need them yourself
    to debug search behaviour, measure speed, and catch time-management
    problems.
- Be **single-threaded** for search. (It may use a separate thread to read
  stdin so that `stop`/`isready`/`quit` are handled during a search, or it may
  poll stdin from inside the search — your choice, but it must work.)
- Expose a **`Hash`** UCI option (in MB) if it uses a transposition table.
  Beyond that, expose whatever options an ordinary UCI engine would normally
  expose for the features you implement; nothing exotic is required.

The engine does **not** need to:

- Ponder (the `ponder`/`ponderhit` commands may be ignored, but should not
  crash it). `Ponder` option not required.
- Support `go nodes`, `go mate`, `searchmoves`.
- Support Chess960 / `UCI_Chess960`.
- Have its own opening book (`OwnBook`), tablebases, or multi-PV.
- Support `MultiPV`, `Threads` (if you expose `Threads`, it may be fixed at 1).

Board representation, move generation style (mailbox / bitboards / magic /
PEXT), make-unmake vs copy-make, and everything about search and evaluation
are entirely up to you.

## Rules of the benchmark

1. **Time limit: 24 hours** from the moment you begin, wall-clock. This
   includes all thinking, coding, compiling, testing and writing. See
   "Time-keeping" below. There is no extension.
2. **Write everything from scratch in this session.** Anything publicly
   available on the internet as *documentation* is fair game: you may read
   and use chessprogramming.org (including its code snippets and published
   tables such as PeSTO or the Simplified Evaluation Function), papers, blog
   posts, forum threads and engine documentation, and you may reproduce
   ideas, formulas and constants from them. The one thing you may **not** do
   is copy, download, transcribe or closely adapt **source code from an
   existing chess engine**, or use any pre-existing neural network file,
   opening book or endgame data file. Values you derive yourself (by
   reasoning, by hand-tuning, or by tuning on games you play in this
   session) are of course fine.
3. **Standard library only.** No third-party chess libraries, no external
   dependencies. The chosen language's standard library and the compiler's
   intrinsics/builtins (e.g. `_pext_u64`, `__builtin_popcountll`) are allowed.
4. **Language: C or C++** — your choice. The toolchain is installed and on
   the PATH:
   - GCC 15.2.0 (MinGW-w64, via scoop): `gcc`, `g++`
   - There is **no** MSVC and **no** clang on this machine.
5. **Single deliverable:** the executable in `final/` at the 24-hour mark is
   what gets rated. Whatever is in `final/` when time runs out is your entry —
   half-finished source in `source/` earns nothing.
6. You may use up to **(physical cores − 2)** CPU cores for testing (fastchess
   `-concurrency`). This machine has **12 physical cores**, so use
   **`-concurrency 10`**. Check with:
   `powershell -NoProfile -Command "(Get-CimInstance Win32_Processor | Measure-Object -Property NumberOfCores -Sum).Sum"`
7. Do not modify anything in `resources/`.

## Folder layout

```
CLAUDE.md                 this file
README.md                 GitHub-facing write-up, created AFTER the 24 hours (see below)
resources/                read-only inputs (see below)
  protocol/uci-protocol.md
  fastchess/fastchess.exe
  fastchess/UHO.pgn       opening book, 223,070 games (use with fastchess -openings)
  fastchess/README.md
  engines/stash-*.exe     Stash calibration engines (BMI2 builds)
  engines/StashStrength.md  Elo of every Stash version
  perft/perft.epd         126 perft positions, depths 1–6
source/                   ALL your source code, build scripts, test scripts, tuning data
final/                    the final executable (Opus5chess24hrs.exe) — nothing else required
docs/                     start_time.txt and progress.md (see below)
```

Intermediate test builds can live anywhere under `source/` (e.g.
`source/build/`); keep `final/` for the current best deliverable only.

## Resources in detail

### `resources/protocol/uci-protocol.md`
The full UCI specification (Shredder, April 2006, converted to Markdown). Your
engine must conform to it for the commands listed above.

### `resources/fastchess/fastchess.exe`
Fastchess tournament manager. Run `fastchess.exe -help` for all options.
Typical uses:

Compliance check of your engine's UCI handling:
```
resources\fastchess\fastchess.exe --compliance final\Opus5chess24hrs.exe
```

A match against a Stash version at the final time control, 10 threads,
opening book, both colours per opening:
```
resources\fastchess\fastchess.exe ^
  -engine cmd=final\Opus5chess24hrs.exe name=mine ^
  -engine cmd=resources\engines\stash-20.0.1-windows-x86_64-bmi2.exe name=stash20 ^
  -each tc=10+0.1 ^
  -openings file=resources\fastchess\UHO.pgn format=pgn order=random plies=16 ^
  -rounds 200 -repeat -concurrency 10 ^
  -ratinginterval 20 -recover ^
  -pgnout file=source\tests\mine_vs_stash20.pgn
```

An SPRT test between two versions of your own engine (if you ever want one):
```
resources\fastchess\fastchess.exe ^
  -engine cmd=source\build\new.exe name=new ^
  -engine cmd=source\build\old.exe name=old ^
  -each tc=10+0.1 ^
  -openings file=resources\fastchess\UHO.pgn format=pgn order=random plies=16 ^
  -sprt elo0=0 elo1=10 alpha=0.05 beta=0.05 ^
  -rounds 5000 -repeat -concurrency 10 -recover
```

Notes:
- fastchess prints `Failed to get console mode. Error code: 6` when run
  without a console — harmless.
- Fastchess treats a timeout, crash or illegal move as a loss for that engine
  and reports it; watch the output for these, they tell you about bugs that
  will cost Elo in the final rating.
- `-recover` keeps the tournament going if an engine crashes.
- You can shorten the time control for faster iteration, but remember that the
  final rating is at 10+0.1 — behaviour at that control is what matters.
- The UHO book consists of unbalanced openings (one side has an edge), which
  reduces draws and makes Elo differences easier to measure. Always use
  `-repeat` (each opening played with both colours) so the imbalance cancels.

### `resources/engines/`
Stash versions 20, 21, 25, 30, 33, 37, all BMI2 Windows builds, all
single-threaded UCI engines. `StashStrength.md` lists the CCRL rating of
every Stash version (Blitz 2'+1" and 40/15). Approximate anchors for the
versions supplied:

| Engine | CCRL Blitz |
|---|---|
| stash-20 | ~2510 |
| stash-21 | ~2710 |
| stash-25 | ~2940 |
| stash-30 | ~3170 |
| stash-33 | ~3286 |
| stash-37 | ~3424 |

Use them as a ladder to estimate your engine's strength: a score of about 50%
against a Stash version at 10+0.1 means roughly that version's rating
(CCRL ratings are at longer controls, so treat this as an estimate, not a
measurement). These are the kinds of opponents your engine will face in the
final rating, along with other engines of various strengths.

### `resources/perft/perft.epd`
126 lines, each `FEN ;D1 n ;D2 n ;D3 n ;D4 n ;D5 n ;D6 n`. The first line is
the start position (D6 = 119,060,324). Includes all the classic tricky
positions (castling through attacked squares, en passant pins, promotions,
double checks, etc.). A move generator that matches every number through at
least depth 5 (depth 6 where it is cheap enough) can be considered correct.

## Time-keeping and progress reporting (mandatory)

- **The moment you start**, before anything else, create `docs/start_time.txt`
  containing the current local date-time (ISO 8601, e.g. from
  `powershell -NoProfile -Command "Get-Date -Format o"`). The deadline is
  exactly 24 hours after that timestamp. Check the current time regularly (at
  least once an hour, and before starting any long test run) and plan
  backwards from the deadline.
- **If `docs/start_time.txt` already exists, do NOT overwrite it.** It means
  your context has been reset and you are resuming a run already in
  progress. The original timestamp still defines the deadline. Read
  `docs/progress.md`, `git log` and the current state of `source/` and
  `final/`, work out where you are, and carry on from there.
- **Every hour**, append an entry to `docs/progress.md` with:
  - the elapsed time (e.g. `Hour 7 — 2026-08-22 14:30`)
  - what you worked on in the last hour
  - what is working / tested right now
  - your current estimate of the engine's Elo (and the evidence — e.g. match
    results against a Stash version), or "not yet measurable"
  - what you plan to do in the next hour

  Keep entries short (a few bullets). This log is the only record of your
  process that will be read; it does not affect the score but it must be kept.
- **Firsts**: record in `docs/progress.md` the time the engine first passes
  the full perft suite (`resources/perft/perft.epd`), and the time the first
  full legal chess game was played by the engine.
- **Keep `final/` always deployable.** As soon as you have an engine that
  plays legal chess without crashing under fastchess, put a build of it in
  `final/`. Thereafter, whenever you have a *verified* stronger and stable
  build, replace it. Never leave an untested or known-broken build in
  `final/`. If time runs out mid-experiment, the previous good build in
  `final/` is your entry.
- **Reserve the last part of the 24 hours** for producing, verifying and
  installing the final build (correct name/author, standalone, optimised,
  compliance-checked, a short sanity match at 10+0.1). Do not start anything
  you cannot finish and verify before the deadline.
- Do not stop early. If your engine works with time left, keep improving it —
  every Elo point counts — but never at the cost of the reliability of what is
  in `final/`.

## After the 24 hours: README.md (mandatory, written after the deadline)

Use the full 24 hours on the engine. **Only once the clock has run out** (or
the final build is installed and you have decided not to make further
changes), write a `README.md` in the project root. This file does not affect
the score and is written outside the 24-hour window, so spend no engine time
on it. It is intended for a public GitHub repository, so write it for a
reader who knows chess programming but knows nothing about this benchmark.
It must cover:

- **What this is** — the benchmark: one model (Opus 5), 24 hours, from
  scratch, UCI engine, rated at 10+0.1; a link/summary of the rules in this
  file.
- **The engine** — name, language chosen and why, how to build it (exact
  commands and compiler versions), how to run it, the UCI options it exposes.
- **Architecture and features** — board representation, move generation,
  search, evaluation, move ordering, time management, and anything else
  implemented; what was deliberately left out and why.
- **How the time was spent** — a summary timeline (you can derive it from
  `docs/progress.md`), what was measured vs. accepted on judgement, what went
  wrong and what was cut.
- **Elo by hour** — a table with one row per hour of the run: the timestamp
  and the estimated Elo rating of the engine at that point (derived from the
  hourly entries in `docs/progress.md`; use "not yet measurable" where no
  estimate existed).
- **All assumptions made** — about the rules, the hardware, the harness, the
  opponents, anything ambiguous you resolved yourself.
- **Estimated strength** — your best estimate of the engine's Elo, with the
  evidence (match results against Stash versions and/or other tests, number
  of games, time control used), and an honest statement of the uncertainty.
  If you could not measure it, say so.
- **Results placeholder** — a section headed "Official results" left for the
  human to fill in after the rating match.

## Version control (mandatory)

A git repository is already initialised in the project root, with a
`.gitignore` that excludes `resources/`, build artefacts, PGN outputs and
large temp files (the deliverable `final/Opus5chess24hrs.exe` *is* tracked, so
every commit records what was in `final/`). Extend `.gitignore` if you need
to. Commit every time you reach a working,
tested state, with a message that says what changed and (if measured) its
effect. This gives you a way back when a change turns out to be a regression
or introduces a bug. Commit `docs/progress.md` with each hourly update.

## A note on judgement

Twenty-four hours is not long enough to test every change with a proper SPRT
at game speed. You will have to decide, for each idea, whether to measure it,
to accept it on reasoning, or to skip it. How you allocate your time between
correctness, speed, search, evaluation, time management and measurement is
the core of this benchmark — **allocation of effort is itself being scored,
through its effect on the final Elo**. Some things to keep in mind:

- Every hour spent on one thing is an hour not spent on another. A change
  worth 5 Elo that costs three hours to build and test may be worse than
  three changes worth 3 Elo each that cost an hour apiece.
- Decide in advance roughly how long a task should take, and notice when you
  are over budget. Chasing a stubborn bug or squeezing the last few percent
  of speed can quietly consume hours; know when to stop, simplify, or revert.
- Long test matches are time you cannot get back. Choose game counts and time
  controls that answer the question you are actually asking, no more.
- The first playable engine in `final/` is worth more than any unfinished
  ambitious one. Get to "plays legal chess, doesn't crash, manages its clock"
  early, then spend the remaining time on strength.
- Re-plan backwards from the deadline at every hourly check-in: what is the
  most valuable thing you can still finish *and verify* before it?

Choose deliberately, and record the reasoning in `docs/progress.md`.
