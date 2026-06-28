#pragma once

#include "position.h"
#include "move.h"
#include <iostream>

struct MoveList {
    Move moves[256];
    int count = 0;
    
    void add(Move m) {
        if (count < 256) {
            moves[count++] = m;
        } else {
            std::cerr << "CRITICAL ERROR: MoveList overflow!" << std::endl;
            exit(1);
        }
    }
};

bool is_attacked(const Position& pos, Square sq, Color by_color);
bool in_check(const Position& pos);
void generate_moves(const Position& pos, MoveList& list);
void generate_legal_moves(Position& pos, MoveList& list);
