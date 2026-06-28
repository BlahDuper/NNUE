#include <iostream>
#include <cstdlib>
#include "bitboard.h"
#include "nnue.h"
#include "position.h"
#include "movegen.h"
#include "search.h"
#include "zobrist.h"

int main() {
    init_zobrist();
    init_bitboards();
    if (!init_nnue("model/nnue_weights.bin")) {
        std::cerr << "Failed to load weights\n";
        return 1;
    }
    
    // Test from start position: engine gets 10 seconds, unlimited depth
    Position pos;
    pos.set_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    
    std::cerr << "=== Test 1: Start position, 10 seconds ===" << std::endl;
    SearchResult res1 = search_position(pos, 64, 10000);
    std::cerr << "Best move score: " << res1.score << std::endl << std::endl;
    
    // Test from a middlegame position
    pos.set_fen("r1bqkb1r/pppppppp/2n2n2/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 4 4");
    
    std::cerr << "=== Test 2: Italian Game, 10 seconds ===" << std::endl;
    SearchResult res2 = search_position(pos, 64, 10000);
    std::cerr << "Best move score: " << res2.score << std::endl;
    
    return 0;
}
