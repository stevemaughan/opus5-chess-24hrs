#pragma once
#include "position.h"

// CAPTURES     : all captures plus every promotion (push- and capture-promotions)
// QUIETS       : all non-captures except promotions (plus castling)
// NON_EVASIONS : CAPTURES + QUIETS, i.e. everything, used when not in check
// EVASIONS     : every move that addresses an existing check
// LEGAL        : fully legal moves
enum GenType { CAPTURES, QUIETS, EVASIONS, NON_EVASIONS, LEGAL };

struct ExtMove {
    Move move;
    int  value;
    operator Move() const { return move; }
    void operator=(Move m) { move = m; }
};

inline bool operator<(const ExtMove& a, const ExtMove& b) { return a.value < b.value; }

constexpr int MAX_MOVES = 256;

namespace MoveGenImpl {

template<Color Us, GenType Type>
ExtMove* generate_promotions(ExtMove* moveList, int from, int to) {
    *moveList++ = { make_move(PROMOTION, from, to, QUEEN), 0 };
    *moveList++ = { make_move(PROMOTION, from, to, KNIGHT), 0 };
    *moveList++ = { make_move(PROMOTION, from, to, ROOK), 0 };
    *moveList++ = { make_move(PROMOTION, from, to, BISHOP), 0 };
    return moveList;
}

template<Color Us, GenType Type>
ExtMove* generate_pawn_moves(const Position& pos, ExtMove* moveList, Bitboard target) {
    constexpr Color  Them     = ~Us;
    constexpr Bitboard TRank7BB = (Us == WHITE ? Rank7BB : Rank2BB);
    constexpr Bitboard TRank3BB = (Us == WHITE ? Rank3BB : Rank6BB);
    constexpr Direction Up      = (Us == WHITE ? NORTH : SOUTH);
    constexpr Direction UpRight = (Us == WHITE ? NORTH_EAST : SOUTH_WEST);
    constexpr Direction UpLeft  = (Us == WHITE ? NORTH_WEST : SOUTH_EAST);

    const Bitboard emptySquares = ~pos.pieces();
    const Bitboard enemies = (Type == EVASIONS) ? pos.pieces(Them) & target : pos.pieces(Them);

    Bitboard pawns       = pos.pieces(Us, PAWN);
    Bitboard pawnsOn7    = pawns & TRank7BB;
    Bitboard pawnsNotOn7 = pawns & ~TRank7BB;

    // ---- Single and double pushes ----
    if (Type != CAPTURES) {
        Bitboard b1 = shift<Up>(pawnsNotOn7) & emptySquares;
        Bitboard b2 = shift<Up>(b1 & TRank3BB) & emptySquares;

        if (Type == EVASIONS) { b1 &= target; b2 &= target; }

        while (b1) { int to = pop_lsb(b1); *moveList++ = { make_move(to - Up, to), 0 }; }
        while (b2) { int to = pop_lsb(b2); *moveList++ = { make_move(to - Up - Up, to), 0 }; }
    }

    // ---- Promotions (generated in the CAPTURES / EVASIONS / NON_EVASIONS sets) ----
    if (pawnsOn7 && Type != QUIETS) {
        Bitboard b1 = shift<UpRight>(pawnsOn7) & enemies;
        Bitboard b2 = shift<UpLeft>(pawnsOn7) & enemies;
        Bitboard b3 = shift<Up>(pawnsOn7) & emptySquares;

        if (Type == EVASIONS) b3 &= target;

        while (b1) { int to = pop_lsb(b1); moveList = generate_promotions<Us, Type>(moveList, to - UpRight, to); }
        while (b2) { int to = pop_lsb(b2); moveList = generate_promotions<Us, Type>(moveList, to - UpLeft, to); }
        while (b3) { int to = pop_lsb(b3); moveList = generate_promotions<Us, Type>(moveList, to - Up, to); }
    }

    // ---- Ordinary captures and en passant ----
    if (Type != QUIETS) {
        Bitboard b1 = shift<UpRight>(pawnsNotOn7) & enemies;
        Bitboard b2 = shift<UpLeft>(pawnsNotOn7) & enemies;

        while (b1) { int to = pop_lsb(b1); *moveList++ = { make_move(to - UpRight, to), 0 }; }
        while (b2) { int to = pop_lsb(b2); *moveList++ = { make_move(to - UpLeft, to), 0 }; }

        if (pos.ep_square() != SQ_NONE) {
            int epsq = pos.ep_square();
            int capsq = epsq - Up;          // the pawn that would be captured
            // While in check, an en passant capture is only an evasion if it removes the
            // checking pawn (capsq is the checker) or interposes on the checking ray (epsq
            // is a blocking square). `target` is (between | checkers), so one test covers both.
            if (Type != EVASIONS || (target & (square_bb(epsq) | square_bb(capsq)))) {
                Bitboard b = pawnsNotOn7 & BB::PawnAttacks[Them][epsq];
                while (b) *moveList++ = { make_move(EN_PASSANT, pop_lsb(b), epsq), 0 };
            }
        }
    }

    return moveList;
}

template<Color Us, PieceType Pt>
ExtMove* generate_piece_moves(const Position& pos, ExtMove* moveList, Bitboard target) {
    static_assert(Pt != PAWN && Pt != KING, "handled separately");
    Bitboard bb = pos.pieces(Us, Pt);
    while (bb) {
        int from = pop_lsb(bb);
        Bitboard b = BB::attacks_bb<Pt>(from, pos.pieces()) & target;
        while (b) *moveList++ = { make_move(from, pop_lsb(b)), 0 };
    }
    return moveList;
}

template<Color Us, GenType Type>
ExtMove* generate_all(const Position& pos, ExtMove* moveList) {
    const int ksq = pos.king_square(Us);
    Bitboard target;

    // In double check only the king can move
    if (Type != EVASIONS || !more_than_one(pos.checkers())) {
        target = (Type == EVASIONS)     ? between_bb(ksq, lsb(pos.checkers())) | pos.checkers()
               : (Type == NON_EVASIONS) ? ~pos.pieces(Us)
               : (Type == CAPTURES)     ? pos.pieces(~Us)
                                        : ~pos.pieces();

        moveList = generate_pawn_moves<Us, Type>(pos, moveList, target);
        moveList = generate_piece_moves<Us, KNIGHT>(pos, moveList, target);
        moveList = generate_piece_moves<Us, BISHOP>(pos, moveList, target);
        moveList = generate_piece_moves<Us, ROOK>(pos, moveList, target);
        moveList = generate_piece_moves<Us, QUEEN>(pos, moveList, target);
    }

    // King moves
    Bitboard kingTarget = (Type == EVASIONS)     ? ~pos.pieces(Us)
                        : (Type == NON_EVASIONS) ? ~pos.pieces(Us)
                        : (Type == CAPTURES)     ? pos.pieces(~Us)
                                                 : ~pos.pieces();
    Bitboard b = BB::PseudoAttacks[KING][ksq] & kingTarget;
    while (b) *moveList++ = { make_move(ksq, pop_lsb(b)), 0 };

    // Castling
    // Never generated while in check: legal() only verifies the squares the king crosses
    // and lands on, not the square it starts from.
    if ((Type == QUIETS || Type == NON_EVASIONS) && !pos.checkers()
        && pos.can_castle(Us == WHITE ? (WHITE_OO | WHITE_OOO) : (BLACK_OO | BLACK_OOO))) {
        int kingSideCr  = (Us == WHITE ? WHITE_OO : BLACK_OO);
        int queenSideCr = (Us == WHITE ? WHITE_OOO : BLACK_OOO);
        if (pos.can_castle(kingSideCr) && !pos.castling_impeded(kingSideCr))
            *moveList++ = { make_move(CASTLING, ksq, relative_square(Us, SQ_G1)), 0 };
        if (pos.can_castle(queenSideCr) && !pos.castling_impeded(queenSideCr))
            *moveList++ = { make_move(CASTLING, ksq, relative_square(Us, SQ_C1)), 0 };
    }

    return moveList;
}

} // namespace MoveGenImpl

template<GenType Type>
ExtMove* generate(const Position& pos, ExtMove* moveList) {
    if constexpr (Type == LEGAL) {
        ExtMove* cur = moveList;
        ExtMove* end = pos.checkers() ? generate<EVASIONS>(pos, moveList)
                                      : generate<NON_EVASIONS>(pos, moveList);
        while (cur != end) {
            if (!pos.legal(*cur)) *cur = *(--end);
            else ++cur;
        }
        return end;
    } else {
        return pos.side_to_move() == WHITE ? MoveGenImpl::generate_all<WHITE, Type>(pos, moveList)
                                           : MoveGenImpl::generate_all<BLACK, Type>(pos, moveList);
    }
}

template<GenType T>
struct MoveList {
    explicit MoveList(const Position& pos) : last(generate<T>(pos, moveList)) {}
    const ExtMove* begin() const { return moveList; }
    const ExtMove* end() const { return last; }
    size_t size() const { return last - moveList; }
    bool contains(Move m) const {
        for (const ExtMove& em : *this) if (em.move == m) return true;
        return false;
    }
private:
    ExtMove moveList[MAX_MOVES], *last;
};
