#include "bitboard.cpp.inc"
#include "position.cpp.inc"
#include "perft.h"

int main(int argc, char** argv) {
    BB::init();
    Zobrist::init();
    std::string path = argc > 1 ? argv[1] : "../../resources/perft/perft.epd";
    int maxDepth = argc > 2 ? std::atoi(argv[2]) : 5;
    return run_perft_suite(path, maxDepth) == 0 ? 0 : 1;
}
