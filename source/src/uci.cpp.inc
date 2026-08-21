#include "position.h"
#include "movegen.h"
#include "search.h"
#include "eval.h"
#include "tt.h"
#include "perft.h"
#include <deque>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>

namespace UCI {

const std::string EngineName = "Opus 5 chess 24hrs";
const std::string EngineAuthor = "Opus 5";

const std::string StartFEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

namespace {

Position pos;
std::deque<StateInfo> states(1);
std::thread searchThread;

void wait_for_search() {
    if (searchThread.joinable()) searchThread.join();
}

void stop_search() {
    Search::Stop = true;
    wait_for_search();
}

// Turn a long-algebraic string into a Move by matching it against the legal moves.
Move to_move(const Position& p, const std::string& str) {
    for (const ExtMove& m : MoveList<LEGAL>(p)) {
        std::string s = square_to_string(from_sq(m.move)) + square_to_string(to_sq(m.move));
        if (type_of_move(m.move) == PROMOTION) s += " pnbrqk"[promotion_type(m.move)];
        if (s == str) return m.move;
    }
    return MOVE_NONE;
}

void set_position(std::istringstream& is) {
    std::string token, fen;
    is >> token;

    if (token == "startpos") {
        fen = StartFEN;
        is >> token;   // consume "moves" if present
    } else if (token == "fen") {
        while (is >> token && token != "moves") fen += token + " ";
    } else
        return;

    states.clear();
    states.emplace_back();
    pos.set(fen, &states.back());

    while (is >> token) {
        Move m = to_move(pos, token);
        if (m == MOVE_NONE) break;
        states.emplace_back();
        pos.do_move(m, states.back());
    }
}

void set_option(std::istringstream& is) {
    std::string token, name, value;
    is >> token;                       // "name"
    while (is >> token && token != "value") name += (name.empty() ? "" : " ") + token;
    while (is >> token) value += (value.empty() ? "" : " ") + token;

    auto lower = [](std::string s) {
        for (char& c : s) c = char(tolower((unsigned char)c));
        return s;
    };
    std::string n = lower(name);

    if (n == "hash") {
        int mb = std::atoi(value.c_str());
        mb = std::clamp(mb, 1, 32768);
        Search::Options.hashMB = mb;
        TT.resize(size_t(mb));
    } else if (n == "move overhead") {
        Search::Options.moveOverhead = std::clamp(std::atoi(value.c_str()), 0, 5000);
    } else if (n == "clear hash") {
        Search::clear();
    }
    // Unknown options are ignored, as the protocol requires.
}

void go(std::istringstream& is) {
    wait_for_search();

    Search::LimitsType limits;
    std::string token;
    bool sawLimit = false;

    while (is >> token) {
        if (token == "wtime")          { is >> limits.time[WHITE]; sawLimit = true; }
        else if (token == "btime")     { is >> limits.time[BLACK]; sawLimit = true; }
        else if (token == "winc")      { is >> limits.inc[WHITE]; sawLimit = true; }
        else if (token == "binc")      { is >> limits.inc[BLACK]; sawLimit = true; }
        else if (token == "movestogo") { is >> limits.movestogo; sawLimit = true; }
        else if (token == "depth")     { is >> limits.depth; sawLimit = true; }
        else if (token == "movetime")  { is >> limits.movetime; sawLimit = true; }
        else if (token == "mate")      { is >> limits.mate; sawLimit = true; }
        else if (token == "infinite")  { limits.infinite = true; sawLimit = true; }
        else if (token == "ponder")    { Search::Pondering = true; sawLimit = true; }
        else if (token == "nodes")     { int64_t n; is >> n; sawLimit = true; }
        else if (token == "searchmoves") { while (is >> token) {} }
        // anything else is ignored
    }

    // A bare "go" is not defined by the protocol; search to a fixed, finite depth
    // rather than risk hanging a harness that never sends "stop".
    if (!sawLimit) limits.depth = 30;

    Search::Limits = limits;
    Search::Stop = false;
    searchThread = std::thread([] { Search::think(pos); });
}

// A fixed workload used to compare the speed of two builds.
void bench(std::istringstream& is) {
    static const char* Positions[] = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
        "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
        "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
        "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
        "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10",
        "2rr3k/pp3pp1/1nnqbN1p/3pN3/2pP4/2P3Q1/PPB4P/R4RK1 w - - 0 1",
        "4rrk1/pp1n3p/3q2pQ/2p1pb2/2PP4/2P3N1/P2B2PP/4RRK1 b - - 7 19",
        "r3r1k1/2p2ppp/p1p1bn2/8/1q2P3/2NPQN2/PPP3PP/R4RK1 b - - 2 15",
        "6k1/6p1/6Pp/ppp5/3pn2P/1P3K2/1PP2P2/3N4 b - - 0 1",
        "8/8/8/8/5kp1/P7/8/1K1N4 w - - 0 1",
        "8/2p4P/8/kr6/6R1/8/8/1K6 w - - 0 1",
        "5k2/7R/4P2p/5K2/p1r2P1p/8/8/8 b - - 0 1",
        "6k1/6p1/P6p/r1N5/5p2/7P/1b3PP1/4R1K1 w - - 0 1",
        "1r3k2/4q3/2Pp3b/3Bp3/2Q2p2/1p1P2P1/1P2KP2/3N4 w - - 0 1",
        "8/8/2p5/3p4/3P4/3K4/8/3k4 w - - 0 1",
    };
    std::string token;
    int depth = 12;
    if (is >> token) depth = std::atoi(token.c_str());

    wait_for_search();
    Search::clear();

    uint64_t totalNodes = 0;
    int64_t start = Search::now();

    for (const char* fen : Positions) {
        states.clear();
        states.emplace_back();
        pos.set(fen, &states.back());

        Search::LimitsType limits;
        limits.depth = depth;
        Search::Limits = limits;
        Search::Stop = false;
        Search::think(pos);
        totalNodes += Search::Nodes;
    }

    int64_t elapsed = std::max<int64_t>(1, Search::now() - start);
    std::cout << "\n===========================\n"
              << "Total time (ms) : " << elapsed << "\n"
              << "Nodes searched  : " << totalNodes << "\n"
              << "Nodes/second    : " << (totalNodes * 1000 / (uint64_t)elapsed) << std::endl;
}

} // anonymous namespace

void loop(int argc, char** argv) {
    states.clear();
    states.emplace_back();
    pos.set(StartFEN, &states.back());

    // Allow commands on the command line, e.g. "engine.exe bench 12"
    std::string cmd;
    for (int i = 1; i < argc; ++i) cmd += std::string(argv[i]) + " ";

    do {
        if (argc == 1 && !std::getline(std::cin, cmd)) cmd = "quit";

        std::istringstream is(cmd);
        std::string token;
        is >> std::skipws >> token;

        if (token == "quit" || token == "stop") {
            Search::Stop = true;
            if (token == "quit") { wait_for_search(); break; }
            wait_for_search();
        } else if (token == "uci") {
            std::cout << "id name " << EngineName << "\n"
                      << "id author " << EngineAuthor << "\n"
                      << "option name Hash type spin default 256 min 1 max 32768\n"
                      << "option name Threads type spin default 1 min 1 max 1\n"
                      << "option name Move Overhead type spin default 25 min 0 max 5000\n"
                      << "option name Clear Hash type button\n"
                      << "uciok" << std::endl;
        } else if (token == "isready") {
            // Must be answered at any time, including during a search.
            std::cout << "readyok" << std::endl;
        } else if (token == "setoption") {
            wait_for_search();
            set_option(is);
        } else if (token == "ucinewgame") {
            wait_for_search();
            Search::clear();
        } else if (token == "position") {
            wait_for_search();
            set_position(is);
        } else if (token == "go") {
            go(is);
        } else if (token == "ponderhit") {
            Search::Pondering = false;
        } else if (token == "bench") {
            bench(is);
        } else if (token == "perft") {
            int d = 5;
            if (is >> token) d = std::atoi(token.c_str());
            wait_for_search();
            std::cout << "Nodes: " << perft<true>(pos, d) << std::endl;
        } else if (token == "perftsuite") {
            std::string path = "../resources/perft/perft.epd";
            int d = 5;
            if (is >> token) path = token;
            if (is >> token) d = std::atoi(token.c_str());
            run_perft_suite(path, d);
        } else if (token == "eval") {
            wait_for_search();
            std::cout << "eval: " << Eval::evaluate(pos) << " (side to move)" << std::endl;
        } else if (token == "d") {
            std::cout << pos.fen() << std::endl;
        }
        // Unknown commands are ignored, as the protocol requires.

    } while (argc == 1);

    stop_search();
}

} // namespace UCI
