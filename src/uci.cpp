#include "uci.h"
#include "position.h"
#include "search.h"
#include "movegen.h"
#include "san.h"
#include "tt.h"
#include "syzygy.h"
#include <iostream>
#include <string>
#include <sstream>

// Very basic move to string converter for UCI (e.g. e2e4, e7e8q)
std::string move_to_string(Move m) {
    if (m == MOVE_NONE) return "0000";

    Square from = m.from_sq();
    Square to   = m.to_sq();

    std::string s = "";
    s += char('a' + (from & 7));
    s += char('1' + (from >> 3));
    s += char('a' + (to & 7));
    s += char('1' + (to >> 3));

    if (m.type() == PROMOTION) {
        PieceType pt = m.promotion_type();
        if      (pt == KNIGHT) s += "n";
        else if (pt == BISHOP) s += "b";
        else if (pt == ROOK)   s += "r";
        else                   s += "q";
    }
    return s;
}

// Convert a UCI string (like "e2e4") to our Move struct
Move string_to_move(Position& pos, const std::string& str) {
    MoveList list;
    generate_moves(pos, list);
    for (int i = 0; i < list.count; ++i) {
        if (move_to_string(list.moves[i]) == str)
            return list.moves[i];
    }
    return MOVE_NONE;
}

// ---------------------------------------------------------------------------
// Time management
// ---------------------------------------------------------------------------
// Given the clock situation, return how many milliseconds we should search.
// We target using roughly 1/40th of remaining time + any increment.
static int calculate_time_ms(int wtime, int btime, int winc, int binc,
                              int movestogo, Color us) {
    int my_time = (us == WHITE) ? wtime : btime;
    int my_inc  = (us == WHITE) ? winc  : binc;

    if (my_time <= 0) return 5000; // Fallback if no time given

    int moves_left = (movestogo > 0) ? movestogo : 40;
    int base_time  = my_time / moves_left + my_inc * 3 / 4;

    // Never use more than 80% of remaining time on a single move
    int max_time = my_time * 8 / 10;
    return std::min(base_time, max_time);
}

// ---------------------------------------------------------------------------
// UCI loop
// ---------------------------------------------------------------------------
void uci_loop() {
    Position pos;
    pos.set_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

    // Default TT size (can be overridden via setoption)
    int hash_mb = 32;

    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream ss(line);
        std::string token;
        ss >> token;

        if (token == "uci") {
            std::cout << "id name NNUE Engine\n";
            std::cout << "id author Hardik\n";
            std::cout << "option name Hash type spin default 32 min 1 max 4096\n";
            std::cout << "option name SyzygyPath type string default <empty>\n";
            std::cout << "option name MoveOverhead type spin default 30 min 0 max 5000\n";
            std::cout << "uciok\n";
        }
        else if (token == "setoption") {
            std::string name_tok, value_tok;
            ss >> name_tok; // "name"
            ss >> name_tok; // actual option name
            ss >> value_tok; // "value"
            ss >> value_tok; // actual value
            if (name_tok == "Hash") {
                hash_mb = std::stoi(value_tok);
                TT.resize(hash_mb);
                std::cerr << "info string Hash set to " << hash_mb << " MB\n";
            }
            else if (name_tok == "SyzygyPath") {
                Syzygy::init(value_tok);
            }
        }
        else if (token == "isready") {
            std::cout << "readyok\n";
        }
        else if (token == "ucinewgame") {
            TT.clear();
            pos.set_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
        }
        else if (token == "position") {
            ss >> token;
            if (token == "startpos") {
                pos.set_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
                ss >> token; // consume optional "moves"
            }
            else if (token == "fen") {
                std::string fen;
                while (ss >> token && token != "moves")
                    fen += token + " ";
                pos.set_fen(fen);
            }
            // Apply moves
            while (ss >> token) {
                Move m = string_to_move(pos, token);
                if (m.is_ok()) pos.make_move(m);
            }
        }
        else if (token == "go") {
            // Parse all go parameters
            int wtime = 0, btime = 0, winc = 0, binc = 0;
            int movetime = -1, depth = 64, movestogo = 0;

            while (ss >> token) {
                if      (token == "wtime")     ss >> wtime;
                else if (token == "btime")     ss >> btime;
                else if (token == "winc")      ss >> winc;
                else if (token == "binc")      ss >> binc;
                else if (token == "movestogo") ss >> movestogo;
                else if (token == "movetime")  ss >> movetime;
                else if (token == "depth")     ss >> depth;
            }

            int time_limit_ms;
            if (movetime > 0) {
                // Fixed time per move
                time_limit_ms = movetime;
            } else if (wtime > 0 || btime > 0) {
                // Clock-based time management
                time_limit_ms = calculate_time_ms(wtime, btime, winc, binc,
                                                  movestogo, pos.side_to_move());
            } else {
                // No time controls — default 10 seconds
                time_limit_ms = 10000;
            }

            SearchResult res = search_position(pos, depth, time_limit_ms);
            std::cout << "bestmove " << move_to_string(res.best_move) << "\n";
        }
        else if (token == "play") {
            // Interactive play mode with optional time-per-move argument (seconds)
            int time_limit_sec = 10;
            if (ss >> token) {
                try { time_limit_sec = std::stoi(token); } catch (...) {}
            }

            std::cout << "Entering Interactive Play Mode (type 'quit' to exit)\n";
            std::cout << "Engine will think for up to " << time_limit_sec << " second(s) per move.\n";
            std::cout << "You play White. Enter moves in SAN (e.g. e4, Nf3, O-O)\n\n";

            while (true) {
                pos.print();
                std::cout << "Your move: ";
                std::string san_move;
                std::cin >> san_move;
                if (san_move == "quit") break;

                Move m = san_to_move(pos, san_move);
                if (!m.is_ok()) {
                    std::cout << "Invalid or illegal move: " << san_move << "\n";
                    continue;
                }
                pos.make_move(m);
                pos.print();

                std::cout << "Engine is thinking...\n";
                SearchResult res = search_position(pos, 64, time_limit_sec * 1000);

                std::string engine_san = move_to_san(pos, res.best_move);
                std::cout << "Engine plays: " << engine_san << " (score: " << res.score << " cp)\n\n";
                pos.make_move(res.best_move);
            }
        }
        else if (token == "quit") {
            break;
        }
    }
}
