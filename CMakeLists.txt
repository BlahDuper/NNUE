cmake_minimum_required(VERSION 3.10)
project(NNUE_Chess CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Enable optimizations for performance
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -march=native -Wall -Wextra")

set(SOURCES
    src/main.cpp
    src/bitboard.cpp
    src/position.cpp
    src/movegen.cpp
    src/evaluate.cpp
    src/search.cpp
)

add_executable(nnue_chess ${SOURCES})
