#ifndef NNUE_H
#define NNUE_H

#include "types.h"
#include <cstdint>
#include <string>

// NNUE Architecture Constants
constexpr int FEATURE_SIZE = 768;
constexpr int LAYER1_SIZE  = 256;
constexpr int LAYER2_SIZE  = 32;
constexpr int LAYER3_SIZE  = 32;

// Quantization Constants
constexpr int QA = 255;
constexpr int QB = 64;

#pragma pack(push, 1)
struct NNUE_Weights {
    int16_t fc1_w[LAYER1_SIZE][FEATURE_SIZE];
    int16_t fc1_b[LAYER1_SIZE];

    int8_t  fc2_w[LAYER2_SIZE][LAYER1_SIZE];
    int32_t fc2_b[LAYER2_SIZE];

    int8_t  fc3_w[LAYER3_SIZE][LAYER2_SIZE];
    int32_t fc3_b[LAYER3_SIZE];

    int8_t  fc4_w[1][LAYER3_SIZE];
    int32_t fc4_b[1];
};
#pragma pack(pop)

// The Layer-1 hidden state — kept in a stack inside Position for incremental updates
struct Accumulator {
    int16_t values[LAYER1_SIZE];
};

// Load weights from disk
bool init_nnue(const std::string& filepath);

// Convert a (piece, color, square) triplet to its feature index [0, 767]
int make_feature_index(int piece, int color, int square);

// Fully rebuild the accumulator from scratch (used after set_fen and as a fallback)
void refresh_accumulator(Accumulator& acc, const int16_t* weights_row_major,
                         const int16_t* biases, const uint64_t* piece_bbs, int num_piece_bbs);

// Apply a single feature add/sub to an accumulator (for incremental update)
void accumulator_add_feature(Accumulator& acc, int feature_idx);
void accumulator_sub_feature(Accumulator& acc, int feature_idx);

// Evaluate using an already-computed accumulator (fast path)
int evaluate_nnue_from_acc(const Accumulator& acc);

// Full evaluate — rebuilds accumulator then runs forward pass (slow, only for testing)
int evaluate_nnue_full(const uint64_t* piece_bbs, int num_piece_bbs, bool white_to_move);

#endif
