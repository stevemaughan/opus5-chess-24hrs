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
#include "evalparams.h"

namespace Eval {

constexpr int PhaseValue[PIECE_TYPE_NB] = { 0, 0, 1, 1, 2, 4, 0 };
constexpr int PhaseMax = 24;


extern Score PSQT[PIECE_NB][64];
extern Bitboard ForwardFileBB[COLOR_NB][64];
extern Bitboard PassedSpanBB[COLOR_NB][64];
extern Bitboard AttackSpanBB[COLOR_NB][64];
extern Bitboard AdjacentFilesBB[8];
extern Bitboard KingFlankBB[8];

void init();
void clear_pawn_table();
Value evaluate(const Position& pos);
} // namespace Eval
