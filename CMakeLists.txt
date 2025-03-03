cmake_minimum_required(VERSION 3.5)
project(pgn2pgc CXX)

set(CMAKE_BUILD_TYPE Release)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_FLAGS         "${CMAKE_CXX_FLAGS}         -Wall  -Wextra -Wpedantic ")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -ggdb  -Ofast -march=native")
#set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}  -flto=auto ")
set(CMAKE_CXX_FLAGS_DEBUG   "${CMAKE_CXX_FLAGS_DEBUG}   -g     -O0 -fno-omit-frame-pointer")
set(CMAKE_CXX_FLAGS_DEBUG   "${CMAKE_CXX_FLAGS_DEBUG}   -fsanitize=address -fsanitize=undefined")

add_executable(pgn2pgc pgnpgc3.cpp
    chess_2.cpp
    stpwatch.cpp
)

add_executable(test_chess2 chess_2.cpp
    stpwatch.cpp
)
target_compile_definitions(test_chess2 PRIVATE TEST)
# target_link_options(test_chess2 PRIVATE -flto=auto)
