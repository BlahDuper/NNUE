#include "position.h"
#include <iostream>
#include <sstream>
#include <cassert>
#include <cstdlib>
#include <cstring>

Position::Position() {
    clear();
}

void Position::clear() {
    for (int i = 0; i < 64; ++i) board[i] = NO_PIECE;
    for (int i = 0; i < PIECE_TYPE_NB; ++i) byTypeBB[i] = 0;
    for (int i = 0; i < COLOR_NB; ++i) byColorBB[i] = 0;

    sideToMove    = WHITE;
    epSquare      = SQ_NONE;
    castlingRights = 0;
    halfMoveClock  = 0;
    fullMoveNumber = 1;
    hashKey        = 0;
    history.clear();
    // Accumulator will be fully rebuilt by set_fen after all pieces are placed
}

// -------------------------------------------------------------------------
// Low-level put/remove — update bitboards, Zobrist hash, but NOT accumulator.
// Used during set_fen (where we do a full acc refresh at the end) and in
// undo_move (where we restore the saved accumulator snapshot instead of
// trying to undo individual feature changes).
// -------------------------------------------------------------------------
void Position::put_piece(Piece pc, Square s) {
    assert(pc != NO_PIECE);
    assert(s >= 0 && s < 64);
    assert(board[s] == NO_PIECE);
    board[s] = pc;
    set_bit(byTypeBB[type_of(pc)], s);
    set_bit(byColorBB[color_of(pc)], s);
    hashKey ^= Zobrist::psq[pc][s];
}

void Position::remove_piece(Square s) {
    assert(s >= 0 && s < 64);
    Piece pc = board[s];
    if (pc != NO_PIECE) {
        clear_bit(byTypeBB[type_of(pc)], s);
        clear_bit(byColorBB[color_of(pc)], s);
        board[s] = NO_PIECE;
        hashKey ^= Zobrist::psq[pc][s];
    }
}

// -------------------------------------------------------------------------
// Accumulator-aware put/remove — also updates the live NNUE accumulator.
// Used in make_move for incremental updates.
// -------------------------------------------------------------------------
void Position::put_piece_with_acc(Piece pc, Square s) {
    put_piece(pc, s);
    accumulator_add_feature(currentAcc, make_feature_index(type_of(pc), color_of(pc), s));
}

void Position::remove_piece_with_acc(Square s) {
    Piece pc = board[s];
    if (pc != NO_PIECE) {
        accumulator_sub_feature(currentAcc, make_feature_index(type_of(pc), color_of(pc), s));
        remove_piece(s);
    }
}

// Helper to rebuild the accumulator from the current board state
static void do_refresh(Accumulator& acc, const Position& pos) {
    // Build a flat array of bitboards matching the refresh_accumulator signature
    // Index: color * 6 + piece_type
    uint64_t piece_bbs[12];
    for (int c = WHITE; c <= BLACK; c++)
        for (int pt = PAWN; pt <= KING; pt++)
            piece_bbs[c * 6 + pt] = pos.pieces(PieceType(pt), Color(c));
    refresh_accumulator(acc, nullptr, nullptr, piece_bbs, 12);
}

void Position::set_fen(const std::string& fen) {
    clear();
    std::istringstream ss(fen);
    std::string token;

    // 1. Piece placement
    ss >> token;
    int sq = 56; // A8
    for (char c : token) {
        if (c >= '1' && c <= '8') {
            sq += (c - '0');
        } else if (c == '/') {
            sq -= 16;
        } else {
            Piece pc = NO_PIECE;
            switch (c) {
                case 'P': pc = W_PAWN;   break; case 'N': pc = W_KNIGHT; break;
                case 'B': pc = W_BISHOP; break; case 'R': pc = W_ROOK;   break;
                case 'Q': pc = W_QUEEN;  break; case 'K': pc = W_KING;   break;
                case 'p': pc = B_PAWN;   break; case 'n': pc = B_KNIGHT; break;
                case 'b': pc = B_BISHOP; break; case 'r': pc = B_ROOK;   break;
                case 'q': pc = B_QUEEN;  break; case 'k': pc = B_KING;   break;
            }
            if (pc != NO_PIECE) { put_piece(pc, Square(sq)); sq++; }
        }
    }

    // 2. Active color
    ss >> token;
    sideToMove = (token == "w") ? WHITE : BLACK;

    // 3. Castling availability
    ss >> token;
    if (token != "-") {
        for (char c : token) {
            if (c == 'K') castlingRights |= 1;
            if (c == 'Q') castlingRights |= 2;
            if (c == 'k') castlingRights |= 4;
            if (c == 'q') castlingRights |= 8;
        }
    }

    // 4. En passant target square
    ss >> token;
    if (token != "-") {
        File f = File(token[0] - 'a');
        Rank r = Rank(token[1] - '1');
        epSquare = make_square(f, r);
    }

    // 5 & 6. Clocks
    ss >> halfMoveClock >> fullMoveNumber;

    // Finish the hash (put_piece already XOR'd piece keys; add the meta keys now)
    if (sideToMove == BLACK)  hashKey ^= Zobrist::side;
    hashKey ^= Zobrist::castling[castlingRights];
    if (epSquare != SQ_NONE) hashKey ^= Zobrist::enpassant[epSquare & 7];

    // Full accumulator refresh — the only time we do this
    do_refresh(currentAcc, *this);
}

void Position::print() const {
    const char piece_chars[] = "PNBRQK..pnbrqk...";
    for (int r = 7; r >= 0; --r) {
        std::cout << r + 1 << "  ";
        for (int f = 0; f < 8; ++f) {
            Square sq = make_square(static_cast<File>(f), static_cast<Rank>(r));
            Piece  pc = board[sq];
            std::cout << piece_chars[pc == NO_PIECE ? 16 : pc] << " ";
        }
        std::cout << "\n";
    }
    std::cout << "   a b c d e f g h\n\n";
    std::cout << "Side to move: " << (sideToMove == WHITE ? "White" : "Black") << "\n";
}

// -------------------------------------------------------------------------
// make_move — incremental updates to hash, bitboards, and accumulator
// -------------------------------------------------------------------------
void Position::make_move(Move m) {
    Square from = m.from_sq();
    Square to   = m.to_sq();
    Piece  piece    = piece_on(from);
    Piece  captured = piece_on(to);
    MoveType type   = m.type();

    // For EP, the captured pawn is not on 'to'
    Square ep_cap_sq = SQ_NONE;
    if (type == EN_PASSANT) {
        ep_cap_sq = make_square(File(to & 7), Rank((from >> 3) & 7));
        captured  = piece_on(ep_cap_sq);
    }

    // Save state snapshot (includes full accumulator for O(1) undo)
    StateInfo state;
    state.capturedPiece  = captured;
    state.epSquare       = epSquare;
    state.castlingRights = castlingRights;
    state.halfMoveClock  = halfMoveClock;
    state.key            = hashKey;
    state.acc            = currentAcc;  // copy accumulator — fast (256 int16s)
    history.push_back(state);

    // Update hash — remove old EP and castling keys
    if (epSquare != SQ_NONE) hashKey ^= Zobrist::enpassant[epSquare & 7];
    hashKey ^= Zobrist::castling[castlingRights];

    // Remove captured piece
    if (type == EN_PASSANT) {
        remove_piece_with_acc(ep_cap_sq);
    } else if (captured != NO_PIECE) {
        remove_piece_with_acc(to);
    }

    // Move the piece
    remove_piece_with_acc(from);
    if (type == PROMOTION) {
        piece = make_piece(sideToMove, m.promotion_type());
    }
    put_piece_with_acc(piece, to);

    // Handle castling rook relocation
    if (type == CASTLING) {
        if      (to == SQ_G1) { remove_piece_with_acc(SQ_H1); put_piece_with_acc(W_ROOK, SQ_F1); }
        else if (to == SQ_C1) { remove_piece_with_acc(SQ_A1); put_piece_with_acc(W_ROOK, SQ_D1); }
        else if (to == SQ_G8) { remove_piece_with_acc(SQ_H8); put_piece_with_acc(B_ROOK, SQ_F8); }
        else if (to == SQ_C8) { remove_piece_with_acc(SQ_A8); put_piece_with_acc(B_ROOK, SQ_D8); }
    }

    // Update castling rights
    if      (piece == W_KING) castlingRights &= ~3;
    else if (piece == B_KING) castlingRights &= ~12;
    if (from == SQ_H1 || to == SQ_H1) castlingRights &= ~1;
    if (from == SQ_A1 || to == SQ_A1) castlingRights &= ~2;
    if (from == SQ_H8 || to == SQ_H8) castlingRights &= ~4;
    if (from == SQ_A8 || to == SQ_A8) castlingRights &= ~8;

    // New EP square
    epSquare = SQ_NONE;
    if (type_of(piece) == PAWN && std::abs(int(to) - int(from)) == 16)
        epSquare = Square((from + to) / 2);

    // Re-add updated hash keys
    hashKey ^= Zobrist::castling[castlingRights];
    if (epSquare != SQ_NONE) hashKey ^= Zobrist::enpassant[epSquare & 7];

    // Halfmove clock
    if (type_of(piece) == PAWN || captured != NO_PIECE) halfMoveClock = 0;
    else halfMoveClock++;

    if (sideToMove == BLACK) fullMoveNumber++;
    sideToMove = ~sideToMove;
    hashKey ^= Zobrist::side;
}

// -------------------------------------------------------------------------
// undo_move — restore saved accumulator snapshot (no feature subtraction needed)
// -------------------------------------------------------------------------
void Position::undo_move(Move m) {
    sideToMove = ~sideToMove;
    if (sideToMove == BLACK) fullMoveNumber--;

    StateInfo state = history.back();
    history.pop_back();

    Square from = m.from_sq();
    Square to   = m.to_sq();
    MoveType type = m.type();

    Piece piece = piece_on(to);
    if (type == PROMOTION) piece = make_piece(sideToMove, PAWN);

    // Restore board state using the non-acc variants (we restore acc from snapshot)
    remove_piece(to);
    put_piece(piece, from);

    if (type == CASTLING) {
        if      (to == SQ_G1) { remove_piece(SQ_F1); put_piece(W_ROOK, SQ_H1); }
        else if (to == SQ_C1) { remove_piece(SQ_D1); put_piece(W_ROOK, SQ_A1); }
        else if (to == SQ_G8) { remove_piece(SQ_F8); put_piece(B_ROOK, SQ_H8); }
        else if (to == SQ_C8) { remove_piece(SQ_D8); put_piece(B_ROOK, SQ_A8); }
    }

    if (type == EN_PASSANT) {
        Square cap_sq = make_square(File(to & 7), Rank((from >> 3) & 7));
        put_piece(state.capturedPiece, cap_sq);
    } else if (state.capturedPiece != NO_PIECE) {
        put_piece(state.capturedPiece, to);
    }

    epSquare       = state.epSquare;
    castlingRights = state.castlingRights;
    halfMoveClock  = state.halfMoveClock;
    hashKey        = state.key;
    currentAcc     = state.acc;  // O(1) accumulator restore
}

// -------------------------------------------------------------------------
// Null move — just flip the side, no piece moves
// -------------------------------------------------------------------------
void Position::make_null_move() {
    StateInfo state;
    state.capturedPiece  = NO_PIECE;
    state.epSquare       = epSquare;
    state.castlingRights = castlingRights;
    state.halfMoveClock  = halfMoveClock;
    state.key            = hashKey;
    state.acc            = currentAcc;
    history.push_back(state);

    if (epSquare != SQ_NONE) {
        hashKey  ^= Zobrist::enpassant[epSquare & 7];
        epSquare  = SQ_NONE;
    }
    sideToMove = ~sideToMove;
    hashKey ^= Zobrist::side;
}

void Position::undo_null_move() {
    StateInfo state = history.back();
    history.pop_back();

    sideToMove     = ~sideToMove;
    epSquare       = state.epSquare;
    castlingRights = state.castlingRights;
    halfMoveClock  = state.halfMoveClock;
    hashKey        = state.key;
    currentAcc     = state.acc;
}
