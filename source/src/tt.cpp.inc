#include "tt.h"
#include <iostream>

TranspositionTable TT;

void TranspositionTable::free_mem() {
    if (mem) { std::free(mem); mem = nullptr; table = nullptr; clusterCount = 0; }
}

void TranspositionTable::resize(size_t mbSize) {
    free_mem();
    if (mbSize < 1) mbSize = 1;

    size_t bytes = mbSize * 1024 * 1024;
    clusterCount = bytes / sizeof(Cluster);
    if (clusterCount == 0) clusterCount = 1;

    // calloc, not malloc+memset: for a block this size the allocator hands back
    // OS zero-pages, which are committed lazily. Twenty engines starting at once and
    // each eagerly zeroing 256 MB saturates memory bandwidth for long enough that a
    // match manager can decide the engine is unresponsive.
    mem = std::calloc(1, clusterCount * sizeof(Cluster) + 63);
    if (!mem) {
        // Fall back to progressively smaller tables rather than dying
        for (size_t mb = mbSize / 2; mb >= 1 && !mem; mb /= 2) {
            clusterCount = (mb * 1024 * 1024) / sizeof(Cluster);
            mem = std::calloc(1, clusterCount * sizeof(Cluster) + 63);
        }
    }
    if (!mem) {
        // Last resort: a tiny static table, so the engine still plays legal chess
        // rather than dereferencing a null pointer on the first probe.
        static Cluster fallback[64];
        std::memset(fallback, 0, sizeof(fallback));
        table = fallback;
        clusterCount = 64;
        return;
    }
    table = (Cluster*)((uintptr_t(mem) + 63) & ~uintptr_t(63));
    // Already zeroed by calloc; no clear() here.
}

void TranspositionTable::clear() {
    if (table) std::memset(table, 0, clusterCount * sizeof(Cluster));
    generation8 = 0;
}

int TranspositionTable::hashfull() const {
    if (!clusterCount) return 0;
    int cnt = 0;
    size_t n = std::min<size_t>(clusterCount, 1000);
    for (size_t i = 0; i < n; ++i)
        for (int j = 0; j < ClusterSize; ++j)
            if (table[i].entry[j].occupied()
                && (table[i].entry[j].genBound8 & 0xF8) == generation8)
                ++cnt;
    return int(cnt / (n * ClusterSize / 1000.0));
}
