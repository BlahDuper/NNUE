#pragma once
#include "position.h"
#include "move.h"

// Define a simple structure to hold the best move found
struct SearchResult {
    Move best_move;
    int score;
};

// Search with a maximum depth and time limit in milliseconds.
// The engine will always return the best move from the deepest completed depth.
SearchResult search_position(Position& pos, int depth, int time_limit_ms = 10000);
