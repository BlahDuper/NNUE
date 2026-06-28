#include "syzygy.h"
#include "tbprobe.h"
#include <iostream>

namespace Syzygy {

    bool TB_INITIALIZED = false;
    int TB_LARGEST = 0;

    void init(const std::string& path) {
        if (TB_INITIALIZED) {
            // Already initialized. For simplicity, Fathom doesn't have a clean re-init
            // without modifying globals, but calling it again generally works or is a no-op.
            // But usually we just let it init once.
            return;
        }

        bool success = tb_init(path.c_str());
        if (success) {
            TB_INITIALIZED = true;
            TB_LARGEST = TB_LARGEST; // TB_LARGEST is a global macro/variable in fathom.
            // Actually fathom exposes an external int TB_LARGEST.
            std::cout << "info string Syzygy tablebases found. Largest piece count: " << ::TB_LARGEST << std::endl;
            TB_LARGEST = ::TB_LARGEST;
        } else {
            std::cout << "info string Failed to load Syzygy tablebases from " << path << std::endl;
        }
    }

    void free() {
        // tb_free() is declared in tbprobe.h if supported.
        // We can just rely on OS cleanup since it's an engine process.
    }

    unsigned probe_wdl(const Position& pos) {
        if (!TB_INITIALIZED || piece_count(pos) > TB_LARGEST || piece_count(pos) == 0)
            return TB_RESULT_FAILED;

        // Fathom castling rights mapping
        // Need to ensure castling rights don't interfere, though endgames usually have none.
        // Fathom expects EP square or 0.
        unsigned ep = pos.ep_square() == SQ_NONE ? 0 : pos.ep_square();
        
        // Fathom rule50: the engine's halfMoveClock
        unsigned rule50 = 0; // Not strictly needed for WDL

        // Castling rights: tb_probe_wdl_impl does not take castling rights!
        // Tablebases do not support castling. If castling is possible, return failed.
        if (pos.castling_rights() != 0) {
            return TB_RESULT_FAILED;
        }

        return tb_probe_wdl(
            pos.pieces(WHITE),
            pos.pieces(BLACK),
            pos.pieces(KING),
            pos.pieces(QUEEN),
            pos.pieces(ROOK),
            pos.pieces(BISHOP),
            pos.pieces(KNIGHT),
            pos.pieces(PAWN),
            rule50,
            pos.castling_rights(),
            ep,
            pos.side_to_move() == WHITE
        );
    }

    unsigned probe_root(const Position& pos, unsigned* results) {
        if (!TB_INITIALIZED || piece_count(pos) > TB_LARGEST)
            return TB_RESULT_FAILED;

        if (pos.castling_rights() != 0) {
            return TB_RESULT_FAILED;
        }

        unsigned ep = pos.ep_square() == SQ_NONE ? 0 : pos.ep_square();

        // Note: Engine halfMoveClock isn't publicly exposed in Position directly except 
        // through history. Let's assume 0 for DTZ root probe if not easily available.
        unsigned rule50 = pos.half_move_clock();

        return tb_probe_root(
            pos.pieces(WHITE),
            pos.pieces(BLACK),
            pos.pieces(KING),
            pos.pieces(QUEEN),
            pos.pieces(ROOK),
            pos.pieces(BISHOP),
            pos.pieces(KNIGHT),
            pos.pieces(PAWN),
            rule50,
            pos.castling_rights(),
            ep,
            pos.side_to_move() == WHITE,
            results
        );
    }

} // namespace Syzygy
