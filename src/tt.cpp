#include "tt.h"
#include <cstring>

TranspositionTable TT;

TranspositionTable::TranspositionTable() : table(nullptr), num_entries(0), generation(0) {
    resize(32); // Default 32MB
}

TranspositionTable::~TranspositionTable() {
    delete[] table;
}

void TranspositionTable::resize(int mb) {
    delete[] table;
    
    uint64_t bytes = uint64_t(mb) * 1024 * 1024;
    num_entries = bytes / sizeof(TTEntry);
    table = new TTEntry[num_entries];
    clear();
}

void TranspositionTable::clear() {
    std::memset(table, 0, num_entries * sizeof(TTEntry));
    generation = 0;
}

void TranspositionTable::new_search() {
    generation++;
}

bool TranspositionTable::probe(uint64_t key, TTEntry& entry) const {
    uint64_t index = key % num_entries;
    const TTEntry& e = table[index];
    
    if (e.key32 == uint32_t(key >> 32) && e.bound != BOUND_NONE) {
        entry = e;
        return true;
    }
    return false;
}

void TranspositionTable::store(uint64_t key, int score, int depth, TTBound bound, Move best_move) {
    uint64_t index = key % num_entries;
    TTEntry& e = table[index];
    
    // Always-replace with depth-preferred: replace if
    //  1) The slot is empty
    //  2) The new entry is from a newer generation (search)
    //  3) The new entry has equal or greater depth
    if (e.bound == BOUND_NONE || e.age != generation || depth >= e.depth) {
        e.key32     = uint32_t(key >> 32);
        e.score     = int16_t(score);
        e.depth     = int16_t(depth);
        e.best_move = best_move.raw();
        e.bound     = uint8_t(bound);
        e.age       = generation;
    }
}
