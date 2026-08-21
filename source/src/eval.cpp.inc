#include "eval.h"

#define S(mg, eg) make_score(mg, eg)

namespace Eval {

Score    PSQT[PIECE_NB][64];
Bitboard ForwardFileBB[COLOR_NB][64];
Bitboard PassedSpanBB[COLOR_NB][64];
Bitboard AttackSpanBB[COLOR_NB][64];
Bitboard AdjacentFilesBB[8];
Bitboard KingFlankBB[8];

void init() {
    for (int f = 0; f < 8; ++f) {
        AdjacentFilesBB[f] = (f > 0 ? file_bb(f - 1) : 0) | (f < 7 ? file_bb(f + 1) : 0);
        int lo = std::max(f - 2, 0), hi = std::min(f + 2, 7);
        if (f <= 2) { lo = 0; hi = 2; }
        else if (f >= 5) { lo = 5; hi = 7; }
        else { lo = 3; hi = 4; }
        Bitboard flank = 0;
        for (int i = lo; i <= hi; ++i) flank |= file_bb(i);
        KingFlankBB[f] = flank;
    }

    for (int s = 0; s < 64; ++s) {
        Bitboard fwdW = 0, fwdB = 0;
        for (int r = rank_of(s) + 1; r <= 7; ++r) fwdW |= square_bb(make_square(file_of(s), r));
        for (int r = rank_of(s) - 1; r >= 0; --r) fwdB |= square_bb(make_square(file_of(s), r));
        ForwardFileBB[WHITE][s] = fwdW;
        ForwardFileBB[BLACK][s] = fwdB;

        Bitboard adjW = 0, adjB = 0;
        Bitboard adjFiles = AdjacentFilesBB[file_of(s)];
        for (int r = rank_of(s) + 1; r <= 7; ++r) adjW |= adjFiles & rank_bb(r);
        for (int r = rank_of(s) - 1; r >= 0; --r) adjB |= adjFiles & rank_bb(r);
        AttackSpanBB[WHITE][s] = adjW;
        AttackSpanBB[BLACK][s] = adjB;

        PassedSpanBB[WHITE][s] = fwdW | adjW;
        PassedSpanBB[BLACK][s] = fwdB | adjB;
    }

    for (int pt = PAWN; pt <= KING; ++pt) {
        const int* mg;
        const int* eg;
        switch (pt) {
            case PAWN:   mg = mg_pawn;   eg = eg_pawn;   break;
            case KNIGHT: mg = mg_knight; eg = eg_knight; break;
            case BISHOP: mg = mg_bishop; eg = eg_bishop; break;
            case ROOK:   mg = mg_rook;   eg = eg_rook;   break;
            case QUEEN:  mg = mg_queen;  eg = eg_queen;  break;
            default:     mg = mg_king;   eg = eg_king;   break;
        }
        for (int s = 0; s < 64; ++s) {
            Score w = PieceScore[pt] + make_score(mg[s ^ 56], eg[s ^ 56]);
            Score b = PieceScore[pt] + make_score(mg[s], eg[s]);
            PSQT[make_piece(WHITE, PieceType(pt))][s] = w;
            PSQT[make_piece(BLACK, PieceType(pt))][s] = -b;
        }
    }
}

// ---------------------------------------------------------------------------
// Pawn hash: the parts of the pawn evaluation that depend only on pawns.
// ---------------------------------------------------------------------------
struct PawnEntry {
    U64      key;
    Score    score;
    Bitboard passedPawns[COLOR_NB];
    Bitboard pawnAttacks[COLOR_NB];
    Bitboard pawnAttacksSpan[COLOR_NB];
    int      kingSafety[COLOR_NB];
    int      kingSq[COLOR_NB];
    int      castlingRights[COLOR_NB];
};

constexpr int PawnTableSize = 1 << 15;  // 32k entries, ~1.5 MB
static PawnEntry PawnTable[PawnTableSize];

void clear_pawn_table() { std::memset(PawnTable, 0, sizeof(PawnTable)); }

template<Color Us>
static Score evaluate_pawns_for(const Position& pos, PawnEntry& e) {
    constexpr Color Them = ~Us;
    constexpr Direction Up = (Us == WHITE ? NORTH : SOUTH);

    Bitboard ourPawns   = pos.pieces(Us, PAWN);
    Bitboard theirPawns = pos.pieces(Them, PAWN);
    Bitboard neighbours, stoppers, support, phalanx, opposed, lever, leverPush, blocked;
    Score score = SCORE_ZERO;

    e.passedPawns[Us] = 0;
    e.pawnAttacks[Us] = pawn_attacks_bb(Us, ourPawns);
    e.pawnAttacksSpan[Us] = 0;

    Bitboard b = ourPawns;
    while (b) {
        int s = pop_lsb(b);
        int r = relative_rank(Us, s);

        e.pawnAttacksSpan[Us] |= AttackSpanBB[Us][s];

        opposed    = theirPawns & ForwardFileBB[Us][s];
        blocked    = theirPawns & square_bb(s + Up);
        stoppers   = theirPawns & PassedSpanBB[Us][s];
        lever      = theirPawns & BB::PawnAttacks[Us][s];
        leverPush  = theirPawns & BB::PawnAttacks[Us][s + Up];
        neighbours = ourPawns & AdjacentFilesBB[file_of(s)];
        phalanx    = neighbours & rank_bb_of(s);
        support    = neighbours & rank_bb(rank_of(s) - (Us == WHITE ? 1 : -1));

        // Passed: no enemy pawn ahead on this or an adjacent file, except levers we
        // can safely push past.
        bool passed = !(stoppers ^ lever)
                   || (!(stoppers ^ leverPush) && popcount(phalanx) >= popcount(leverPush));
        // A pawn that is doubled behind a friendly pawn is not passed.
        if (passed && (ourPawns & ForwardFileBB[Us][s])) passed = false;
        if (passed) e.passedPawns[Us] |= square_bb(s);

        if (support | phalanx) {
            int v = mg_value(ConnectedRank[r]) * (2 + (phalanx ? 1 : 0) - (opposed ? 1 : 0))
                  + ConnectedSupport * popcount(support);
            score += make_score(v / 2, v * (r - 2) / 8);
        } else if (!neighbours) {
            score += Isolated;
            if (!opposed) score += WeakUnopposed;
        } else if (blocked && r <= 4 && !(ourPawns & AttackSpanBB[Them][s])) {
            score += Backward;
            if (!opposed) score += WeakUnopposed;
        }

        if (ourPawns & ForwardFileBB[Us][s]) score += Doubled;
    }
    return score;
}

// Pawn shelter and storm in front of the king, from the king's point of view.
template<Color Us>
static int pawn_shelter(const Position& pos, int ksq) {
    constexpr Color Them = ~Us;
    Bitboard ourPawns   = pos.pieces(Us, PAWN);
    Bitboard theirPawns = pos.pieces(Them, PAWN);

    int centre = std::clamp(file_of(ksq), 1, 6);
    int safety = 5;

    for (int f = centre - 1; f <= centre + 1; ++f) {
        Bitboard fileMask = file_bb(f);
        Bitboard ours   = ourPawns & fileMask & ~ForwardFileBB[Them][ksq];
        Bitboard theirs = theirPawns & fileMask;

        int ourRank   = ours ? relative_rank(Us, (Us == WHITE ? lsb(ours) : msb(ours))) : 0;
        int theirRank = theirs ? relative_rank(Us, (Us == WHITE ? lsb(theirs) : msb(theirs))) : 0;

        bool edge = (f == 0 || f == 7);
        // Our own pawn close in front of the king is good.
        static const int ShelterBonus[8]  = { -6, 32, 26, 12, 4, 0, 0, 0 };
        // An enemy pawn bearing down on the king is bad; worse the closer it gets,
        // and worse still when it is not blocked by one of ours.
        static const int StormPenalty[8]  = { 0, 0, -8, -22, -34, -48, -60, 0 };

        safety += ShelterBonus[ourRank] / (edge ? 2 : 1);
        if (theirRank && theirRank != ourRank + 1)
            safety += StormPenalty[theirRank] / (edge ? 2 : 1);
        else if (theirRank)
            safety += StormPenalty[theirRank] / 3;   // blocked storm is much less dangerous
    }
    return safety;
}

template<Color Us>
static int king_safety_value(const Position& pos) {
    int ksq = pos.king_square(Us);
    int best = pawn_shelter<Us>(pos, ksq);

    // Assume the king will reach safety if it can still castle.
    if (pos.can_castle(Us == WHITE ? WHITE_OO : BLACK_OO))
        best = std::max(best, pawn_shelter<Us>(pos, relative_square(Us, SQ_G1)));
    if (pos.can_castle(Us == WHITE ? WHITE_OOO : BLACK_OOO))
        best = std::max(best, pawn_shelter<Us>(pos, relative_square(Us, SQ_C1)));

    return best;
}

static PawnEntry* probe_pawns(const Position& pos) {
#ifdef TUNE
    // The tuner changes evaluation parameters between passes and runs the error
    // function on several threads, so the shared cache would be both stale and
    // racy. Recompute into thread-local storage instead.
    static thread_local PawnEntry tls;
    PawnEntry* e = &tls;
    e->key = pos.pawn_key();
    e->score = evaluate_pawns_for<WHITE>(pos, *e) - evaluate_pawns_for<BLACK>(pos, *e);
    e->kingSafety[WHITE] = king_safety_value<WHITE>(pos);
    e->kingSafety[BLACK] = king_safety_value<BLACK>(pos);
    return e;
#else
    U64 key = pos.pawn_key();
    PawnEntry* e = &PawnTable[key & (PawnTableSize - 1)];

    if (e->key != key) {
        e->key = key;
        e->score = evaluate_pawns_for<WHITE>(pos, *e) - evaluate_pawns_for<BLACK>(pos, *e);
        e->kingSq[WHITE] = e->kingSq[BLACK] = SQ_NONE;
    }

    // King safety depends on the king square and castling rights as well as the pawns,
    // so it is cached separately inside the same entry.
    for (int c = 0; c < 2; ++c) {
        Color col = Color(c);
        int ksq = pos.king_square(col);
        int cr = pos.castling_rights() & (col == WHITE ? (WHITE_OO | WHITE_OOO) : (BLACK_OO | BLACK_OOO));
        if (e->kingSq[c] != ksq || e->castlingRights[c] != cr) {
            e->kingSq[c] = ksq;
            e->castlingRights[c] = cr;
            e->kingSafety[c] = (col == WHITE) ? king_safety_value<WHITE>(pos) : king_safety_value<BLACK>(pos);
        }
    }
    return e;
#endif
}

// ---------------------------------------------------------------------------
// Main evaluation
// ---------------------------------------------------------------------------
class Evaluation {
public:
    explicit Evaluation(const Position& p) : pos(p) {}
    Value value();

private:
    template<Color Us> void initialize();
    template<Color Us, PieceType Pt> Score pieces();
    template<Color Us> Score king() const;
    template<Color Us> Score threats() const;
    template<Color Us> Score passed() const;
    template<Color Us> int   scale_factor(Value eg) const;

    const Position& pos;
    PawnEntry* pe = nullptr;

    Bitboard mobilityArea[COLOR_NB];
    Score    mobility[COLOR_NB] = { SCORE_ZERO, SCORE_ZERO };
    Bitboard attackedBy[COLOR_NB][PIECE_TYPE_NB];
    Bitboard attackedBy2[COLOR_NB];
    Bitboard kingRing[COLOR_NB];
    int      kingAttackersCount[COLOR_NB];
    int      kingAttackersWeight[COLOR_NB];
    int      kingAttacksCount[COLOR_NB];
};

template<Color Us>
void Evaluation::initialize() {
    constexpr Color Them = ~Us;
    constexpr Direction Up = (Us == WHITE ? NORTH : SOUTH);
    constexpr Direction Down = (Us == WHITE ? SOUTH : NORTH);
    constexpr Bitboard LowRanks = (Us == WHITE ? Rank2BB | Rank3BB : Rank7BB | Rank6BB);

    const int ksq = pos.king_square(Us);

    // Squares our own pieces should not count as mobility: occupied by our blocked
    // pawns or pawns on low ranks, our king and queen, and squares attacked by enemy pawns.
    Bitboard b = pos.pieces(Us, PAWN) & (shift_up(pos.pieces(), Them) | LowRanks);
    mobilityArea[Us] = ~(b | pos.pieces(Us, KING, QUEEN) | pe->pawnAttacks[Them]);

    attackedBy[Us][PAWN] = pe->pawnAttacks[Us];
    attackedBy[Us][KING] = BB::PseudoAttacks[KING][ksq];
    attackedBy[Us][ALL_PIECES] = attackedBy[Us][KING] | attackedBy[Us][PAWN];
    attackedBy2[Us] = attackedBy[Us][KING] & attackedBy[Us][PAWN];

    // King ring: the king's neighbourhood, shifted inwards at the board edge and
    // extended by one rank in front of the king.
    kingRing[Us] = BB::PseudoAttacks[KING][ksq] | square_bb(ksq);
    if (rank_of(ksq) == (Us == WHITE ? 0 : 7)) kingRing[Us] |= shift_up(kingRing[Us], Us);
    if (file_of(ksq) == 0) kingRing[Us] |= shift<EAST>(kingRing[Us]);
    else if (file_of(ksq) == 7) kingRing[Us] |= shift<WEST>(kingRing[Us]);

    kingAttackersCount[Them] = popcount(kingRing[Us] & pe->pawnAttacks[Them]);
    kingAttackersWeight[Them] = 0;
    kingAttacksCount[Them] = 0;
    (void)Up; (void)Down;
}

template<Color Us, PieceType Pt>
Score Evaluation::pieces() {
    constexpr Color Them = ~Us;
    constexpr Direction Down = (Us == WHITE ? SOUTH : NORTH);
    constexpr Bitboard OutpostRanks = (Us == WHITE ? Rank4BB | Rank5BB | Rank6BB
                                                   : Rank5BB | Rank4BB | Rank3BB);
    Score score = SCORE_ZERO;
    attackedBy[Us][Pt] = 0;

    Bitboard bb = pos.pieces(Us, Pt);
    while (bb) {
        int s = pop_lsb(bb);

        // X-ray through our own queens/bishops (batteries) for a truer mobility count
        Bitboard occ = pos.pieces();
        if (Pt == BISHOP) occ ^= pos.pieces(Us, QUEEN);
        else if (Pt == ROOK) occ ^= pos.pieces(Us, QUEEN, ROOK) ^ square_bb(s);
        else if (Pt == QUEEN) occ ^= pos.pieces(Us, QUEEN) ^ square_bb(s);

        Bitboard att = BB::attacks_bb<Pt>(s, occ);
        // A pinned piece may only move along the pin ray
        if (pos.blockers_for_king(Us) & square_bb(s))
            att &= line_bb(pos.king_square(Us), s);

        attackedBy2[Us] |= attackedBy[Us][ALL_PIECES] & att;
        attackedBy[Us][Pt] |= att;
        attackedBy[Us][ALL_PIECES] |= att;

        if (att & kingRing[Them]) {
            kingAttackersCount[Us]++;
            kingAttackersWeight[Us] += KingAttackWeight[Pt];
            kingAttacksCount[Us] += popcount(att & BB::PseudoAttacks[KING][pos.king_square(Them)]);
        }

        int mob = popcount(att & mobilityArea[Us]);
        if constexpr (Pt == KNIGHT) mobility[Us] += KnightMobility[mob];
        else if constexpr (Pt == BISHOP) mobility[Us] += BishopMobility[mob];
        else if constexpr (Pt == ROOK) mobility[Us] += RookMobility[mob];
        else mobility[Us] += QueenMobility[mob];

        if constexpr (Pt == KNIGHT || Pt == BISHOP) {
            // Outposts: protected by one of our pawns and out of reach of enemy pawns
            Bitboard outpostSquares = OutpostRanks & ~pe->pawnAttacksSpan[Them];
            if (outpostSquares & square_bb(s) & attackedBy[Us][PAWN])
                score += (Pt == KNIGHT ? KnightOutpost : BishopOutpost);
            else if (Pt == KNIGHT && (att & outpostSquares & ~pos.pieces(Us) & attackedBy[Us][PAWN]))
                score += ReachableOutpost;

            // A minor directly behind one of our pawns is well sheltered
            if (shift_up(pos.pieces(PAWN), Them) & square_bb(s))
                score += MinorBehindPawn;

            if constexpr (Pt == BISHOP) {
                // Penalise a bishop hemmed in by our own pawns on its colour
                Bitboard sameColourPawns = pos.pieces(Us, PAWN)
                                         & ((DarkSquares & square_bb(s)) ? DarkSquares : ~DarkSquares);
                Bitboard blockedCentral = pos.pieces(Us, PAWN) & (FileDBB | FileEBB)
                                        & shift_up(pos.pieces(), Them);
                score += BishopPawns * popcount(sameColourPawns)
                       * (1 + popcount(blockedCentral));

                // A bishop on a long diagonal that sees the centre
                Bitboard centre = square_bb(SQ_D4) | square_bb(SQ_E4) | square_bb(SQ_D5) | square_bb(SQ_E5);
                if (more_than_one(BB::bishop_attacks(s, pos.pieces(PAWN)) & centre))
                    score += LongDiagonalBishop;
            }
        }

        if constexpr (Pt == ROOK) {
            Bitboard ourPawnsOnFile   = pos.pieces(Us, PAWN) & file_bb_of(s);
            Bitboard theirPawnsOnFile = pos.pieces(Them, PAWN) & file_bb_of(s);
            if (!ourPawnsOnFile)
                score += theirPawnsOnFile ? RookOnSemiOpen : RookOnOpenFile;
            if (relative_rank(Us, s) == 6) score += RookOnSeventh;

            if (mob <= 3 && !(!ourPawnsOnFile)) {
                // Rook trapped by its own king on the wrong side, castling gone
                int ksq = pos.king_square(Us);
                if (((file_of(ksq) < 4) == (file_of(s) < file_of(ksq)))
                    && relative_rank(Us, ksq) == 0
                    && !pos.can_castle(Us == WHITE ? (WHITE_OO | WHITE_OOO) : (BLACK_OO | BLACK_OOO)))
                    score += TrappedRook;
            }
        }

        if constexpr (Pt == QUEEN) {
            // A queen that is loose against a discovered attack
            Bitboard pinners = 0;
            Bitboard snipers = ((BB::PseudoAttacks[ROOK][s] & pos.pieces(Them, ROOK, QUEEN))
                              | (BB::PseudoAttacks[BISHOP][s] & pos.pieces(Them, BISHOP, QUEEN)));
            Bitboard occAll = pos.pieces() ^ snipers;
            while (snipers) {
                int sn = pop_lsb(snipers);
                Bitboard btw = between_bb(s, sn) & occAll;
                if (btw && !more_than_one(btw)) pinners |= square_bb(sn);
            }
            if (pinners) score += QueenPinned;
        }
        (void)Down;
    }
    return score;
}

template<Color Us>
Score Evaluation::king() const {
    constexpr Color Them = ~Us;
    const int ksq = pos.king_square(Us);
    Score score = SCORE_ZERO;

    // Pawn shelter / storm (cached)
    int shelter = pe->kingSafety[Us];
    score += make_score(shelter, 0);

    // Squares around the king that the enemy attacks and we do not defend well
    Bitboard weak = attackedBy[Them][ALL_PIECES]
                  & ~attackedBy2[Us]
                  & (~attackedBy[Us][ALL_PIECES] | attackedBy[Us][QUEEN] | attackedBy[Us][KING]);

    // Squares from which an enemy piece could give check without being captured
    Bitboard b1 = BB::rook_attacks(ksq, pos.pieces() ^ pos.pieces(Us, QUEEN));
    Bitboard b2 = BB::bishop_attacks(ksq, pos.pieces() ^ pos.pieces(Us, QUEEN));

    Bitboard safe = ~pos.pieces(Them)
                  & (~attackedBy[Us][ALL_PIECES] | (weak & attackedBy2[Them]));

    Bitboard rookChecks   = b1 & attackedBy[Them][ROOK] & safe;
    Bitboard queenChecks  = (b1 | b2) & attackedBy[Them][QUEEN] & safe
                          & ~attackedBy[Us][QUEEN] & ~rookChecks;
    Bitboard bishopChecks = b2 & attackedBy[Them][BISHOP] & safe & ~queenChecks;
    Bitboard knightChecks = BB::PseudoAttacks[KNIGHT][ksq] & attackedBy[Them][KNIGHT] & safe;

    int kingDanger = kingAttackersCount[Them] * kingAttackersWeight[Them] / std::max(1, KSAttackerScale)
                   + KSAttacksWeight * kingAttacksCount[Them]
                   + KSWeakSquares * popcount(kingRing[Us] & weak)
                   + KSRookCheck * popcount(rookChecks)
                   + KSQueenCheck * popcount(queenChecks)
                   + KSBishopCheck * popcount(bishopChecks)
                   + KSKnightCheck * popcount(knightChecks)
                   - KSNoQueen * (pos.count(Them, QUEEN) == 0)
                   - KSShelterScale * shelter
                   - 6;

    if (kingDanger > 0) {
        // Quadratic in the middlegame, linear in the endgame
        score -= make_score(kingDanger * kingDanger / std::max(1, KSQuadDiv), kingDanger / std::max(1, KSLinearDiv));
    }

    // Penalty for a king on a flank with no pawns at all
    Bitboard flank = KingFlankBB[file_of(ksq)];
    if (!(pos.pieces(PAWN) & flank)) score += KingFlankNoPawns;

    return score;
}

template<Color Us>
Score Evaluation::threats() const {
    constexpr Color Them = ~Us;
    constexpr Direction Up = (Us == WHITE ? NORTH : SOUTH);
    constexpr Bitboard TRank3BB = (Us == WHITE ? Rank3BB : Rank6BB);
    Score score = SCORE_ZERO;

    Bitboard nonPawnEnemies = pos.pieces(Them) & ~pos.pieces(Them, PAWN);
    Bitboard stronglyProtected = attackedBy[Them][PAWN]
                               | (attackedBy2[Them] & ~attackedBy2[Us]);
    Bitboard defended = nonPawnEnemies & stronglyProtected;
    Bitboard weak = pos.pieces(Them) & ~stronglyProtected & attackedBy[Us][ALL_PIECES];

    if (defended | weak) {
        Bitboard b = (defended | weak) & (attackedBy[Us][KNIGHT] | attackedBy[Us][BISHOP]);
        while (b) score += ThreatByMinor[type_of(pos.piece_on(pop_lsb(b)))];

        b = weak & attackedBy[Us][ROOK];
        while (b) score += ThreatByRook[type_of(pos.piece_on(pop_lsb(b)))];

        if (weak & attackedBy[Us][KING]) score += ThreatByKing;

        b = weak & ~attackedBy[Them][ALL_PIECES];
        score += HangingPiece * popcount(b);
    }

    // Restricting enemy piece moves
    Bitboard b = attackedBy[Them][ALL_PIECES] & ~stronglyProtected & attackedBy[Us][ALL_PIECES];
    score += RestrictedPiece * popcount(b);

    // Pawns attacking pieces, from squares the pawn is safe on
    Bitboard safe = ~attackedBy[Them][ALL_PIECES] | attackedBy[Us][ALL_PIECES];
    b = pawn_attacks_bb(Us, pos.pieces(Us, PAWN) & safe) & nonPawnEnemies;
    score += ThreatBySafePawn * popcount(b);

    // Pawn pushes that would attack an enemy piece
    b = shift_up(pos.pieces(Us, PAWN), Us) & ~pos.pieces();
    b |= shift_up(b & TRank3BB, Us) & ~pos.pieces();
    b &= ~attackedBy[Them][PAWN] & (attackedBy[Us][ALL_PIECES] | ~attackedBy[Them][ALL_PIECES]);
    b = pawn_attacks_bb(Us, b) & nonPawnEnemies & ~attackedBy[Us][PAWN];
    score += PawnPushThreat * popcount(b);

    (void)Up;
    return score;
}

template<Color Us>
Score Evaluation::passed() const {
    constexpr Color Them = ~Us;
    constexpr Direction Up = (Us == WHITE ? NORTH : SOUTH);
    Score score = SCORE_ZERO;

    Bitboard b = pe->passedPawns[Us];
    while (b) {
        int s = pop_lsb(b);
        int r = relative_rank(Us, s);

        Score bonus = PassedRank[r];

        if (r > 2) {
            int blockSq = s + Up;
            int w = 5 * r - 13;

            // In the endgame, how close each king is to the queening path matters a lot
            int egBonus = (square_distance(pos.king_square(Them), blockSq) * 19 / 4
                         - square_distance(pos.king_square(Us), blockSq) * 2) * w;
            if (r != 6)
                egBonus -= square_distance(pos.king_square(Us), blockSq + Up) * w;
            bonus += make_score(0, egBonus);

            // Is the path to promotion clear and defended?
            if (pos.empty(blockSq)) {
                Bitboard squaresToQueen = ForwardFileBB[Us][s];
                Bitboard unsafeSquares = PassedSpanBB[Us][s];
                Bitboard behind = ForwardFileBB[Them][s] & pos.pieces(ROOK, QUEEN);
                if (!(behind & pos.pieces(Them))) unsafeSquares &= attackedBy[Them][ALL_PIECES] | pos.pieces(Them);

                int k = !(unsafeSquares & squaresToQueen) ? 36
                      : !(unsafeSquares & square_bb(blockSq)) ? 20 : 0;
                if (behind & pos.pieces(Us)) k += 5;
                bonus += make_score(k * w, k * w);
            } else if (pos.pieces(Us) & square_bb(blockSq)) {
                bonus += PassedBlocked;
            }
        }

        score += bonus + PassedFile * std::min(file_of(s), 7 - file_of(s));
    }
    return score;
}

// Scale down clearly drawish endgames (opposite-coloured bishops, few pawns, ...)
template<Color Us>
int Evaluation::scale_factor(Value eg) const {
    constexpr Color Them = ~Us;
    Color strong = (eg > 0) ? WHITE : BLACK;
    Color weakSide = ~strong;
    int sf = 64;

    int strongPawns = pos.count(strong, PAWN);

    bool oppositeBishops = pos.count(WHITE, BISHOP) == 1 && pos.count(BLACK, BISHOP) == 1
                        && ((pos.pieces(WHITE, BISHOP) & DarkSquares) != 0)
                           != ((pos.pieces(BLACK, BISHOP) & DarkSquares) != 0);

    if (oppositeBishops) {
        if (pos.non_pawn_material(WHITE) == PieceValue[BISHOP]
            && pos.non_pawn_material(BLACK) == PieceValue[BISHOP])
            sf = 22 + 4 * popcount(pos.pieces(strong, PAWN));
        else
            sf = 22 + 3 * pos.count(strong, PAWN) + 22;
        sf = std::min(sf, 64);
    } else if (strongPawns == 0) {
        // No pawns for the stronger side: very hard to win without a big material edge
        int diff = pos.non_pawn_material(strong) - pos.non_pawn_material(weakSide);
        if (diff <= PieceValue[BISHOP]) sf = 8;
        else if (diff <= PieceValue[ROOK]) sf = 22;
        else sf = 40;
    } else if (strongPawns <= 2) {
        sf = 40 + 10 * strongPawns;
    }

    (void)Them;
    return std::min(sf, 64);
}

Value Evaluation::value() {
    pe = probe_pawns(pos);

    Score score = pe->score;

    // Material + piece-square tables
    Bitboard occ = pos.pieces();
    int phase = 0;
    while (occ) {
        int s = pop_lsb(occ);
        Piece pc = pos.piece_on(s);
        score += PSQT[pc][s];
        phase += PhaseValue[type_of(pc)];
    }

    // Bishop pair
    if (pos.count(WHITE, BISHOP) >= 2) score += BishopPair;
    if (pos.count(BLACK, BISHOP) >= 2) score -= BishopPair;

    initialize<WHITE>();
    initialize<BLACK>();

    score += pieces<WHITE, KNIGHT>() - pieces<BLACK, KNIGHT>();
    score += pieces<WHITE, BISHOP>() - pieces<BLACK, BISHOP>();
    score += pieces<WHITE, ROOK>()   - pieces<BLACK, ROOK>();
    score += pieces<WHITE, QUEEN>()  - pieces<BLACK, QUEEN>();

    score += mobility[WHITE] - mobility[BLACK];
    score += king<WHITE>()    - king<BLACK>();
    score += threats<WHITE>() - threats<BLACK>();
    score += passed<WHITE>()  - passed<BLACK>();

    Value mg = mg_value(score);
    Value eg = eg_value(score);

    int sf = (pos.side_to_move() == WHITE) ? scale_factor<WHITE>(eg) : scale_factor<BLACK>(eg);
    eg = Value(eg * sf / 64);

    phase = std::min(phase, PhaseMax);
    Value v = (mg * phase + eg * (PhaseMax - phase)) / PhaseMax;

    // Damp the score as the fifty-move counter runs up
    v = Value(v * (200 - pos.rule50_count()) / 200);

    return (pos.side_to_move() == WHITE ? v : -v) + TempoValue;
}

Value evaluate(const Position& pos) {
    return Evaluation(pos).value();
}

} // namespace Eval

#undef S
