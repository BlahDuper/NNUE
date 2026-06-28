# Comprehensive NNUE Development Roadmap for Chess

This document outlines a 12-week development plan for building an Efficiently Updatable Neural Network (NNUE) inference engine from scratch, designed for chess evaluation. It covers the data pipeline, model training, and low-level hardware optimization required for state-of-the-art performance.

## Phase 1: Engine Fundamentals (Weeks 1–2)
Before implementing an NNUE, a testing environment is required. If a standalone inference library is the goal, this phase can be bypassed by mocking engine inputs, but full integration requires standard chess engine components.

* **Board Representation:** Implement Bitboards for tracking piece positions efficiently.
* **Move Generation:** Develop legal or pseudo-legal move generation algorithms.
* **Search Framework:** Implement a standard Alpha-Beta pruning search tree to navigate game states.

## Phase 2: Data Engineering (Week 3)
The NNUE requires millions of evaluated chess positions. 

* **Dataset Acquisition:** Utilize open-source datasets (e.g., Stockfish or Leela binpacks).
* **Data Parsing:** Write a Python parser to read the binary data and extract positions (FENs) and their corresponding evaluations.
* **Feature Representation:** Convert the board states into the `HalfKP` sparse binary format (768 features: 6 piece types × 2 colors × 64 squares).
* **Batching:** Optimize the I/O pipeline using PyTorch DataLoaders to handle datasets that exceed available system RAM.

## Phase 3: Model Training with PyTorch (Weeks 4–5)
Develop the architecture and train the model using quantization.

* **Network Architecture:**
    * Input: 768 sparse binary features.
    * Accumulator (Hidden Layer 1): 256 neurons (maintaining separate White and Black perspectives).
    * Hidden Layers 2 & 3: 32 neurons each.
    * Output: 1 scalar (centipawn evaluation).
* **Quantization-Aware Training (QAT):** Apply scaling factors (QA, QB) to restrict weights to 16-bit integers and activations to 8-bit integers during training.
* **Loss Function:** Utilize Mean Squared Error (MSE) between network output and target engine evaluation.
* **Serialization:** Export the converged integer weights and biases into a flat `.bin` binary file.

## Phase 4: C++ Inference Engine (Weeks 6–8)
This phase focuses on high-performance memory management and mathematics.

* **Memory Architecture:** Write a C++ loader to read the `.bin` file into memory-aligned arrays (32-byte or 64-byte alignment) to optimize CPU cache usage.
* **The Accumulator:** * Implement $O(1)$ time complexity updates.
    * When a move occurs, update the 256-dimension vector by adding the weights of the arriving piece and subtracting the weights of the departing piece (`accumulatorAdd`, `accumulatorSub`).
* **SIMD Vectorization:** * Replace standard C++ matrix-multiplication loops with AVX2 or AVX-512 intrinsic instructions (e.g., `_mm256_add_epi16`).
    * Process chunks of 16-bit integers in parallel to reduce dense hidden-layer execution latency.

## Phase 5: Advanced Optimization Techniques (Weeks 9–12)
Implement modern techniques to improve network accuracy and computational efficiency.

* **Squared Clipped ReLU (SCReLU):** Replace standard CReLU to increase non-linearity. Requires manual AVX2 implementation to manage 32-bit register overflow during the squaring phase.
* **King Input Buckets:** Divide the board into regions and train separate accumulator weights based on the King's location. Manage the $O(n)$ state invalidation penalty when the King crosses bucket boundaries.
* **Horizontal Mirroring:** Mathematically flip the board when the King is on the Kingside. Halves the required input parameters and increases training data density.
* **Feature Factorization:** Train a 'shadow' bucket for universal chess principles in PyTorch, then coalesce its weights into the King buckets before exporting the `.bin` file, achieving an Elo increase with zero runtime overhead.
* **Big-Little Routing:** (Optional) Implement dual networks. Route quiet positions to a compressed network for speed, and volatile positions to a heavily parameterized network for accuracy.

## Resume Integration Guidelines
Focus on specific engineering accomplishments when adding this to a professional portfolio:
* Highlight the transition from $O(n)$ to $O(1)$ calculations via incremental state updates.
* Specify hardware-level optimizations (SIMD intrinsics, memory alignment, cache efficiency).
* Emphasize the management of large-scale binary datasets and integer quantization techniques.
