#include "evaluate.h"
#include "nnue.h"

int evaluate(const Position& pos) {
    // Fast path: use the incrementally-maintained accumulator.
    // evaluate_nnue_from_acc only runs layers 2-4 (not layer 1),
    // making evaluation ~10x faster than a full refresh.
    int score = evaluate_nnue_from_acc(pos.accumulator());

    // The network outputs an absolute score (positive = White advantage).
    // The search uses negamax, so we flip for Black.
    return (pos.side_to_move() == WHITE) ? score : -score;
}
