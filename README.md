# NNUE Chess Engine

A fully featured, from-scratch UCI chess engine written in C++, featuring a custom-trained NNUE (Efficiently Updatable Neural Network) evaluation function and perfect endgame play via Syzygy tablebases. It includes a beautiful web-based graphical user interface (GUI) built with Node.js and Socket.io.

## Features

### Core Engine (C++)
- **UCI Protocol Support**: Compatible with all major chess GUIs (Arena, LucasChess, CuteChess, etc.).
- **Move Generation**: Highly optimized using Magic Bitboards.
- **Search Capabilities**:
  - Alpha-Beta Pruning framework with Iterative Deepening
  - Transposition Tables (TT) for fast memoization
  - Quiescence Search with Delta Pruning to resolve tactical sequences
  - Null Move Pruning (NMP)
  - Late Move Reductions (LMR)
  - Killer Heuristics & History Heuristics for move ordering
  - Aspiration Windows and Check Extensions

### Neural Network Evaluation (NNUE)
- **HalfKP Architecture**: Evaluates positions based on King-Piece relationships.
- **Incremental Updates**: Uses an accumulator to update the network state incrementally during `make_move` and `undo_move`, making it up to 10x faster than full recalculations at leaf nodes.
- **Custom Trained**: Network weights were trained using PyTorch on a custom dataset, producing a resilient positional evaluation that far exceeds traditional hand-crafted evaluation functions.

### Endgame Tablebases
- **Syzygy Support**: Native integration with the famous **Fathom** library.
- **Root DTZ Probing**: Immediately plays the theoretically perfect move (shortest path to mate or longest defense) if the position is found in the tablebases.
- **Search WDL Probing**: Hard bounds the search tree by querying the Win/Draw/Loss table during alpha-beta search, instantly solving any endgame branches without deep calculation.

### Web GUI
- **Full-Stack Interface**: A sleek web interface running on Node.js and Socket.io.
- **Features**: 
  - Play as White or Black against the engine.
  - Interactive board using `chess.js` and `chessboard.js`.
  - Real-time engine analysis (Depth, Centipawn Score, Nodes, NPS).
  - Move history (SAN format) and captured pieces tracker.
  - Configurable engine "think time".

## Getting Started

### Prerequisites
- **C++ Compiler**: GCC (MinGW on Windows) with C++17 support.
- **Node.js**: Required to run the web interface.

### Building the Engine
To compile the C++ engine, run the provided build script:
```bash
./build.bat
```
This will compile all sources (including Fathom for Syzygy support) into `nnue_chess.exe`.

### Running the Web GUI
1. Navigate to the `gui` folder.
2. Install dependencies:
   ```bash
   npm install
   ```
3. Start the server:
   ```bash
   node server.js
   ```
4. Open your browser and navigate to `http://localhost:3000` to play against the engine!

## Using with Standard GUIs (Arena, LucasChess, etc.)
Because the engine fully supports the UCI protocol, you can load `nnue_chess.exe` directly into any UCI-compatible GUI. 

**To enable Syzygy Tablebases in a GUI:**
Configure the `SyzygyPath` UCI parameter in the engine settings to point to the directory containing your `.rtbw` and `.rtbz` files (e.g., `C:\tablebases\3-4-5`).

## Architecture
- `src/`: C++ engine source code (Search, Movegen, Bitboards, NNUE inference, Syzygy wrapper).
- `src/tbprobe.cpp` / `tbprobe.h`: The Fathom library for endgame tablebases.
- `gui/`: Node.js server and static web assets (`index.html`, `app.js`, `style.css`).
- `model/`: Contains the compiled `.bin` network weights generated from the PyTorch training pipeline.
- `Colab_Training.ipynb`: Google Colab notebook used to train the HalfKP NNUE model from raw PGN/FEN datasets.
