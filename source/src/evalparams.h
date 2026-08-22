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
    S(0,0), S(98,146), S(513,383), S(521,409), S(705,894), S(1573,1494), S(0,0)
};

EVP_SCORE KnightMobility[9] = {
    S(-30,92), S(-20,67), S(-6,97), S(6,114), S(18,119), 
    S(27,126), S(35,115), S(42,107), S(57,65)
};
EVP_SCORE BishopMobility[14] = {
    S(-20,44), S(-10,80), S(13,83), S(20,97), S(26,122), 
    S(35,118), S(39,128), S(46,127), S(48,126), S(60,109), 
    S(70,107), S(82,69), S(80,85), S(17,31)
};
EVP_SCORE RookMobility[15] = {
    S(-64,-28), S(-24,-3), S(-11,28), S(-9,56), S(-7,62), 
    S(-2,79), S(1,99), S(12,96), S(21,102), S(27,116), 
    S(29,123), S(43,124), S(38,130), S(45,136), S(42,127)
};
EVP_SCORE QueenMobility[28] = {
    S(-110,378), S(-19,306), S(-25,227), S(-17,268), S(-8,184), 
    S(-2,215), S(0,274), S(8,262), S(12,276), S(15,283), 
    S(21,282), S(24,282), S(29,285), S(26,301), S(27,299), 
    S(24,305), S(43,255), S(50,256), S(17,292), S(38,255), 
    S(32,254), S(57,169), S(104,166), S(42,89), S(159,14), 
    S(-21,15), S(-4,118), S(-82,-105)
};
EVP_SCORE PassedRank[8] = {
    S(0,0), S(-10,43), S(-6,42), S(-8,36), S(18,29), S(80,62), S(106,-104), S(0,0)
};
EVP_SCORE ConnectedRank[8] = {
    S(0,0), S(6,2), S(12,3), S(19,6), S(44,14), S(108,32), S(150,66), S(0,0)
};
EVP_SCORE ThreatByMinor[7] = {
    S(0,0), S(2,38), S(20,47), S(48,36), S(70,43), S(52,60), S(0,0)
};
EVP_SCORE ThreatByRook[7] = {
    S(0,0), S(6,30), S(44,32), S(58,20), S(0,14), S(88,-14), S(0,0)
};
EVP_SCORE PassedFile = S(-4,-1);
EVP_SCORE PassedBlocked = S(-10,62);
EVP_SCORE Isolated = S(-1,-9);
EVP_SCORE Doubled = S(15,-6);
EVP_SCORE Backward = S(5,-15);
EVP_SCORE WeakUnopposed = S(0,-28);
EVP_SCORE BishopPair = S(40,100);
EVP_SCORE RookOnOpenFile = S(52,-3);
EVP_SCORE RookOnSemiOpen = S(30,-35);
EVP_SCORE RookOnSeventh = S(-10,22);
EVP_SCORE KnightOutpost = S(40,24);
EVP_SCORE BishopOutpost = S(47,20);
EVP_SCORE ReachableOutpost = S(27,30);
EVP_SCORE BishopPawns = S(-1,-4);
EVP_SCORE TrappedRook = S(-52,-24);
EVP_SCORE MinorBehindPawn = S(7,21);
EVP_SCORE LongDiagonalBishop = S(26,27);
EVP_SCORE QueenPinned = S(-34,0);
EVP_SCORE KingFlankNoPawns = S(-40,-42);
EVP_SCORE ThreatByPawn = S(42,26);
EVP_SCORE ThreatByKing = S(-2,52);
EVP_SCORE HangingPiece = S(20,30);
EVP_SCORE RestrictedPiece = S(4,0);
EVP_SCORE ThreatBySafePawn = S(84,68);
EVP_SCORE PawnPushThreat = S(28,28);

EVP_INT TempoValue = 22;
EVP_INT KSAttacksWeight = 128;
EVP_INT KSWeakSquares   = 66;
EVP_INT KSRookCheck     = 258;
EVP_INT KSQueenCheck    = 174;
EVP_INT KSBishopCheck   = 224;
EVP_INT KSKnightCheck   = 240;
EVP_INT KSNoQueen       = 496;
EVP_INT KSShelterScale  = -2;
EVP_INT KSLinearMul     = 16;
EVP_INT ConnectedSupport = 27;
EVP_INT KingAttackWeight[7] = {
    0, 0, 34, 26, 34, 46, 0
};

EVP_INT mg_pawn[64] = {
    0, 0, 0, 0, 0, 0, 0, 0, 
    238, 46, 103, 197, 58, 86, -112, 357, 
    28, -1, 0, 35, 21, 78, 37, 44, 
    20, 7, 8, 25, 27, 36, 11, 3, 
    -5, -10, 3, 20, 25, -2, -14, -17, 
    -2, -10, -6, -14, 5, -23, 9, -10, 
    -7, 11, -10, -7, 9, -6, 26, -14, 
    0, 0, 0, 0, 0, 0, 0, 0
};
EVP_INT eg_pawn[64] = {
    0, 0, 0, 0, 0, 0, 0, 0, 
    128, 323, 298, 270, 247, 280, 349, -155, 
    16, 46, 47, 73, 54, 19, 40, -12, 
    20, 28, 23, 11, 14, 4, 21, 3, 
    25, 27, 11, 11, 11, 8, 11, 15, 
    14, 15, 24, 33, 30, 27, 1, 4, 
    27, 22, 38, 36, 33, 42, 12, 11, 
    0, 0, 0, 0, 0, 0, 0, 0
};
EVP_INT mg_knight[64] = {
    -141, -107, -180, -57, 103, -213, -245, -65, 
    -33, -103, 26, 98, 67, 14, 21, -15, 
    -65, -4, 5, 47, 60, 31, 11, 16, 
    25, 37, 35, 55, 49, 87, 48, 66, 
    7, -4, 40, 43, 60, 57, 71, 32, 
    -15, 1, 8, 20, 37, 21, 25, 6, 
    -25, -15, 4, 11, 15, 18, -14, -7, 
    -119, -21, -16, -19, -1, 8, -11, -125
};
EVP_INT eg_knight[64] = {
    -96, 36, 71, 38, 11, 119, 55, -77, 
    -13, 82, 47, 44, 43, 59, 30, 26, 
    76, 38, 84, 91, 99, 67, 65, 21, 
    19, 37, 72, 100, 96, 79, 46, 0, 
    34, 52, 76, 71, 90, 69, 32, 32, 
    1, 39, 61, 53, 68, 61, 12, 6, 
    -2, 26, 6, 41, 34, 8, 31, 2, 
    -43, 5, 15, 29, 32, 0, -8, -2
};
EVP_INT mg_bishop[64] = {
    -35, -130, -186, -43, -75, -194, -103, -2, 
    -30, -20, -46, 29, -14, -15, -24, -3, 
    20, -5, -3, 46, 27, 36, 37, 36, 
    2, 13, 35, 50, 55, 33, 21, 24, 
    -4, 37, 31, 28, 50, 30, 28, 20, 
    18, 35, 3, 21, 24, 17, 36, 10, 
    14, 13, 26, 14, 21, 45, 17, -21, 
    -35, 15, 6, -5, 1, -10, -5, -17
};
EVP_INT eg_bishop[64] = {
    -14, 101, 83, 42, 65, 99, 45, 12, 
    50, 62, 85, 44, 69, 67, 34, 48, 
    42, 54, 38, 49, 58, 40, 20, 48, 
    21, 57, 38, 67, 62, 20, 79, 20, 
    10, 43, 65, 87, 59, 88, 51, 11, 
    -14, 21, 40, 62, 65, 21, 43, 15, 
    14, -4, -7, 37, 42, 9, 17, 19, 
    -3, -83, 23, 25, 15, 28, -11, -51
};
EVP_INT mg_rook[64] = {
    -8, 6, 30, -25, 19, 47, -37, -23, 
    9, -6, 48, 62, 30, 55, 0, 22, 
    -5, 19, -24, 40, 35, 45, -21, 18, 
    -12, 27, 39, 26, 38, 61, 16, -2, 
    -44, -24, -26, -21, -7, 35, 30, -7, 
    -41, -3, -42, -15, -31, -20, -19, -57, 
    -28, 4, -6, -7, 1, 7, -8, -67, 
    -17, -11, -13, -9, 6, 15, -17, -4
};
EVP_INT eg_rook[64] = {
    59, 60, 38, 77, 50, 54, 86, 73, 
    25, 33, 11, 13, 27, 25, 60, 15, 
    59, 47, 73, 49, 56, 47, 85, 41, 
    68, 55, 49, 43, 48, 43, 67, 52, 
    65, 65, 50, 52, 51, 26, 24, 33, 
    24, 8, 49, 25, 3, 22, 24, 12, 
    -6, -2, 10, 8, -9, -5, 11, -5, 
    15, 2, 11, -1, -15, -3, 14, -12
};
EVP_INT mg_queen[64] = {
    -146, 78, 65, 100, 43, 142, -63, -69, 
    -16, -69, -9, -53, 6, -7, -10, 0, 
    -9, -11, -29, 12, -49, 50, -33, 25, 
    -27, -29, 22, -34, -25, -5, 0, 25, 
    9, -2, -7, -10, -12, 0, 27, 23, 
    -4, 6, 11, -12, 3, 8, 36, -3, 
    -23, -20, 11, 10, 12, 39, 55, 25, 
    41, 4, 21, 14, 15, 1, 21, 10
};
EVP_INT eg_queen[64] = {
    175, -12, -10, 23, 59, -37, 96, 108, 
    61, 122, 106, 205, 134, 85, 122, 88, 
    108, 100, 103, 161, 207, 107, 173, 53, 
    119, 154, 80, 209, 195, 156, 171, 98, 
    20, 20, 127, 131, 169, 128, 97, 63, 
    34, 25, 33, 70, 61, 75, -30, 27, 
    10, 25, -24, -24, 8, -135, -222, -46, 
    -147, -70, -130, -27, -85, -70, -202, -25
};
EVP_INT mg_king[64] = {
    -275, 13, 134, -11, 132, -48, 60, -81, 
    -27, 35, 24, 73, 122, 38, 140, 219, 
    -33, 104, 22, -8, 12, 56, 240, 12, 
    55, 108, 42, -7, -6, -67, 6, -112, 
    -69, 35, -31, -41, -118, -58, 7, -99, 
    -8, 26, -64, -92, -78, -42, -5, -47, 
    13, -55, -46, -78, -83, -20, 19, 36, 
    -325, 32, 16, -58, -16, -10, 68, 22
};
EVP_INT eg_king[64] = {
    -320, 31, -90, 6, -25, 97, -2, -379, 
    -100, 53, 38, 35, 21, 32, 65, -173, 
    2, -11, 57, 67, 80, 43, -74, -49, 
    -18, -12, 56, 103, 82, 71, 26, 9, 
    0, 8, 65, 80, 109, 71, 17, 31, 
    -11, -3, 55, 61, 69, 50, 17, -13, 
    -13, 11, 18, 29, 36, 22, -3, -37, 
    123, -44, -17, -5, -32, -26, -52, -55
};

} // namespace Eval

#undef S
