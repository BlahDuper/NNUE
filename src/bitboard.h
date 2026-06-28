#pragma once

#include "types.h"
#include <iostream>

#ifdef _MSC_VER
#include <intrin.h>
#endif

inline Bitboard square_bb(Square s) {
    return 1ULL << s;
}

inline void set_bit(Bitboard& b, Square s) {
    b |= square_bb(s);
}

inline void clear_bit(Bitboard& b, Square s) {
    b &= ~square_bb(s);
}

inline bool test_bit(Bitboard b, Square s) {
    return (b & square_bb(s)) != 0;
}

inline int popcount(Bitboard b) {
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_popcountll(b);
#elif defined(_MSC_VER)
#ifdef _WIN64
    return static_cast<int>(__popcnt64(b));
#else
    return static_cast<int>(__popcnt(static_cast<uint32_t>(b)) + __popcnt(static_cast<uint32_t>(b >> 32)));
#endif
#else
    int count = 0;
    while (b) { count++; b &= b - 1; }
    return count;
#endif
}

inline Square lsb(Bitboard b) {
#if defined(__GNUC__) || defined(__clang__)
    return Square(__builtin_ctzll(b));
#elif defined(_MSC_VER)
    unsigned long idx;
#ifdef _WIN64
    _BitScanForward64(&idx, b);
#else
    if (_BitScanForward(&idx, static_cast<uint32_t>(b))) return Square(idx);
    _BitScanForward(&idx, static_cast<uint32_t>(b >> 32));
    return Square(idx + 32);
#endif
    return Square(idx);
#else
    if (b == 0) return SQ_NONE;
    int s = 0;
    while ((b & 1) == 0) { b >>= 1; s++; }
    return Square(s);
#endif
}

inline Square pop_lsb(Bitboard& b) {
    Square s = lsb(b);
    b &= b - 1;
    return s;
}

// Pre-computed attack tables (non-sliding pieces)
extern Bitboard PawnAttacks[COLOR_NB][64];
extern Bitboard KnightAttacks[64];
extern Bitboard KingAttacks[64];

// Magic Bitboard structures for sliding pieces
struct MagicEntry {
    Bitboard mask;      // Relevant occupancy bits for this square
    Bitboard magic;     // Magic multiplier
    Bitboard* attacks;  // Pointer into the attack table
    int shift;          // Right-shift amount
};

extern MagicEntry RookMagics[64];
extern MagicEntry BishopMagics[64];

// Attack lookup using magic bitboards — O(1) per call
inline Bitboard rook_attacks(Square sq, Bitboard occupancy) {
    const MagicEntry& m = RookMagics[sq];
    return m.attacks[((occupancy & m.mask) * m.magic) >> m.shift];
}

inline Bitboard bishop_attacks(Square sq, Bitboard occupancy) {
    const MagicEntry& m = BishopMagics[sq];
    return m.attacks[((occupancy & m.mask) * m.magic) >> m.shift];
}

inline Bitboard queen_attacks(Square sq, Bitboard occupancy) {
    return rook_attacks(sq, occupancy) | bishop_attacks(sq, occupancy);
}

void init_bitboards();
void print_bitboard(Bitboard b);
