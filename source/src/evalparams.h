#pragma once
#include "types.h"

// ---------------------------------------------------------------------------
// Every tunable evaluation parameter lives here and nowhere else, so the tuner
// can regenerate this whole file and applying a tuning run is a file copy.
//
// The material values and piece-square tables started as PeSTO's (published on
// the Chess Programming Wiki, which the benchmark rules explicitly permit);
// everything else began as my own estimates on the pawn=100 scale. All of it is
// then fitted to self-play results by source/src/tuner_main.cpp.
// ---------------------------------------------------------------------------

// constexpr in the release build, mutable only in the tuner build, so tuning
// costs the shipped engine nothing.
#ifdef TUNE
  #define EVP_SCORE inline Score
  #define EVP_INT   inline int
#else
  #define EVP_SCORE constexpr Score
  #define EVP_INT   constexpr int
#endif

#define S(mg, eg) make_score(mg, eg)

namespace Eval {

EVP_SCORE PieceScore[PIECE_TYPE_NB] = {
    S(0,0), S(90,134), S(473,361), S(493,385), S(637,808), S(1401,1360), S(0,0)
};

EVP_SCORE KnightMobility[9] = {
    S(-32,80), S(-20,27), S(-6,63), S(0,88), S(14,101), 
    S(19,106), S(31,93), S(34,95), S(53,49)
};
EVP_SCORE BishopMobility[14] = {
    S(-20,50), S(-14,72), S(7,75), S(14,91), S(20,106), 
    S(33,104), S(37,108), S(40,111), S(42,106), S(52,85), 
    S(70,87), S(88,49), S(138,51), S(67,13)
};
EVP_SCORE RookMobility[15] = {
    S(-48,-8), S(-22,25), S(-7,30), S(-3,56), S(-1,56), 
    S(2,71), S(5,85), S(16,82), S(27,86), S(29,106), 
    S(31,109), S(33,120), S(18,130), S(27,132), S(52,117)
};
EVP_SCORE QueenMobility[28] = {
    S(56,230), S(-19,240), S(-31,183), S(-11,214), S(0,156), 
    S(-6,217), S(4,222), S(6,234), S(8,222), S(17,217), 
    S(19,228), S(20,230), S(21,249), S(22,243), S(31,237), 
    S(24,239), S(33,225), S(34,218), S(43,204), S(60,197), 
    S(68,198), S(69,119), S(54,160), S(46,81), S(55,90), 
    S(-1,43), S(-128,52), S(-8,-203)
};
EVP_SCORE PassedRank[8] = {
    S(0,0), S(24,39), S(12,30), S(-4,36), S(-2,47), S(66,58), S(82,-98), S(0,0)
};
EVP_SCORE ConnectedRank[8] = {
    S(0,0), S(4,2), S(14,3), S(17,6), S(40,14), S(86,32), S(122,66), S(0,0)
};
EVP_SCORE ThreatByMinor[7] = {
    S(0,0), S(-2,34), S(22,27), S(40,38), S(64,37), S(60,-22), S(0,0)
};
EVP_SCORE ThreatByRook[7] = {
    S(0,0), S(4,30), S(36,32), S(62,18), S(0,28), S(66,-2), S(0,0)
};
EVP_SCORE PassedFile = S(-2,-3);
EVP_SCORE PassedBlocked = S(8,42);
EVP_SCORE Isolated = S(1,-13);
EVP_SCORE Doubled = S(7,2);
EVP_SCORE Backward = S(9,-11);
EVP_SCORE WeakUnopposed = S(2,-26);
EVP_SCORE BishopPair = S(32,78);
EVP_SCORE RookOnOpenFile = S(48,-1);
EVP_SCORE RookOnSemiOpen = S(26,-27);
EVP_SCORE RookOnSeventh = S(-26,12);
EVP_SCORE KnightOutpost = S(30,26);
EVP_SCORE BishopOutpost = S(45,14);
EVP_SCORE ReachableOutpost = S(25,28);
EVP_SCORE BishopPawns = S(-3,-6);
EVP_SCORE TrappedRook = S(-44,12);
EVP_SCORE MinorBehindPawn = S(7,11);
EVP_SCORE LongDiagonalBishop = S(20,19);
EVP_SCORE QueenPinned = S(-32,-24);
EVP_SCORE KingFlankNoPawns = S(-62,-28);
EVP_SCORE ThreatByPawn = S(42,26);
EVP_SCORE ThreatByKing = S(-34,46);
EVP_SCORE HangingPiece = S(12,38);
EVP_SCORE RestrictedPiece = S(2,2);
EVP_SCORE ThreatBySafePawn = S(80,48);
EVP_SCORE PawnPushThreat = S(22,26);

EVP_INT TempoValue = 18;
EVP_INT KSAttackerScale = 8;
EVP_INT KSAttacksWeight = 44;
EVP_INT KSWeakSquares   = 14;
EVP_INT KSRookCheck     = 262;
EVP_INT KSQueenCheck    = 74;
EVP_INT KSBishopCheck   = 172;
EVP_INT KSKnightCheck   = 142;
EVP_INT KSNoQueen       = 868;
EVP_INT KSShelterScale  = -4;
EVP_INT KSQuadDiv       = 3600;
EVP_INT KSLinearDiv     = -2;
EVP_INT ConnectedSupport = 19;
EVP_INT KingAttackWeight[7] = {
    0, 0, 346, 314, 210, -82, 0
};

EVP_INT mg_pawn[64] = {
    0, 0, 0, 0, 0, 0, 0, 0, 
    298, 46, 101, 111, -28, -26, -30, 213, 
    50, -1, 42, 23, -7, 40, -7, 36, 
    18, 5, 14, 29, 23, 28, 9, -15, 
    -19, -10, -5, 20, 17, 6, -6, -17, 
    -10, -12, -4, -10, 3, -29, 17, -12, 
    -11, 7, -12, -7, 9, 0, 30, -14, 
    0, 0, 0, 0, 0, 0, 0, 0
};
EVP_INT eg_pawn[64] = {
    0, 0, 0, 0, 0, 0, 0, 0, 
    90, 277, 254, 294, 307, 300, 293, -77, 
    -2, 44, 53, 91, 80, 37, 58, -20, 
    16, 16, 13, 5, 22, 4, 9, 9, 
    21, 25, 13, 9, 9, 0, 3, 7, 
    12, 15, 18, 33, 32, 19, -1, 0, 
    21, 16, 40, 26, 21, 32, 2, 1, 
    0, 0, 0, 0, 0, 0, 0, 0
};
EVP_INT mg_knight[64] = {
    -231, -177, -250, -97, -35, -249, -319, -131, 
    -41, -49, -8, -36, -9, 46, -73, -25, 
    9, -28, -11, 49, 60, 33, 1, 4, 
    -9, 17, 43, 45, 29, 77, 42, 46, 
    3, 20, 32, 37, 36, 35, 45, 16, 
    -15, -17, 4, 10, 51, 17, 33, -8, 
    -69, -5, 12, -3, 7, 2, -6, -19, 
    -9, -21, -10, -17, -1, 4, -19, -63
};
EVP_INT eg_knight[64] = {
    -26, 66, 51, 36, 17, 69, 33, -75, 
    -25, 0, 31, 54, 31, 7, 40, 4, 
    24, 20, 42, 41, 55, 39, 37, 39, 
    15, 35, 38, 62, 70, 43, 24, -26, 
    -2, 10, 48, 41, 64, 49, 20, 54, 
    1, 21, 39, 23, 42, 29, -12, 26, 
    46, 12, -18, 11, -2, 4, 33, 12, 
    -117, -27, 33, 1, 2, -10, 6, 16
};
EVP_INT mg_bishop[64] = {
    -69, -28, -186, -37, -73, -234, -209, -48, 
    14, 0, -34, -53, -2, -45, -6, -7, 
    24, -19, -13, 0, 3, 90, -11, 46, 
    -4, -3, 11, 18, 45, 37, 7, -2, 
    -30, 13, 21, 18, 18, 28, 2, 4, 
    8, 7, -1, 15, 30, 19, 26, -6, 
    12, 15, 24, 8, 15, 45, 9, -7, 
    -17, -11, 10, -45, 19, -12, 81, 3
};
EVP_INT eg_bishop[64] = {
    -54, 43, 53, -8, 17, 87, 47, 64, 
    8, 44, 47, 20, 45, 51, 36, 2, 
    18, 40, 40, 47, 46, 22, 32, 4, 
    13, 33, 60, 33, 54, 34, 59, 18, 
    10, 27, 45, 75, 39, 90, 53, 23, 
    12, 5, 48, 42, 53, 11, -7, -7, 
    10, -10, -7, 23, 28, -1, 1, -19, 
    -39, -17, 1, 27, -1, 0, -85, -97
};
EVP_INT mg_rook[64] = {
    -16, -38, 16, 51, 39, -7, -73, 3, 
    3, 16, 34, 86, 56, 91, -38, 28, 
    -21, 11, 10, 28, 105, -11, 77, 32, 
    -32, 29, 15, 50, 32, 19, -32, -20, 
    -36, -10, -12, 7, -7, -15, 14, -7, 
    -45, -17, -48, -9, -37, -40, -21, -41, 
    -20, 16, -4, 15, 15, 3, -22, -63, 
    -11, -5, -7, -7, 0, 7, -29, -10
};
EVP_INT eg_rook[64] = {
    45, 50, 34, 23, 28, 60, 88, 53, 
    27, 29, 21, 19, 13, 3, 72, 27, 
    47, 39, 39, 37, 12, 45, 35, 21, 
    60, 35, 45, 9, 34, 49, 79, 58, 
    51, 37, 24, 20, 43, 34, 8, 13, 
    20, 0, 43, -17, 1, 28, 0, -16, 
    -22, -14, -8, -14, -25, -9, 13, 13, 
    -1, -14, 3, -1, -13, -5, 4, -12
};
EVP_INT mg_queen[64] = {
    -140, 40, -43, 148, 147, 60, -77, -83, 
    -48, -71, -13, -15, 48, -31, 12, 30, 
    35, -25, -1, 80, -19, 64, -17, 17, 
    -19, -27, 8, -24, 15, 9, -10, 25, 
    7, 14, -9, -10, -18, 4, 11, 5, 
    26, 2, -3, -18, -5, -6, 14, -19, 
    -11, -8, 11, 10, 16, 39, 21, 17, 
    23, -26, 15, 10, -7, -25, -23, 62
};
EVP_INT eg_queen[64] = {
    191, -18, 30, -61, -29, -5, 98, 132, 
    79, 52, 88, 145, 50, 153, 166, 8, 
    4, 134, 49, 17, 151, 131, 219, 113, 
    51, 110, 72, 133, 113, 80, 241, 140, 
    6, -60, 59, 119, 143, 82, 47, 127, 
    -104, -19, -1, 38, 81, 49, -6, -11, 
    -22, 1, -78, -56, -16, -167, -204, -168, 
    -25, -4, -110, -51, -21, -72, -116, -105
};
EVP_INT mg_king[64] = {
    -105, 103, 8, 41, 72, -146, -38, 45, 
    77, -41, -100, -55, 16, -60, 74, 123, 
    -57, 80, 18, -64, -92, -10, 150, 74, 
    23, 68, -20, -11, -86, -9, 26, -20, 
    -41, -25, -19, -79, -70, -68, 15, -19, 
    -38, -38, -46, -70, -92, -70, -31, -91, 
    9, -57, -48, -80, -91, -24, 9, 24, 
    -191, 28, 12, -78, -16, -28, 64, 22
};
EVP_INT eg_king[64] = {
    -378, 21, -58, -2, -67, -1, -100, -345, 
    -84, 41, 70, 41, 17, -10, -33, -141, 
    -14, -23, 55, 95, 108, 29, -28, -3, 
    -8, -2, 56, 83, 74, 41, 10, -13, 
    -18, -4, 37, 72, 75, 55, 17, 5, 
    -35, 5, 27, 37, 55, 40, 15, 7, 
    -51, 5, 4, 13, 30, 12, -5, -25, 
    59, -42, -21, 5, -12, -14, -56, -59
};

} // namespace Eval

#undef S
