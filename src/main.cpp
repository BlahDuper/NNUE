#include <iostream>
#include "bitboard.h"
#include "nnue.h"
#include "uci.h"
#include "zobrist.h"

int main() {
    try {
        // 1. Initialize Zobrist keys (must be before any Position is created)
        init_zobrist();
        
        // 2. Initialize bitboards
        init_bitboards();
        
        // 2. Initialize NNUE
        if (!init_nnue("model/nnue_weights.bin")) {
            std::cerr << "CRITICAL ERROR: Could not load NNUE weights.\n";
            return 1;
        }
        
        // 3. Start the UCI loop
        // The engine will now wait for standard input from the chess GUI
        uci_loop();

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FATAL EXCEPTION: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "FATAL UNKNOWN EXCEPTION!" << std::endl;
        return 1;
    }
}
