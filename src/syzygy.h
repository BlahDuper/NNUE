#pragma once

#include "position.h"
#include <string>

namespace Syzygy {

    extern bool TB_INITIALIZED;
    extern int TB_LARGEST; // Usually 5 or 6 depending on installed files

    // Initialize Fathom with the given path
    void init(const std::string& path);

    // Free resources
    void free();

    // Probe the Win/Draw/Loss table (for use during the search tree)
    // Returns TB_WIN, TB_LOSS, TB_DRAW, TB_CURSED_WIN, TB_BLESSED_LOSS, or TB_RESULT_FAILED
    unsigned probe_wdl(const Position& pos);

    // Probe the Distance-to-Zero table (for use at the root to pick the best move)
    // Results are stored in the provided array.
    unsigned probe_root(const Position& pos, unsigned* results);

    // Utility: Count pieces (excluding kings) to check if we can probe
    inline int piece_count(const Position& pos) {
        return popcount(pos.pieces(WHITE) | pos.pieces(BLACK));
    }

} // namespace Syzygy
