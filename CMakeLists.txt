project(pgn2pgc CXX)
cmake_minimum_required(VERSION 3.5)

add_executable(pgn2pgc pgnpgc3.cpp
chess_2.cpp
#memcheck.cpp
stpwatch.cpp
strparse.cpp
)
