#pragma once
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <string>
#include <algorithm>
#include <immintrin.h>

typedef uint64_t U64;
typedef uint64_t Bitboard;

enum Color : int { WHITE = 0, BLACK = 1, COLOR_NB = 2 };

enum PieceType : int {
    NO_PIECE_TYPE = 0, PAWN = 1, KNIGHT = 2, BISHOP = 3, ROOK = 4, QUEEN = 5, KING = 6,
    ALL_PIECES = 0, PIECE_TYPE_NB = 7
};

enum Piece : int {
    NO_PIECE = 0,
    W_PAWN = 1, W_KNIGHT, W_BISHOP, W_ROOK, W_QUEEN, W_KING,
    B_PAWN = 9, B_KNIGHT, B_BISHOP, B_ROOK, B_QUEEN, B_KING,
    PIECE_NB = 16
};

constexpr Piece make_piece(Color c, PieceType pt) { return Piece((c << 3) + pt); }
constexpr PieceType type_of(Piece p) { return PieceType(p & 7); }
constexpr Color color_of(Piece p) { return Color(p >> 3); }
constexpr Color operator~(Color c) { return Color(c ^ 1); }

enum Square : int {
    SQ_A1 = 0, SQ_B1, SQ_C1, SQ_D1, SQ_E1, SQ_F1, SQ_G1, SQ_H1,
    SQ_A2, SQ_B2, SQ_C2, SQ_D2, SQ_E2, SQ_F2, SQ_G2, SQ_H2,
    SQ_A3, SQ_B3, SQ_C3, SQ_D3, SQ_E3, SQ_F3, SQ_G3, SQ_H3,
    SQ_A4, SQ_B4, SQ_C4, SQ_D4, SQ_E4, SQ_F4, SQ_G4, SQ_H4,
    SQ_A5, SQ_B5, SQ_C5, SQ_D5, SQ_E5, SQ_F5, SQ_G5, SQ_H5,
    SQ_A6, SQ_B6, SQ_C6, SQ_D6, SQ_E6, SQ_F6, SQ_G6, SQ_H6,
    SQ_A7, SQ_B7, SQ_C7, SQ_D7, SQ_E7, SQ_F7, SQ_G7, SQ_H7,
    SQ_A8, SQ_B8, SQ_C8, SQ_D8, SQ_E8, SQ_F8, SQ_G8, SQ_H8,
    SQ_NONE = 64, SQUARE_NB = 64
};

enum Direction : int { NORTH = 8, EAST = 1, SOUTH = -8, WEST = -1,
    NORTH_EAST = 9, SOUTH_EAST = -7, SOUTH_WEST = -9, NORTH_WEST = 7 };

constexpr int rank_of(int s) { return s >> 3; }
constexpr int file_of(int s) { return s & 7; }
constexpr int make_square(int f, int r) { return (r << 3) + f; }
constexpr int relative_square(Color c, int s) { return s ^ (c * 56); }
constexpr int relative_rank(Color c, int s) { return rank_of(s) ^ (c * 7); }
inline bool is_ok_sq(int s) { return s >= 0 && s < 64; }

// Castling rights
enum CastlingRight : int {
    NO_CASTLING = 0, WHITE_OO = 1, WHITE_OOO = 2, BLACK_OO = 4, BLACK_OOO = 8,
    ANY_CASTLING = 15
};

// ---------------- Move encoding (16 bit) ----------------
// bits 0-5 : from square
// bits 6-11: to square
// bits 12-13: promotion piece type - KNIGHT  (0=N,1=B,2=R,3=Q)
// bits 14-15: move type (0 normal, 1 promotion, 2 en passant, 3 castling)
typedef uint16_t Move;

enum MoveType : int { NORMAL = 0, PROMOTION = 1, EN_PASSANT = 2, CASTLING = 3 };

constexpr Move MOVE_NONE = 0;
constexpr Move MOVE_NULL = 65;  // a1a2, impossible as a real move

constexpr int from_sq(Move m) { return m & 0x3F; }
constexpr int to_sq(Move m) { return (m >> 6) & 0x3F; }
constexpr MoveType type_of_move(Move m) { return MoveType((m >> 14) & 3); }
constexpr PieceType promotion_type(Move m) { return PieceType(((m >> 12) & 3) + KNIGHT); }
constexpr Move make_move(int from, int to) { return Move(from | (to << 6)); }
constexpr Move make_move(MoveType mt, int from, int to, PieceType pt = KNIGHT) {
    return Move(from | (to << 6) | ((pt - KNIGHT) << 12) | (mt << 14));
}
constexpr bool is_ok_move(Move m) { return m != MOVE_NONE && m != MOVE_NULL; }

// ---------------- Value ----------------
typedef int Value;
constexpr Value VALUE_ZERO = 0;
constexpr Value VALUE_DRAW = 0;
constexpr Value VALUE_INFINITE = 32001;
constexpr Value VALUE_NONE = 32002;
constexpr Value VALUE_MATE = 32000;
constexpr int MAX_PLY = 246;
constexpr Value VALUE_MATE_IN_MAX_PLY = VALUE_MATE - MAX_PLY;
constexpr Value VALUE_MATED_IN_MAX_PLY = -VALUE_MATE_IN_MAX_PLY;
constexpr Value VALUE_TB_WIN = VALUE_MATE_IN_MAX_PLY - 1;

constexpr Value mate_in(int ply) { return VALUE_MATE - ply; }
constexpr Value mated_in(int ply) { return -VALUE_MATE + ply; }

// Packed score: middlegame in high 16 bits (biased), endgame in low 16
typedef int32_t Score;
constexpr Score make_score(int mg, int eg) { return Score((int32_t)((uint32_t)eg << 16) + mg); }
inline Value eg_value(Score s) { return Value(int16_t(uint16_t(uint32_t(s + 0x8000) >> 16))); }
inline Value mg_value(Score s) { return Value(int16_t(uint16_t(uint32_t(s)))); }
constexpr Score SCORE_ZERO = 0;

// ---------------- Bitboard helpers ----------------
constexpr Bitboard FileABB = 0x0101010101010101ULL;
constexpr Bitboard FileBBB = FileABB << 1;
constexpr Bitboard FileCBB = FileABB << 2;
constexpr Bitboard FileDBB = FileABB << 3;
constexpr Bitboard FileEBB = FileABB << 4;
constexpr Bitboard FileFBB = FileABB << 5;
constexpr Bitboard FileGBB = FileABB << 6;
constexpr Bitboard FileHBB = FileABB << 7;

constexpr Bitboard Rank1BB = 0xFFULL;
constexpr Bitboard Rank2BB = Rank1BB << 8;
constexpr Bitboard Rank3BB = Rank1BB << 16;
constexpr Bitboard Rank4BB = Rank1BB << 24;
constexpr Bitboard Rank5BB = Rank1BB << 32;
constexpr Bitboard Rank6BB = Rank1BB << 40;
constexpr Bitboard Rank7BB = Rank1BB << 48;
constexpr Bitboard Rank8BB = Rank1BB << 56;

constexpr Bitboard AllSquares = ~Bitboard(0);
constexpr Bitboard DarkSquares = 0xAA55AA55AA55AA55ULL;

constexpr Bitboard square_bb(int s) { return 1ULL << s; }
constexpr Bitboard file_bb(int f) { return FileABB << f; }
constexpr Bitboard rank_bb(int r) { return Rank1BB << (8 * r); }
constexpr Bitboard file_bb_of(int s) { return file_bb(file_of(s)); }
constexpr Bitboard rank_bb_of(int s) { return rank_bb(rank_of(s)); }

inline int popcount(Bitboard b) { return (int)__builtin_popcountll(b); }
inline int lsb(Bitboard b) { return (int)__builtin_ctzll(b); }
inline int msb(Bitboard b) { return 63 ^ (int)__builtin_clzll(b); }
inline int pop_lsb(Bitboard& b) { int s = lsb(b); b &= b - 1; return s; }
inline bool more_than_one(Bitboard b) { return b & (b - 1); }

template<Direction D> constexpr Bitboard shift(Bitboard b) {
    return D == NORTH ? b << 8 : D == SOUTH ? b >> 8
         : D == EAST ? (b & ~FileHBB) << 1 : D == WEST ? (b & ~FileABB) >> 1
         : D == NORTH_EAST ? (b & ~FileHBB) << 9 : D == NORTH_WEST ? (b & ~FileABB) << 7
         : D == SOUTH_EAST ? (b & ~FileHBB) >> 7 : D == SOUTH_WEST ? (b & ~FileABB) >> 9
         : 0;
}

inline Bitboard shift_up(Bitboard b, Color c) { return c == WHITE ? b << 8 : b >> 8; }

// pawn attacks from a set of pawns of colour c
inline Bitboard pawn_attacks_bb(Color c, Bitboard b) {
    return c == WHITE ? shift<NORTH_WEST>(b) | shift<NORTH_EAST>(b)
                      : shift<SOUTH_WEST>(b) | shift<SOUTH_EAST>(b);
}

inline int square_distance(int a, int b);

// String helpers
inline std::string square_to_string(int s) {
    if (s == SQ_NONE) return "-";
    char buf[3] = { char('a' + file_of(s)), char('1' + rank_of(s)), 0 };
    return std::string(buf);
}
