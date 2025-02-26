cmake_minimum_required(VERSION 3.5)
project(pgn2pgc CXX)

add_executable(pgn2pgc pgnpgc3.cpp
    chess_2.cpp
# memcheck.cpp
    stpwatch.cpp
    strparse.cpp
)

add_executable(test_list list5.cpp)
target_compile_definitions(test_list PRIVATE TEST)
