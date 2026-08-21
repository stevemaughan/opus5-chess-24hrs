#pragma once
#include "types.h"

// ---------------------------------------------------------------------------
// Attack tables.  Sliding attacks use BMI2 PEXT (the target CPU is guaranteed
// to have fast PEXT), with a plain magic-multiply fallback compiled in when
// USE_PEXT is not defined so the engine still builds/runs on other hardware.
// ---------------------------------------------------------------------------

#if defined(__BMI2__)
#define USE_PEXT 1
#endif

namespace BB {

extern uint8_t SquareDistance[64][64];
extern Bitboard BetweenBB[64][64];   // squares strictly between s1 and s2 (empty if not aligned)
extern Bitboard LineBB[64][64];      // full line through s1,s2 (0 if not aligned)
extern Bitboard PseudoAttacks[PIECE_TYPE_NB][64];
extern Bitboard PawnAttacks[COLOR_NB][64];

struct Magic {
    Bitboard  mask;
    Bitboard* attacks;
#ifndef USE_PEXT
    Bitboard  magic;
    unsigned  shift;
#endif
    inline unsigned index(Bitboard occ) const {
#ifdef USE_PEXT
        return unsigned(_pext_u64(occ, mask));
#else
        return unsigned(((occ & mask) * magic) >> shift);
#endif
    }
};

extern Magic RookMagics[64];
extern Magic BishopMagics[64];

void init();

inline Bitboard rook_attacks(int s, Bitboard occ) {
    return RookMagics[s].attacks[RookMagics[s].index(occ)];
}
inline Bitboard bishop_attacks(int s, Bitboard occ) {
    return BishopMagics[s].attacks[BishopMagics[s].index(occ)];
}

template<PieceType Pt>
inline Bitboard attacks_bb(int s, Bitboard occ) {
    if constexpr (Pt == ROOK)   return rook_attacks(s, occ);
    if constexpr (Pt == BISHOP) return bishop_attacks(s, occ);
    if constexpr (Pt == QUEEN)  return rook_attacks(s, occ) | bishop_attacks(s, occ);
    return PseudoAttacks[Pt][s];
}

inline Bitboard attacks_bb(PieceType pt, int s, Bitboard occ) {
    switch (pt) {
        case BISHOP: return bishop_attacks(s, occ);
        case ROOK:   return rook_attacks(s, occ);
        case QUEEN:  return rook_attacks(s, occ) | bishop_attacks(s, occ);
        default:     return PseudoAttacks[pt][s];
    }
}

inline bool aligned(int s1, int s2, int s3) { return LineBB[s1][s2] & square_bb(s3); }

} // namespace BB

inline int square_distance(int a, int b) { return BB::SquareDistance[a][b]; }
inline Bitboard between_bb(int a, int b) { return BB::BetweenBB[a][b]; }
inline Bitboard line_bb(int a, int b) { return BB::LineBB[a][b]; }
