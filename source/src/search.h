#pragma once
#include "position.h"
#include "history.h"
#include "movepick.h"
#include <atomic>
#include <vector>
#include <chrono>

namespace Search {

struct Stack {
    Move*                 pv;
    const PieceToHistory* continuationHistory;
    int                   ply;
    Move                  currentMove;
    Move                  excludedMove;
    Move                  killers[2];
    Value                 staticEval;
    int                   statScore;
    int                   moveCount;
    bool                  inCheck;
    bool                  ttPv;
    bool                  ttHit;
    int                   cutoffCnt;
};

struct RootMove {
    explicit RootMove(Move m) : pv(1, m) {}
    bool operator==(const Move& m) const { return pv[0] == m; }
    bool operator<(const RootMove& m) const {
        return m.score != score ? m.score < score : m.previousScore < previousScore;
    }
    Value score = -VALUE_INFINITE;
    Value previousScore = -VALUE_INFINITE;
    Value averageScore = -VALUE_INFINITE;
    Value uciScore = -VALUE_INFINITE;
    int   selDepth = 0;
    std::vector<Move> pv;
};

struct LimitsType {
    int64_t time[COLOR_NB] = { 0, 0 };
    int64_t inc[COLOR_NB]  = { 0, 0 };
    int     movestogo = 0;
    int     depth = 0;
    int64_t movetime = 0;
    int     mate = 0;
    uint64_t nodes = 0;
    bool    infinite = false;
    // Set when the go command carried any clock token at all.  Testing the clock
    // values themselves is wrong: "go wtime 0 btime 0" is a legitimate command and
    // must produce an instant move, not an unbounded search.
    bool    timed = false;
    bool    useTimeManagement() const { return timed; }
};

// UCI-settable options that the search reads
struct OptionsType {
    int hashMB = 256;
    int moveOverhead = 25;
};

extern OptionsType Options;
extern LimitsType Limits;
extern std::atomic<bool> Stop;
extern bool Quiet;            // suppress all UCI output (used by datagen/tuning)
extern std::atomic<bool> Pondering;
extern uint64_t Nodes;
extern std::vector<RootMove> RootMoves;

void init();
void clear();
void clear_soft();      // histories only; the TT is already zeroed after resize
void think(Position& pos);        // runs in the search thread
Value search_quiet(Position& pos, const LimitsType& lim, Move& bestMove);
int64_t now();

} // namespace Search
