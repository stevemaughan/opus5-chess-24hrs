#include "search.h"
#include "eval.h"
#include "tt.h"
#include <cmath>
#include <iostream>
#include <sstream>

namespace Search {

OptionsType Options;
LimitsType  Limits;
std::atomic<bool> Stop{false};
std::atomic<bool> Pondering{false};
bool Quiet = false;
uint64_t Nodes = 0;
std::vector<RootMove> RootMoves;

namespace {

enum NodeType { NonPV, PV, Root };

Histories Hist;

// Timing
uint64_t NextTimeCheck = 0;
int64_t StartTime;
int64_t OptimumTime;
int64_t MaximumTime;
bool    UseTimeMan = false;

std::vector<Move> BestPV;
Value BestScore = -VALUE_INFINITE;
int  BestSelDepth;
int  NmpMinPly;
int  RootDepth;
int  CompletedDepth;
int  SelDepth;
int  BestMoveChanges;
Move LastBestMove;
int  LastBestMoveDepth;
Value LastBestScore;
Position* RootPos = nullptr;

// Search constants that are worth A/B testing.  Defaults reproduce the current
// behaviour exactly, so a plain build is unaffected.
#ifndef TM_DIV
#define TM_DIV 28
#endif
#ifndef TM_DIV_OPENING
#define TM_DIV_OPENING 34
#endif
#ifndef LMR_CUTNODE
#define LMR_CUTNODE 2048
#endif
#ifndef QS_MOVECOUNT
#define QS_MOVECOUNT 2
#endif
#ifndef ASP_DELTA
#define ASP_DELTA 10
#endif
#ifndef SINGULAR_DEPTH
#define SINGULAR_DEPTH 6
#endif
#ifndef NMP_BASE
#define NMP_BASE 4
#endif
#ifndef RFP_MARGIN
#define RFP_MARGIN 86
#endif
#ifndef FUT_MARGIN
#define FUT_MARGIN 138
#endif
#ifndef LMP_BASE
#define LMP_BASE 3
#endif

// Bitmask used to disable individual pruning features while bisecting search behaviour.
// Set the PRUNEMASK environment variable to a decimal value; every bit set means "on".
enum PruneBit {
    P_RAZOR = 1, P_RFP = 2, P_NULL = 4, P_PROBCUT = 8, P_LMP = 16, P_FUTILITY = 32,
    P_SEE = 64, P_LMR = 128, P_IIR = 256, P_HISTPRUNE = 512, P_LOOPRED = 1024,
    P_QCAP = 2048, P_SINGULAR = 4096, P_ALL = 0x7FFFFFFF
};
int PruneMask = P_ALL;
inline bool pruning_on(int bit) { return (PruneMask & bit) != 0; }

// Reduction table: LMR[depth][moveCount]
int Reductions[64][64];

inline int lmr_reduction(int d, int mn) {
    return Reductions[std::min(d, 63)][std::min(mn, 63)];
}

inline Value value_to_tt(Value v, int ply) {
    return v >= VALUE_MATE_IN_MAX_PLY  ? Value(v + ply)
         : v <= VALUE_MATED_IN_MAX_PLY ? Value(v - ply)
                                       : v;
}

inline Value value_from_tt(Value v, int ply, int r50) {
    if (v == VALUE_NONE) return VALUE_NONE;
    if (v >= VALUE_MATE_IN_MAX_PLY) {
        // A "mate" that depends on a position the fifty-move rule may cut short is not safe
        if (VALUE_MATE - v > 99 - r50) return Value(VALUE_MATE_IN_MAX_PLY - 1);
        return Value(v - ply);
    }
    if (v <= VALUE_MATED_IN_MAX_PLY) {
        if (VALUE_MATE + v > 99 - r50) return Value(VALUE_MATED_IN_MAX_PLY + 1);
        return Value(v + ply);
    }
    return v;
}

// Apply the learned correction for this pawn structure to a raw static evaluation.
// The transposition table always stores the *raw* value; the correction is applied
// on read, so a stale correction can never be baked into the table.
inline Value corrected_eval(const Position& pos, Value raw) {
#ifdef NO_CORRHIST
    (void)pos;
    return raw;
#else
    int cv = corr_value(Hist.corr[pos.side_to_move()][corr_hist_index(pos.pawn_key())]);
    return Value(std::clamp(int(raw) + cv / 2,
                            int(VALUE_MATED_IN_MAX_PLY) + 1, int(VALUE_MATE_IN_MAX_PLY) - 1));
#endif
}

inline int stat_bonus(int d) { return std::min(268 * d - 352, 1153); }
inline int stat_malus(int d) { return std::min(400 * d - 354, 1201); }

// Draw value with a small random-ish offset to avoid 3-fold blindness
inline Value draw_value(uint64_t nodes) { return VALUE_DRAW + Value(2 * int(nodes & 1) - 1); }

void update_pv(Move* pv, Move move, const Move* childPv) {
    for (*pv++ = move; childPv && *childPv != MOVE_NONE; )
        *pv++ = *childPv++;
    *pv = MOVE_NONE;
}

void update_continuation_histories(Stack* ss, Piece pc, int to, int bonus) {
    static const int Weight[CONT_HIST_PLIES] = { 1024, 573, 291, 615, 0, 397 };
    for (int i : { 1, 2, 3, 4, 6 }) {
        if (ss->inCheck && i > 2) break;
        if ((ss - i)->currentMove != MOVE_NONE) {
            int w = Weight[std::min(i - 1, CONT_HIST_PLIES - 1)];
            hist_update(const_cast<PieceToHistory*>((ss - i)->continuationHistory)->v[pc][to],
                        bonus * w / 1024, CONT_HIST_MAX);
        }
    }
}

void update_quiet_stats(const Position& pos, Stack* ss, Move move, int bonus) {
    Color us = pos.side_to_move();
    Piece pc = pos.moved_piece(move);
    int to = to_sq(move);

    if (ss->killers[0] != move) {
        ss->killers[1] = ss->killers[0];
        ss->killers[0] = move;
    }

    hist_update(Hist.main[us][from_to(move)], bonus, MAIN_HIST_MAX);
    hist_update(Hist.pawnHist[pawn_hist_index(pos.pawn_key())][pc][to], bonus, PAWN_HIST_MAX);
    update_continuation_histories(ss, pc, to, bonus);

    if ((ss - 1)->currentMove != MOVE_NONE && (ss - 1)->currentMove != MOVE_NULL) {
        Piece prevPc = pos.piece_on(to_sq((ss - 1)->currentMove));
        if (prevPc != NO_PIECE)
            Hist.counterMove[prevPc][to_sq((ss - 1)->currentMove)] = move;
    }
}

void update_all_stats(const Position& pos, Stack* ss, Move bestMove, int depth,
                      Move* quietsSearched, int quietCount,
                      Move* capturesSearched, int captureCount) {
    int bonus = stat_bonus(depth);
    int malus = stat_malus(depth);

    if (!pos.capture_stage(bestMove)) {
        update_quiet_stats(pos, ss, bestMove, bonus);
        for (int i = 0; i < quietCount; ++i) {
            Move m = quietsSearched[i];
            hist_update(Hist.main[pos.side_to_move()][from_to(m)], -malus, MAIN_HIST_MAX);
            hist_update(Hist.pawnHist[pawn_hist_index(pos.pawn_key())][pos.moved_piece(m)][to_sq(m)],
                        -malus, PAWN_HIST_MAX);
            update_continuation_histories(ss, pos.moved_piece(m), to_sq(m), -malus);
        }
    } else {
        Piece pc = pos.moved_piece(bestMove);
        PieceType captured = type_of(pos.piece_on(to_sq(bestMove)));
        if (type_of_move(bestMove) == EN_PASSANT) captured = PAWN;
        hist_update(Hist.capture[pc][to_sq(bestMove)][captured], bonus, CAPT_HIST_MAX);
    }

    for (int i = 0; i < captureCount; ++i) {
        Move m = capturesSearched[i];
        Piece pc = pos.moved_piece(m);
        PieceType captured = type_of(pos.piece_on(to_sq(m)));
        if (type_of_move(m) == EN_PASSANT) captured = PAWN;
        hist_update(Hist.capture[pc][to_sq(m)][captured], -malus, CAPT_HIST_MAX);
    }
}

// ---------------------------------------------------------------------------
// Time control
// ---------------------------------------------------------------------------
void init_time(Color us, int gamePly) {
    UseTimeMan = false;
    OptimumTime = MaximumTime = 0;

    if (Limits.movetime) {
        OptimumTime = MaximumTime = std::max<int64_t>(1, Limits.movetime - Options.moveOverhead / 2);
        UseTimeMan = true;
        return;
    }
    if (!Limits.useTimeManagement()) return;

    UseTimeMan = true;
    int64_t remaining = Limits.time[us];
    int64_t inc = Limits.inc[us];
    int64_t overhead = Options.moveOverhead;

    // Self-correcting allocation: a constant fraction of what is left plus most of
    // the increment.  Because it is proportional, it decays gracefully and cannot
    // run the clock down to zero.
    int64_t opt;
    if (Limits.movestogo > 0) {
        int mtg = std::min(Limits.movestogo, 50);
        opt = remaining / std::max(1, mtg) + inc * 85 / 100;
        if (mtg == 1) opt = remaining * 70 / 100;
    } else {
        // Slightly quicker in the opening, where positions are cheap and book-like.
        int div = (gamePly < 12) ? TM_DIV_OPENING : TM_DIV;
        opt = remaining / div + inc * 85 / 100;
    }

    // Never gamble more than a third of the clock on one move
    opt = std::min<int64_t>(opt, remaining * 30 / 100);

    int64_t maxT = std::min<int64_t>(opt * 4, remaining * 50 / 100);

    // Absolute ceiling: always leave the overhead in hand
    int64_t hardCeiling = std::max<int64_t>(1, remaining - overhead);
    maxT = std::min(maxT, hardCeiling);
    opt  = std::min(opt, maxT);

    OptimumTime = std::max<int64_t>(1, opt);
    MaximumTime = std::max<int64_t>(1, maxT);
}

inline int64_t elapsed() { return now() - StartTime; }

// Called from inside the search every so often
void check_time() {
    if (Limits.nodes && Nodes >= Limits.nodes) { Stop = true; return; }
    if (!UseTimeMan || Limits.infinite) return;
    if (elapsed() >= MaximumTime) Stop = true;
}

std::string uci_move(Move m) {
    if (m == MOVE_NONE) return "0000";
    std::string s = square_to_string(from_sq(m)) + square_to_string(to_sq(m));
    if (type_of_move(m) == PROMOTION) s += " pnbrqk"[promotion_type(m)];
    return s;
}

std::string uci_score(Value v) {
    std::ostringstream ss;
    if (std::abs(v) < VALUE_MATE_IN_MAX_PLY)
        ss << "cp " << v;
    else
        ss << "mate " << (v > 0 ? (VALUE_MATE - v + 1) / 2 : -(VALUE_MATE + v) / 2);
    return ss.str();
}

void print_info(const Position& pos, int depth, Value score, bool bounded, Bound b) {
    int64_t t = std::max<int64_t>(1, elapsed());

    std::ostringstream ss;
    ss << "info depth " << depth
       << " seldepth " << BestSelDepth
       << " score " << uci_score(score);
    if (bounded) ss << (b == BOUND_LOWER ? " lowerbound" : " upperbound");
    ss << " nodes " << Nodes
       << " nps " << (Nodes * 1000 / (uint64_t)t)
       << " hashfull " << TT.hashfull()
       << " time " << t
       << " pv";
    for (Move m : BestPV) {
        if (m == MOVE_NONE) break;
        ss << ' ' << uci_move(m);
    }
    if (!Quiet) std::cout << ss.str() << std::endl;
    (void)pos;
}

template<NodeType nodeType> Value search(Position& pos, Stack* ss, Value alpha, Value beta, int depth, bool cutNode);
template<NodeType nodeType> Value qsearch(Position& pos, Stack* ss, Value alpha, Value beta, int depth = 0);

// ---------------------------------------------------------------------------
// Quiescence search
// ---------------------------------------------------------------------------
template<NodeType nodeType>
Value qsearch(Position& pos, Stack* ss, Value alpha, Value beta, int depth) {
    constexpr bool PvNode = nodeType == PV;

    Move pv[MAX_PLY + 1];
    StateInfo st;
    Move move, bestMove = MOVE_NONE;
    Value bestValue, value, futilityBase;
    bool pvHit, givesCheck, capture;
    int moveCount = 0;

    if (PvNode) {
        (ss + 1)->pv = pv;
        ss->pv[0] = MOVE_NONE;
    }

    ss->inCheck = pos.checkers();
    ss->currentMove = MOVE_NONE;
    Value oldAlpha = alpha;

    if (PvNode && SelDepth < ss->ply + 1) SelDepth = ss->ply + 1;

    if (pos.is_draw(ss->ply) || ss->ply >= MAX_PLY)
        return (ss->ply >= MAX_PLY && !ss->inCheck) ? Eval::evaluate(pos) : draw_value(Nodes);

    // Transposition table
    U64 posKey = pos.key();
    bool ttHit;
    TTEntry* tte = TT.probe(posKey, ttHit);
    Value ttValue = ttHit ? value_from_tt(tte->value(), ss->ply, pos.rule50_count()) : VALUE_NONE;
    Move ttMove = ttHit ? tte->move() : MOVE_NONE;
    pvHit = ttHit && tte->is_pv();

    // qsearch stores at depth 0 (with checks) or -1 (without)
    int ttDepth = ss->inCheck || depth >= 0 ? 0 : -1;

    if (!PvNode && ttHit && tte->depth() >= ttDepth && ttValue != VALUE_NONE
        && (tte->bound() & (ttValue >= beta ? BOUND_LOWER : BOUND_UPPER)))
        return ttValue;

    // Static evaluation / stand pat
    Value rawEval = VALUE_NONE;
    if (ss->inCheck) {
        ss->staticEval = VALUE_NONE;
        bestValue = futilityBase = -VALUE_INFINITE;
    } else {
        if (ttHit) {
            rawEval = (tte->eval() != VALUE_NONE) ? tte->eval() : Eval::evaluate(pos);
            ss->staticEval = bestValue = corrected_eval(pos, rawEval);
            if (ttValue != VALUE_NONE && (tte->bound() & (ttValue > bestValue ? BOUND_LOWER : BOUND_UPPER)))
                bestValue = ttValue;
        } else {
            rawEval = Eval::evaluate(pos);
            ss->staticEval = bestValue = corrected_eval(pos, rawEval);
        }

        if (bestValue >= beta) {
            if (!ttHit)
                tte->save(posKey, value_to_tt(bestValue, ss->ply), false, BOUND_LOWER,
                          -6, MOVE_NONE, rawEval, TT.generation());
            return bestValue;
        }
        if (bestValue > alpha) alpha = bestValue;
        futilityBase = bestValue + 217;
    }

    const PieceToHistory* contHist[CONT_HIST_PLIES] = {
        (ss - 1)->continuationHistory, (ss - 2)->continuationHistory,
        (ss - 3)->continuationHistory, (ss - 4)->continuationHistory,
        (ss - 5)->continuationHistory, (ss - 6)->continuationHistory
    };

    MovePicker mp(pos, ttMove, depth, &Hist, contHist);

    while ((move = mp.next_move()) != MOVE_NONE) {
        if (!pos.legal(move)) continue;
        ++moveCount;

        givesCheck = pos.gives_check(move);
        capture = pos.capture(move);

        // Pruning of hopeless captures once we are safely above alpha
        if (bestValue > VALUE_MATED_IN_MAX_PLY && pos.non_pawn_material(pos.side_to_move())) {
            if (!givesCheck && to_sq(move) != to_sq((ss - 1)->currentMove)
                && futilityBase > -VALUE_INFINITE && type_of_move(move) != PROMOTION) {
                if (pruning_on(P_QCAP) && moveCount > QS_MOVECOUNT) continue;

                Value futilityValue = futilityBase + SEEValue[type_of(pos.piece_on(to_sq(move)))];
                if (futilityValue <= alpha) { bestValue = std::max(bestValue, futilityValue); continue; }
                if (futilityBase <= alpha && !pos.see_ge(move, 1)) {
                    bestValue = std::max(bestValue, futilityBase); continue;
                }
                if (futilityBase > alpha && !pos.see_ge(move, (alpha - futilityBase) * 4)) {
                    bestValue = alpha; continue;
                }
            }

            // Skip losing captures outright
            if (!pos.see_ge(move, -83)) continue;
        }

        ss->currentMove = move;
        ss->continuationHistory = &Hist.continuation[ss->inCheck][capture][pos.moved_piece(move)][to_sq(move)];

        ++Nodes;
        pos.do_move(move, st, givesCheck);
        value = -qsearch<nodeType>(pos, ss + 1, -beta, -alpha, depth - 1);
        pos.undo_move(move);

        if (value > bestValue) {
            bestValue = value;
            if (value > alpha) {
                bestMove = move;
                if (PvNode) update_pv(ss->pv, move, (ss + 1)->pv);
                if (PvNode && value < beta) alpha = value;
                else break;   // fail high
            }
        }
        if (Stop.load(std::memory_order_relaxed)) return VALUE_ZERO;
    }

    // Mate detection: if we are in check and found no legal move, it is mate
    if (ss->inCheck && bestValue == -VALUE_INFINITE)
        return mated_in(ss->ply);

    tte->save(posKey, value_to_tt(bestValue, ss->ply), pvHit,
              bestValue >= beta ? BOUND_LOWER : (PvNode && bestValue > oldAlpha) ? BOUND_EXACT : BOUND_UPPER,
              ttDepth, bestMove, rawEval, TT.generation());
    return bestValue;
}

// ---------------------------------------------------------------------------
// Main alpha-beta search
// ---------------------------------------------------------------------------
template<NodeType nodeType>
Value search(Position& pos, Stack* ss, Value alpha, Value beta, int depth, bool cutNode) {
    constexpr bool PvNode = nodeType != NonPV;
    constexpr bool rootNode = nodeType == Root;

    if (depth <= 0)
        return qsearch<PvNode ? PV : NonPV>(pos, ss, alpha, beta);

    depth = std::min(depth, MAX_PLY - 1);

    Move pv[MAX_PLY + 1];
    Move capturesSearched[32], quietsSearched[64];
    StateInfo st;
    Move move, excludedMove, bestMove;
    int extension, newDepth;
    Value bestValue, value, ttValue, eval, probCutBeta;
    bool givesCheck, improving, priorCapture, capture, ttCapture;
    int moveCount, captureCount, quietCount;

    // ---- Step 1: initialise ----
    ss->inCheck = pos.checkers();
    priorCapture = pos.captured_piece() != NO_PIECE;
    Color us = pos.side_to_move();
    moveCount = captureCount = quietCount = ss->moveCount = 0;
    bestValue = -VALUE_INFINITE;
    bestMove = MOVE_NONE;
    ss->statScore = 0;

    if (PvNode && SelDepth < ss->ply + 1) SelDepth = ss->ply + 1;

    if (!rootNode) {
        // Periodic clock check
        // A counter, not a bitmask on Nodes: qsearch also increments Nodes, so an
        // exact "(Nodes & 1023) == 0" test can be stepped over entirely and let the
        // search blow past the hard time limit.
        if (Nodes >= NextTimeCheck) { NextTimeCheck = Nodes + 1024; check_time(); }
        if (Stop.load(std::memory_order_relaxed)) return VALUE_ZERO;

        // ---- Step 2: draws and mate-distance pruning ----
        if (pos.is_draw(ss->ply) || ss->ply >= MAX_PLY)
            return (ss->ply >= MAX_PLY && !ss->inCheck) ? Eval::evaluate(pos) : draw_value(Nodes);

        alpha = std::max(mated_in(ss->ply), alpha);
        beta  = std::min(mate_in(ss->ply + 1), beta);
        if (alpha >= beta) return alpha;
    }

    (ss + 1)->excludedMove = MOVE_NONE;
    (ss + 1)->killers[0] = (ss + 1)->killers[1] = MOVE_NONE;
    (ss + 2)->cutoffCnt = 0;

    // ---- Step 3: transposition table ----
    excludedMove = ss->excludedMove;
    U64 posKey = pos.key();
    bool ttHit;
    TTEntry* tte = TT.probe(posKey, ttHit);
    ttValue = ttHit ? value_from_tt(tte->value(), ss->ply, pos.rule50_count()) : VALUE_NONE;
    Move ttMove = rootNode ? RootMoves[0].pv[0] : (ttHit ? tte->move() : MOVE_NONE);
    ttCapture = ttMove && pos.capture_stage(ttMove);
    // Snapshot what the entry says now. Searching a child can evict this cluster slot,
    // after which `tte` describes some other position entirely, so nothing inside the
    // move loop may dereference it.
    const int   ttDepthSnap = ttHit ? tte->depth() : -100;
    const Bound ttBoundSnap = ttHit ? tte->bound() : BOUND_NONE;
    if (!excludedMove) ss->ttPv = PvNode || (ttHit && tte->is_pv());
    ss->ttHit = ttHit;

    if (excludedMove) { ttHit = false; ttMove = MOVE_NONE; ttValue = VALUE_NONE; }

    // A sufficiently deep TT hit cuts the node off immediately
    if (!PvNode && ttHit && tte->depth() > depth - (ttValue <= beta)
        && ttValue != VALUE_NONE
        && (tte->bound() & (ttValue >= beta ? BOUND_LOWER : BOUND_UPPER))) {
        if (ttMove && ttValue >= beta) {
            if (!pos.capture_stage(ttMove))
                update_quiet_stats(pos, ss, ttMove, stat_bonus(depth));
            // Penalise the previous move if it allowed this easy refutation
            if (priorCapture == false && (ss - 1)->moveCount <= 2 && (ss - 1)->currentMove != MOVE_NULL
                && (ss - 1)->currentMove != MOVE_NONE)
                update_continuation_histories(ss - 1, pos.piece_on(to_sq((ss - 1)->currentMove)),
                                              to_sq((ss - 1)->currentMove), -stat_malus(depth + 1));
        }
        if (pos.rule50_count() < 90) return ttValue;
    }

    // ---- Step 4: static evaluation ----
    Value unadjustedEval = VALUE_NONE;
    if (ss->inCheck) {
        ss->staticEval = eval = VALUE_NONE;
        improving = false;
        goto moves_loop;
    } else if (excludedMove) {
        eval = ss->staticEval;
    } else if (ttHit) {
        unadjustedEval = (tte->eval() != VALUE_NONE) ? tte->eval() : Eval::evaluate(pos);
        ss->staticEval = eval = corrected_eval(pos, unadjustedEval);
        // Use the TT value as a better estimate of the true score when the bound allows
        if (ttValue != VALUE_NONE && (tte->bound() & (ttValue > eval ? BOUND_LOWER : BOUND_UPPER)))
            eval = ttValue;
    } else {
        unadjustedEval = Eval::evaluate(pos);
        ss->staticEval = eval = corrected_eval(pos, unadjustedEval);
        tte->save(posKey, VALUE_NONE, ss->ttPv, BOUND_NONE, -6, MOVE_NONE, unadjustedEval, TT.generation());
    }

    // Are we better off than we were two plies ago?
    improving = (ss - 2)->staticEval != VALUE_NONE ? ss->staticEval > (ss - 2)->staticEval
              : (ss - 4)->staticEval != VALUE_NONE ? ss->staticEval > (ss - 4)->staticEval
                                                   : true;

    // Bonus for a quiet previous move that turned out badly for the opponent
    if (priorCapture == false && (ss - 1)->currentMove != MOVE_NULL && (ss - 1)->currentMove != MOVE_NONE
        && (ss - 1)->staticEval != VALUE_NONE && ss->staticEval != VALUE_NONE) {
        int bonus = std::clamp(-10 * int((ss - 1)->staticEval + ss->staticEval), -1583, 1452);
        Piece prevPc = pos.piece_on(to_sq((ss - 1)->currentMove));
        if (prevPc != NO_PIECE)
            hist_update(Hist.main[~us][from_to((ss - 1)->currentMove)], bonus, MAIN_HIST_MAX);
    }

    // ---- Step 5: razoring ----
    if (!PvNode && pruning_on(P_RAZOR) && depth <= 5 && eval < alpha - 462 - 297 * depth * depth) {
        value = qsearch<NonPV>(pos, ss, alpha - 1, alpha);
        if (value < alpha) return value;
    }

    // ---- Step 6: reverse futility / static null move pruning ----
    if (!ss->ttPv && pruning_on(P_RFP) && depth < 11
        && eval - (RFP_MARGIN * (depth - improving) - 20 * (ss - 1)->statScore / 256) >= beta
        && eval >= beta && eval < 25000 && (!ttMove || ttCapture))
        return (eval + beta) / 2;

    // ---- Step 7: null move pruning ----
    if (!PvNode && pruning_on(P_NULL) && (ss - 1)->currentMove != MOVE_NULL && excludedMove == MOVE_NONE
        && eval >= beta && ss->staticEval >= beta - 21 * depth + 390
        && pos.non_pawn_material(us) > 0 && ss->ply >= NmpMinPly
        && beta > VALUE_MATED_IN_MAX_PLY) {

        int R = std::min(int(eval - beta) / 202, 6) + depth / 3 + NMP_BASE;
        ss->currentMove = MOVE_NULL;
        ss->continuationHistory = &Hist.continuation[0][0][NO_PIECE][0];

        pos.do_null_move(st);
        Value nullValue = -search<NonPV>(pos, ss + 1, -beta, -beta + 1, depth - R, !cutNode);
        pos.undo_null_move();

        if (nullValue >= beta && nullValue < VALUE_MATE_IN_MAX_PLY) {
            if (NmpMinPly || depth < 15) return nullValue;
            // Verification search at high depth with null move disabled, so that
            // zugzwang positions are not thrown away.
            NmpMinPly = ss->ply + 3 * (depth - R) / 4;
            Value v = search<NonPV>(pos, ss, beta - 1, beta, depth - R, false);
            NmpMinPly = 0;
            if (v >= beta) return nullValue;
        }
    }

    // ---- Step 8: ProbCut ----
    probCutBeta = beta + 184 - 53 * improving;
    if (!PvNode && depth > 3 && std::abs(beta) < VALUE_MATE_IN_MAX_PLY && pruning_on(P_PROBCUT)
        && !(ttHit && ttDepthSnap >= depth - 3 && ttValue != VALUE_NONE && ttValue < probCutBeta)) {

        MovePicker mp(pos, ttMove, probCutBeta - ss->staticEval, &Hist);
        while ((move = mp.next_move()) != MOVE_NONE) {
            if (move == excludedMove || !pos.legal(move)) continue;

            ss->currentMove = move;
            ss->continuationHistory = &Hist.continuation[ss->inCheck][true][pos.moved_piece(move)][to_sq(move)];

            ++Nodes;
            pos.do_move(move, st);
            value = -qsearch<NonPV>(pos, ss + 1, -probCutBeta, -probCutBeta + 1);
            if (value >= probCutBeta)
                value = -search<NonPV>(pos, ss + 1, -probCutBeta, -probCutBeta + 1, depth - 4, !cutNode);
            pos.undo_move(move);

            if (value >= probCutBeta) {
                tte->save(posKey, value_to_tt(value, ss->ply), ss->ttPv, BOUND_LOWER,
                          depth - 3, move, unadjustedEval, TT.generation());
                return value;
            }
            if (Stop.load(std::memory_order_relaxed)) return VALUE_ZERO;
        }
    }

moves_loop:

    // ---- Step 9: internal iterative reduction ----
    if (PvNode && !ttMove && pruning_on(P_IIR)) depth -= 3;
    if (depth <= 0) return qsearch<PvNode ? PV : NonPV>(pos, ss, alpha, beta);
    if (cutNode && depth >= 8 && !ttMove && pruning_on(P_IIR)) depth -= 2;

    {
        const PieceToHistory* contHist[CONT_HIST_PLIES] = {
            (ss - 1)->continuationHistory, (ss - 2)->continuationHistory,
            (ss - 3)->continuationHistory, (ss - 4)->continuationHistory,
            (ss - 5)->continuationHistory, (ss - 6)->continuationHistory
        };

        Move countermove = MOVE_NONE;
        if ((ss - 1)->currentMove != MOVE_NONE && (ss - 1)->currentMove != MOVE_NULL) {
            Piece prevPc = pos.piece_on(to_sq((ss - 1)->currentMove));
            if (prevPc != NO_PIECE) countermove = Hist.counterMove[prevPc][to_sq((ss - 1)->currentMove)];
        }

        MovePicker mp(pos, ttMove, depth, &Hist, contHist, countermove, ss->killers);
        bool skipQuiets = false;
        value = bestValue;

        // ---- Step 10: loop over moves ----
        while ((move = mp.next_move(skipQuiets)) != MOVE_NONE) {
            if (move == excludedMove) continue;
            if (!pos.legal(move)) continue;

            ss->moveCount = ++moveCount;

            if (rootNode && moveCount == 1 && depth > 20 && elapsed() > 3000)
                std::cout << "info depth " << depth << " currmove " << uci_move(move)
                          << " currmovenumber " << moveCount << std::endl;

            if (PvNode) (ss + 1)->pv = nullptr;

            extension = 0;
            capture = pos.capture_stage(move);
            Piece movedPiece = pos.moved_piece(move);
            givesCheck = pos.gives_check(move);
            newDepth = depth - 1;

            int r = lmr_reduction(depth, moveCount);

            // ---- Step 11: pruning at shallow depth ----
            if (!rootNode && pos.non_pawn_material(us) > 0 && bestValue > VALUE_MATED_IN_MAX_PLY) {
                // Move-count (late move) pruning
                if (!skipQuiets && pruning_on(P_LMP) && moveCount >= (LMP_BASE + depth * depth) / (2 - improving))
                    skipQuiets = true;

                int lmrDepth = std::max(0, newDepth - r / 1024);

                if (capture || givesCheck) {
                    // Static exchange pruning for captures
                    if (pruning_on(P_SEE) && !pos.see_ge(move, -190 * depth)) continue;
                } else {
                    int history = (*contHist[0])[movedPiece][to_sq(move)]
                                + (*contHist[1])[movedPiece][to_sq(move)]
                                + (*contHist[3])[movedPiece][to_sq(move)];

                    // Continuation-history based pruning
                    if (pruning_on(P_HISTPRUNE) && lmrDepth < 6 && history < -3752 * depth) continue;

                    history += 2 * Hist.main[us][from_to(move)];
                    lmrDepth += history / 7838;
                    lmrDepth = std::max(lmrDepth, 0);

                    // Futility pruning at low depth
                    if (pruning_on(P_FUTILITY) && !ss->inCheck && lmrDepth < 12
                        && ss->staticEval != VALUE_NONE
                        && ss->staticEval + 112 + FUT_MARGIN * lmrDepth <= alpha)
                        continue;

                    // SEE pruning for quiets
                    if (pruning_on(P_SEE) && !pos.see_ge(move, -26 * lmrDepth * lmrDepth))
                        continue;
                }
            }

            // ---- Step 12: extensions ----
            if (ss->ply < RootDepth * 2) {
                // Singular extension: is the TT move the only good one?
                if (!rootNode && move == ttMove && !excludedMove && depth >= 6 && pruning_on(P_SINGULAR)
                    && ttValue != VALUE_NONE && std::abs(ttValue) < VALUE_MATE_IN_MAX_PLY
                    && (ttBoundSnap & BOUND_LOWER) && ttDepthSnap >= depth - 3) {

                    Value singularBeta = ttValue - (58 + 58 * (ss->ttPv && !PvNode)) * depth / 64;
                    int singularDepth = (depth - 1) / 2;

                    ss->excludedMove = move;
                    value = search<NonPV>(pos, ss, singularBeta - 1, singularBeta, singularDepth, cutNode);
                    ss->excludedMove = MOVE_NONE;

                    if (value < singularBeta) {
                        extension = 1;
                        if (!PvNode && value < singularBeta - 17 && ss->ply < RootDepth)
                            extension = 2;
                    } else if (singularBeta >= beta) {
                        return singularBeta;          // multi-cut
                    } else if (ttValue >= beta) {
                        extension = -2;
                    } else if (cutNode) {
                        extension = -2;
                    }
                } else if (givesCheck && depth > 9) {
                    extension = 1;
                }
            }

            newDepth += extension;

            ss->currentMove = move;
            ss->continuationHistory = &Hist.continuation[ss->inCheck][capture][movedPiece][to_sq(move)];

            ++Nodes;
            pos.do_move(move, st, givesCheck);

            // ---- Step 13: late move reduction ----
            if (ss->ttPv) r -= 1024;
            if (cutNode) r += LMR_CUTNODE;
            if (ttCapture && !capture) r += 1024;
            if (PvNode) r -= 1024;
            if (!improving) r += 1024;
            if ((ss + 1)->cutoffCnt > 3) r += 1024;
            if (move == ss->killers[0] || move == countermove) r -= 512;

            if (!capture) {
                ss->statScore = 2 * Hist.main[us][from_to(move)]
                              + (*contHist[0])[movedPiece][to_sq(move)]
                              + (*contHist[1])[movedPiece][to_sq(move)]
                              + (*contHist[3])[movedPiece][to_sq(move)]
                              - 3817;
                r -= ss->statScore * 1024 / 14000;
            } else {
                ss->statScore = 0;
                r -= 512;
            }

            if (depth >= 2 && moveCount > 1 + rootNode && pruning_on(P_LMR)) {
                int rr = std::max(0, r) / 1024;
                int d = std::clamp(newDepth - rr, 1, newDepth + 1);

                value = -search<NonPV>(pos, ss + 1, -(alpha + 1), -alpha, d, true);

                if (value > alpha && d < newDepth) {
                    // Research at full depth
                    bool doDeeper = value > bestValue + 45 + 2 * newDepth;
                    bool doShallower = value < bestValue + newDepth;
                    newDepth += doDeeper - doShallower;
                    if (newDepth > d)
                        value = -search<NonPV>(pos, ss + 1, -(alpha + 1), -alpha, newDepth, !cutNode);

                    int bonus = value >= beta ? stat_bonus(newDepth) : -stat_malus(newDepth);
                    update_continuation_histories(ss, movedPiece, to_sq(move), bonus);
                }
            } else if (!PvNode || moveCount > 1) {
                if (!ttMove && cutNode) r += 2048;
                value = -search<NonPV>(pos, ss + 1, -(alpha + 1), -alpha,
                                       newDepth - (r > 3500 ? 1 : 0), !cutNode);
            }

            // Full-window search for the PV move, or when a reduced search beat alpha
            if (PvNode && (moveCount == 1 || value > alpha)) {
                (ss + 1)->pv = pv;
                (ss + 1)->pv[0] = MOVE_NONE;
                value = -search<PV>(pos, ss + 1, -beta, -alpha, newDepth, false);
            }

            pos.undo_move(move);

            if (Stop.load(std::memory_order_relaxed)) return VALUE_ZERO;

            // ---- Step 14: check for a new best move ----
            if (rootNode) {
                auto it = std::find(RootMoves.begin(), RootMoves.end(), move);
                RootMove& rm = *it;
                rm.averageScore = rm.averageScore == -VALUE_INFINITE ? value
                                                                     : (2 * value + rm.averageScore) / 3;

                if (moveCount == 1 || value > alpha) {
                    rm.score = rm.uciScore = value;
                    rm.selDepth = SelDepth;
                    rm.pv.resize(1);
                    for (Move* m = (ss + 1)->pv; m && *m != MOVE_NONE; ++m) rm.pv.push_back(*m);

                    // Publish the new best line straight away.  This point is only
                    // reached when the search was NOT aborted, so whatever we finally
                    // report as bestmove is always a move we finished searching, and
                    // always matches the last PV we printed.  Re-sorting the root list
                    // after an aborted iteration mixes fresh -INF scores with stale ones
                    // and can otherwise surface a move that was never validated.
                    BestPV = rm.pv;
                    BestScore = value;
                    BestSelDepth = SelDepth;

                    // Deliberately NOT reporting an info line here, though the protocol
                    // says "ideally whenever the best move changes". Doing so was tried:
                    // it removed the cosmetic "bestmove does not match last PV" warning,
                    // but 600 games without it produced no stalls while ~1140 games with
                    // it produced two "engine not responsive" losses. Writing more to a
                    // pipe that the match manager is not draining can block the search
                    // thread, and a stall is scored as a loss while the warning costs
                    // nothing. Reliability wins.
                    if (moveCount > 1) ++BestMoveChanges;
                } else {
                    rm.score = -VALUE_INFINITE;
                }
            }

            if (value > bestValue) {
                bestValue = value;
                if (value > alpha) {
                    bestMove = move;
                    if (PvNode && !rootNode) update_pv(ss->pv, move, (ss + 1)->pv);

                    if (value >= beta) {
                        ss->cutoffCnt += 1 + !ttMove;
                        break;
                    }
                    alpha = value;
                    if (pruning_on(P_LOOPRED) && depth > 2 && depth < 14 && std::abs(value) < 12000) depth -= 2;
                }
            }

            // Record the move so its history can be penalised if it was not the best
            if (move != bestMove) {
                if (capture && captureCount < 32) capturesSearched[captureCount++] = move;
                else if (!capture && quietCount < 64) quietsSearched[quietCount++] = move;
            }
        }
    }

    // ---- Step 15: checkmate / stalemate ----
    if (moveCount == 0)
        bestValue = excludedMove ? alpha : ss->inCheck ? mated_in(ss->ply) : VALUE_DRAW;
    else if (bestMove)
        update_all_stats(pos, ss, bestMove, depth, quietsSearched, quietCount,
                         capturesSearched, captureCount);
    else if ((depth >= 5 || PvNode) && !priorCapture
             && (ss - 1)->currentMove != MOVE_NONE && (ss - 1)->currentMove != MOVE_NULL) {
        // No move improved alpha: credit the opponent's previous move
        Piece prevPc = pos.piece_on(to_sq((ss - 1)->currentMove));
        if (prevPc != NO_PIECE)
            update_continuation_histories(ss - 1, prevPc, to_sq((ss - 1)->currentMove), stat_bonus(depth));
    }

    // Feed the search result back into the correction history, but only when the
    // static evaluation was actually meaningful for this node: not in check, the
    // best move was not a capture, and the bound points the same way as the error.
#ifndef NO_CORRHIST
    if (!ss->inCheck && ss->staticEval != VALUE_NONE
        && !(bestMove && pos.capture_stage(bestMove))
        && ((bestValue < ss->staticEval && bestValue < beta)
            || (bestValue > ss->staticEval && bestMove)))
        corr_update(Hist.corr[us][corr_hist_index(pos.pawn_key())],
                    int(bestValue) - int(ss->staticEval), depth);
#endif

    if (!excludedMove)
        tte->save(posKey, value_to_tt(bestValue, ss->ply), ss->ttPv,
                  bestValue >= beta ? BOUND_LOWER
                  : (PvNode && bestMove) ? BOUND_EXACT : BOUND_UPPER,
                  depth, bestMove, unadjustedEval, TT.generation());

    return bestValue;
}

} // anonymous namespace

int64_t now() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch()).count();
}

void init() {
    if (const char* e = std::getenv("PRUNEMASK")) PruneMask = std::atoi(e);
    for (int d = 0; d < 64; ++d)
        for (int m = 0; m < 64; ++m)
            Reductions[d][m] = (d == 0 || m == 0)
                             ? 0
                             : int(1024 * (0.78 + std::log(double(d)) * std::log(double(m)) / 2.32));
}

void clear() {
    Hist.clear();
    TT.clear();
    Eval::clear_pawn_table();
}

// Used at start-up, where TT::resize has just handed back an already-zeroed table.
// Skipping the redundant 256 MB memset halves the memory traffic every engine
// process generates while starting, which is what a match manager sees as a stall.
void clear_soft() {
    Hist.clear();
    Eval::clear_pawn_table();
}

// ---------------------------------------------------------------------------
// Iterative deepening driver, run on the search thread
// ---------------------------------------------------------------------------
void think(Position& pos) {
    StartTime = now();
    Nodes = 0;
    NextTimeCheck = 1024;
    BestMoveChanges = 0;
    CompletedDepth = 0;
    LastBestMove = MOVE_NONE;
    LastBestMoveDepth = 0;
    LastBestScore = -VALUE_INFINITE;
    RootPos = &pos;

    NmpMinPly = 0;
    init_time(pos.side_to_move(), pos.game_ply());
    TT.new_search();

    // Build the root move list
    RootMoves.clear();
    for (const ExtMove& m : MoveList<LEGAL>(pos)) RootMoves.emplace_back(m.move);

    if (RootMoves.empty()) {
        if (!Quiet) {
            std::cout << "info depth 0 score " << (pos.checkers() ? "mate 0" : "cp 0") << std::endl;
            std::cout << "bestmove 0000" << std::endl;
        }
        BestPV.clear();
        BestScore = pos.checkers() ? -VALUE_MATE : VALUE_DRAW;
        return;
    }

    // Always have a legal move ready to report, even if we are stopped instantly.
    BestPV.assign(1, RootMoves[0].pv[0]);
    BestScore = -VALUE_INFINITE;
    BestSelDepth = 0;

    // Stack with room for the ss-6 .. ss+2 accesses
    Stack stack[MAX_PLY + 12] = {};
    Stack* ss = stack + 7;
    Move pv[MAX_PLY + 1];
    ss->pv = pv;

    for (int i = 7; i > 0; --i) {
        (ss - i)->continuationHistory = &Hist.continuation[0][0][NO_PIECE][0];
        (ss - i)->staticEval = VALUE_NONE;
        (ss - i)->currentMove = MOVE_NONE;
    }
    for (int i = 0; i <= MAX_PLY + 2; ++i) {
        if (ss + i < stack + MAX_PLY + 12) {
            (ss + i)->ply = i;
            (ss + i)->staticEval = VALUE_NONE;
        }
    }

    Value alpha, beta, delta;
    Value bestValue = -VALUE_INFINITE;
    int maxDepth = Limits.depth ? Limits.depth : MAX_PLY - 1;
    double prevTimeReduction = 1.0;
    int stableCount = 0;

    for (RootDepth = 1; RootDepth <= maxDepth && !Stop; ++RootDepth) {
        for (RootMove& rm : RootMoves) rm.previousScore = rm.score;

        SelDepth = 0;
        BestMoveChanges = 0;

        // ---- Aspiration window ----
        Value prev = RootMoves[0].averageScore;
        delta = Value(ASP_DELTA + std::abs(prev) / 12000);
        alpha = RootDepth >= 4 ? std::max(prev - delta, -VALUE_INFINITE) : -VALUE_INFINITE;
        beta  = RootDepth >= 4 ? std::min(prev + delta, VALUE_INFINITE) : VALUE_INFINITE;

        int failedHighCnt = 0;
        while (true) {
            int adjustedDepth = std::max(1, RootDepth - failedHighCnt);
            bestValue = search<Root>(pos, ss, alpha, beta, adjustedDepth, false);

            // Only re-order the root list when the iteration actually finished: an
            // aborted one leaves a mix of fresh -INF and stale scores.
            if (Stop) break;
            std::stable_sort(RootMoves.begin(), RootMoves.end());

            if (bestValue <= alpha) {
                beta = Value((alpha + beta) / 2);
                alpha = std::max(bestValue - delta, -VALUE_INFINITE);
                failedHighCnt = 0;
                if (RootDepth > 6) print_info(pos, RootDepth, bestValue, true, BOUND_UPPER);
            } else if (bestValue >= beta) {
                beta = std::min(bestValue + delta, VALUE_INFINITE);
                ++failedHighCnt;
                if (RootDepth > 6) print_info(pos, RootDepth, bestValue, true, BOUND_LOWER);
            } else
                break;

            delta += delta / 3;
        }

        if (!Stop) {
            std::stable_sort(RootMoves.begin(), RootMoves.end());
            CompletedDepth = RootDepth;
            print_info(pos, RootDepth, bestValue, false, BOUND_EXACT);

            if (BestPV[0] != LastBestMove) {
                LastBestMove = BestPV[0];
                LastBestMoveDepth = RootDepth;
                stableCount = 0;
            } else
                ++stableCount;
            LastBestScore = bestValue;
        }

        if (Limits.mate && bestValue >= VALUE_MATE_IN_MAX_PLY
            && VALUE_MATE - bestValue <= 2 * Limits.mate) break;

        // ---- Should we start another iteration? ----
        if (UseTimeMan && !Stop && !Limits.infinite) {
            // Spend longer when the best move is unstable or the score is falling
            double instability = 1.0 + 1.5 * double(BestMoveChanges);
            double fallingEval = 1.0;
            if (LastBestScore != -VALUE_INFINITE && RootDepth > 4)
                fallingEval = std::clamp(1.0 + 0.008 * double(RootMoves[0].previousScore - bestValue),
                                         0.75, 1.55);
            double stability = stableCount >= 8 ? 0.68 : stableCount >= 4 ? 0.82
                             : stableCount >= 2 ? 0.93 : 1.0;

            double total = double(OptimumTime) * instability * fallingEval * stability;
            total = std::min(total, double(MaximumTime));

            // A forced win is found - no reason to keep burning clock on it
            if (bestValue >= VALUE_MATE_IN_MAX_PLY && RootDepth >= 8) break;

            // Do not start an iteration we are unlikely to finish
            if (double(elapsed()) > total) break;
        }
        prevTimeReduction = 1.0;
        (void)prevTimeReduction;
    }

    // Always answer with a legal move, and always the one whose PV we last printed.
    Move best = !BestPV.empty() ? BestPV[0]
              : (!RootMoves.empty() ? RootMoves[0].pv[0] : MOVE_NONE);
    if (!Quiet) std::cout << "bestmove " << uci_move(best) << std::endl;
}

// Run a search with no UCI output; returns the score and sets bestMove.
Value search_quiet(Position& pos, const LimitsType& lim, Move& bestMove) {
    Limits = lim;
    Stop = false;
    Quiet = true;
    think(pos);
    Quiet = false;
    bestMove = BestPV.empty() ? MOVE_NONE : BestPV[0];
    return BestScore;
}

} // namespace Search
