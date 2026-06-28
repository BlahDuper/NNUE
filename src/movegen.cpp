#include "movegen.h"

bool is_attacked(const Position& pos, Square sq, Color by_color);

// Helper to add pawn promotions
void add_promotions(Square from, Square to, MoveList& list) {
    list.add(Move(from, to, PROMOTION, QUEEN));
    list.add(Move(from, to, PROMOTION, ROOK));
    list.add(Move(from, to, PROMOTION, BISHOP));
    list.add(Move(from, to, PROMOTION, KNIGHT));
}

void generate_moves(const Position& pos, MoveList& list) {
    Color us = pos.side_to_move();
    Color them = ~us;
    Bitboard ourPieces = pos.pieces(us);
    Bitboard theirPieces = pos.pieces(them);
    Bitboard allPieces = ourPieces | theirPieces;
    Bitboard emptySquares = ~allPieces;

    // Pawns
    Bitboard pawns = pos.pieces(PAWN, us);
    int up = (us == WHITE) ? 8 : -8;
    int upRight = (us == WHITE) ? 9 : -7;
    int upLeft = (us == WHITE) ? 7 : -9;
    Bitboard rank3 = (us == WHITE) ? Rank3BB : Rank6BB;
    Bitboard promotion_rank = (us == WHITE) ? Rank8BB : Rank1BB;

    // Single push
    Bitboard push1 = (us == WHITE) ? (pawns << 8) & emptySquares : (pawns >> 8) & emptySquares;
    Bitboard prom1 = push1 & promotion_rank;
    Bitboard push1_no_prom = push1 & ~promotion_rank;

    while (push1_no_prom) {
        Square to = pop_lsb(push1_no_prom);
        list.add(Move(Square(to - up), to));
    }
    while (prom1) {
        Square to = pop_lsb(prom1);
        add_promotions(Square(to - up), to, list);
    }

    // Double push
    Bitboard push2 = (us == WHITE) ? ((push1 & rank3) << 8) & emptySquares : ((push1 & rank3) >> 8) & emptySquares;
    while (push2) {
        Square to = pop_lsb(push2);
        list.add(Move(Square(to - 2 * up), to));
    }

    // Captures
    Bitboard capRight = (us == WHITE) ? (pawns << 9) & theirPieces & ~FileABB : (pawns >> 7) & theirPieces & ~FileABB;
    Bitboard capRightProm = capRight & promotion_rank;
    Bitboard capRightNoProm = capRight & ~promotion_rank;

    while (capRightNoProm) {
        Square to = pop_lsb(capRightNoProm);
        list.add(Move(Square(to - upRight), to));
    }
    while (capRightProm) {
        Square to = pop_lsb(capRightProm);
        add_promotions(Square(to - upRight), to, list);
    }

    Bitboard capLeft = (us == WHITE) ? (pawns << 7) & theirPieces & ~FileHBB : (pawns >> 9) & theirPieces & ~FileHBB;
    Bitboard capLeftProm = capLeft & promotion_rank;
    Bitboard capLeftNoProm = capLeft & ~promotion_rank;

    while (capLeftNoProm) {
        Square to = pop_lsb(capLeftNoProm);
        list.add(Move(Square(to - upLeft), to));
    }
    while (capLeftProm) {
        Square to = pop_lsb(capLeftProm);
        add_promotions(Square(to - upLeft), to, list);
    }

    // Knights
    Bitboard knights = pos.pieces(KNIGHT, us);
    while (knights) {
        Square from = pop_lsb(knights);
        Bitboard attacks = KnightAttacks[from] & ~ourPieces;
        while (attacks) {
            Square to = pop_lsb(attacks);
            list.add(Move(from, to));
        }
    }

    // Bishops
    Bitboard bishops = pos.pieces(BISHOP, us);
    while (bishops) {
        Square from = pop_lsb(bishops);
        Bitboard attacks = bishop_attacks(from, allPieces) & ~ourPieces;
        while (attacks) {
            Square to = pop_lsb(attacks);
            list.add(Move(from, to));
        }
    }

    // Rooks
    Bitboard rooks = pos.pieces(ROOK, us);
    while (rooks) {
        Square from = pop_lsb(rooks);
        Bitboard attacks = rook_attacks(from, allPieces) & ~ourPieces;
        while (attacks) {
            Square to = pop_lsb(attacks);
            list.add(Move(from, to));
        }
    }

    // Queens
    Bitboard queens = pos.pieces(QUEEN, us);
    while (queens) {
        Square from = pop_lsb(queens);
        Bitboard attacks = queen_attacks(from, allPieces) & ~ourPieces;
        while (attacks) {
            Square to = pop_lsb(attacks);
            list.add(Move(from, to));
        }
    }

    // King
    Bitboard king = pos.pieces(KING, us);
    if (king) {
        Square from = pop_lsb(king);
        Bitboard attacks = KingAttacks[from] & ~ourPieces;
        while (attacks) {
            Square to = pop_lsb(attacks);
            list.add(Move(from, to));
        }
    }
    // Castling
    if (us == WHITE) {
        if (pos.castling_rights() & 1) { // WK
            if (pos.piece_on(SQ_F1) == NO_PIECE && pos.piece_on(SQ_G1) == NO_PIECE) {
                if (!is_attacked(pos, SQ_E1, BLACK) && !is_attacked(pos, SQ_F1, BLACK) && !is_attacked(pos, SQ_G1, BLACK)) {
                    list.add(Move(SQ_E1, SQ_G1, CASTLING, KNIGHT));
                }
            }
        }
        if (pos.castling_rights() & 2) { // WQ
            if (pos.piece_on(SQ_D1) == NO_PIECE && pos.piece_on(SQ_C1) == NO_PIECE && pos.piece_on(SQ_B1) == NO_PIECE) {
                if (!is_attacked(pos, SQ_E1, BLACK) && !is_attacked(pos, SQ_D1, BLACK) && !is_attacked(pos, SQ_C1, BLACK)) {
                    list.add(Move(SQ_E1, SQ_C1, CASTLING, KNIGHT));
                }
            }
        }
    } else {
        if (pos.castling_rights() & 4) { // BK
            if (pos.piece_on(SQ_F8) == NO_PIECE && pos.piece_on(SQ_G8) == NO_PIECE) {
                if (!is_attacked(pos, SQ_E8, WHITE) && !is_attacked(pos, SQ_F8, WHITE) && !is_attacked(pos, SQ_G8, WHITE)) {
                    list.add(Move(SQ_E8, SQ_G8, CASTLING, KNIGHT));
                }
            }
        }
        if (pos.castling_rights() & 8) { // BQ
            if (pos.piece_on(SQ_D8) == NO_PIECE && pos.piece_on(SQ_C8) == NO_PIECE && pos.piece_on(SQ_B8) == NO_PIECE) {
                if (!is_attacked(pos, SQ_E8, WHITE) && !is_attacked(pos, SQ_D8, WHITE) && !is_attacked(pos, SQ_C8, WHITE)) {
                    list.add(Move(SQ_E8, SQ_C8, CASTLING, KNIGHT));
                }
            }
        }
    }
    
    // En Passant
    Square ep_sq = pos.ep_square();
    if (ep_sq != SQ_NONE) {
        if (us == WHITE) {
            if ((ep_sq % 8) > 0 && pos.piece_on(Square(ep_sq - 9)) == W_PAWN) list.add(Move(Square(ep_sq - 9), ep_sq, EN_PASSANT));
            if ((ep_sq % 8) < 7 && pos.piece_on(Square(ep_sq - 7)) == W_PAWN) list.add(Move(Square(ep_sq - 7), ep_sq, EN_PASSANT));
        } else {
            if ((ep_sq % 8) > 0 && pos.piece_on(Square(ep_sq + 7)) == B_PAWN) list.add(Move(Square(ep_sq + 7), ep_sq, EN_PASSANT));
            if ((ep_sq % 8) < 7 && pos.piece_on(Square(ep_sq + 9)) == B_PAWN) list.add(Move(Square(ep_sq + 9), ep_sq, EN_PASSANT));
        }
    }
}

bool is_attacked(const Position& pos, Square sq, Color by_color) {
    Bitboard allPieces = pos.pieces(WHITE) | pos.pieces(BLACK);
    
    // Pawns
    Bitboard pawns = pos.pieces(PAWN, by_color);
    if (by_color == WHITE) {
        if (((1ULL << sq) >> 7) & pawns & ~FileABB) return true;
        if (((1ULL << sq) >> 9) & pawns & ~FileHBB) return true;
    } else {
        if (((1ULL << sq) << 9) & pawns & ~FileABB) return true;
        if (((1ULL << sq) << 7) & pawns & ~FileHBB) return true;
    }
    
    // Knights
    if (KnightAttacks[sq] & pos.pieces(KNIGHT, by_color)) return true;
    
    // King
    if (KingAttacks[sq] & pos.pieces(KING, by_color)) return true;
    
    // Bishops / Queens
    Bitboard bq = pos.pieces(BISHOP, by_color) | pos.pieces(QUEEN, by_color);
    if (bishop_attacks(sq, allPieces) & bq) return true;
    
    // Rooks / Queens
    Bitboard rq = pos.pieces(ROOK, by_color) | pos.pieces(QUEEN, by_color);
    if (rook_attacks(sq, allPieces) & rq) return true;
    
    return false;
}

bool in_check(const Position& pos) {
    Color us = pos.side_to_move();
    Bitboard king = pos.pieces(KING, us);
    if (!king) return false; // Should only happen if board is malformed
    Square ksq = Square(__builtin_ctzll(king));
    return is_attacked(pos, ksq, ~us);
}

void generate_legal_moves(Position& pos, MoveList& list) {
    MoveList pseudo;
    generate_moves(pos, pseudo);
    
    Color us = pos.side_to_move();
    
    for (int i = 0; i < pseudo.count; ++i) {
        Move m = pseudo.moves[i];
        pos.make_move(m);
        // After make_move, the side to move is flipped. We check if the previous side's king is in check.
        // But in_check() uses pos.side_to_move(). Wait, we need to check if 'us' king is attacked!
        // pos.side_to_move() is now ~us. So we check if the king of ~pos.side_to_move() is attacked.
        Bitboard king = pos.pieces(KING, us);
        bool legal = true;
        if (king) {
            Square ksq = Square(__builtin_ctzll(king));
            if (is_attacked(pos, ksq, ~us)) {
                legal = false;
            }
        }
        pos.undo_move(m);
        
        if (legal) {
            list.add(m);
        }
    }
}
