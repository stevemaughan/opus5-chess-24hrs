#pragma once
#include "position.h"
#include "movegen.h"

// ---------------------------------------------------------------------------
// Hand-crafted tapered evaluation.
//
// The material values and piece-square tables are PeSTO's (published on the
// Chess Programming Wiki), which the benchmark rules explicitly permit.  Every
// other term below - mobility, pawn structure, passed pawns, king safety,
// threats, endgame scaling - is my own, with values chosen by reasoning about
// the pawn=100 scale and refined by testing.
// ---------------------------------------------------------------------------

#define S(mg, eg) make_score(mg, eg)

namespace Eval {

// ---- Material ----
constexpr Score PieceScore[PIECE_TYPE_NB] = {
    S(0, 0), S(82, 94), S(337, 281), S(365, 297), S(477, 512), S(1025, 936), S(0, 0)
};

// Game-phase weights: N=1, B=1, R=2, Q=4  ->  24 at the initial position
constexpr int PhaseValue[PIECE_TYPE_NB] = { 0, 0, 1, 1, 2, 4, 0 };
constexpr int PhaseMax = 24;

// ---- Piece-square tables (PeSTO), written rank 8 first ----
constexpr int mg_pawn[64] = {
      0,   0,   0,   0,   0,   0,   0,   0,
     98, 134,  61,  95,  68, 126,  34, -11,
     -6,   7,  26,  31,  65,  56,  25, -20,
    -14,  13,   6,  21,  23,  12,  17, -23,
    -27,  -2,  -5,  12,  17,   6,  10, -25,
    -26,  -4,  -4, -10,   3,   3,  33, -12,
    -35,  -1, -20, -23, -15,  24,  38, -22,
      0,   0,   0,   0,   0,   0,   0,   0,
};
constexpr int eg_pawn[64] = {
      0,   0,   0,   0,   0,   0,   0,   0,
    178, 173, 158, 134, 147, 132, 165, 187,
     94, 100,  85,  67,  56,  53,  82,  84,
     32,  24,  13,   5,  -2,   4,  17,  17,
     13,   9,  -3,  -7,  -7,  -8,   3,  -1,
      4,   7,  -6,   1,   0,  -5,  -1,  -8,
     13,   8,   8,  10,  13,   0,   2,  -7,
      0,   0,   0,   0,   0,   0,   0,   0,
};
constexpr int mg_knight[64] = {
   -167, -89, -34, -49,  61, -97, -15, -107,
    -73, -41,  72,  36,  23,  62,   7,  -17,
    -47,  60,  37,  65,  84, 129,  73,   44,
     -9,  17,  19,  53,  37,  69,  18,   22,
    -13,   4,  16,  13,  28,  19,  21,   -8,
    -23,  -9,  12,  10,  19,  17,  25,  -16,
    -29, -53, -12,  -3,  -1,  18, -14,  -19,
   -105, -21, -58, -33, -17, -28, -19,  -23,
};
constexpr int eg_knight[64] = {
    -58, -38, -13, -28, -31, -27, -63, -99,
    -25,  -8, -25,  -2,  -9, -25, -24, -52,
    -24, -20,  10,   9,  -1,  -9, -19, -41,
    -17,   3,  22,  22,  22,  11,   8, -18,
    -18,  -6,  16,  25,  16,  17,   4, -18,
    -23,  -3,  -1,  15,  10,  -3, -20, -22,
    -42, -20, -10,  -5,  -2, -20, -23, -44,
    -29, -51, -23, -15, -22, -18, -50, -64,
};
constexpr int mg_bishop[64] = {
    -29,   4, -82, -37, -25, -42,   7,  -8,
    -26,  16, -18, -13,  30,  59,  18, -47,
    -16,  37,  43,  40,  35,  50,  37,  -2,
     -4,   5,  19,  50,  37,  37,   7,  -2,
     -6,  13,  13,  26,  34,  12,  10,   4,
      0,  15,  15,  15,  14,  27,  18,  10,
      4,  15,  16,   0,   7,  21,  33,   1,
    -33,  -3, -14, -21, -13, -12, -39, -21,
};
constexpr int eg_bishop[64] = {
    -14, -21, -11,  -8,  -7,  -9, -17, -24,
     -8,  -4,   7, -12,  -3, -13,  -4, -14,
      2,  -8,   0,  -1,  -2,   6,   0,   4,
     -3,   9,  12,   9,  14,  10,   3,   2,
     -6,   3,  13,  19,   7,  10,  -3,  -9,
    -12,  -3,   8,  10,  13,   3,  -7, -15,
    -14, -18,  -7,  -1,   4,  -9, -15, -27,
    -23,  -9, -23,  -5,  -9, -16,  -5, -17,
};
constexpr int mg_rook[64] = {
     32,  42,  32,  51,  63,   9,  31,  43,
     27,  32,  58,  62,  80,  67,  26,  44,
     -5,  19,  26,  36,  17,  45,  61,  16,
    -24, -11,   7,  26,  24,  35,  -8, -20,
    -36, -26, -12,  -1,   9,  -7,   6, -23,
    -45, -25, -16, -17,   3,   0,  -5, -33,
    -44, -16, -20,  -9,  -1,  11,  -6, -71,
    -19, -13,   1,  17,  16,   7, -37, -26,
};
constexpr int eg_rook[64] = {
    13, 10, 18, 15, 12,  12,   8,   5,
    11, 13, 13, 11, -3,   3,   8,   3,
     7,  7,  7,  5,  4,  -3,  -5,  -3,
     4,  3, 13,  1,  2,   1,  -1,   2,
     3,  5,  8,  4, -5,  -6,  -8, -11,
    -4,  0, -5, -1, -7, -12,  -8, -16,
    -6, -6,  0,  2, -9,  -9, -11,  -3,
    -9,  2,  3, -1, -5, -13,   4, -20,
};
constexpr int mg_queen[64] = {
    -28,   0,  29,  12,  59,  44,  43,  45,
    -24, -39,  -5,   1, -16,  57,  28,  54,
    -13, -17,   7,   8,  29,  56,  47,  57,
    -27, -27, -16, -16,  -1,  17,  -2,   1,
     -9, -26,  -9, -10,  -2,  -4,   3,  -3,
    -14,   2, -11,  -2,  -5,   2,  14,   5,
    -35,  -8,  11,   2,   8,  15,  -3,   1,
     -1, -18,  -9,  10, -15, -25, -31, -50,
};
constexpr int eg_queen[64] = {
     -9,  22,  22,  27,  27,  19,  10,  20,
    -17,  20,  32,  41,  58,  25,  30,   0,
    -20,   6,   9,  49,  47,  35,  19,   9,
      3,  22,  24,  45,  57,  40,  57,  36,
    -18,  28,  19,  47,  31,  34,  39,  23,
    -16, -27,  15,   6,   9,  17,  10,   5,
    -22, -23, -30, -16, -16, -23, -36, -32,
    -33, -28, -22, -43,  -5, -32, -20, -41,
};
constexpr int mg_king[64] = {
    -65,  23,  16, -15, -56, -34,   2,  13,
     29,  -1, -20,  -7,  -8,  -4, -38, -29,
     -9,  24,   2, -16, -20,   6,  22, -22,
    -17, -20, -12, -27, -30, -25, -14, -36,
    -49,  -1, -27, -39, -46, -44, -33, -51,
    -14, -14, -22, -46, -44, -30, -15, -27,
      1,   7,  -8, -64, -43, -16,   9,   8,
    -15,  36,  12, -54,   8, -28,  24,  14,
};
constexpr int eg_king[64] = {
    -74, -35, -18, -18, -11,  15,   4, -17,
    -12,  17,  14,  17,  17,  38,  23,  11,
     10,  17,  23,  15,  20,  45,  44,  13,
     -8,  22,  24,  27,  26,  33,  26,   3,
    -18,  -4,  21,  24,  27,  23,   9, -11,
    -19,  -3,  11,  21,  23,  16,   7,  -9,
    -27, -11,   4,  13,  14,   4,  -5, -17,
    -53, -34, -21, -11, -28, -14, -24, -43,
};

extern Score PSQT[PIECE_NB][64];
extern Bitboard ForwardFileBB[COLOR_NB][64];
extern Bitboard PassedSpanBB[COLOR_NB][64];
extern Bitboard AttackSpanBB[COLOR_NB][64];
extern Bitboard AdjacentFilesBB[8];
extern Bitboard KingFlankBB[8];

void init();
void clear_pawn_table();
Value evaluate(const Position& pos);

// ---- Positional term values (pawn = 100 scale) ----
constexpr Score KnightMobility[9] = {
    S(-32,-32), S(-20,-21), S(-6,-9), S(0,0), S(6,5), S(11,10), S(15,13), S(18,15), S(21,17)
};
constexpr Score BishopMobility[14] = {
    S(-28,-30), S(-14,-16), S(-1,-5), S(6,3), S(12,10), S(17,16), S(21,20),
    S(24,23), S(26,26), S(28,29), S(30,31), S(32,33), S(34,35), S(35,37)
};
constexpr Score RookMobility[15] = {
    S(-24,-32), S(-14,-15), S(-7,-2), S(-3,8), S(-1,16), S(2,23), S(5,29), S(8,34),
    S(11,38), S(13,42), S(15,45), S(17,48), S(18,50), S(19,52), S(20,53)
};
constexpr Score QueenMobility[28] = {
    S(-16,-26), S(-11,-16), S(-7,-9), S(-3,-2), S(0,4), S(2,9), S(4,14), S(6,18),
    S(8,22), S(9,25), S(11,28), S(12,30), S(13,33), S(14,35), S(15,37), S(16,39),
    S(17,41), S(18,42), S(19,44), S(20,45), S(20,46), S(21,47), S(22,48), S(22,49),
    S(23,50), S(23,51), S(24,52), S(24,53)
};

constexpr Score PassedRank[8]    = { S(0,0), S(0,7), S(4,14), S(12,28), S(30,55), S(58,98), S(98,158), S(0,0) };
constexpr Score PassedFile       = S(-2,-3);
constexpr Score PassedBlocked    = S(-8,-22);
constexpr Score ConnectedRank[8] = { S(0,0), S(4,2), S(6,3), S(9,6), S(16,14), S(30,32), S(58,66), S(0,0) };

constexpr Score Isolated       = S(-7,-13);
constexpr Score Doubled        = S(-9,-22);
constexpr Score Backward       = S(-7,-11);
constexpr Score WeakUnopposed  = S(-6,-10);

constexpr Score BishopPair       = S(24,46);
constexpr Score RookOnOpenFile   = S(24,7);
constexpr Score RookOnSemiOpen   = S(10,5);
constexpr Score RookOnSeventh    = S(6,20);
constexpr Score KnightOutpost    = S(22,10);
constexpr Score BishopOutpost    = S(13,6);
constexpr Score ReachableOutpost = S(9,4);
constexpr Score BishopPawns      = S(-3,-6);
constexpr Score TrappedRook      = S(-28,-4);
constexpr Score MinorBehindPawn  = S(7,3);
constexpr Score LongDiagonalBishop = S(12,3);
constexpr Score KnightKingProximity = S(4,2);

constexpr Score ThreatByMinor[PIECE_TYPE_NB] = {
    S(0,0), S(6,18), S(38,35), S(40,38), S(56,45), S(52,42), S(0,0)
};
constexpr Score ThreatByRook[PIECE_TYPE_NB] = {
    S(0,0), S(4,22), S(28,32), S(30,34), S(0,12), S(42,30), S(0,0)
};
constexpr Score ThreatByPawn       = S(42,26);
constexpr Score ThreatByKing       = S(14,22);
constexpr Score HangingPiece       = S(28,14);
constexpr Score RestrictedPiece    = S(2,2);
constexpr Score ThreatBySafePawn   = S(48,32);
constexpr Score PawnPushThreat     = S(14,10);

constexpr int TempoValue = 18;

// King-safety attacker weights, indexed by piece type
constexpr int KingAttackWeight[PIECE_TYPE_NB] = { 0, 0, 34, 26, 34, 46, 0 };

} // namespace Eval

#undef S
