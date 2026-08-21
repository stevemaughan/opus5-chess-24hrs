// Exhaustively verifies pseudo_legal() against the real generator: for every one of
// the 65536 possible 16-bit move encodings, pseudo_legal() must agree with membership
// in the generated pseudo-legal move list.
#include "bitboard.cpp.inc"
#include "position.cpp.inc"
#include "movegen.h"
#include <fstream>
#include <iostream>
#include <sstream>

static bool in_generated(const Position& pos, Move m) {
    ExtMove list[MAX_MOVES];
    ExtMove* end = pos.checkers() ? generate<EVASIONS>(pos, list) : generate<NON_EVASIONS>(pos, list);
    for (ExtMove* p = list; p != end; ++p) if (p->move == m) return true;
    return false;
}

int main(int argc, char** argv) {
    BB::init();
    Zobrist::init();

    std::string path = argc > 1 ? argv[1] : "../../resources/perft/perft.epd";
    std::ifstream f(path);
    if (!f) { std::cout << "cannot open " << path << "\n"; return 2; }

    std::string line;
    int positions = 0;
    long long falsePos = 0, falseNeg = 0;

    while (std::getline(f, line)) {
        size_t semi = line.find(';');
        if (semi == std::string::npos) continue;
        std::string fen = line.substr(0, semi);
        while (!fen.empty() && fen.back() == ' ') fen.pop_back();

        Position pos; StateInfo si;
        pos.set(fen, &si);
        ++positions;

        for (unsigned code = 0; code < 65536; ++code) {
            Move m = Move(code);
            bool gen = in_generated(pos, m);
            bool ps  = pos.pseudo_legal(m);
            // MOVE_NONE / MOVE_NULL are never legal moves
            if (m == MOVE_NONE || m == MOVE_NULL) { if (ps) ++falsePos; continue; }
            if (ps && !gen) {
                if (falsePos < 8)
                    std::cout << "FALSE POSITIVE " << square_to_string(from_sq(m))
                              << square_to_string(to_sq(m)) << " type=" << type_of_move(m)
                              << " promo=" << promotion_type(m)
                              << "  fen: " << fen << "\n";
                ++falsePos;
            } else if (!ps && gen) {
                if (falseNeg < 8)
                    std::cout << "FALSE NEGATIVE " << square_to_string(from_sq(m))
                              << square_to_string(to_sq(m)) << " type=" << type_of_move(m)
                              << "  fen: " << fen << "\n";
                ++falseNeg;
            }
        }
    }
    std::cout << "positions " << positions << "  false positives " << falsePos
              << "  false negatives " << falseNeg << std::endl;
    return (falsePos || falseNeg) ? 1 : 0;
}
