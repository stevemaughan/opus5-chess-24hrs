// Texel-style evaluation tuner.
//
// Fits the evaluation parameters to (position, game-result) pairs produced by
// self-play, by minimising the mean squared error between the game result and
// sigmoid(eval).  Optimisation is plain coordinate descent with a shrinking
// step, which is slow but needs no gradients and cannot diverge.
//
// Build:  g++ -O3 -march=x86-64-v3 -std=c++20 -DTUNE -o build/tuner.exe src/tuner_main.cpp
#include "bitboard.cpp.inc"
#include "position.cpp.inc"
#include "eval.cpp.inc"

#include <atomic>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

// ---------------------------------------------------------------------------
// Samples
// ---------------------------------------------------------------------------
struct Sample {
    Position  pos;
    StateInfo st;
    float     result;   // from White's point of view: 1.0 / 0.5 / 0.0
};

static Sample*  Samples = nullptr;
static size_t   NumSamples = 0;
static int      NumThreads = 10;

static size_t count_lines(const std::vector<std::string>& files) {
    size_t n = 0;
    for (const std::string& f : files) {
        std::ifstream in(f);
        std::string line;
        while (std::getline(in, line)) if (line.size() > 10) ++n;
    }
    return n;
}

static void load(const std::vector<std::string>& files, size_t maxSamples) {
    size_t total = count_lines(files);
    std::cout << "found " << total << " positions" << std::endl;

    size_t stride = (maxSamples && total > maxSamples) ? (total / maxSamples) + 1 : 1;
    size_t capacity = (total / stride) + 2;

    Samples = new Sample[capacity];
    NumSamples = 0;

    size_t seen = 0;
    for (const std::string& f : files) {
        std::ifstream in(f);
        std::string line;
        while (std::getline(in, line)) {
            if (line.size() <= 10) continue;
            if (seen++ % stride) continue;
            if (NumSamples >= capacity) break;

            size_t br = line.rfind('[');
            if (br == std::string::npos) continue;
            std::string fen = line.substr(0, br);
            std::string res = line.substr(br + 1);

            float r = 0.5f;
            if (res.rfind("1.0", 0) == 0) r = 1.0f;
            else if (res.rfind("0.0", 0) == 0) r = 0.0f;
            else if (res.rfind("0.5", 0) == 0) r = 0.5f;
            else continue;

            Sample& s = Samples[NumSamples];
            s.pos.set(fen, &s.st);
            s.result = r;
            ++NumSamples;
        }
    }
    std::cout << "loaded " << NumSamples << " samples (stride " << stride << ")" << std::endl;
}

// ---------------------------------------------------------------------------
// Error
// ---------------------------------------------------------------------------
static double K = 1.0;

static inline double sigmoid(double s) { return 1.0 / (1.0 + std::pow(10.0, -K * s / 400.0)); }

static double error_range(size_t lo, size_t hi) {
    double e = 0.0;
    for (size_t i = lo; i < hi; ++i) {
        Value v = Eval::evaluate(Samples[i].pos);
        if (Samples[i].pos.side_to_move() == BLACK) v = Value(-v);
        double d = double(Samples[i].result) - sigmoid(double(v));
        e += d * d;
    }
    return e;
}

static double total_error() {
    if (NumThreads <= 1) return error_range(0, NumSamples) / double(NumSamples);

    std::vector<std::thread> th;
    std::vector<double> partial(NumThreads, 0.0);
    size_t chunk = NumSamples / NumThreads;
    for (int t = 0; t < NumThreads; ++t) {
        size_t lo = t * chunk;
        size_t hi = (t == NumThreads - 1) ? NumSamples : lo + chunk;
        th.emplace_back([lo, hi, t, &partial] { partial[t] = error_range(lo, hi); });
    }
    for (auto& x : th) x.join();
    double e = 0.0;
    for (double p : partial) e += p;
    return e / double(NumSamples);
}

static void refresh() { Eval::init(); }

// ---------------------------------------------------------------------------
// Tunable knobs
// ---------------------------------------------------------------------------
struct Knob {
    std::string name;
    Score* s = nullptr;
    int*   i = nullptr;
    bool   mg = true;
};

static std::vector<Knob> Knobs;

static void add_score(const std::string& name, Score* arr, int n, bool skipZero = false) {
    for (int k = 0; k < n; ++k) {
        if (skipZero && arr[k] == 0) continue;
        std::string base = name + "[" + std::to_string(k) + "]";
        Knobs.push_back({ base + ".mg", &arr[k], nullptr, true });
        Knobs.push_back({ base + ".eg", &arr[k], nullptr, false });
    }
}
static void add_score1(const std::string& name, Score* p) {
    Knobs.push_back({ name + ".mg", p, nullptr, true });
    Knobs.push_back({ name + ".eg", p, nullptr, false });
}
static void add_int(const std::string& name, int* p) {
    Knobs.push_back({ name, nullptr, p, true });
}
static void add_int_array(const std::string& name, int* arr, int n, int from = 0) {
    for (int k = from; k < n; ++k)
        Knobs.push_back({ name + "[" + std::to_string(k) + "]", nullptr, &arr[k], true });
}

static int get(const Knob& k) {
    if (k.i) return *k.i;
    return k.mg ? mg_value(*k.s) : eg_value(*k.s);
}
static void set(const Knob& k, int v) {
    if (k.i) { *k.i = v; return; }
    if (k.mg) *k.s = make_score(v, eg_value(*k.s));
    else      *k.s = make_score(mg_value(*k.s), v);
}

static void build_knobs(bool includePSQT) {
    using namespace Eval;

    // Material (skip the king)
    for (int pt = PAWN; pt <= QUEEN; ++pt)
        add_score1("PieceScore[" + std::to_string(pt) + "]", &PieceScore[pt]);

    add_score("KnightMobility", KnightMobility, 9);
    add_score("BishopMobility", BishopMobility, 14);
    add_score("RookMobility",   RookMobility,   15);
    add_score("QueenMobility",  QueenMobility,  28);

    add_score("PassedRank",    PassedRank, 8, true);
    add_score1("PassedFile",   &PassedFile);
    add_score1("PassedBlocked", &PassedBlocked);
    add_score("ConnectedRank", ConnectedRank, 8, true);

    add_score1("Isolated",      &Isolated);
    add_score1("Doubled",       &Doubled);
    add_score1("Backward",      &Backward);
    add_score1("WeakUnopposed", &WeakUnopposed);

    add_score1("BishopPair",         &BishopPair);
    add_score1("RookOnOpenFile",     &RookOnOpenFile);
    add_score1("RookOnSemiOpen",     &RookOnSemiOpen);
    add_score1("RookOnSeventh",      &RookOnSeventh);
    add_score1("KnightOutpost",      &KnightOutpost);
    add_score1("BishopOutpost",      &BishopOutpost);
    add_score1("ReachableOutpost",   &ReachableOutpost);
    add_score1("BishopPawns",        &BishopPawns);
    add_score1("TrappedRook",        &TrappedRook);
    add_score1("MinorBehindPawn",    &MinorBehindPawn);
    add_score1("LongDiagonalBishop", &LongDiagonalBishop);
    add_score1("QueenPinned",        &QueenPinned);
    add_score1("KingFlankNoPawns",   &KingFlankNoPawns);

    add_score("ThreatByMinor", ThreatByMinor, PIECE_TYPE_NB, true);
    add_score("ThreatByRook",  ThreatByRook,  PIECE_TYPE_NB, true);
    add_score1("ThreatByPawn",     &ThreatByPawn);
    add_score1("ThreatByKing",     &ThreatByKing);
    add_score1("HangingPiece",     &HangingPiece);
    add_score1("RestrictedPiece",  &RestrictedPiece);
    add_score1("ThreatBySafePawn", &ThreatBySafePawn);
    add_score1("PawnPushThreat",   &PawnPushThreat);

    add_int("TempoValue",      &TempoValue);
    add_int("KSAttackerScale", &KSAttackerScale);
    add_int("KSAttacksWeight", &KSAttacksWeight);
    add_int("KSWeakSquares",   &KSWeakSquares);
    add_int("KSRookCheck",     &KSRookCheck);
    add_int("KSQueenCheck",    &KSQueenCheck);
    add_int("KSBishopCheck",   &KSBishopCheck);
    add_int("KSKnightCheck",   &KSKnightCheck);
    add_int("KSNoQueen",       &KSNoQueen);
    add_int("KSShelterScale",  &KSShelterScale);
    add_int("KSQuadDiv",       &KSQuadDiv);
    add_int("KSLinearDiv",     &KSLinearDiv);
    add_int("ConnectedSupport", &ConnectedSupport);
    add_int_array("KingAttackWeight", KingAttackWeight, 6, 2);

    if (includePSQT) {
        // Rank 1 and rank 8 pawn entries are structurally zero; leave them alone.
        add_int_array("mg_pawn", mg_pawn, 56, 8);
        add_int_array("eg_pawn", eg_pawn, 56, 8);
        add_int_array("mg_knight", mg_knight, 64);
        add_int_array("eg_knight", eg_knight, 64);
        add_int_array("mg_bishop", mg_bishop, 64);
        add_int_array("eg_bishop", eg_bishop, 64);
        add_int_array("mg_rook", mg_rook, 64);
        add_int_array("eg_rook", eg_rook, 64);
        add_int_array("mg_queen", mg_queen, 64);
        add_int_array("eg_queen", eg_queen, 64);
        add_int_array("mg_king", mg_king, 64);
        add_int_array("eg_king", eg_king, 64);
    }
}

// ---------------------------------------------------------------------------
// Output
// ---------------------------------------------------------------------------
static void dump_score_array(std::ostream& o, const char* name, const Score* a, int n, int perLine) {
    o << "EVP_SCORE " << name << "[" << n << "] = {\n    ";
    for (int k = 0; k < n; ++k) {
        o << "S(" << mg_value(a[k]) << "," << eg_value(a[k]) << ")";
        if (k != n - 1) o << ", ";
        if ((k + 1) % perLine == 0 && k != n - 1) o << "\n    ";
    }
    o << "\n};\n";
}
static void dump_int_array(std::ostream& o, const char* name, const int* a, int n, int perLine) {
    o << "EVP_INT " << name << "[" << n << "] = {\n    ";
    for (int k = 0; k < n; ++k) {
        o << a[k];
        if (k != n - 1) o << ", ";
        if ((k + 1) % perLine == 0 && k != n - 1) o << "\n    ";
    }
    o << "\n};\n";
}

static void dump(const std::string& path, bool includePSQT) {
    using namespace Eval;
    std::ofstream o(path);
    o << "// Tuned by source/src/tuner_main.cpp on self-play data. Paste into eval.h.\n\n";

    o << "EVP_SCORE PieceScore[PIECE_TYPE_NB] = {\n    S(0,0)";
    for (int pt = PAWN; pt <= QUEEN; ++pt)
        o << ", S(" << mg_value(PieceScore[pt]) << "," << eg_value(PieceScore[pt]) << ")";
    o << ", S(0,0)\n};\n\n";

    dump_score_array(o, "KnightMobility", KnightMobility, 9, 5);
    dump_score_array(o, "BishopMobility", BishopMobility, 14, 5);
    dump_score_array(o, "RookMobility", RookMobility, 15, 5);
    dump_score_array(o, "QueenMobility", QueenMobility, 28, 5);
    dump_score_array(o, "PassedRank", PassedRank, 8, 8);
    dump_score_array(o, "ConnectedRank", ConnectedRank, 8, 8);
    dump_score_array(o, "ThreatByMinor", ThreatByMinor, PIECE_TYPE_NB, 7);
    dump_score_array(o, "ThreatByRook", ThreatByRook, PIECE_TYPE_NB, 7);

    auto s1 = [&](const char* n, Score v) {
        o << "EVP_SCORE " << n << " = S(" << mg_value(v) << "," << eg_value(v) << ");\n";
    };
    s1("PassedFile", PassedFile);
    s1("PassedBlocked", PassedBlocked);
    s1("Isolated", Isolated);
    s1("Doubled", Doubled);
    s1("Backward", Backward);
    s1("WeakUnopposed", WeakUnopposed);
    s1("BishopPair", BishopPair);
    s1("RookOnOpenFile", RookOnOpenFile);
    s1("RookOnSemiOpen", RookOnSemiOpen);
    s1("RookOnSeventh", RookOnSeventh);
    s1("KnightOutpost", KnightOutpost);
    s1("BishopOutpost", BishopOutpost);
    s1("ReachableOutpost", ReachableOutpost);
    s1("BishopPawns", BishopPawns);
    s1("TrappedRook", TrappedRook);
    s1("MinorBehindPawn", MinorBehindPawn);
    s1("LongDiagonalBishop", LongDiagonalBishop);
    s1("QueenPinned", QueenPinned);
    s1("KingFlankNoPawns", KingFlankNoPawns);
    s1("ThreatByPawn", ThreatByPawn);
    s1("ThreatByKing", ThreatByKing);
    s1("HangingPiece", HangingPiece);
    s1("RestrictedPiece", RestrictedPiece);
    s1("ThreatBySafePawn", ThreatBySafePawn);
    s1("PawnPushThreat", PawnPushThreat);

    o << "\nEVP_INT TempoValue = " << TempoValue << ";\n";
    o << "EVP_INT KSAttackerScale = " << KSAttackerScale << ";\n";
    o << "EVP_INT KSAttacksWeight = " << KSAttacksWeight << ";\n";
    o << "EVP_INT KSWeakSquares   = " << KSWeakSquares << ";\n";
    o << "EVP_INT KSRookCheck     = " << KSRookCheck << ";\n";
    o << "EVP_INT KSQueenCheck    = " << KSQueenCheck << ";\n";
    o << "EVP_INT KSBishopCheck   = " << KSBishopCheck << ";\n";
    o << "EVP_INT KSKnightCheck   = " << KSKnightCheck << ";\n";
    o << "EVP_INT KSNoQueen       = " << KSNoQueen << ";\n";
    o << "EVP_INT KSShelterScale  = " << KSShelterScale << ";\n";
    o << "EVP_INT KSQuadDiv       = " << KSQuadDiv << ";\n";
    o << "EVP_INT KSLinearDiv     = " << KSLinearDiv << ";\n";
    o << "EVP_INT ConnectedSupport = " << ConnectedSupport << ";\n";
    dump_int_array(o, "KingAttackWeight", KingAttackWeight, PIECE_TYPE_NB, 7);

    if (includePSQT) {
        o << "\n";
        dump_int_array(o, "mg_pawn", mg_pawn, 64, 8);
        dump_int_array(o, "eg_pawn", eg_pawn, 64, 8);
        dump_int_array(o, "mg_knight", mg_knight, 64, 8);
        dump_int_array(o, "eg_knight", eg_knight, 64, 8);
        dump_int_array(o, "mg_bishop", mg_bishop, 64, 8);
        dump_int_array(o, "eg_bishop", eg_bishop, 64, 8);
        dump_int_array(o, "mg_rook", mg_rook, 64, 8);
        dump_int_array(o, "eg_rook", eg_rook, 64, 8);
        dump_int_array(o, "mg_queen", mg_queen, 64, 8);
        dump_int_array(o, "eg_queen", eg_queen, 64, 8);
        dump_int_array(o, "mg_king", mg_king, 64, 8);
        dump_int_array(o, "eg_king", eg_king, 64, 8);
    }
    o.flush();
    std::cout << "wrote " << path << std::endl;
}

// ---------------------------------------------------------------------------
int main(int argc, char** argv) {
    BB::init();
    Zobrist::init();
    Eval::init();

    std::vector<std::string> files;
    size_t maxSamples = 600000;
    bool includePSQT = false;
    int timeBudgetMin = 30;
    std::string outPath = "tuned.h";

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "-n" && i + 1 < argc) maxSamples = std::strtoull(argv[++i], nullptr, 10);
        else if (a == "-psqt") includePSQT = true;
        else if (a == "-t" && i + 1 < argc) NumThreads = std::atoi(argv[++i]);
        else if (a == "-o" && i + 1 < argc) outPath = argv[++i];
        else if (a == "-mins" && i + 1 < argc) timeBudgetMin = std::atoi(argv[++i]);
        else files.push_back(a);
    }
    if (files.empty()) { std::cout << "usage: tuner [-n N] [-t T] [-psqt] [-mins M] [-o out.h] data...\n"; return 1; }

    load(files, maxSamples);
    if (NumSamples < 1000) { std::cout << "not enough samples\n"; return 1; }

    auto t0 = std::chrono::steady_clock::now();
    auto minutes = [&] {
        return std::chrono::duration_cast<std::chrono::seconds>(
                   std::chrono::steady_clock::now() - t0).count() / 60.0;
    };

    // ---- Fit K first ----
    double bestK = 1.0, bestE = 1e9;
    for (double k = 0.4; k <= 2.01; k += 0.1) {
        K = k;
        double e = total_error();
        if (e < bestE) { bestE = e; bestK = k; }
    }
    for (double k = bestK - 0.09; k <= bestK + 0.091; k += 0.01) {
        K = k;
        double e = total_error();
        if (e < bestE) { bestE = e; bestK = k; }
    }
    K = bestK;
    std::cout << "K = " << K << "  initial error = " << bestE << std::endl;

    build_knobs(includePSQT);
    std::cout << "tuning " << Knobs.size() << " parameters, budget " << timeBudgetMin << " min" << std::endl;

    double best = total_error();
    const double startErr = best;

    for (int step = 8; step >= 1 && minutes() < timeBudgetMin; step /= 2) {
        bool improved = true;
        int sweep = 0;
        while (improved && minutes() < timeBudgetMin) {
            improved = false;
            ++sweep;
            int changes = 0;
            for (size_t idx = 0; idx < Knobs.size(); ++idx) {
                if (minutes() >= timeBudgetMin) break;
                const Knob& kb = Knobs[idx];
                int orig = get(kb);

                set(kb, orig + step);
                refresh();
                double e = total_error();
                if (e < best - 1e-10) { best = e; improved = true; ++changes; continue; }

                set(kb, orig - step);
                refresh();
                e = total_error();
                if (e < best - 1e-10) { best = e; improved = true; ++changes; continue; }

                set(kb, orig);
                refresh();
            }
            std::cout << "step " << step << " sweep " << sweep
                      << ": error " << best << "  changed " << changes
                      << "  (" << minutes() << " min)" << std::endl;
            dump(outPath, includePSQT);
        }
    }

    std::cout << "start error " << startErr << " -> final " << best
              << "  (improvement " << (startErr - best) << ")" << std::endl;
    dump(outPath, includePSQT);
    return 0;
}
