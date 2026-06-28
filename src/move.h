#pragma once

#include "types.h"

// Move representation:
// 0-5: From square (0-63)
// 6-11: To square (0-63)
// 12-13: Promotion piece type (0 = KNIGHT, 1 = BISHOP, 2 = ROOK, 3 = QUEEN)
// 14-15: Special flags (0 = quiet/capture, 1 = promotion, 2 = en passant, 3 = castling)

enum MoveType {
    NORMAL = 0,
    PROMOTION = 1 << 14,
    EN_PASSANT = 2 << 14,
    CASTLING = 3 << 14
};

class Move {
public:
    Move() : data(0) {}
    Move(uint16_t d) : data(d) {}
    Move(Square from, Square to, MoveType type = NORMAL, PieceType pt = KNIGHT) {
        data = from | (to << 6) | type | ((pt - KNIGHT) << 12);
    }
    
    Square from_sq() const { return Square(data & 0x3F); }
    Square to_sq() const { return Square((data >> 6) & 0x3F); }
    MoveType type() const { return MoveType(data & (3 << 14)); }
    PieceType promotion_type() const { return PieceType(((data >> 12) & 3) + KNIGHT); }
    
    bool is_ok() const { return data != 0; }
    uint16_t raw() const { return data; }
    
    bool operator==(const Move& m) const { return data == m.data; }
    bool operator!=(const Move& m) const { return data != m.data; }

private:
    uint16_t data;
};

const Move MOVE_NONE = Move(0);
