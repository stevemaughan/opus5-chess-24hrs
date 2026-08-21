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
    S(0,0), S(82,142), S(401,385), S(421,409), S(541,784), S(1297,1280), S(0,0)
};

EVP_SCORE KnightMobility[9] = {
    S(-16,56), S(-4,19), S(2,71), S(8,80), S(14,85), 
    S(19,98), S(23,93), S(26,95), S(29,73)
};
EVP_SCORE BishopMobility[14] = {
    S(-20,42), S(-14,56), S(7,67), S(14,83), S(12,106), 
    S(25,104), S(29,100), S(32,103), S(34,98), S(44,93), 
    S(62,71), S(64,65), S(114,59), S(83,37)
};
EVP_SCORE RookMobility[15] = {
    S(-112,72), S(-14,9), S(-7,38), S(-11,72), S(-1,64), 
    S(2,79), S(5,85), S(16,82), S(19,94), S(29,98), 
    S(23,109), S(25,120), S(18,122), S(11,132), S(28,141)
};
EVP_SCORE QueenMobility[28] = {
    S(-24,390), S(-11,104), S(-23,159), S(-19,142), S(-16,156), 
    S(-6,169), S(-4,182), S(6,194), S(8,198), S(17,201), 
    S(19,220), S(20,230), S(21,233), S(22,227), S(39,205), 
    S(40,191), S(33,225), S(42,178), S(75,164), S(28,213), 
    S(84,166), S(53,159), S(-34,208), S(78,1), S(23,154), 
    S(-137,123), S(-16,236), S(-120,-115)
};
EVP_SCORE PassedRank[8] = {
    S(0,0), S(24,23), S(-4,38), S(-20,44), S(-10,63), S(42,90), S(98,-66), S(0,0)
};
EVP_SCORE ConnectedRank[8] = {
    S(0,0), S(4,2), S(6,3), S(17,6), S(32,14), S(38,32), S(106,66), S(0,0)
};
EVP_SCORE ThreatByMinor[7] = {
    S(0,0), S(-2,26), S(22,27), S(48,30), S(48,53), S(44,42), S(0,0)
};
EVP_SCORE ThreatByRook[7] = {
    S(0,0), S(-4,30), S(20,32), S(30,34), S(0,12), S(66,62), S(0,0)
};
EVP_SCORE PassedFile = S(-2,-3);
EVP_SCORE PassedBlocked = S(40,34);
EVP_SCORE Isolated = S(-7,-5);
EVP_SCORE Doubled = S(-1,-30);
EVP_SCORE Backward = S(1,-11);
EVP_SCORE WeakUnopposed = S(2,-26);
EVP_SCORE BishopPair = S(32,94);
EVP_SCORE RookOnOpenFile = S(40,7);
EVP_SCORE RookOnSemiOpen = S(18,-11);
EVP_SCORE RookOnSeventh = S(-26,20);
EVP_SCORE KnightOutpost = S(38,26);
EVP_SCORE BishopOutpost = S(37,-2);
EVP_SCORE ReachableOutpost = S(17,20);
EVP_SCORE BishopPawns = S(-3,-6);
EVP_SCORE TrappedRook = S(-12,-12);
EVP_SCORE MinorBehindPawn = S(7,11);
EVP_SCORE LongDiagonalBishop = S(12,19);
EVP_SCORE QueenPinned = S(-24,-48);
EVP_SCORE KingFlankNoPawns = S(-62,-28);
EVP_SCORE ThreatByPawn = S(42,26);
EVP_SCORE ThreatByKing = S(-26,46);
EVP_SCORE HangingPiece = S(20,30);
EVP_SCORE RestrictedPiece = S(2,2);
EVP_SCORE ThreatBySafePawn = S(72,64);
EVP_SCORE PawnPushThreat = S(22,18);

EVP_INT TempoValue = 18;
EVP_INT KSAttackerScale = 8;
EVP_INT KSAttacksWeight = 44;
EVP_INT KSWeakSquares   = -10;
EVP_INT KSRookCheck     = 238;
EVP_INT KSQueenCheck    = 74;
EVP_INT KSBishopCheck   = 188;
EVP_INT KSKnightCheck   = 150;
EVP_INT KSNoQueen       = 764;
EVP_INT KSShelterScale  = -4;
EVP_INT KSQuadDiv       = 3592;
EVP_INT KSLinearDiv     = -2;
EVP_INT ConnectedSupport = 19;
EVP_INT KingAttackWeight[7] = {
    0, 0, 218, 258, 98, 6, 0
};

EVP_INT mg_pawn[64] = {
    0, 0, 0, 0, 0, 0, 0, 0, 
    226, 6, 13, 63, 108, 14, -30, 93, 
    2, -17, 10, 23, 17, 16, 25, -12, 
    2, 5, 30, 37, 23, 4, 17, -15, 
    -19, -2, 3, 12, 17, 14, 2, -25, 
    -2, -20, -4, -10, 3, -13, 25, -12, 
    -11, 7, -4, -23, 1, 0, 38, -6, 
    0, 0, 0, 0, 0, 0, 0, 0
};
EVP_INT eg_pawn[64] = {
    0, 0, 0, 0, 0, 0, 0, 0, 
    114, 245, 302, 254, 203, 220, 317, 35, 
    46, 68, 45, 67, 48, 45, 58, 44, 
    32, 24, -3, 5, 14, 20, 1, 25, 
    29, 17, -3, -7, 9, 0, 3, 15, 
    4, 7, 2, 1, 16, 19, -9, 0, 
    29, 16, 32, 42, 29, 24, -6, -7, 
    0, 0, 0, 0, 0, 0, 0, 0
};
EVP_INT mg_knight[64] = {
    -231, -193, -66, -81, -83, -241, -135, -99, 
    -65, -1, -8, 4, -1, -34, -25, -9, 
    -47, -4, -3, 33, 76, -15, -7, -52, 
    -1, 9, 59, 37, 29, 61, 2, 22, 
    -5, 4, 32, 29, 28, 35, 45, 8, 
    -23, -1, 4, 10, 19, 17, 9, -24, 
    -37, -61, -4, 5, -1, 10, 2, 5, 
    -57, -21, -34, 15, 39, -20, -19, -23
};
EVP_INT eg_knight[64] = {
    14, 58, 35, 52, 65, 85, 25, -147, 
    -49, 32, -17, 6, 23, -49, 24, -28, 
    -16, 28, 34, 49, 7, 23, 5, 23, 
    47, 27, 22, 54, 38, 27, 16, -10, 
    -18, 18, 32, 25, 48, 17, 28, -10, 
    1, 13, 7, 7, 42, 5, 4, -22, 
    62, 4, 22, 3, 30, 12, 1, -12, 
    -53, -35, 33, 17, -14, -18, -18, -72
};
EVP_INT mg_bishop[64] = {
    -37, 68, -114, -157, -1, -186, -105, -56, 
    -26, -64, -66, 83, 30, -45, -78, -47, 
    -16, 13, -13, -16, -37, -22, -3, 14, 
    -4, 13, 19, 26, 29, 37, 15, 22, 
    -22, 13, 21, 26, 10, 12, 26, -4, 
    16, -9, 7, 23, 22, 3, 42, -6, 
    36, 7, 0, 0, 7, 53, 9, 17, 
    31, 13, 2, 11, 35, -12, -103, 19
};
EVP_INT eg_bishop[64] = {
    -30, 19, 53, 32, 25, 39, 31, 0, 
    -16, 12, 71, -44, -3, 35, 52, 18, 
    -46, -8, 32, 71, 46, 78, 48, 28, 
    5, 25, 52, 33, 62, 10, 27, 2, 
    10, 11, 21, 59, 47, 42, 37, -1, 
    -4, 21, 16, 2, 53, 11, -15, 9, 
    34, -26, 9, 31, 12, -41, -31, -11, 
    -47, -89, 1, 11, -17, -8, 43, -73
};
EVP_INT mg_rook[64] = {
    24, -30, 80, 59, -25, 73, -73, -29, 
    19, 32, 34, 70, 40, 43, 18, 36, 
    -29, 75, 34, 12, 57, 85, 77, 0, 
    -40, 29, 39, 42, 0, 59, -8, -36, 
    -12, 6, -28, -25, 9, -23, 14, -15, 
    -29, -1, -40, -33, -21, 8, -5, -49, 
    -28, 0, 12, -25, 15, 3, -14, -55, 
    -19, -5, 1, -7, -8, -17, -37, -26
};
EVP_INT eg_rook[64] = {
    45, 42, 10, 23, 36, 44, 72, 53, 
    19, 29, 21, 19, 21, 11, 32, 27, 
    63, 31, 31, 61, 20, 29, 27, 45, 
    52, 27, 13, 9, 66, 17, 39, 26, 
    35, 37, 32, 28, 19, 50, 16, 5, 
    20, -8, 19, -1, -15, -28, 8, -16, 
    2, -38, -40, -14, -33, -9, 13, -27, 
    -17, -30, -13, -1, -5, 3, 12, -20
};
EVP_INT mg_queen[64] = {
    -28, 40, 13, 124, 59, 84, -5, -27, 
    24, -47, 3, 1, 16, 1, -20, 30, 
    19, -1, 7, 8, 21, 80, -25, 1, 
    -35, -11, -32, -8, 39, 41, -10, 41, 
    -1, -10, -1, -2, -18, -4, -5, 5, 
    18, 10, -11, -2, -21, 2, 14, 13, 
    -27, 16, 11, 10, 16, 23, 21, -31, 
    -17, -42, -1, 10, -7, -25, -39, -42
};
EVP_INT eg_queen[64] = {
    111, -10, 46, -53, -13, 35, 18, 116, 
    -57, 100, 64, 89, 2, 73, 62, -40, 
    12, 126, 81, 89, 87, 59, 147, 105, 
    91, 110, 120, 53, 49, 40, 129, 20, 
    -34, 20, 19, 79, 87, 90, 127, -1, 
    -32, -19, 7, -2, 65, 33, 58, -11, 
    -70, -87, -54, -64, -16, -119, -76, -8, 
    -1, 44, -46, -67, -93, -72, 68, -33
};
EVP_INT mg_king[64] = {
    79, 167, 160, 121, 8, -10, 146, -131, 
    173, 55, 52, 129, 48, -92, 106, 83, 
    127, 64, -38, 80, 76, -74, -34, 74, 
    -33, 84, -36, 21, -14, -9, -46, -52, 
    -97, 31, -83, -95, -38, -100, -1, -59, 
    -86, -94, -54, -78, -100, -86, -55, -107, 
    -55, -33, -40, -56, -67, -32, 33, 16, 
    -127, 20, 4, -62, -24, -36, 56, -2
};
EVP_INT eg_king[64] = {
    -194, -11, -10, -2, 29, 23, 84, -161, 
    -100, 17, 46, -7, -31, 78, -9, -29, 
    -54, 17, 63, 7, 20, 29, 36, -43, 
    0, -2, 48, 43, 50, 25, 10, -5, 
    -2, 4, 45, 56, 51, 47, 1, 5, 
    -11, 13, 19, 29, 47, 32, 7, 15, 
    -19, 5, 12, -3, 22, 4, -29, -25, 
    59, -26, -21, -19, -20, -22, -56, -35
};

} // namespace Eval

#undef S
