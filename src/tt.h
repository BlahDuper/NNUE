#pragma once

#include "move.h"
#include <cstdint>

// Transposition Table entry bound types
enum TTBound : uint8_t {
    BOUND_NONE  = 0,
    BOUND_UPPER = 1,   // Score is an upper bound (failed low, score <= alpha)
    BOUND_LOWER = 2,   // Score is a lower bound (failed high, score >= beta)
    BOUND_EXACT = 3    // Exact score (alpha < score < beta)
};

struct TTEntry {
    uint32_t key32;     // Upper 32 bits of the Zobrist key (verification)
    int16_t  score;     // The score
    int16_t  depth;     // Depth of the search that produced this entry
    uint16_t best_move; // Best move (raw data)
    uint8_t  bound;     // Bound type
    uint8_t  age;       // Generation counter for replacement
};

class TranspositionTable {
public:
    TranspositionTable();
    ~TranspositionTable();
    
    // Resize the table to the given size in megabytes
    void resize(int mb);
    
    // Clear all entries
    void clear();
    
    // Increment the generation counter (call at the start of each search)
    void new_search();
    
    // Probe the table for the given key.
    // Returns true if a matching entry was found, and fills 'entry'.
    bool probe(uint64_t key, TTEntry& entry) const;
    
    // Store an entry in the table.
    void store(uint64_t key, int score, int depth, TTBound bound, Move best_move);

private:
    TTEntry* table;
    uint64_t num_entries;
    uint8_t generation;
};

extern TranspositionTable TT;
