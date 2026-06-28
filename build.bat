@echo off
echo Compiling Engine with Fathom Syzygy...
g++ -O3 -march=native -std=c++17 -DTB_NO_THREADS -Isrc ^
src\main.cpp ^
src\bitboard.cpp ^
src\position.cpp ^
src\movegen.cpp ^
src\evaluate.cpp ^
src\search.cpp ^
src\uci.cpp ^
src\san.cpp ^
src\tt.cpp ^
src\nnue.cpp ^
src\zobrist.cpp ^
src\syzygy.cpp ^
src\tbprobe.cpp ^
-o nnue_chess.exe

if %errorlevel% neq 0 (
    echo Compilation FAILED!
    exit /b %errorlevel%
)

echo Compilation SUCCESSFUL!
echo Run nnue_chess.exe to start the engine.
