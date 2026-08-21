#pragma once
#include "position.h"
#include "movegen.h"
#include "history.h"

// Staged move generation.  Moves are produced one at a time in roughly
// best-first order, so the expensive stages are only reached when the cheap
// ones fail to produce a cutoff.
enum MPStage : int {
    MAIN_TT = 0, CAPTURE_INIT, GOOD_CAPTURE, REFUTATION, QUIET_INIT, QUIET, BAD_CAPTURE,
    EVASION_TT, EVASION_INIT, EVASION,
    PROBCUT_TT, PROBCUT_INIT, PROBCUT,
    QSEARCH_TT, QCAPTURE_INIT, QCAPTURE
};

constexpr int CONT_HIST_PLIES = 6;

class MovePicker {
public:
    MovePicker(const MovePicker&) = delete;
    MovePicker& operator=(const MovePicker&) = delete;

    // Main search
    MovePicker(const Position& p, Move ttm, int d, const Histories* h,
               const PieceToHistory** ch, Move cm, const Move* killers)
        : pos(p), hist(h), contHist(ch), ttMove(ttm), depth(d) {
        refutations[0] = { killers[0], 0 };
        refutations[1] = { killers[1], 0 };
        refutations[2] = { cm, 0 };
        stage = (pos.checkers() ? EVASION_TT : MAIN_TT) + !(ttm && pos.pseudo_legal(ttm));
    }

    // Quiescence search
    MovePicker(const Position& p, Move ttm, int d, const Histories* h, const PieceToHistory** ch)
        : pos(p), hist(h), contHist(ch), ttMove(ttm), depth(d) {
        refutations[0] = refutations[1] = refutations[2] = { MOVE_NONE, 0 };
        stage = (pos.checkers() ? EVASION_TT : QSEARCH_TT) + !(ttm && pos.pseudo_legal(ttm));
    }

    // ProbCut
    MovePicker(const Position& p, Move ttm, int th, const Histories* h)
        : pos(p), hist(h), ttMove(ttm), threshold(th) {
        refutations[0] = refutations[1] = refutations[2] = { MOVE_NONE, 0 };
        stage = PROBCUT_TT + !(ttm && pos.capture_stage(ttm) && pos.pseudo_legal(ttm)
                               && pos.see_ge(ttm, threshold));
    }

    Move next_move(bool skipQuiets = false);
    int  last_move_score() const { return lastScore; }
    int  current_stage() const { return stage; }

private:
    template<GenType T> void score();

    // Partial selection sort: pick the best remaining move, apply a filter, advance.
    template<typename Pred>
    Move select(Pred filter) {
        while (cur < endMoves) {
            ExtMove* best = cur;
            for (ExtMove* p = cur + 1; p < endMoves; ++p)
                if (p->value > best->value) best = p;
            if (best != cur) std::swap(*best, *cur);

            if (cur->move != ttMove && filter()) {
                lastScore = cur->value;
                return (cur++)->move;
            }
            ++cur;
        }
        return MOVE_NONE;
    }

    const Position& pos;
    const Histories* hist = nullptr;
    const PieceToHistory** contHist = nullptr;
    Move ttMove = MOVE_NONE;
    ExtMove refutations[3];
    ExtMove *cur = nullptr, *endMoves = nullptr, *endBadCaptures = nullptr;
    int stage = 0;
    int depth = 0;
    int threshold = 0;
    int lastScore = 0;
    ExtMove moves[MAX_MOVES];
};

template<GenType Type>
void MovePicker::score() {
    static_assert(Type == CAPTURES || Type == QUIETS || Type == EVASIONS, "bad gen type");

    Bitboard threatenedByPawn = 0, threatenedByMinor = 0, threatenedByRook = 0, threatenedPieces = 0;
    const Color us = pos.side_to_move();
    if constexpr (Type == QUIETS) {
        threatenedByPawn  = pos.attacks_by(~us, PAWN);
        threatenedByMinor = pos.attacks_by(~us, KNIGHT) | pos.attacks_by(~us, BISHOP) | threatenedByPawn;
        threatenedByRook  = pos.attacks_by(~us, ROOK) | threatenedByMinor;
        threatenedPieces  = (pos.pieces(us, QUEEN) & threatenedByRook)
                          | (pos.pieces(us, ROOK) & threatenedByMinor)
                          | (pos.pieces(us, KNIGHT, BISHOP) & threatenedByPawn);
    }

    const int pawnIdx = pawn_hist_index(pos.pawn_key());

    for (ExtMove* m = cur; m < endMoves; ++m) {
        const Move mv = m->move;
        const int from = from_sq(mv), to = to_sq(mv);
        const Piece pc = pos.moved_piece(mv);
        const PieceType pt = type_of(pc);

        if constexpr (Type == CAPTURES) {
            PieceType captured = (type_of_move(mv) == EN_PASSANT) ? PAWN : type_of(pos.piece_on(to));
            int promoBonus = (type_of_move(mv) == PROMOTION)
                           ? SEEValue[promotion_type(mv)] - SEEValue[PAWN] : 0;
            m->value = 7 * (SEEValue[captured] + promoBonus)
                     + hist->capture[pc][to][captured] / 16;
        } else if constexpr (Type == QUIETS) {
            m->value  = 2 * hist->main[us][from_to(mv)];
            m->value += 2 * hist->pawnHist[pawnIdx][pc][to];
            m->value += (*contHist[0])[pc][to];
            m->value += (*contHist[1])[pc][to];
            m->value += (*contHist[2])[pc][to] / 4;
            m->value += (*contHist[3])[pc][to];
            m->value += (*contHist[5])[pc][to];

            // Moving a threatened piece to safety, or walking into a threat
            if (threatenedPieces & square_bb(from))
                m->value += (pt == QUEEN && !(square_bb(to) & threatenedByRook))  ? 51000
                          : (pt == ROOK && !(square_bb(to) & threatenedByMinor))  ? 25000
                          : (!(square_bb(to) & threatenedByPawn))                 ? 14000
                                                                                  : 0;
            if (pt != KING && pt != PAWN) {
                Bitboard danger = (pt == QUEEN) ? threatenedByRook
                                : (pt == ROOK)  ? threatenedByMinor
                                                : threatenedByPawn;
                if (square_bb(to) & danger) m->value -= 12000;
            }

            // A quiet move that gives check deserves an early look
            if (pos.check_squares(pt) & square_bb(to)) m->value += 16384;
        } else {  // EVASIONS
            if (pos.capture(mv))
                m->value = SEEValue[type_of(pos.piece_on(to))] - int(pt) + (1 << 28);
            else
                m->value = hist->main[us][from_to(mv)]
                         + (*contHist[0])[pc][to]
                         + hist->pawnHist[pawnIdx][pc][to];
        }
    }
}

inline Move MovePicker::next_move(bool skipQuiets) {
top:
    switch (stage) {

    case MAIN_TT:
    case EVASION_TT:
    case QSEARCH_TT:
    case PROBCUT_TT:
        ++stage;
        return ttMove;

    case CAPTURE_INIT:
    case PROBCUT_INIT:
    case QCAPTURE_INIT:
        cur = endBadCaptures = moves;
        endMoves = generate<CAPTURES>(pos, cur);
        score<CAPTURES>();
        ++stage;
        goto top;

    case GOOD_CAPTURE:
        if (Move m = select([&]() {
                // Losing captures are copied to the front of the array and revisited last
                if (pos.see_ge(cur->move, -cur->value / 18)) return true;
                *endBadCaptures++ = *cur;
                return false;
            }))
            return m;

        cur = refutations;
        endMoves = refutations + 3;
        if (refutations[2].move == refutations[0].move || refutations[2].move == refutations[1].move)
            --endMoves;
        ++stage;
        [[fallthrough]];

    case REFUTATION:
        if (Move m = select([&]() {
                return cur->move != MOVE_NONE && !pos.capture_stage(cur->move)
                    && pos.pseudo_legal(cur->move);
            }))
            return m;
        ++stage;
        [[fallthrough]];

    case QUIET_INIT:
        if (!skipQuiets) {
            cur = endBadCaptures;
            endMoves = generate<QUIETS>(pos, cur);
            score<QUIETS>();
        }
        ++stage;
        [[fallthrough]];

    case QUIET:
        if (!skipQuiets)
            if (Move m = select([&]() {
                    return cur->move != refutations[0].move
                        && cur->move != refutations[1].move
                        && cur->move != refutations[2].move;
                }))
                return m;
        cur = moves;
        endMoves = endBadCaptures;
        ++stage;
        [[fallthrough]];

    case BAD_CAPTURE:
        return select([]() { return true; });

    case EVASION_INIT:
        cur = moves;
        endMoves = generate<EVASIONS>(pos, cur);
        score<EVASIONS>();
        ++stage;
        [[fallthrough]];

    case EVASION:
        return select([]() { return true; });

    case PROBCUT:
        return select([&]() { return pos.see_ge(cur->move, threshold); });

    case QCAPTURE:
        return select([]() { return true; });
    }
    return MOVE_NONE;
}
