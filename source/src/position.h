#pragma once
#include "types.h"
#include "bitboard.h"
#include <string>

namespace Zobrist {
extern U64 psq[PIECE_NB][64];
extern U64 enpassant[8];
extern U64 castling[16];
extern U64 side;
void init();
}

// Rough piece values used for the null-move zugzwang guard and SEE.
constexpr int PieceValue[PIECE_TYPE_NB] = { 0, 100, 320, 330, 500, 950, 0 };
constexpr int SEEValue[PIECE_TYPE_NB]   = { 0, 100, 325, 340, 500, 985, 0 };

struct StateInfo {
    // Copied when a move is made
    U64   pawnKey;
    int   nonPawnMaterial[COLOR_NB];
    int   castlingRights;
    int   rule50;
    int   pliesFromNull;
    int   epSquare;

    // Recomputed when a move is made
    U64      key;
    Bitboard checkersBB;
    Bitboard blockersForKing[COLOR_NB];
    Bitboard pinners[COLOR_NB];
    Bitboard checkSquares[PIECE_TYPE_NB];
    Piece    capturedPiece;
    int      repetition;
    Move     currentMove;
    Piece    movedPiece;
    StateInfo* previous;
};

class Position {
public:
    Position() = default;
    Position(const Position&) = delete;
    Position& operator=(const Position&) = delete;

    void set(const std::string& fen, StateInfo* si);
    std::string fen() const;

    // Board accessors
    Bitboard pieces() const { return byTypeBB[ALL_PIECES]; }
    Bitboard pieces(PieceType pt) const { return byTypeBB[pt]; }
    Bitboard pieces(PieceType p1, PieceType p2) const { return byTypeBB[p1] | byTypeBB[p2]; }
    Bitboard pieces(Color c) const { return byColorBB[c]; }
    Bitboard pieces(Color c, PieceType pt) const { return byColorBB[c] & byTypeBB[pt]; }
    Bitboard pieces(Color c, PieceType p1, PieceType p2) const { return byColorBB[c] & (byTypeBB[p1] | byTypeBB[p2]); }
    Piece piece_on(int s) const { return board[s]; }
    bool empty(int s) const { return board[s] == NO_PIECE; }
    int count(Color c, PieceType pt) const { return pieceCount[make_piece(c, pt)]; }
    int count(PieceType pt) const { return pieceCount[make_piece(WHITE, pt)] + pieceCount[make_piece(BLACK, pt)]; }
    int king_square(Color c) const { return kingSquare[c]; }

    // State accessors
    Color side_to_move() const { return sideToMove; }
    int ep_square() const { return st->epSquare; }
    int castling_rights() const { return st->castlingRights; }
    bool can_castle(int cr) const { return st->castlingRights & cr; }
    bool castling_impeded(int cr) const { return pieces() & castlingPath[cr]; }
    int castling_rook_square(int cr) const { return castlingRookSquare[cr]; }
    Bitboard checkers() const { return st->checkersBB; }
    Bitboard blockers_for_king(Color c) const { return st->blockersForKing[c]; }
    Bitboard check_squares(PieceType pt) const { return st->checkSquares[pt]; }
    U64 key() const { return st->key; }
    U64 pawn_key() const { return st->pawnKey; }
    int rule50_count() const { return st->rule50; }
    int game_ply() const { return gamePly; }
    int non_pawn_material(Color c) const { return st->nonPawnMaterial[c]; }
    int non_pawn_material() const { return st->nonPawnMaterial[WHITE] + st->nonPawnMaterial[BLACK]; }
    StateInfo* state() const { return st; }
    Piece captured_piece() const { return st->capturedPiece; }

    // Attacks
    Bitboard attackers_to(int s, Bitboard occ) const;
    Bitboard attackers_to(int s) const { return attackers_to(s, pieces()); }
    Bitboard attacks_by(Color c, PieceType pt) const;

    // Move properties
    bool legal(Move m) const;
    bool pseudo_legal(Move m) const;
    bool capture(Move m) const {
        return (!empty(to_sq(m)) && type_of_move(m) != CASTLING) || type_of_move(m) == EN_PASSANT;
    }
    bool capture_stage(Move m) const {  // captures + queen promotions
        return capture(m) || (type_of_move(m) == PROMOTION && promotion_type(m) == QUEEN);
    }
    bool gives_check(Move m) const;
    Piece moved_piece(Move m) const { return board[from_sq(m)]; }
    bool advanced_pawn_push(Move m) const {
        return type_of(board[from_sq(m)]) == PAWN && relative_rank(sideToMove, to_sq(m)) >= 5;
    }

    // Doing and undoing moves
    void do_move(Move m, StateInfo& newSt) { do_move(m, newSt, gives_check(m)); }
    void do_move(Move m, StateInfo& newSt, bool givesCheck);
    void undo_move(Move m);
    void do_null_move(StateInfo& newSt);
    void undo_null_move();

    // Static exchange evaluation
    bool see_ge(Move m, int threshold = 0) const;

    // Draw detection
    bool is_draw(int ply) const;
    bool upcoming_repetition(int ply) const;
    bool has_non_pawn_material(Color c) const { return st->nonPawnMaterial[c] > 0; }

    U64 key_after(Move m) const;

    void set_state() const;

private:
    void put_piece(Piece pc, int s);
    void remove_piece(int s);
    void move_piece(int from, int to);
    template<bool Do> void do_castling(Color us, int from, int to, int& rfrom, int& rto);
    void set_castling_right(Color c, int rfrom);
    void set_check_info() const;

    Bitboard byTypeBB[PIECE_TYPE_NB];
    Bitboard byColorBB[COLOR_NB];
    Piece    board[64];
    int      pieceCount[PIECE_NB];
    int      kingSquare[COLOR_NB];
    int      castlingRightsMask[64];
    int      castlingRookSquare[16];
    Bitboard castlingPath[16];
    int      gamePly;
    Color    sideToMove;
    StateInfo* st;
};

inline void Position::put_piece(Piece pc, int s) {
    board[s] = pc;
    byTypeBB[ALL_PIECES] |= byTypeBB[type_of(pc)] |= square_bb(s);
    byColorBB[color_of(pc)] |= square_bb(s);
    pieceCount[pc]++;
    if (type_of(pc) == KING) kingSquare[color_of(pc)] = s;
}

inline void Position::remove_piece(int s) {
    Piece pc = board[s];
    byTypeBB[ALL_PIECES] ^= square_bb(s);
    byTypeBB[type_of(pc)] ^= square_bb(s);
    byColorBB[color_of(pc)] ^= square_bb(s);
    board[s] = NO_PIECE;
    pieceCount[pc]--;
}

inline void Position::move_piece(int from, int to) {
    Piece pc = board[from];
    Bitboard fromTo = square_bb(from) | square_bb(to);
    byTypeBB[ALL_PIECES] ^= fromTo;
    byTypeBB[type_of(pc)] ^= fromTo;
    byColorBB[color_of(pc)] ^= fromTo;
    board[from] = NO_PIECE;
    board[to] = pc;
    if (type_of(pc) == KING) kingSquare[color_of(pc)] = to;
}
