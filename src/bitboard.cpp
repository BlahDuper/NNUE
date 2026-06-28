#include "bitboard.h"
#include <cstring>

Bitboard PawnAttacks[COLOR_NB][64];
Bitboard KnightAttacks[64];
Bitboard KingAttacks[64];

MagicEntry RookMagics[64];
MagicEntry BishopMagics[64];

// Backing attack tables. Rooks need up to 4096 entries per square (2^12),
// bishops need up to 512 (2^9). We pre-allocate the maximum needed.
static Bitboard RookAttackTable[64 * 4096];
static Bitboard BishopAttackTable[64 * 512];

// Well-known magic numbers for rooks (from Chessprogramming wiki / Stockfish)
static const Bitboard RookMagicNumbers[64] = {
    0x8a80104000800020ULL, 0x140002000100040ULL,  0x2801880a0017001ULL,  0x100081001000420ULL,
    0x200020010080420ULL,  0x3001c0002010008ULL,  0x8480008002000100ULL, 0x2080088004402900ULL,
    0x800098204000ULL,     0x2024401000200040ULL, 0x100802000801000ULL,  0x120800800801000ULL,
    0x208808088000400ULL,  0x2802200800400ULL,    0x2200800100020080ULL, 0x801000060821100ULL,
    0x80044006422000ULL,   0x100808020004000ULL,  0x12108a0010204200ULL, 0x140848010000802ULL,
    0x481828014002800ULL,  0x8094004002004100ULL, 0x4010040010010802ULL, 0x20008806104ULL,
    0x100400080208000ULL,  0x2040002120081000ULL, 0x21200680100081ULL,   0x20100080080080ULL,
    0x2000a00200410ULL,    0x20080800400ULL,      0x80088400100102ULL,   0x80004600042881ULL,
    0x4040008040800020ULL, 0x440003000200801ULL,  0x4200011004500ULL,    0x188020010100100ULL,
    0x14800401802800ULL,   0x2080040080800200ULL, 0x124080204001001ULL,  0x200046502000484ULL,
    0x480400080088020ULL,  0x1000422010034000ULL, 0x30200100110040ULL,   0x100021010009ULL,
    0x2002080100110004ULL, 0x202008004008002ULL,  0x20020004010100ULL,   0x2048440040820001ULL,
    0x101002200408200ULL,  0x40802000401080ULL,   0x4008142004410100ULL, 0x2060820c0120200ULL,
    0x1001004080100ULL,    0x20c020080040080ULL,  0x2935610830022400ULL, 0x44440041009200ULL,
    0x280001040802101ULL,  0x2100190040002085ULL, 0x80c0084100102001ULL, 0x4024081001000421ULL,
    0x20030a0244872ULL,    0x12001008414402ULL,   0x2006104900a0804ULL,  0x1004081002402ULL
};

// Well-known magic numbers for bishops
static const Bitboard BishopMagicNumbers[64] = {
    0x40040844404084ULL,   0x2004208a004208ULL,   0x10190041080202ULL,   0x108060845042010ULL,
    0x581104180800210ULL,  0x2112080446200010ULL, 0x1080820820060210ULL, 0x3c0808410220200ULL,
    0x4050404440404ULL,    0x21001420088ULL,      0x24d0080801082102ULL, 0x1020a0a020400ULL,
    0x40308200402ULL,      0x4011002100800ULL,    0x401484104104005ULL,  0x801010402020200ULL,
    0x400210c3880100ULL,   0x404022024108200ULL,  0x810018200204102ULL,  0x4002801a02003ULL,
    0x85040820080400ULL,   0x810102c808880400ULL, 0xe900410884800ULL,    0x8002020480840102ULL,
    0x220200865090201ULL,  0x2010100a02021202ULL, 0x152048408022401ULL,  0x20080002081110ULL,
    0x4001001021004000ULL, 0x800040400a011002ULL, 0xe4004081011002ULL,   0x1c004001012080ULL,
    0x8004200962a00220ULL, 0x8422100208500202ULL, 0x2000402200300c08ULL, 0x8646020080080080ULL,
    0x80020a0200100808ULL, 0x2010004880111000ULL, 0x623000a080011400ULL, 0x42008c0340209202ULL,
    0x209188240001000ULL,  0x400408a884001800ULL, 0x110400a6080400ULL,   0x1840060a44020800ULL,
    0x90080104000041ULL,   0x201011000808101ULL,  0x1a2208080504f080ULL, 0x8012020600211212ULL,
    0x500861011240000ULL,  0x180806108200800ULL,  0x4000020e01040044ULL, 0x300000261044000aULL,
    0x802241102020002ULL,  0x20906061210001ULL,   0x5a84841004010310ULL, 0x4010801011c04ULL,
    0xa010109502200ULL,    0x4a02012000ULL,       0x500201010098b028ULL, 0x8040002811040900ULL,
    0x28000010020204ULL,   0x6000020202d0240ULL,  0x8918844842082200ULL, 0x4010011029020020ULL
};

// Compute sliding attacks via ray tracing — used only during init
static Bitboard slow_rook_attacks(Square sq, Bitboard occ) {
    Bitboard attacks = 0;
    int r = sq >> 3, f = sq & 7;
    for (int nr = r+1; nr < 8; nr++) { Square s = Square(nr*8+f); attacks |= 1ULL<<s; if (occ & (1ULL<<s)) break; }
    for (int nr = r-1; nr >= 0; nr--){ Square s = Square(nr*8+f); attacks |= 1ULL<<s; if (occ & (1ULL<<s)) break; }
    for (int nf = f+1; nf < 8; nf++) { Square s = Square(r*8+nf); attacks |= 1ULL<<s; if (occ & (1ULL<<s)) break; }
    for (int nf = f-1; nf >= 0; nf--){ Square s = Square(r*8+nf); attacks |= 1ULL<<s; if (occ & (1ULL<<s)) break; }
    return attacks;
}

static Bitboard slow_bishop_attacks(Square sq, Bitboard occ) {
    Bitboard attacks = 0;
    int r = sq >> 3, f = sq & 7;
    for (int i = 1; r+i<8 && f+i<8; i++) { Square s=Square((r+i)*8+f+i); attacks|=1ULL<<s; if(occ&(1ULL<<s)) break; }
    for (int i = 1; r+i<8 && f-i>=0; i++){ Square s=Square((r+i)*8+f-i); attacks|=1ULL<<s; if(occ&(1ULL<<s)) break; }
    for (int i = 1; r-i>=0 && f+i<8; i++){ Square s=Square((r-i)*8+f+i); attacks|=1ULL<<s; if(occ&(1ULL<<s)) break; }
    for (int i = 1; r-i>=0 && f-i>=0; i++){ Square s=Square((r-i)*8+f-i); attacks|=1ULL<<s; if(occ&(1ULL<<s)) break; }
    return attacks;
}

// Compute the relevant-occupancy mask for a sliding piece
// (the squares a piece can reach excluding the board edges it hits)
static Bitboard rook_mask(Square sq) {
    int r = sq >> 3, f = sq & 7;
    Bitboard mask = 0;
    for (int nr = r+1; nr <= 6; nr++) mask |= 1ULL << (nr*8+f);
    for (int nr = r-1; nr >= 1; nr--) mask |= 1ULL << (nr*8+f);
    for (int nf = f+1; nf <= 6; nf++) mask |= 1ULL << (r*8+nf);
    for (int nf = f-1; nf >= 1; nf--) mask |= 1ULL << (r*8+nf);
    return mask;
}

static Bitboard bishop_mask(Square sq) {
    int r = sq >> 3, f = sq & 7;
    Bitboard mask = 0;
    for (int i = 1; r+i<=6 && f+i<=6; i++) mask |= 1ULL << ((r+i)*8+f+i);
    for (int i = 1; r+i<=6 && f-i>=1; i++) mask |= 1ULL << ((r+i)*8+f-i);
    for (int i = 1; r-i>=1 && f+i<=6; i++) mask |= 1ULL << ((r-i)*8+f+i);
    for (int i = 1; r-i>=1 && f-i>=1; i++) mask |= 1ULL << ((r-i)*8+f-i);
    return mask;
}

// Enumerate all subsets of a bitboard (Carry-Rippler trick)
static Bitboard subset_at(Bitboard mask, int index) {
    Bitboard subset = 0;
    while (index) {
        int bit = index & 1;
        Square lsb_sq = lsb(mask);
        if (bit) subset |= 1ULL << lsb_sq;
        mask &= mask - 1;
        index >>= 1;
    }
    return subset;
}

static void init_magic_table(bool is_rook) {
    MagicEntry* magics = is_rook ? RookMagics : BishopMagics;
    Bitboard* table = is_rook ? RookAttackTable : BishopAttackTable;
    const Bitboard* magic_numbers = is_rook ? RookMagicNumbers : BishopMagicNumbers;

    int offset = 0;
    for (int sq = 0; sq < 64; sq++) {
        Bitboard mask = is_rook ? rook_mask(Square(sq)) : bishop_mask(Square(sq));
        int bits = popcount(mask);
        int size = 1 << bits; // number of occupancy subsets
        int shift = 64 - bits;

        magics[sq].mask = mask;
        magics[sq].magic = magic_numbers[sq];
        magics[sq].shift = shift;
        magics[sq].attacks = table + offset;

        // Populate the attack table for every possible occupancy subset
        for (int i = 0; i < size; i++) {
            Bitboard occ = subset_at(mask, i);
            Bitboard attacks = is_rook ? slow_rook_attacks(Square(sq), occ)
                                       : slow_bishop_attacks(Square(sq), occ);
            uint64_t index = (occ * magic_numbers[sq]) >> shift;
            magics[sq].attacks[index] = attacks;
        }

        offset += size;
    }
}

void init_bitboards() {
    for (int sq = 0; sq < 64; sq++) {
        int r = sq >> 3;
        int f = sq & 7;

        // Pawn attacks
        PawnAttacks[WHITE][sq] = 0;
        if (r < 7 && f > 0) PawnAttacks[WHITE][sq] |= square_bb(Square(sq + 7));
        if (r < 7 && f < 7) PawnAttacks[WHITE][sq] |= square_bb(Square(sq + 9));

        PawnAttacks[BLACK][sq] = 0;
        if (r > 0 && f > 0) PawnAttacks[BLACK][sq] |= square_bb(Square(sq - 9));
        if (r > 0 && f < 7) PawnAttacks[BLACK][sq] |= square_bb(Square(sq - 7));

        // Knight attacks
        KnightAttacks[sq] = 0;
        const int kn[8][2] = {{2,1},{1,2},{-1,2},{-2,1},{-2,-1},{-1,-2},{1,-2},{2,-1}};
        for (auto& d : kn) {
            int nr = r + d[0], nf = f + d[1];
            if (nr >= 0 && nr <= 7 && nf >= 0 && nf <= 7)
                KnightAttacks[sq] |= square_bb(make_square(static_cast<File>(nf), static_cast<Rank>(nr)));
        }

        // King attacks
        KingAttacks[sq] = 0;
        const int kg[8][2] = {{1,0},{1,1},{0,1},{-1,1},{-1,0},{-1,-1},{0,-1},{1,-1}};
        for (auto& d : kg) {
            int nr = r + d[0], nf = f + d[1];
            if (nr >= 0 && nr <= 7 && nf >= 0 && nf <= 7)
                KingAttacks[sq] |= square_bb(make_square(static_cast<File>(nf), static_cast<Rank>(nr)));
        }
    }

    // Initialize magic bitboard tables
    init_magic_table(true);  // Rooks
    init_magic_table(false); // Bishops
}

void print_bitboard(Bitboard b) {
    for (int r = 7; r >= 0; --r) {
        for (int f = 0; f < 8; ++f) {
            Square sq = make_square(static_cast<File>(f), static_cast<Rank>(r));
            std::cout << (test_bit(b, sq) ? "1 " : ". ");
        }
        std::cout << "\n";
    }
    std::cout << "\n";
}
