#pragma once
#include "position.h"
#include "movegen.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

inline std::string move_to_uci(Move m) {
    if (m == MOVE_NONE) return "0000";
    std::string s = square_to_string(from_sq(m)) + square_to_string(to_sq(m));
    if (type_of_move(m) == PROMOTION) s += " pnbrqk"[promotion_type(m)];
    return s;
}

template<bool Root>
U64 perft(Position& pos, int depth) {
    if (!Root) {
        if (depth <= 0) return 1;
        if (depth == 1) return MoveList<LEGAL>(pos).size();
    }
    StateInfo st;
    U64 nodes = 0, cnt;
    const bool leaf = (depth == 2);

    for (const ExtMove& m : MoveList<LEGAL>(pos)) {
        if (Root && depth <= 1) { cnt = 1; nodes++; }
        else {
            pos.do_move(m, st);
            cnt = leaf ? MoveList<LEGAL>(pos).size() : perft<false>(pos, depth - 1);
            nodes += cnt;
            pos.undo_move(m);
        }
        if (Root) std::cout << move_to_uci(m) << ": " << cnt << std::endl;
    }
    return nodes;
}

// Run resources/perft/perft.epd. Returns the number of failures.
inline int run_perft_suite(const std::string& path, int maxDepth) {
    std::ifstream file(path);
    if (!file) { std::cout << "cannot open " << path << std::endl; return -1; }

    std::string line;
    int lineNo = 0, failures = 0, checks = 0;
    U64 totalNodes = 0;
    auto start = std::chrono::steady_clock::now();

    while (std::getline(file, line)) {
        if (line.empty()) continue;
        ++lineNo;

        // FEN ;D1 n ;D2 n ...
        size_t semi = line.find(';');
        if (semi == std::string::npos) continue;
        std::string fen = line.substr(0, semi);
        while (!fen.empty() && fen.back() == ' ') fen.pop_back();

        Position pos;
        StateInfo si;
        pos.set(fen, &si);

        std::string rest = line.substr(semi);
        std::istringstream ss(rest);
        std::string tok;
        while (ss >> tok) {
            if (tok.size() < 2 || tok[0] != ';') continue;
            int d = std::atoi(tok.c_str() + 2);
            U64 expected;
            if (!(ss >> expected)) break;
            if (d > maxDepth) continue;

            U64 got = perft<false>(pos, d);
            totalNodes += got;
            ++checks;
            if (got != expected) {
                ++failures;
                std::cout << "FAIL line " << lineNo << " depth " << d
                          << " expected " << expected << " got " << got
                          << "  fen: " << fen << std::endl;
            }
        }
    }

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::steady_clock::now() - start).count();
    std::cout << "perft suite: " << checks << " checks, " << failures << " failures, "
              << totalNodes << " nodes in " << elapsed << " ms";
    if (elapsed > 0) std::cout << " (" << (totalNodes / (U64)elapsed) * 1000 << " nps)";
    std::cout << std::endl;
    return failures;
}
