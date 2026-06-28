#include "search.h"
#include "movegen.h"
#include "evaluate.h"
#include "nnue.h"
#include "tt.h"
#include "syzygy.h"
#include "tbprobe.h"
#include <algorithm>
#include <iostream>
#include <chrono>
#include <cstring>

const int INF = 30000;
const int MATE_SCORE = 29000;

// Time management globals
static std::chrono::steady_clock::time_point search_start_time;
static int search_time_limit;
static bool search_aborted;
static int nodes_searched;

// Killer moves: two per ply (moves that caused beta cutoffs but aren't captures)
static Move killers[128][2];

// History heuristic: indexed by [side][from][to]
static int history_table[2][64][64];

// Check the clock every 4096 nodes to avoid overhead
static void check_time() {
    if ((nodes_searched & 4095) == 0) {
        auto now = std::chrono::steady_clock::now();
        int elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - search_start_time).count();
        if (elapsed >= search_time_limit) {
            search_aborted = true;
        }
    }
}

// Piece values for MVV-LVA and SEE-like ordering
static const int piece_values[PIECE_TYPE_NB] = {100, 300, 320, 500, 900, 10000};

// Move scoring for ordering.
// Priority: TT best move > Captures (MVV-LVA) > Killers > History heuristic
static int move_score(const Position& pos, Move m, Move tt_move, int ply) {
    // TT best move gets searched first
    if (m == tt_move) return 200000;
    
    Piece captured = pos.piece_on(m.to_sq());
    MoveType type = m.type();
    
    // En passant captures
    if (type == EN_PASSANT) return 100000 + 100;
    
    // Captures: MVV-LVA
    if (captured != NO_PIECE) {
        int victim = piece_values[type_of(captured)];
        int attacker = piece_values[type_of(pos.piece_on(m.from_sq()))];
        return 100000 + victim * 10 - attacker;
    }
    
    // Promotions
    if (type == PROMOTION) return 95000 + piece_values[m.promotion_type()];
    
    // Killer moves
    if (ply < 128) {
        if (m == killers[ply][0]) return 90000;
        if (m == killers[ply][1]) return 89000;
    }
    
    // History heuristic
    Color us = pos.side_to_move();
    return history_table[us][m.from_sq()][m.to_sq()];
}

static void order_moves(const Position& pos, MoveList& list, Move tt_move, int ply) {
    // Incremental selection sort: pick the best-scored move at each position.
    // This is efficient because alpha-beta often prunes early, so we rarely
    // need to sort the entire list.
    int scores[256];
    for (int i = 0; i < list.count; ++i) {
        scores[i] = move_score(pos, list.moves[i], tt_move, ply);
    }
    
    for (int i = 0; i < list.count - 1; ++i) {
        int best_idx = i;
        for (int j = i + 1; j < list.count; ++j) {
            if (scores[j] > scores[best_idx]) {
                best_idx = j;
            }
        }
        if (best_idx != i) {
            std::swap(list.moves[i], list.moves[best_idx]);
            std::swap(scores[i], scores[best_idx]);
        }
    }
}

// Generate only captures and promotions for quiescence search
static void generate_captures(const Position& pos, MoveList& list) {
    // Generate all moves, then filter to captures/promotions only
    MoveList all;
    generate_moves(pos, all);
    
    for (int i = 0; i < all.count; ++i) {
        Move m = all.moves[i];
        if (pos.piece_on(m.to_sq()) != NO_PIECE || 
            m.type() == EN_PASSANT || 
            m.type() == PROMOTION) {
            list.add(m);
        }
    }
}

// Quiescence Search: search captures until the position is quiet
static int quiescence(Position& pos, int alpha, int beta) {
    if (search_aborted) return 0;
    
    nodes_searched++;
    check_time();
    if (search_aborted) return 0;
    
    // Stand pat: the static evaluation as a baseline
    int stand_pat = evaluate(pos);
    
    if (stand_pat >= beta) return beta;     // Position is already too good
    if (stand_pat > alpha) alpha = stand_pat; // Raise alpha to stand pat
    
    MoveList list;
    generate_captures(pos, list);
    
    Color us = pos.side_to_move();
    
    for (int i = 0; i < list.count; ++i) {
        Move m = list.moves[i];
        
        // Delta pruning: skip captures that can't possibly raise alpha
        // (only for non-promotions)
        if (m.type() != PROMOTION) {
            Piece captured = pos.piece_on(m.to_sq());
            if (captured != NO_PIECE) {
                int delta = piece_values[type_of(captured)] + 200; // margin
                if (stand_pat + delta < alpha) continue;
            }
        }
        
        pos.make_move(m);
        
        // Legality check
        Bitboard king = pos.pieces(KING, us);
        if (king) {
            Square ksq = Square(__builtin_ctzll(king));
            if (is_attacked(pos, ksq, ~us)) {
                pos.undo_move(m);
                continue;
            }
        }
        
        int score = -quiescence(pos, -beta, -alpha);
        pos.undo_move(m);
        
        if (search_aborted) return 0;
        
        if (score > alpha) {
            alpha = score;
            if (alpha >= beta) return beta; // Beta cutoff
        }
    }
    
    return alpha;
}

// Alpha-Beta search with TT, NMP, LMR
int alpha_beta(Position& pos, int depth, int alpha, int beta, int ply, bool do_null) {
    if (search_aborted) return 0;
    
    nodes_searched++;
    check_time();
    if (search_aborted) return 0;
    
    // Drop into quiescence at the horizon
    if (depth <= 0) {
        return quiescence(pos, alpha, beta);
    }
    
    bool is_pv = (beta - alpha) > 1;
    bool is_root = (ply == 0);

    // --- Syzygy WDL Probe ---
    if (!is_root && !is_pv && Syzygy::TB_INITIALIZED && Syzygy::piece_count(pos) <= Syzygy::TB_LARGEST && pos.castling_rights() == 0) {
        unsigned wdl = Syzygy::probe_wdl(pos);
        if (wdl != TB_RESULT_FAILED) {
            int bound_score;
            if (wdl == TB_WIN) {
                bound_score = MATE_SCORE - 100 - ply;
                if (bound_score >= beta) return bound_score;
            } else if (wdl == TB_LOSS) {
                bound_score = -MATE_SCORE + 100 + ply;
                if (bound_score <= alpha) return bound_score;
            } else if (wdl == TB_DRAW || wdl == TB_CURSED_WIN || wdl == TB_BLESSED_LOSS) {
                if (0 >= beta) return 0;
                if (0 <= alpha) return 0;
                // If it's a draw and alpha < 0 < beta, return 0 (exact)
                return 0;
            }
        }
    }
    // --- Transposition Table Probe ---
    TTEntry tt_entry;
    Move tt_move = MOVE_NONE;
    bool tt_hit = TT.probe(pos.key(), tt_entry);
    
    if (tt_hit) {
        tt_move = Move(tt_entry.best_move);
        
        // Use the TT score if the depth is sufficient and we're not in a PV node
        if (!is_pv && tt_entry.depth >= depth) {
            int tt_score = int(tt_entry.score);
            if (tt_entry.bound == BOUND_EXACT) return tt_score;
            if (tt_entry.bound == BOUND_LOWER && tt_score >= beta) return tt_score;
            if (tt_entry.bound == BOUND_UPPER && tt_score <= alpha) return tt_score;
        }
    }
    
    bool in_chk = in_check(pos);
    
    // Check extension: if we're in check, extend by 1 to avoid horizon issues
    if (in_chk) depth++;
    
    // --- Null Move Pruning ---
    // If giving the opponent a free move still results in a beta cutoff,
    // the position is overwhelmingly strong and we can prune.
    if (do_null && !is_pv && !in_chk && depth >= 3) {
        // Don't do NMP in positions where we only have pawns + king (zugzwang risk)
        Color us = pos.side_to_move();
        bool has_non_pawn = (pos.pieces(KNIGHT, us) | pos.pieces(BISHOP, us) |
                             pos.pieces(ROOK, us) | pos.pieces(QUEEN, us)) != 0;
        if (has_non_pawn) {
            int R = 2 + depth / 4; // Adaptive reduction
            pos.make_null_move();
            int null_score = -alpha_beta(pos, depth - 1 - R, -beta, -beta + 1, ply + 1, false);
            pos.undo_null_move();
            
            if (search_aborted) return 0;
            if (null_score >= beta) return beta;
        }
    }
    
    // --- Move Generation and Search ---
    MoveList list;
    generate_moves(pos, list);
    order_moves(pos, list, tt_move, ply);
    
    int best_score = -INF;
    Move best_move = MOVE_NONE;
    bool has_legal_move = false;
    Color us = pos.side_to_move();
    int moves_searched = 0;
    
    for (int i = 0; i < list.count; ++i) {
        Move m = list.moves[i];
        
        pos.make_move(m);
        
        // Legality check
        Bitboard king = pos.pieces(KING, us);
        if (king) {
            Square ksq = Square(__builtin_ctzll(king));
            if (is_attacked(pos, ksq, ~us)) {
                pos.undo_move(m);
                continue;
            }
        }
        
        has_legal_move = true;
        moves_searched++;
        
        int score;
        bool is_capture = pos.piece_on(m.to_sq()) != NO_PIECE || m.type() == EN_PASSANT;
        bool is_promotion = m.type() == PROMOTION;
        bool gives_check = in_check(pos); // Check if the move gives check
        
        // --- Late Move Reductions (LMR) ---
        // Search late quiet moves at reduced depth. If they surprise us, re-search at full depth.
        if (moves_searched > 3 && depth >= 3 && !is_capture && !is_promotion && !in_chk && !gives_check) {
            // Logarithmic reduction
            int R = 1;
            if (moves_searched > 6) R = 2;
            if (moves_searched > 12) R = 3;
            
            // Reduced-depth search with a null window
            score = -alpha_beta(pos, depth - 1 - R, -alpha - 1, -alpha, ply + 1, true);
            
            // If the reduced search beats alpha, re-search at full depth
            if (score > alpha) {
                score = -alpha_beta(pos, depth - 1, -beta, -alpha, ply + 1, true);
            }
        } else {
            // Full-depth search
            score = -alpha_beta(pos, depth - 1, -beta, -alpha, ply + 1, true);
        }
        
        pos.undo_move(m);
        
        if (search_aborted) return best_score > -INF ? best_score : 0;
        
        if (score > best_score) {
            best_score = score;
            best_move = m;
        }
        if (best_score > alpha) {
            alpha = best_score;
        }
        if (alpha >= beta) {
            // Beta cutoff — update killer moves and history for quiet moves
            if (!is_capture && !is_promotion) {
                // Shift killers
                if (ply < 128) {
                    killers[ply][1] = killers[ply][0];
                    killers[ply][0] = m;
                }
                // Boost history
                history_table[us][m.from_sq()][m.to_sq()] += depth * depth;
                // Cap history values to prevent overflow
                if (history_table[us][m.from_sq()][m.to_sq()] > 80000) {
                    history_table[us][m.from_sq()][m.to_sq()] = 80000;
                }
            }
            break;
        }
    }
    
    if (!has_legal_move) {
        if (in_chk) {
            return -MATE_SCORE + ply;  // Checkmate (closer mates score higher)
        }
        return 0;  // Stalemate
    }
    
    // --- Store in Transposition Table ---
    TTBound bound;
    if (best_score >= beta) bound = BOUND_LOWER;
    else if (best_score > alpha - 1) bound = BOUND_EXACT; // PV node
    else bound = BOUND_UPPER;
    
    TT.store(pos.key(), best_score, depth, bound, best_move);
    
    return best_score;
}

SearchResult search_position(Position& pos, int depth, int time_limit_ms) {
    SearchResult res;
    res.best_move = MOVE_NONE;
    res.score = -INF;
    
    // Initialize time management
    search_start_time = std::chrono::steady_clock::now();
    search_time_limit = time_limit_ms;
    search_aborted = false;
    nodes_searched = 0;
    
    // Clear killer and history tables
    std::memset(killers, 0, sizeof(killers));
    // Don't fully clear history — let it accumulate across iterative deepening.
    // But age it down to prevent stale data from dominating.
    for (int c = 0; c < 2; ++c)
        for (int f = 0; f < 64; ++f)
            for (int t = 0; t < 64; ++t)
                history_table[c][f][t] /= 4;
    
    TT.new_search();
    
    // --- Syzygy Root DTZ Probe ---
    if (Syzygy::TB_INITIALIZED && Syzygy::piece_count(pos) <= Syzygy::TB_LARGEST && pos.castling_rights() == 0) {
        unsigned results[TB_MAX_MOVES];
        unsigned tb_res = Syzygy::probe_root(pos, results);
        
        if (tb_res != TB_RESULT_FAILED) {
            unsigned wdl = TB_GET_WDL(tb_res);
            int tb_score = 0;
            if (wdl == TB_WIN) tb_score = MATE_SCORE - 100;
            else if (wdl == TB_LOSS) tb_score = -MATE_SCORE + 100;
            
            unsigned from = TB_GET_FROM(tb_res);
            unsigned to = TB_GET_TO(tb_res);
            unsigned promotes = TB_GET_PROMOTES(tb_res);
            
            MoveList list;
            generate_moves(pos, list);
            for (int i = 0; i < list.count; ++i) {
                Move m = list.moves[i];
                if (m.from_sq() == from && m.to_sq() == to) {
                    bool match = true;
                    if (m.type() == PROMOTION) {
                        PieceType pt = m.promotion_type();
                        if (promotes == TB_PROMOTES_QUEEN && pt != QUEEN) match = false;
                        if (promotes == TB_PROMOTES_ROOK && pt != ROOK) match = false;
                        if (promotes == TB_PROMOTES_BISHOP && pt != BISHOP) match = false;
                        if (promotes == TB_PROMOTES_KNIGHT && pt != KNIGHT) match = false;
                    } else if (promotes != TB_PROMOTES_NONE) {
                        match = false;
                    }
                    if (match) {
                        res.best_move = m;
                        res.score = tb_score;
                        std::cout << "info depth 1 score cp " << tb_score << " string Syzygy hit\n";
                        return res;
                    }
                }
            }
        }
    }
    
    // Iterative Deepening with Aspiration Windows
    int prev_score = 0;
    
    for (int d = 1; d <= depth; d++) {
        int alpha, beta;
        
        // Aspiration window: use a narrow window around the previous score
        // for d >= 4. This speeds up the search by ~15-20% when the score is stable.
        if (d >= 4) {
            alpha = prev_score - 50;
            beta  = prev_score + 50;
        } else {
            alpha = -INF;
            beta  = INF;
        }
        
        int score = alpha_beta(pos, d, alpha, beta, 0, true);
        
        // If the aspiration window failed, re-search with full window
        if (!search_aborted && (score <= alpha || score >= beta)) {
            alpha = -INF;
            beta  = INF;
            score = alpha_beta(pos, d, alpha, beta, 0, true);
        }
        
        if (search_aborted) {
            // If we haven't completed any depth, use the TT best move
            if (res.best_move == MOVE_NONE) {
                TTEntry entry;
                if (TT.probe(pos.key(), entry) && entry.best_move != 0) {
                    res.best_move = Move(entry.best_move);
                    res.score = entry.score;
                }
            }
            break;
        }
        
        // Retrieve the best move from the TT for the root position
        TTEntry entry;
        if (TT.probe(pos.key(), entry) && entry.best_move != 0) {
            res.best_move = Move(entry.best_move);
            res.score = score;
        }
        prev_score = score;
        
        auto now = std::chrono::steady_clock::now();
        int elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - search_start_time).count();
        int nps = elapsed > 0 ? (nodes_searched * 1000 / elapsed) : 0;
        
        std::cout << "info depth " << d << " score cp " << score
                  << " nodes " << nodes_searched
                  << " nps " << nps
                  << " time " << elapsed << std::endl;
    }
    
    return res;
}
