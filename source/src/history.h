#pragma once
#include "types.h"

// Move-ordering statistics.  All tables use the same "gravity" update rule: the
// entry moves towards +/-D by an amount proportional to how far it currently is
// from the limit, so frequently-confirmed entries saturate instead of overflowing.

constexpr int MAIN_HIST_MAX = 7183;
constexpr int CONT_HIST_MAX = 29952;
constexpr int CAPT_HIST_MAX = 10692;
constexpr int PAWN_HIST_MAX = 8192;

// Correction history: a running estimate of how wrong the static evaluation tends
// to be for a given pawn structure, learned from how searches actually resolve.
// Entries hold (average error in cp) * 32.
constexpr int CORR_HIST_SIZE  = 16384;
constexpr int CORR_HIST_GRAIN = 32;
constexpr int CORR_HIST_LIMIT = 8192;   // +/- 256 cp

inline void corr_update(int16_t& e, int diff, int depth) {
    int w = std::min(depth, 15) + 1;                       // deeper searches carry more weight
    int target = std::clamp(diff, -256, 256) * CORR_HIST_GRAIN;
    int v = (int(e) * (32 - w) + target * w) / 32;
    e = int16_t(std::clamp(v, -CORR_HIST_LIMIT, CORR_HIST_LIMIT));
}
inline int corr_value(int16_t e) { return int(e) / CORR_HIST_GRAIN; }

inline void hist_update(int16_t& entry, int bonus, int D) {
    bonus = std::clamp(bonus, -D, D);
    entry += int16_t(bonus - entry * std::abs(bonus) / D);
}

// [piece][to]  — one plane of the continuation history
struct PieceToHistory {
    int16_t v[PIECE_NB][64];
    int16_t* operator[](int pc) { return v[pc]; }
    const int16_t* operator[](int pc) const { return v[pc]; }
    void fill(int16_t x) {
        for (int p = 0; p < PIECE_NB; ++p)
            for (int s = 0; s < 64; ++s) v[p][s] = x;
    }
};

struct Histories {
    // [color][from * 64 + to]
    int16_t main[COLOR_NB][64 * 64];
    // [piece][to][captured piece type]
    int16_t capture[PIECE_NB][64][PIECE_TYPE_NB];
    // [pawn structure bucket][piece][to]
    int16_t pawnHist[512][PIECE_NB][64];
    // [in check][is capture][piece][to] -> PieceToHistory
    PieceToHistory continuation[2][2][PIECE_NB][64];
    // [piece][to]
    Move counterMove[PIECE_NB][64];
    // [color][pawn structure bucket]
    int16_t corr[COLOR_NB][CORR_HIST_SIZE];

    void clear() {
        std::memset(main, 0, sizeof(main));
        std::memset(capture, 0, sizeof(capture));
        std::memset(pawnHist, 0, sizeof(pawnHist));
        std::memset(counterMove, 0, sizeof(counterMove));
        std::memset(corr, 0, sizeof(corr));
        for (int a = 0; a < 2; ++a)
            for (int b = 0; b < 2; ++b)
                for (int p = 0; p < PIECE_NB; ++p)
                    for (int s = 0; s < 64; ++s)
                        continuation[a][b][p][s].fill(-60);
    }
};

inline int pawn_hist_index(U64 pawnKey) { return int(pawnKey & 511); }
inline int corr_hist_index(U64 pawnKey) { return int(pawnKey & (CORR_HIST_SIZE - 1)); }
inline int from_to(Move m) { return (from_sq(m) << 6) | to_sq(m); }
