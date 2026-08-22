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
    S(0,0), S(98,142), S(497,377), S(509,401), S(677,880), S(1537,1456), S(0,0)
};

EVP_SCORE KnightMobility[9] = {
    S(-40,96), S(-28,83), S(-14,111), S(0,112), S(14,109), 
    S(27,122), S(31,117), S(42,103), S(53,73)
};
EVP_SCORE BishopMobility[14] = {
    S(-28,42), S(-6,88), S(15,83), S(22,91), S(28,122), 
    S(33,112), S(37,124), S(48,119), S(42,130), S(60,101), 
    S(78,95), S(64,65), S(74,83), S(-13,37)
};
EVP_SCORE RookMobility[15] = {
    S(-72,-16), S(-30,-15), S(-15,38), S(-11,48), S(-9,48), 
    S(-6,79), S(-3,101), S(8,98), S(19,102), S(29,114), 
    S(23,125), S(41,120), S(42,122), S(51,124), S(28,141)
};
EVP_SCORE QueenMobility[28] = {
    S(-80,374), S(-19,336), S(-23,231), S(-19,286), S(-8,164), 
    S(-6,225), S(-4,270), S(6,258), S(8,278), S(17,273), 
    S(19,284), S(28,270), S(29,281), S(22,299), S(31,293), 
    S(32,287), S(49,257), S(50,258), S(3,308), S(28,253), 
    S(4,262), S(93,175), S(134,160), S(54,89), S(159,-14), 
    S(15,35), S(0,148), S(-56,-91)
};
EVP_SCORE PassedRank[8] = {
    S(0,0), S(-16,47), S(-4,46), S(-12,36), S(22,23), S(66,50), S(98,-114), S(0,0)
};
EVP_SCORE ConnectedRank[8] = {
    S(0,0), S(4,2), S(14,3), S(17,6), S(40,14), S(118,32), S(146,66), S(0,0)
};
EVP_SCORE ThreatByMinor[7] = {
    S(0,0), S(6,34), S(14,43), S(48,30), S(64,37), S(44,74), S(0,0)
};
EVP_SCORE ThreatByRook[7] = {
    S(0,0), S(4,30), S(52,24), S(54,26), S(-8,20), S(82,-26), S(0,0)
};
EVP_SCORE PassedFile = S(-2,-3);
EVP_SCORE PassedBlocked = S(-24,66);
EVP_SCORE Isolated = S(1,-5);
EVP_SCORE Doubled = S(23,-6);
EVP_SCORE Backward = S(9,-11);
EVP_SCORE WeakUnopposed = S(-6,-26);
EVP_SCORE BishopPair = S(40,110);
EVP_SCORE RookOnOpenFile = S(48,-1);
EVP_SCORE RookOnSemiOpen = S(26,-35);
EVP_SCORE RookOnSeventh = S(-18,20);
EVP_SCORE KnightOutpost = S(38,26);
EVP_SCORE BishopOutpost = S(45,22);
EVP_SCORE ReachableOutpost = S(25,36);
EVP_SCORE BishopPawns = S(-3,-6);
EVP_SCORE TrappedRook = S(-52,-28);
EVP_SCORE MinorBehindPawn = S(7,19);
EVP_SCORE LongDiagonalBishop = S(20,27);
EVP_SCORE QueenPinned = S(-24,0);
EVP_SCORE KingFlankNoPawns = S(-46,-44);
EVP_SCORE ThreatByPawn = S(42,26);
EVP_SCORE ThreatByKing = S(-2,46);
EVP_SCORE HangingPiece = S(20,30);
EVP_SCORE RestrictedPiece = S(2,2);
EVP_SCORE ThreatBySafePawn = S(80,72);
EVP_SCORE PawnPushThreat = S(30,26);

EVP_INT TempoValue = 18;
EVP_INT KSAttacksWeight = 132;
EVP_INT KSWeakSquares   = 62;
EVP_INT KSRookCheck     = 222;
EVP_INT KSQueenCheck    = 186;
EVP_INT KSBishopCheck   = 196;
EVP_INT KSKnightCheck   = 206;
EVP_INT KSNoQueen       = 460;
EVP_INT KSShelterScale  = -4;
EVP_INT KSLinearMul     = 18;
EVP_INT ConnectedSupport = 27;
EVP_INT KingAttackWeight[7] = {
    0, 0, 34, 26, 34, 46, 0
};

EVP_INT mg_pawn[64] = {
    0, 0, 0, 0, 0, 0, 0, 0, 
    210, 78, 93, 207, 92, 94, -150, 333, 
    18, -17, 2, 55, 33, 56, 33, 44, 
    10, 5, 6, 21, 23, 28, 9, 1, 
    -11, -10, -5, 20, 25, -10, -14, -17, 
    -10, -12, -12, -18, 3, -13, 9, -12, 
    -11, 15, -12, 1, 17, -8, 30, -14, 
    0, 0, 0, 0, 0, 0, 0, 0
};
EVP_INT eg_pawn[64] = {
    0, 0, 0, 0, 0, 0, 0, 0, 
    130, 325, 294, 262, 235, 252, 349, -173, 
    6, 36, 29, 83, 64, 21, 58, -12, 
    16, 24, 21, 13, 14, -4, 17, 1, 
    21, 25, 13, 17, 9, 16, 3, 15, 
    12, 15, 26, 41, 24, 27, -9, 8, 
    21, 24, 32, 26, 37, 48, 10, 9, 
    0, 0, 0, 0, 0, 0, 0, 0
};
EVP_INT mg_knight[64] = {
    -103, -73, -162, -73, 109, -201, -231, -27, 
    -49, -137, 64, 108, 71, -2, 39, -33, 
    -95, 4, -11, 17, 28, 17, -7, 12, 
    15, 25, 19, 29, 29, 69, 50, 94, 
    3, -4, 32, 37, 60, 51, 69, 24, 
    -23, 7, 4, 10, 19, 17, 17, 8, 
    -5, -37, -12, 13, 15, 34, -30, 13, 
    -153, -21, -26, -33, 7, 4, -3, -159
};
EVP_INT eg_knight[64] = {
    -90, 34, 83, 60, -7, 125, 49, -43, 
    -25, 104, 39, 30, 31, 79, 16, 44, 
    72, 44, 74, 81, 95, 71, 61, 15, 
    7, 35, 70, 110, 102, 75, 40, 6, 
    54, 42, 64, 65, 80, 81, 20, 14, 
    -7, 21, 55, 47, 58, 61, 4, -6, 
    -10, 28, -2, 27, 38, -12, 17, -36, 
    -5, 13, 1, 41, 42, -2, -18, 8
};
EVP_INT mg_bishop[64] = {
    -61, -156, -202, -13, -113, -186, -65, 16, 
    -26, -40, -74, 59, -2, 3, -14, 17, 
    0, 5, -21, 48, -5, 10, 37, 6, 
    -4, -3, 35, 50, 45, 37, 31, 30, 
    10, 37, 29, 18, 50, 28, 34, 36, 
    16, 47, 15, 15, 22, 19, 42, 10, 
    36, 15, 16, 16, 15, 61, 9, -23, 
    -57, 13, 2, 11, 27, -20, -7, -45
};
EVP_INT eg_bishop[64] = {
    18, 91, 93, 24, 81, 71, 39, 24, 
    40, 68, 95, 52, 61, 75, 12, 58, 
    50, 48, 32, 39, 70, 30, 8, 68, 
    21, 73, 20, 65, 46, 10, 91, 18, 
    26, 35, 69, 83, 63, 82, 37, -17, 
    -44, 29, 32, 58, 53, 19, 49, 9, 
    -14, -10, -15, 39, 36, -1, 17, 21, 
    17, -121, 25, 35, 31, 24, 11, -57
};
EVP_INT mg_rook[64] = {
    -16, 26, 32, -29, 23, 57, -25, -37, 
    11, 0, 34, 70, 16, 35, -6, 12, 
    -13, 3, -46, 44, 1, 77, -59, 0, 
    -16, 29, 23, 10, 32, 51, 40, -20, 
    -52, -10, -44, -25, 1, 49, 62, 1, 
    -13, -1, -24, -9, -29, 0, -13, -73, 
    -28, -8, -12, 15, -9, 11, 2, -55, 
    -27, -21, -23, -15, 8, 15, -13, -2
};
EVP_INT eg_rook[64] = {
    53, 42, 34, 71, 44, 44, 64, 53, 
    27, 29, 13, 11, 37, 35, 48, 11, 
    55, 47, 71, 53, 60, 45, 91, 45, 
    60, 43, 45, 41, 42, 41, 55, 58, 
    59, 53, 48, 44, 43, 26, 0, 29, 
    20, 0, 35, 31, 1, 12, 24, 16, 
    -6, 10, 8, -14, -1, -17, -3, -27, 
    15, 2, 11, -9, -21, 3, 12, -12
};
EVP_INT mg_queen[64] = {
    -148, 80, 77, 132, 11, 148, -29, -67, 
    -40, -79, 3, -63, 0, 25, -28, -18, 
    -21, -9, -17, -24, -27, 48, -41, 41, 
    -35, -35, 8, -40, -41, -7, -10, 17, 
    -1, -10, -1, -10, -18, -4, 19, 29, 
    -22, 2, 13, -10, 3, 10, 30, 5, 
    -3, -24, 3, 10, 8, 31, 77, 49, 
    39, -2, 15, 10, 33, -1, 17, 6
};
EVP_INT eg_queen[64] = {
    175, -42, -2, -13, 27, -37, 82, 124, 
    55, 100, 80, 201, 138, 65, 102, 88, 
    140, 78, 73, 153, 223, 75, 203, 49, 
    115, 190, 64, 197, 193, 168, 177, 92, 
    6, 12, 131, 119, 167, 138, 103, 39, 
    32, 45, 23, 70, 41, 73, -38, 29, 
    10, 41, -30, -32, 8, -135, -244, -32, 
    -161, -100, -134, -27, -109, -72, -204, -25
};
EVP_INT mg_king[64] = {
    -241, -9, 144, -39, 168, -10, 98, -91, 
    -59, 31, 36, 81, 152, 76, 178, 243, 
    -33, 80, 18, -24, 4, 78, 238, 10, 
    47, 116, 36, -11, 10, -81, -14, -124, 
    -97, 63, -51, -47, -118, -60, 15, -123, 
    -22, 42, -86, -102, -92, -38, 1, -43, 
    -7, -49, -64, -64, -83, -24, 17, 40, 
    -327, 28, 20, -38, -16, -4, 64, 14
};
EVP_INT eg_king[64] = {
    -282, 21, -90, 22, 5, 135, 36, -409, 
    -124, 41, 14, 25, 9, 70, 103, -165, 
    -6, 9, 39, 79, 52, 45, -84, -59, 
    -40, -34, 56, 99, 74, 73, 34, 19, 
    -18, 4, 61, 80, 107, 71, 17, 45, 
    -27, -11, 59, 61, 71, 48, 7, -9, 
    -27, -3, 28, 37, 38, 20, -5, -41, 
    99, -58, -21, -11, -36, -38, -56, -51
};

} // namespace Eval

#undef S
