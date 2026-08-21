#include "position.h"
#include <sstream>

namespace Zobrist {
U64 psq[PIECE_NB][64];
U64 enpassant[8];
U64 castling[16];
U64 side;

static U64 s_state = 1070372ULL;
static U64 rand64() {
    s_state ^= s_state >> 12; s_state ^= s_state << 25; s_state ^= s_state >> 27;
    return s_state * 2685821657736338717ULL;
}

void init() {
    for (int pc = 0; pc < PIECE_NB; ++pc)
        for (int s = 0; s < 64; ++s)
            psq[pc][s] = rand64();
    for (int f = 0; f < 8; ++f) enpassant[f] = rand64();
    for (int cr = 0; cr < 16; ++cr) castling[cr] = rand64();
    side = rand64();
}
}

static const std::string PieceToChar(" PNBRQK  pnbrqk");

// ---------------------------------------------------------------------------
// FEN
// ---------------------------------------------------------------------------
void Position::set(const std::string& fenStr, StateInfo* si) {
    std::memset(this, 0, sizeof(Position));
    std::memset(si, 0, sizeof(StateInfo));
    st = si;
    st->epSquare = SQ_NONE;
    for (int i = 0; i < 16; ++i) castlingRookSquare[i] = SQ_NONE;

    int col = 0, row = 7;
    unsigned char token;
    size_t idx;
    std::istringstream ss(fenStr);
    ss >> std::noskipws;

    // 1. Piece placement
    while ((ss >> token) && !isspace(token)) {
        if (isdigit(token)) col += (token - '0');
        else if (token == '/') { row--; col = 0; }
        else if ((idx = PieceToChar.find(token)) != std::string::npos) {
            if (col < 8 && row >= 0) put_piece(Piece(idx), make_square(col, row));
            ++col;
        }
    }

    // 2. Side to move
    ss >> token;
    sideToMove = (token == 'w' ? WHITE : BLACK);
    ss >> token;

    // 3. Castling rights
    while ((ss >> token) && !isspace(token)) {
        Color c;
        int rsq;
        if (token == '-') continue;
        else if (token == 'K') { c = WHITE; rsq = SQ_H1; }
        else if (token == 'Q') { c = WHITE; rsq = SQ_A1; }
        else if (token == 'k') { c = BLACK; rsq = SQ_H8; }
        else if (token == 'q') { c = BLACK; rsq = SQ_A8; }
        else continue;
        if (piece_on(rsq) == make_piece(c, ROOK)) set_castling_right(c, rsq);
    }

    // 4. En passant square
    bool epOk = false;
    {
        unsigned char c1, r1;
        if ((ss >> c1) && (c1 >= 'a') && (c1 <= 'h') && (ss >> r1)
            && (r1 == (sideToMove == WHITE ? '6' : '3'))) {
            st->epSquare = make_square(c1 - 'a', r1 - '1');
            epOk = true;
        }
    }
    if (epOk) {
        // Keep the ep square only if a pawn could genuinely capture there.
        int capsq = st->epSquare - (sideToMove == WHITE ? 8 : -8);
        int pushFrom = st->epSquare + (sideToMove == WHITE ? 8 : -8);
        if (!(BB::PawnAttacks[~sideToMove][st->epSquare] & pieces(sideToMove, PAWN))
            || !(pieces(~sideToMove, PAWN) & square_bb(capsq))
            || (pieces() & (square_bb(st->epSquare) | square_bb(pushFrom))))
            st->epSquare = SQ_NONE;
    } else
        st->epSquare = SQ_NONE;

    // 5. Halfmove clock and fullmove number
    int hm = 0, fm = 1;
    ss >> std::skipws >> hm >> fm;
    st->rule50 = hm;
    gamePly = std::max(2 * (fm - 1), 0) + (sideToMove == BLACK);

    set_state();
}

void Position::set_castling_right(Color c, int rfrom) {
    int kfrom = king_square(c);
    int cr = (c == WHITE ? WHITE_OO : BLACK_OO) << (rfrom < kfrom ? 1 : 0);

    st->castlingRights |= cr;
    castlingRightsMask[kfrom] |= cr;
    castlingRightsMask[rfrom] |= cr;
    castlingRookSquare[cr] = rfrom;

    bool kingSide = (cr & (WHITE_OO | BLACK_OO)) != 0;
    int kto = relative_square(c, kingSide ? SQ_G1 : SQ_C1);
    int rto = relative_square(c, kingSide ? SQ_F1 : SQ_D1);

    castlingPath[cr] = (between_bb(rfrom, rto) | between_bb(kfrom, kto)
                        | square_bb(rto) | square_bb(kto))
                       & ~(square_bb(kfrom) | square_bb(rfrom));
}

void Position::set_check_info() const {
    for (int ci = 0; ci < 2; ++ci) {
        Color c = Color(ci);
        int ksq = kingSquare[c];
        Bitboard blockers = 0, pinners = 0;
        Bitboard snipers = ((BB::PseudoAttacks[ROOK][ksq] & pieces(ROOK, QUEEN))
                          | (BB::PseudoAttacks[BISHOP][ksq] & pieces(BISHOP, QUEEN))) & pieces(~c);
        Bitboard occupancy = pieces() ^ snipers;
        while (snipers) {
            int sniperSq = pop_lsb(snipers);
            Bitboard b = between_bb(ksq, sniperSq) & occupancy;
            if (b && !more_than_one(b)) {
                blockers |= b;
                if (b & pieces(c)) pinners |= square_bb(sniperSq);
            }
        }
        st->blockersForKing[c] = blockers;
        st->pinners[c] = pinners;
    }

    int ksq = kingSquare[~sideToMove];
    st->checkSquares[PAWN]   = BB::PawnAttacks[~sideToMove][ksq];
    st->checkSquares[KNIGHT] = BB::PseudoAttacks[KNIGHT][ksq];
    st->checkSquares[BISHOP] = BB::bishop_attacks(ksq, pieces());
    st->checkSquares[ROOK]   = BB::rook_attacks(ksq, pieces());
    st->checkSquares[QUEEN]  = st->checkSquares[BISHOP] | st->checkSquares[ROOK];
    st->checkSquares[KING]   = 0;
}

void Position::set_state() const {
    st->key = 0;
    st->pawnKey = 0;
    st->nonPawnMaterial[WHITE] = st->nonPawnMaterial[BLACK] = 0;
    st->checkersBB = attackers_to(king_square(sideToMove)) & pieces(~sideToMove);

    Bitboard b = pieces();
    while (b) {
        int s = pop_lsb(b);
        Piece pc = piece_on(s);
        st->key ^= Zobrist::psq[pc][s];
        if (type_of(pc) == PAWN) st->pawnKey ^= Zobrist::psq[pc][s];
        else if (type_of(pc) != KING) st->nonPawnMaterial[color_of(pc)] += PieceValue[type_of(pc)];
    }
    if (st->epSquare != SQ_NONE) st->key ^= Zobrist::enpassant[file_of(st->epSquare)];
    if (sideToMove == BLACK) st->key ^= Zobrist::side;
    st->key ^= Zobrist::castling[st->castlingRights];

    set_check_info();
}

std::string Position::fen() const {
    std::ostringstream ss;
    for (int r = 7; r >= 0; --r) {
        for (int f = 0; f < 8; ++f) {
            int emptyCnt = 0;
            while (f < 8 && empty(make_square(f, r))) { ++emptyCnt; ++f; }
            if (emptyCnt) ss << emptyCnt;
            if (f < 8) ss << PieceToChar[piece_on(make_square(f, r))];
        }
        if (r > 0) ss << '/';
    }
    ss << (sideToMove == WHITE ? " w " : " b ");
    if (can_castle(WHITE_OO)) ss << 'K';
    if (can_castle(WHITE_OOO)) ss << 'Q';
    if (can_castle(BLACK_OO)) ss << 'k';
    if (can_castle(BLACK_OOO)) ss << 'q';
    if (!can_castle(ANY_CASTLING)) ss << '-';
    ss << ' ' << (st->epSquare == SQ_NONE ? std::string("-") : square_to_string(st->epSquare))
       << ' ' << st->rule50 << ' ' << (1 + (gamePly - (sideToMove == BLACK)) / 2);
    return ss.str();
}

// ---------------------------------------------------------------------------
// Attacks
// ---------------------------------------------------------------------------
Bitboard Position::attackers_to(int s, Bitboard occ) const {
    return (BB::PawnAttacks[BLACK][s] & pieces(WHITE, PAWN))
         | (BB::PawnAttacks[WHITE][s] & pieces(BLACK, PAWN))
         | (BB::PseudoAttacks[KNIGHT][s] & pieces(KNIGHT))
         | (BB::rook_attacks(s, occ) & pieces(ROOK, QUEEN))
         | (BB::bishop_attacks(s, occ) & pieces(BISHOP, QUEEN))
         | (BB::PseudoAttacks[KING][s] & pieces(KING));
}

Bitboard Position::attacks_by(Color c, PieceType pt) const {
    if (pt == PAWN) return pawn_attacks_bb(c, pieces(c, PAWN));
    Bitboard threats = 0, attackers = pieces(c, pt);
    while (attackers) threats |= BB::attacks_bb(pt, pop_lsb(attackers), pieces());
    return threats;
}

// ---------------------------------------------------------------------------
// Legality
// ---------------------------------------------------------------------------
bool Position::legal(Move m) const {
    Color us = sideToMove;
    int from = from_sq(m), to = to_sq(m);
    MoveType mt = type_of_move(m);

    if (mt == CASTLING) {
        // Emptiness of the path and the rights were checked at generation time.
        // Here we verify that the king does not start, cross or land on an attacked square.
        int step = (to > from) ? 1 : -1;
        for (int s = from; s != to; s += step)
            if (attackers_to(s + step) & pieces(~us)) return false;
        return true;
    }

    if (mt == EN_PASSANT) {
        int ksq = king_square(us);
        int capsq = to - (us == WHITE ? 8 : -8);
        Bitboard occ = (pieces() ^ square_bb(from) ^ square_bb(capsq)) | square_bb(to);
        return !(BB::rook_attacks(ksq, occ) & pieces(~us, ROOK, QUEEN))
            && !(BB::bishop_attacks(ksq, occ) & pieces(~us, BISHOP, QUEEN));
    }

    if (type_of(piece_on(from)) == KING)
        return !(attackers_to(to, pieces() ^ square_bb(from)) & pieces(~us));

    // Any other move is legal if the piece is not pinned, or moves along the pin ray.
    return !(blockers_for_king(us) & square_bb(from)) || BB::aligned(from, to, king_square(us));
}

// Validate a possibly-corrupt move (e.g. a transposition table hit) against this position.
bool Position::pseudo_legal(Move m) const {
    Color us = sideToMove;
    int from = from_sq(m), to = to_sq(m);
    MoveType mt = type_of_move(m);

    if (!is_ok_move(m)) return false;
    // The promotion-type bits must be zero unless this really is a promotion, otherwise
    // several distinct encodings would denote the same move and a corrupt transposition
    // table entry could smuggle one of them into the search.
    if (mt != PROMOTION && ((m >> 12) & 3) != 0) return false;
    Piece pc = piece_on(from);
    if (pc == NO_PIECE || color_of(pc) != us) return false;
    if (pieces(us) & square_bb(to)) return false;

    if (mt == CASTLING) {
        if (type_of(pc) != KING) return false;
        if (from != relative_square(us, SQ_E1)) return false;
        bool kingSide = to > from;
        int cr = (us == WHITE ? WHITE_OO : BLACK_OO) << (kingSide ? 0 : 1);
        if (to != relative_square(us, kingSide ? SQ_G1 : SQ_C1)) return false;
        if (!can_castle(cr) || castling_impeded(cr) || checkers()) return false;
        return true;
    }

    if (mt == EN_PASSANT) {
        if (type_of(pc) != PAWN) return false;
        if (st->epSquare == SQ_NONE || to != st->epSquare) return false;
        if (!(BB::PawnAttacks[us][from] & square_bb(to))) return false;
        if (!empty(to)) return false;
        return true;
    }

    if (mt == PROMOTION && (type_of(pc) != PAWN || relative_rank(us, to) != 7)) return false;
    if (mt == NORMAL && type_of(pc) == PAWN && relative_rank(us, to) == 7) return false;

    if (type_of(pc) == PAWN) {
        int up = (us == WHITE ? 8 : -8);
        if (BB::PawnAttacks[us][from] & square_bb(to)) {
            if (empty(to)) return false;               // en passant is handled above
        } else if (to == from + up) {
            if (!empty(to)) return false;
        } else if (to == from + 2 * up) {
            if (relative_rank(us, from) != 1) return false;
            if (!empty(to) || !empty(from + up)) return false;
        } else return false;
    } else {
        if (!(BB::attacks_bb(type_of(pc), from, pieces()) & square_bb(to))) return false;
    }

    // If in check, weed out moves that obviously fail to address it.
    if (checkers() && type_of(pc) != KING) {
        if (more_than_one(checkers())) return false;
        int checksq = lsb(checkers());
        if (!((between_bb(king_square(us), checksq) | checkers()) & square_bb(to))) {
            // An en-passant capture of the checking pawn is the one exception, but
            // that path already returned above.
            return false;
        }
    }
    return true;
}

bool Position::gives_check(Move m) const {
    int from = from_sq(m), to = to_sq(m);
    Color us = sideToMove;
    int theirKing = king_square(~us);
    MoveType mt = type_of_move(m);

    // Direct check
    PieceType pt = type_of(piece_on(from));
    if (mt != PROMOTION && (check_squares(pt) & square_bb(to))) return true;

    // Discovered check
    if ((blockers_for_king(~us) & square_bb(from)) && !BB::aligned(from, to, theirKing))
        return true;

    switch (mt) {
        case NORMAL:
            return false;
        case PROMOTION:
            return (BB::attacks_bb(promotion_type(m), to, pieces() ^ square_bb(from))
                    & square_bb(theirKing)) != 0;
        case EN_PASSANT: {
            int capsq = make_square(file_of(to), rank_of(from));
            Bitboard b = (pieces() ^ square_bb(from) ^ square_bb(capsq)) | square_bb(to);
            return ((BB::rook_attacks(theirKing, b) & pieces(us, ROOK, QUEEN))
                  | (BB::bishop_attacks(theirKing, b) & pieces(us, BISHOP, QUEEN))) != 0;
        }
        default: {  // CASTLING: only the rook can be giving check after the move
            int rto = relative_square(us, (to > from) ? SQ_F1 : SQ_D1);
            Bitboard occ = (pieces() ^ square_bb(from)) | square_bb(to);
            return (BB::rook_attacks(rto, occ) & square_bb(theirKing)) != 0;
        }
    }
}

// ---------------------------------------------------------------------------
// do_move / undo_move
// ---------------------------------------------------------------------------
void Position::do_move(Move m, StateInfo& newSt, bool givesCheck) {
    U64 k = st->key ^ Zobrist::side;

    // Copy the part of the state that persists across the move
    std::memcpy(&newSt, st, offsetof(StateInfo, key));
    newSt.previous = st;
    st = &newSt;

    ++gamePly;
    ++st->rule50;
    ++st->pliesFromNull;

    Color us = sideToMove, them = ~us;
    int from = from_sq(m), to = to_sq(m);
    MoveType mt = type_of_move(m);
    Piece pc = piece_on(from);
    Piece captured = (mt == EN_PASSANT) ? make_piece(them, PAWN) : piece_on(to);

    st->movedPiece = pc;
    st->currentMove = m;

    if (mt == CASTLING) {
        int rfrom, rto;
        do_castling<true>(us, from, to, rfrom, rto);
        k ^= Zobrist::psq[make_piece(us, ROOK)][rfrom] ^ Zobrist::psq[make_piece(us, ROOK)][rto];
        k ^= Zobrist::psq[pc][from] ^ Zobrist::psq[pc][to];
        captured = NO_PIECE;
    }

    if (captured) {
        int capsq = to;
        if (type_of(captured) == PAWN) {
            if (mt == EN_PASSANT) capsq = to - (us == WHITE ? 8 : -8);
            st->pawnKey ^= Zobrist::psq[captured][capsq];
        } else
            st->nonPawnMaterial[them] -= PieceValue[type_of(captured)];

        remove_piece(capsq);
        k ^= Zobrist::psq[captured][capsq];
        st->rule50 = 0;
    }

    // Retire the previous en passant square
    if (st->epSquare != SQ_NONE) {
        k ^= Zobrist::enpassant[file_of(st->epSquare)];
        st->epSquare = SQ_NONE;
    }

    // Update castling rights
    if (st->castlingRights && (castlingRightsMask[from] | castlingRightsMask[to])) {
        k ^= Zobrist::castling[st->castlingRights];
        st->castlingRights &= ~(castlingRightsMask[from] | castlingRightsMask[to]);
        k ^= Zobrist::castling[st->castlingRights];
    }

    if (mt != CASTLING) {
        move_piece(from, to);
        k ^= Zobrist::psq[pc][from] ^ Zobrist::psq[pc][to];
    }

    if (type_of(pc) == PAWN) {
        st->rule50 = 0;

        if ((from ^ to) == 16) {
            int epsq = (from + to) / 2;
            // Only publish the ep square if an enemy pawn is in a position to take
            if (BB::PawnAttacks[us][epsq] & pieces(them, PAWN)) {
                st->epSquare = epsq;
                k ^= Zobrist::enpassant[file_of(epsq)];
            }
        }

        if (mt == PROMOTION) {
            Piece promo = make_piece(us, promotion_type(m));
            remove_piece(to);
            put_piece(promo, to);
            k ^= Zobrist::psq[pc][to] ^ Zobrist::psq[promo][to];
            st->pawnKey ^= Zobrist::psq[pc][from];
            st->nonPawnMaterial[us] += PieceValue[promotion_type(m)];
        } else {
            st->pawnKey ^= Zobrist::psq[pc][from] ^ Zobrist::psq[pc][to];
        }
    }

    st->capturedPiece = captured;
    st->key = k;
    sideToMove = them;

    st->checkersBB = givesCheck ? (attackers_to(king_square(them)) & pieces(us)) : Bitboard(0);
    set_check_info();

    // Repetition bookkeeping
    st->repetition = 0;
    int end = std::min(st->rule50, st->pliesFromNull);
    if (end >= 4) {
        StateInfo* stp = st->previous->previous;
        for (int i = 4; i <= end; i += 2) {
            stp = stp->previous->previous;
            if (stp->key == st->key) {
                st->repetition = stp->repetition ? -i : i;
                break;
            }
        }
    }
}

void Position::undo_move(Move m) {
    sideToMove = ~sideToMove;

    Color us = sideToMove;
    int from = from_sq(m), to = to_sq(m);
    MoveType mt = type_of_move(m);

    if (mt == PROMOTION) {
        remove_piece(to);
        put_piece(make_piece(us, PAWN), to);
    }

    if (mt == CASTLING) {
        int rfrom, rto;
        do_castling<false>(us, from, to, rfrom, rto);
    } else {
        move_piece(to, from);
        if (st->capturedPiece) {
            int capsq = to;
            if (mt == EN_PASSANT) capsq = to - (us == WHITE ? 8 : -8);
            put_piece(st->capturedPiece, capsq);
        }
    }

    st = st->previous;
    --gamePly;
}

template<bool Do>
void Position::do_castling(Color us, int from, int to, int& rfrom, int& rto) {
    bool kingSide = to > from;
    rfrom = relative_square(us, kingSide ? SQ_H1 : SQ_A1);
    rto   = relative_square(us, kingSide ? SQ_F1 : SQ_D1);

    remove_piece(Do ? from : to);
    remove_piece(Do ? rfrom : rto);
    put_piece(make_piece(us, KING), Do ? to : from);
    put_piece(make_piece(us, ROOK), Do ? rto : rfrom);
}

void Position::do_null_move(StateInfo& newSt) {
    std::memcpy(&newSt, st, sizeof(StateInfo));
    newSt.previous = st;
    st = &newSt;

    if (st->epSquare != SQ_NONE) {
        st->key ^= Zobrist::enpassant[file_of(st->epSquare)];
        st->epSquare = SQ_NONE;
    }
    st->key ^= Zobrist::side;
    ++st->rule50;
    st->pliesFromNull = 0;
    st->capturedPiece = NO_PIECE;
    st->currentMove = MOVE_NULL;
    st->movedPiece = NO_PIECE;
    st->repetition = 0;
    sideToMove = ~sideToMove;
    set_check_info();
}

void Position::undo_null_move() {
    st = st->previous;
    sideToMove = ~sideToMove;
}

U64 Position::key_after(Move m) const {
    int from = from_sq(m), to = to_sq(m);
    Piece pc = piece_on(from);
    Piece captured = piece_on(to);
    U64 k = st->key ^ Zobrist::side;
    if (captured) k ^= Zobrist::psq[captured][to];
    k ^= Zobrist::psq[pc][to] ^ Zobrist::psq[pc][from];
    if (st->epSquare != SQ_NONE) k ^= Zobrist::enpassant[file_of(st->epSquare)];
    return k;
}

// ---------------------------------------------------------------------------
// Static exchange evaluation: is the exchange on the destination square worth
// at least `threshold` to the side to move?
// ---------------------------------------------------------------------------
bool Position::see_ge(Move m, int threshold) const {
    if (type_of_move(m) != NORMAL) return 0 >= threshold;

    int from = from_sq(m), to = to_sq(m);
    int swap = SEEValue[type_of(piece_on(to))] - threshold;
    if (swap < 0) return false;

    swap = SEEValue[type_of(piece_on(from))] - swap;
    if (swap <= 0) return true;

    Bitboard occupied = pieces() ^ square_bb(from) ^ square_bb(to);
    Color stm = sideToMove;
    Bitboard attackers = attackers_to(to, occupied);
    Bitboard stmAttackers, bb;
    int res = 1;

    while (true) {
        stm = ~stm;
        attackers &= occupied;
        if (!(stmAttackers = attackers & pieces(stm))) break;

        // Pieces pinned against their own king cannot join the exchange.
        if (st->pinners[~stm] & occupied) {
            stmAttackers &= ~st->blockersForKing[stm];
            if (!stmAttackers) break;
        }

        res ^= 1;

        if ((bb = stmAttackers & pieces(PAWN))) {
            if ((swap = SEEValue[PAWN] - swap) < res) break;
            occupied ^= square_bb(lsb(bb));
            attackers |= BB::bishop_attacks(to, occupied) & pieces(BISHOP, QUEEN);
        } else if ((bb = stmAttackers & pieces(KNIGHT))) {
            if ((swap = SEEValue[KNIGHT] - swap) < res) break;
            occupied ^= square_bb(lsb(bb));
        } else if ((bb = stmAttackers & pieces(BISHOP))) {
            if ((swap = SEEValue[BISHOP] - swap) < res) break;
            occupied ^= square_bb(lsb(bb));
            attackers |= BB::bishop_attacks(to, occupied) & pieces(BISHOP, QUEEN);
        } else if ((bb = stmAttackers & pieces(ROOK))) {
            if ((swap = SEEValue[ROOK] - swap) < res) break;
            occupied ^= square_bb(lsb(bb));
            attackers |= BB::rook_attacks(to, occupied) & pieces(ROOK, QUEEN);
        } else if ((bb = stmAttackers & pieces(QUEEN))) {
            if ((swap = SEEValue[QUEEN] - swap) < res) break;
            occupied ^= square_bb(lsb(bb));
            attackers |= (BB::bishop_attacks(to, occupied) & pieces(BISHOP, QUEEN))
                       | (BB::rook_attacks(to, occupied) & pieces(ROOK, QUEEN));
        } else {
            // Only the king is left: it may not recapture into an attacked square.
            return (attackers & ~pieces(stm)) ? bool(res ^ 1) : bool(res);
        }
    }
    return bool(res);
}

// ---------------------------------------------------------------------------
// Draw detection
// ---------------------------------------------------------------------------
static bool insufficient_material(const Position& pos) {
    if (pos.pieces(PAWN) || pos.pieces(ROOK) || pos.pieces(QUEEN)) return false;
    int wn = pos.count(WHITE, KNIGHT), bn = pos.count(BLACK, KNIGHT);
    int wb = pos.count(WHITE, BISHOP), bb = pos.count(BLACK, BISHOP);
    if (wn + wb <= 1 && bn + bb <= 1) return true;            // K(+one minor) vs K(+one minor)
    if (wb == 0 && bb == 0 && ((wn == 2 && bn == 0) || (bn == 2 && wn == 0))) return true;  // KNN vs K
    return false;
}

bool Position::is_draw(int ply) const {
    if (st->rule50 > 99) {
        // Only a real draw if the side to move is not already checkmated; the search
        // calls this before generating moves, and a mate-on-move-100 is vanishingly
        // rare, so treat it as a draw.
        return true;
    }
    if (st->repetition && st->repetition < ply) return true;
    if (insufficient_material(*this)) return true;
    return false;
}
