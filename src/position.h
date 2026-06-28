#pragma once

#include "bitboard.h"
#include "move.h"
#include "zobrist.h"
#include "nnue.h"
#include <string>
#include <vector>
#include <cstdint>

struct StateInfo {
    Piece    capturedPiece;
    Square   epSquare;
    int      castlingRights;
    int      halfMoveClock;
    uint64_t key;
    Accumulator acc;  // NNUE accumulator snapshot for fast undo
};

class Position {
public:
    Position();
    void set_fen(const std::string& fen);

    Bitboard pieces(PieceType pt) const { return byTypeBB[pt]; }
    Bitboard pieces(Color c) const { return byColorBB[c]; }
    Bitboard pieces(PieceType pt, Color c) const { return byTypeBB[pt] & byColorBB[c]; }

    Piece  piece_on(Square s) const { return board[s]; }
    Color  side_to_move() const { return sideToMove; }
    Square ep_square() const { return epSquare; }
    int    castling_rights() const { return castlingRights; }
    uint64_t key() const { return hashKey; }
    int half_move_clock() const { return halfMoveClock; }

    // Access the current live accumulator for fast NNUE evaluation
    const Accumulator& accumulator() const { return currentAcc; }

    void make_move(Move m);
    void undo_move(Move m);

    // Null move support for Null Move Pruning
    void make_null_move();
    void undo_null_move();

    void print() const;

private:
    Piece    board[64];
    Bitboard byTypeBB[PIECE_TYPE_NB];
    Bitboard byColorBB[COLOR_NB];

    Color  sideToMove;
    Square epSquare;
    int    castlingRights;
    int    halfMoveClock;
    int    fullMoveNumber;
    uint64_t hashKey;

    Accumulator currentAcc;  // Live accumulator, incrementally updated

    std::vector<StateInfo> history;

    void clear();
    void put_piece(Piece pc, Square s);
    void remove_piece(Square s);

    // Low-level piece manipulation that also updates the accumulator
    void put_piece_with_acc(Piece pc, Square s);
    void remove_piece_with_acc(Square s);
};
