// Unity build: one translation unit so the optimiser can inline freely.
#include "bitboard.cpp.inc"
#include "position.cpp.inc"
#include "eval.cpp.inc"
#include "tt.cpp.inc"
#include "search.cpp.inc"
#include "uci.cpp.inc"

#include <iostream>

int main(int argc, char** argv) {
    std::ios_base::sync_with_stdio(false);
    std::cout.setf(std::ios::unitbuf);

    BB::init();
    Zobrist::init();
    Eval::init();
    Search::init();
    TT.resize(size_t(Search::Options.hashMB));
    Search::clear();

    UCI::loop(argc, argv);
    return 0;
}
