#include "san.h"
#include "movegen.h"
#include <iostream>

std::string move_to_san(Position& pos, Move m) {
    if (m == MOVE_NONE) return "0000";
    if (m.type() == CASTLING) {
        if (m.to_sq() == SQ_G1 || m.to_sq() == SQ_G8) return "O-O";
        return "O-O-O";
    }

    Piece pc = pos.piece_on(m.from_sq());
    PieceType pt = type_of(pc);
    bool capture = (pos.piece_on(m.to_sq()) != NO_PIECE) || (m.type() == EN_PASSANT);
    
    std::string san = "";
    if (pt != PAWN) {
        if (pt == KNIGHT) san += "N";
        else if (pt == BISHOP) san += "B";
        else if (pt == ROOK) san += "R";
        else if (pt == QUEEN) san += "Q";
        else if (pt == KING) san += "K";
        
        // Disambiguation
        MoveList list;
        generate_legal_moves(pos, list);
        bool needs_file = false;
        bool needs_rank = false;
        int conflicts = 0;
        for (int i = 0; i < list.count; ++i) {
            Move alt = list.moves[i];
            if (alt.from_sq() != m.from_sq() && alt.to_sq() == m.to_sq() && type_of(pos.piece_on(alt.from_sq())) == pt) {
                conflicts++;
                if ((alt.from_sq() & 7) == (m.from_sq() & 7)) needs_rank = true;
                else needs_file = true;
            }
        }
        if (conflicts > 0) {
            if (!needs_rank) san += char('a' + (m.from_sq() & 7));
            else if (!needs_file) san += char('1' + (m.from_sq() >> 3));
            else {
                san += char('a' + (m.from_sq() & 7));
                san += char('1' + (m.from_sq() >> 3));
            }
        }
    } else {
        if (capture) {
            san += char('a' + (m.from_sq() & 7));
        }
    }
    
    if (capture) san += "x";
    
    san += char('a' + (m.to_sq() & 7));
    san += char('1' + (m.to_sq() >> 3));
    
    if (m.type() == PROMOTION) {
        san += "=";
        PieceType pro_pt = m.promotion_type();
        if (pro_pt == KNIGHT) san += "N";
        else if (pro_pt == BISHOP) san += "B";
        else if (pro_pt == ROOK) san += "R";
        else if (pro_pt == QUEEN) san += "Q";
    }
    
    pos.make_move(m);
    if (in_check(pos)) {
        MoveList resp;
        generate_legal_moves(pos, resp);
        if (resp.count == 0) san += "#";
        else san += "+";
    }
    pos.undo_move(m);
    
    return san;
}

Move san_to_move(Position& pos, const std::string& san) {
    MoveList list;
    generate_legal_moves(pos, list);
    
    // First try strict SAN matching
    for (int i = 0; i < list.count; ++i) {
        if (move_to_san(pos, list.moves[i]) == san) {
            return list.moves[i];
        }
    }
    
    // Fallback: strip '+' or '#' from user input and try again
    std::string stripped = san;
    if (!stripped.empty() && (stripped.back() == '+' || stripped.back() == '#')) {
        stripped.pop_back();
        for (int i = 0; i < list.count; ++i) {
            std::string generated = move_to_san(pos, list.moves[i]);
            if (!generated.empty() && (generated.back() == '+' || generated.back() == '#')) {
                generated.pop_back();
            }
            if (generated == stripped) {
                return list.moves[i];
            }
        }
    }
    
    return MOVE_NONE;
}
