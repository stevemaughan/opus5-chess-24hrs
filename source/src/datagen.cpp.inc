#include "position.h"
#include "movegen.h"
#include "search.h"
#include "eval.h"
#include "tt.h"
#include <deque>
#include <fstream>
#include <iostream>
#include <vector>

// ---------------------------------------------------------------------------
// Self-play data generation for evaluation tuning.
//
// Each game starts from a short random opening, is played out with a fixed node
// budget per move, and every *quiet* position along the way is written out with
// the eventual game result.  Those (position, result) pairs are what the tuner
// fits the evaluation to.  The rules permit this: the values are derived from
// games this engine plays in this session.
// ---------------------------------------------------------------------------

namespace DataGen {

struct Rng {
    uint64_t s;
    explicit Rng(uint64_t seed) : s(seed ? seed : 0x9E3779B97F4A7C15ULL) {}
    uint64_t next() {
        s ^= s >> 12; s ^= s << 25; s ^= s >> 27;
        return s * 2685821657736338717ULL;
    }
    unsigned pick(unsigned n) { return n ? unsigned(next() % n) : 0u; }
};

// Is the game over for a reason other than checkmate/stalemate?
static bool material_draw(const Position& pos) {
    if (pos.pieces(PAWN) || pos.pieces(ROOK) || pos.pieces(QUEEN)) return false;
    int wn = pos.count(WHITE, KNIGHT), bn = pos.count(BLACK, KNIGHT);
    int wb = pos.count(WHITE, BISHOP), bb = pos.count(BLACK, BISHOP);
    return (wn + wb <= 1 && bn + bb <= 1);
}

void run(int games, uint64_t nodesPerMove, const std::string& outPath, uint64_t seed) {
    std::ofstream out(outPath, std::ios::out | std::ios::trunc);
    if (!out) { std::cout << "cannot open " << outPath << std::endl; return; }

    Rng rng(seed);
    Position pos;
    std::deque<StateInfo> states;

    // A small table is plenty here and keeps the per-game clear cheap.
    TT.resize(16);

    int written = 0, played = 0, discarded = 0;
    std::vector<std::string> pendingFens;
    std::vector<U64> keyHistory;

    const std::string StartFEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

    for (int g = 0; g < games; ++g) {
        states.clear();
        states.emplace_back();
        pos.set(StartFEN, &states.back());
        pendingFens.clear();
        keyHistory.clear();

        // ---- Random opening ----
        int randomPlies = 8 + int(rng.pick(3)) * 2;
        bool aborted = false;
        for (int i = 0; i < randomPlies; ++i) {
            MoveList<LEGAL> ml(pos);
            if (ml.size() == 0) { aborted = true; break; }
            Move m = ml.begin()[rng.pick(unsigned(ml.size()))].move;
            states.emplace_back();
            pos.do_move(m, states.back());
        }
        if (aborted) { ++discarded; continue; }
        if (MoveList<LEGAL>(pos).size() == 0) { ++discarded; continue; }

        // Reject openings that are already lopsided - they teach the tuner little.
        Search::clear();
        {
            Search::LimitsType lim;
            lim.nodes = 5000;
            Move bm = MOVE_NONE;
            Value v = Search::search_quiet(pos, lim, bm);
            if (bm == MOVE_NONE || std::abs(int(v)) > 400) { ++discarded; continue; }
        }

        // ---- Play the game out ----
        Search::clear();
        double result = 0.5;
        bool resolved = false;
        int decisivePlies = 0;
        Color decisiveSide = WHITE;

        for (int ply = 0; ply < 400; ++ply) {
            // Terminal conditions
            MoveList<LEGAL> ml(pos);
            if (ml.size() == 0) {
                if (pos.checkers()) result = (pos.side_to_move() == WHITE) ? 0.0 : 1.0;
                else result = 0.5;
                resolved = true;
                break;
            }
            if (pos.rule50_count() >= 100 || material_draw(pos)) { result = 0.5; resolved = true; break; }

            int reps = 0;
            for (U64 k : keyHistory) if (k == pos.key()) ++reps;
            if (reps >= 2) { result = 0.5; resolved = true; break; }
            keyHistory.push_back(pos.key());

            Search::LimitsType lim;
            lim.nodes = nodesPerMove;
            Move best = MOVE_NONE;
            Value score = Search::search_quiet(pos, lim, best);
            if (best == MOVE_NONE) { result = 0.5; resolved = true; break; }

            // Record the position if it is quiet: not in check, best move is not a
            // capture or promotion, and the score is not already decisive.
            if (!pos.checkers() && !pos.capture_stage(best) && std::abs(int(score)) < 1500)
                pendingFens.push_back(pos.fen());

            // Early adjudication once one side is clearly winning
            Color stm = pos.side_to_move();
            if (std::abs(int(score)) >= 1000) {
                Color winning = (score > 0) ? stm : ~stm;
                if (decisivePlies && winning == decisiveSide) ++decisivePlies;
                else { decisiveSide = winning; decisivePlies = 1; }
                if (decisivePlies >= 6) {
                    result = (decisiveSide == WHITE) ? 1.0 : 0.0;
                    resolved = true;
                    break;
                }
            } else
                decisivePlies = 0;

            if (std::abs(int(score)) >= VALUE_MATE_IN_MAX_PLY) {
                Color winning = (score > 0) ? stm : ~stm;
                result = (winning == WHITE) ? 1.0 : 0.0;
                resolved = true;
                break;
            }

            states.emplace_back();
            pos.do_move(best, states.back());
        }

        if (!resolved) result = 0.5;
        ++played;

        for (const std::string& f : pendingFens) {
            out << f << " [" << (result == 1.0 ? "1.0" : result == 0.0 ? "0.0" : "0.5") << "]\n";
            ++written;
        }

        if ((g + 1) % 50 == 0) {
            out.flush();
            std::cerr << "game " << (g + 1) << "/" << games
                      << "  played " << played << "  discarded " << discarded
                      << "  positions " << written << std::endl;
        }
    }

    out.flush();
    std::cerr << "done: " << played << " games, " << discarded << " discarded, "
              << written << " positions -> " << outPath << std::endl;
}

} // namespace DataGen
