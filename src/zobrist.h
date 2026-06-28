#pragma once

#include "types.h"
#include <cstdint>

// Zobrist hashing keys for incremental position hashing.
// Initialized once at startup via init_zobrist().

namespace Zobrist {
    extern uint64_t psq[PIECE_NB][64];   // piece on square
    extern uint64_t enpassant[8];        // en-passant file
    extern uint64_t castling[16];        // castling rights (4-bit index)
    extern uint64_t side;               // side to move
}

void init_zobrist();
