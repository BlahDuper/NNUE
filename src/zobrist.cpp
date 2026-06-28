#include "zobrist.h"
#include <random>

namespace Zobrist {
    uint64_t psq[PIECE_NB][64];
    uint64_t enpassant[8];
    uint64_t castling[16];
    uint64_t side;
}

void init_zobrist() {
    // Use a fixed seed for deterministic, reproducible hashing
    std::mt19937_64 rng(0xBEEF2024ULL);

    for (int pc = 0; pc < PIECE_NB; ++pc)
        for (int sq = 0; sq < 64; ++sq)
            Zobrist::psq[pc][sq] = rng();

    for (int f = 0; f < 8; ++f)
        Zobrist::enpassant[f] = rng();

    for (int cr = 0; cr < 16; ++cr)
        Zobrist::castling[cr] = rng();

    Zobrist::side = rng();
}
