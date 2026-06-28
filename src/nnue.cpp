#include "nnue.h"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <cstring>

NNUE_Weights nnue_weights;

bool init_nnue(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open NNUE weights file: " << filepath << std::endl;
        return false;
    }
    file.read(reinterpret_cast<char*>(&nnue_weights), sizeof(NNUE_Weights));
    if (!file) {
        std::cerr << "Failed to read full NNUE weights from: " << filepath << std::endl;
        return false;
    }
    return true;
}

// Convert (piece, color, square) -> feature index [0, 767]
// Matches the Python halfkp training logic exactly.
int make_feature_index(int piece, int color, int square) {
    // Python uses True (1) for White, False (0) for Black — invert our C++ color
    int python_color = (color == 0) ? 1 : 0;
    int p_idx = 0;
    switch (piece) {
        case PAWN:   p_idx = 0; break;
        case KNIGHT: p_idx = 1; break;
        case BISHOP: p_idx = 2; break;
        case ROOK:   p_idx = 3; break;
        case QUEEN:  p_idx = 4; break;
        case KING:   p_idx = 5; break;
    }
    return (python_color * 6 + p_idx) * 64 + square;
}

// -------------------------------------------------------------------------
// Accumulator helpers — these are the hot path for incremental updates
// -------------------------------------------------------------------------

void accumulator_add_feature(Accumulator& acc, int feature_idx) {
    const int16_t* col = nnue_weights.fc1_w[0] + feature_idx; // wrong layout — see below
    // fc1_w is [LAYER1_SIZE][FEATURE_SIZE], so row i, col j = fc1_w[i][j]
    // We want: for each output neuron i, add fc1_w[i][feature_idx]
    for (int i = 0; i < LAYER1_SIZE; i++)
        acc.values[i] += nnue_weights.fc1_w[i][feature_idx];
}

void accumulator_sub_feature(Accumulator& acc, int feature_idx) {
    for (int i = 0; i < LAYER1_SIZE; i++)
        acc.values[i] -= nnue_weights.fc1_w[i][feature_idx];
}

// Full refresh — called from set_fen and as a fallback
void refresh_accumulator(Accumulator& acc, const int16_t* /*unused*/, const int16_t* /*unused*/,
                         const uint64_t* piece_bbs, int /*unused*/) {
    // Start with biases
    for (int i = 0; i < LAYER1_SIZE; i++)
        acc.values[i] = nnue_weights.fc1_b[i];

    // Add weights for every piece on the board
    for (int color = WHITE; color <= BLACK; color++) {
        for (int piece = PAWN; piece <= KING; piece++) {
            uint64_t bb = piece_bbs[color * 6 + piece];
            while (bb) {
                int sq = __builtin_ctzll(bb);
                accumulator_add_feature(acc, make_feature_index(piece, color, sq));
                bb &= bb - 1;
            }
        }
    }
}

// -------------------------------------------------------------------------
// Forward pass through layers 2, 3, and output
// -------------------------------------------------------------------------
int evaluate_nnue_from_acc(const Accumulator& acc) {
    // Layer 1 -> 2
    int32_t l2[LAYER2_SIZE];
    for (int i = 0; i < LAYER2_SIZE; i++) {
        l2[i] = nnue_weights.fc2_b[i];
        for (int j = 0; j < LAYER1_SIZE; j++) {
            int16_t act = std::max((int16_t)0, std::min(acc.values[j], (int16_t)QA));
            l2[i] += (int32_t)act * nnue_weights.fc2_w[i][j];
        }
        l2[i] /= QA;
    }

    // Layer 2 -> 3
    int32_t l3[LAYER3_SIZE];
    for (int i = 0; i < LAYER3_SIZE; i++) {
        l3[i] = nnue_weights.fc3_b[i];
        for (int j = 0; j < LAYER2_SIZE; j++) {
            int32_t act = std::max((int32_t)0, std::min(l2[j], (int32_t)QB));
            l3[i] += act * nnue_weights.fc3_w[i][j];
        }
        l3[i] /= QB;
    }

    // Layer 3 -> output
    int32_t out = nnue_weights.fc4_b[0];
    for (int j = 0; j < LAYER3_SIZE; j++) {
        int32_t act = std::max((int32_t)0, std::min(l3[j], (int32_t)QB));
        out += act * nnue_weights.fc4_w[0][j];
    }
    out /= QB;

    // Convert quantized logit to centipawns
    return (174 * out) / QB;
}

// Slow full evaluation — only kept for compatibility / testing
int evaluate_nnue_full(const uint64_t* piece_bbs, int /*num*/, bool white_to_move) {
    Accumulator acc;
    refresh_accumulator(acc, nullptr, nullptr, piece_bbs, 0);
    int score = evaluate_nnue_from_acc(acc);
    return white_to_move ? score : -score;
}
