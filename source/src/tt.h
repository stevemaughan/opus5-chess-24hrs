#pragma once
#include "types.h"
#include <cstdlib>

enum Bound : int { BOUND_NONE = 0, BOUND_UPPER = 1, BOUND_LOWER = 2, BOUND_EXACT = 3 };

// Quiescence stores entries at depth 0 and -1, and the static-eval-only store uses -6,
// so the encoded depth needs room below zero.  depth8 == 0 is reserved for "empty".
constexpr int TT_DEPTH_OFFSET = 8;

// 10 bytes per entry; three entries plus two padding bytes fill one 32-byte cluster.
struct TTEntry {
    uint16_t key16;
    uint8_t  depth8;      // depth + TT_DEPTH_OFFSET, clamped to [1,255]; 0 means "empty"
    uint8_t  genBound8;   // generation (upper 5 bits) | pv (bit 2) | bound (low 2 bits)
    Move     move16;
    int16_t  value16;
    int16_t  eval16;

    Move  move() const { return move16; }
    Value value() const { return Value(value16); }
    Value eval() const { return Value(eval16); }
    int   depth() const { return int(depth8) - TT_DEPTH_OFFSET; }
    bool  is_pv() const { return bool(genBound8 & 0x4); }
    Bound bound() const { return Bound(genBound8 & 0x3); }
    bool  occupied() const { return depth8 != 0; }

    void save(U64 k, Value v, bool pv, Bound b, int d, Move m, Value ev, uint8_t gen);
};

class TranspositionTable {
    static constexpr int ClusterSize = 3;
    struct Cluster {
        TTEntry entry[ClusterSize];
        char    padding[2];
    };
    static_assert(sizeof(Cluster) == 32, "cluster must be 32 bytes");

public:
    ~TranspositionTable() { free_mem(); }

    void resize(size_t mbSize);
    void clear();
    TTEntry* probe(U64 key, bool& found) const;
    void new_search() { generation8 += 8; }
    uint8_t generation() const { return generation8; }
    int hashfull() const;
    void prefetch_entry(U64 key) const {
        if (clusterCount) __builtin_prefetch(&table[mul_hi64(key, clusterCount)]);
    }

private:
    void free_mem();
    static size_t mul_hi64(U64 a, U64 b) {
        return (size_t)(((unsigned __int128)a * (unsigned __int128)b) >> 64);
    }

    Cluster* table = nullptr;
    void*    mem = nullptr;
    size_t   clusterCount = 0;
    uint8_t  generation8 = 0;
};

extern TranspositionTable TT;

inline void TTEntry::save(U64 k, Value v, bool pv, Bound b, int d, Move m, Value ev, uint8_t gen) {
    // Keep an existing move if we have none of our own
    if (m || uint16_t(k) != key16) move16 = m;

    // Overwrite unless the stored entry is a deeper, same-position, exact-ish result
    if (b == BOUND_EXACT || uint16_t(k) != key16 || d + 5 + 2 * pv > depth()) {
        key16 = uint16_t(k);
        depth8 = uint8_t(std::clamp(d + TT_DEPTH_OFFSET, 1, 255));
        genBound8 = uint8_t(gen | (unsigned(pv) << 2) | b);
        value16 = int16_t(v);
        eval16 = int16_t(ev);
    }
}

inline TTEntry* TranspositionTable::probe(U64 key, bool& found) const {
    TTEntry* const tte = &table[mul_hi64(key, clusterCount)].entry[0];
    const uint16_t key16 = uint16_t(key);

    for (int i = 0; i < ClusterSize; ++i)
        if (tte[i].key16 == key16 && tte[i].occupied()) {
            // Refresh the generation so this entry survives replacement
            tte[i].genBound8 = uint8_t(generation8 | (tte[i].genBound8 & 0x7));
            found = true;
            return &tte[i];
        }

    // Pick the entry to replace: lowest (depth - relative age)
    TTEntry* replace = tte;
    for (int i = 1; i < ClusterSize; ++i) {
        int rDepth = int(replace->depth8) - ((263 + generation8 - replace->genBound8) & 0xF8);
        int cDepth = int(tte[i].depth8) - ((263 + generation8 - tte[i].genBound8) & 0xF8);
        if (rDepth > cDepth) replace = &tte[i];
    }
    found = false;
    return replace;
}
