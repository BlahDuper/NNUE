#pragma once
#include "position.h"
#include "move.h"
#include <string>

std::string move_to_san(Position& pos, Move m);
Move san_to_move(Position& pos, const std::string& san);
